/**
 *
 * ============================================================
 *  ESP8266 + Waveshare 2.66" e-Paper (G)  —  E-ink Mini Clock
 *
 *  Author: @liiseng
 *  Assisted by @claude
 *  Created: 8-05-2026
 *
 *  Panel:    360 x 184 px,  4-colour  Black / White / Yellow / Red (SSD1680)
 *  Driver:   Waveshare official epd2in66g library (included)
 *  MCU:      ESP32-C3
 *  RTC:      PCF8563T  (I²C)
 *  Sensor:   AHT20     (I²C)
 *
 *  FEATURES
 *  ─────────────────────────────────────────────────────────
 *  • Show time, date, weekday
 *  • Precise timekeeping with NTP sync + PCF8563 backup RTC
 *  • Measure temperature & humidity with AHT20 sensor
 *  • Wi-Fi connectivity for NTP sync and web config portal
 *  • Web config portal http://eink-clock.local
 *  • Rechargeable Li-Po Battery 1200mAh
 *  • USB-C port for charging and programming (HW CDC)
 *  • OTA firmware updates (future)
 *
 *  POWER STRATEGY
 *  ─────────────────────────────────────────────
 *  Deep sleep between display refreshes + optional web server shutdown for power saving
 *  Deep sleep during inavtive periods (e.g. night time)
 *  Active window  ≈  5–10 s  (draw + optional NTP + sensors)
 *  Sleep current  ≈  ~5 µA  (ESP32-C3 deep sleep)
 *  Display holds image with zero power after refresh.
 *
 *  WAKE SOURCES
 *  ────────────
 *  1. Timer  — fires every cfg.refreshMin minutes (primary)
 *  2. User Button  — held down -> clock wake -> web portal up for 1 min then deep sleep
 *
 *  NTP SYNC SCHEDULE
 *  ──────────────────
 *  Normal  : sync once per week
 *  On fail : retry every x-hour (user config) until success, then back to weekly
 *  Sync state persisted in RTC NVRAM (survives deep sleep, not power loss)
 *  Fallback: PCF8563 keeps time during WiFi-off wakes
 * ============================================================
 *
 */

#include "global.h"
#include "config.h"
#include "wifi.h"
#include "time_sync.h"
#include "display.h"
#include "sensors.h"
#include "web.h"
#include "battery.h"
#include "button.h"
#include "system.h"
 // #include "WebSerial.h"

unsigned long lastRefresh = 0;
unsigned long lastSensorRead = 0;

uint8_t g_batteryPct = 0;
bool    g_isVbusConnected = false;
bool    g_powerSaveMode = true;

bool lastB1 = HIGH;
bool lastB2 = HIGH;

void beep(int freq, int duration) {
  tone(BZ_PIN, freq);
  delay(duration);
  noTone(BZ_PIN);
}
void alarmSound() {
  Serial.println("=== ALARM ===");

  unsigned long start = millis();

  while (millis() - start < 3000) {
    // Beep Beep
    tone(BZ_PIN, 2700);
    delay(90);
    noTone(BZ_PIN);
    delay(40);

    tone(BZ_PIN, 2700);
    delay(90);
    noTone(BZ_PIN);
    delay(180);
  }
}
void alarmSound2() {
  unsigned long start = millis();

  while (millis() - start < 5000) {
    tone(BZ_PIN, 2400);
    delay(80);
    tone(BZ_PIN, 3000);
    delay(80);
    noTone(BZ_PIN);
    delay(120);
  }
}
// EPD_266 epd2;
// ════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  // Short delay only on first boot; skip on subsequent wakes to save time
  // if (rtcNvBootCount == 0) delay(2000);
  delay(3000);  // wait for serial monitor

  // ── Load config from LittleFS ─────────────────────────
  init_fs();
  rtcNvBootCount++;

  Serial.println("\n=== e-ink Mini Clock booting... ===");
  Serial.printf("Firmware version: %s\n", FW_VERSION);
  Serial.printf("build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("\n=== ePaper Clock  wake #%u ===\n", rtcNvBootCount);

  // ── I2C initialization ────────────────────────────────
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Serial.println("I2C initialized.");
  delay(100);

#ifdef DEBUG
  // Optional I2C scan (debug)
  Serial.println("Scanning I2C sensor devices...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
      Serial.printf("Found: 0x%02X\n", addr);
  }
#endif

  // ── Periperial initialization ───────────────────────────────
  aht20_init();
  delay(100);
  rtc_init();
  delay(100);
  // epd2.begin();
  epd_init();
  // if (!epd_init()) {
  //   Serial.println("e-Paper init failed — going to sleep");
  //   goToDeepSleep();
  // }
  pinMode(B1_PIN, INPUT);
  pinMode(B2_PIN, INPUT);
  pinMode(BZ_PIN, OUTPUT);
  delay(100);
  beep(1200, 80);
  delay(30);
  beep(1800, 80);
  delay(30);
  beep(2400, 120);
  // ── GPIO (charger / VBUS sense) ──────────────────────
  pinMode(USER_BUTTON, INPUT);
  pinMode(VBUS_PIN, INPUT);

  // ── Optional boot splash screen ───────────────────────
  // if (g_enSplash) boot_splash();

  // ── Read sensors (fast, no WiFi needed) ──────────────
  g_batteryPct = readBattery();
  g_isVbusConnected = isVbusConnected();

  // ── First boot / factory state ────────────────────────
  if (strlen(cfg.wifi->ssid) == 0 || strlen(cfg.wifi->password) == 0) {
    Serial.println("No WiFi configured — entering portal mode");
    enterPortalMode(true);
  }

  // ── Power saving mode ───────────────────────────────────
  if (!cfg.clockCfg.powerSave) {
    Serial.println("Power save mode: OFF — full features enabled");
    g_powerSaveMode = false;
    // ── WiFi initialization ──────────────────────────────
    if (wifi_init()) web_init(); // start web server only if WiFi connected
    // ── First draw ────────────────────────────────────────
    drawDisplay();
  }
  else {
    // ── Portal mode check (GPIO0/BOOT held LOW) ───────────
    // if (WakeButtonHeld()) {
    //   Serial.println("Portal button held — entering setup mode");
    //   enterPortalMode();
    // }
    // ── NTP sync (WiFi only when needed) ─────────────────
    if (shouldSyncNtp()) {
      doNtpSync();
    }
    else {
      // // Restore system clock from RTC (no WiFi needed)
      // restore_rtc();
      Serial.println("Clock from RTC — skipped sync");
    }
    // ── Draw display ─────────────────────────────────────
    drawDisplay();
    // ── spawn the web for the first power on ───────────────
    if (rtcNvBootCount == 1) {
      Serial.println("First boot — entering portal mode");
      enterPortalMode();   // blocks for 60 s then sleeps
    }
    else {
      Serial.println("going to sleep");
      goToDeepSleep();
    }
    // // ── Go back to sleep ─────────────────────────────────
    // goToDeepSleep();
  }
}

// void drawScreen0() {
//   epd2.clear();
//   epd2.setRotation(ROT_180);
//   epd2.drawString(10, 10, "Screen 0", LAYER_BW);
//   epd2.drawString(10, 30, "Normal orientation", LAYER_RED);
//   epd2.drawRect(5, 50, 100, 40, LAYER_RED);
//   epd2.drawLine(0, 0, 150, 80, LAYER_BW);
//   epd2.displayFull();
// }

// ════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════
void loop() {
  bool b1 = digitalRead(B1_PIN);
  bool b2 = digitalRead(B2_PIN);

  // Both buttons pressed
  if (b1 == LOW && b2 == LOW) {
    alarmSound2();

    // Wait until both buttons are released
    while (digitalRead(B1_PIN) == LOW || digitalRead(B2_PIN) == LOW) {
      delay(10);
    }

    lastB1 = HIGH;
    lastB2 = HIGH;
    return;
  }

  if (lastB1 == HIGH && b1 == LOW) {
    Serial.println("B1 Pressed");
    beep(2500, 100);
  }

  if (lastB2 == HIGH && b2 == LOW) {
    Serial.println("B2 Pressed");
    beep(1200, 80);
    delay(50);
    beep(1200, 80);
  }

  lastB1 = b1;
  lastB2 = b2;

  if (!g_powerSaveMode) {
    maintainWifi();
    web_loop();
    unsigned long ms = millis();
    // Read sensors every second for smooth display updates
    if (ms - lastSensorRead >= 60000UL) {
      lastSensorRead = ms;
      aht20_read();
      // drawScreen0();
      drawDisplay();
      g_batteryPct = readBattery();
      g_isVbusConnected = isVbusConnected();
      Serial.println("\nUpdated Sensor Values: \n");
      Serial.printf("AHT20: %.1f °C, %.1f %%RH\n", g_temperature, g_humidity);
      Serial.printf("Battery: %d%%  VBUS: %d\n", g_batteryPct, g_isVbusConnected);
    }

    // Refresh display on schedule
    if (ms - lastRefresh >= static_cast<unsigned long>(cfg.clockCfg.refreshMin) * 60000UL) {
      lastRefresh = ms;
      // sync_time();
      DateTime d = rtc.now();
      lastRefreshEpoch = d.unixtime();
      // drawDisplay();
    }

    if (!isVbusConnected() && cfg.clockCfg.powerSave) {
      Serial.println("USB power lost — entering power save mode");
      g_powerSaveMode = true;
      goToDeepSleep();
    }
  }
  delay(1);
}
