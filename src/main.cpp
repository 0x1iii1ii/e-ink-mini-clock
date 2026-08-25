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

unsigned long lastRefresh = 0;
unsigned long lastSensorRead = 0;

uint8_t g_batteryPct = 0;
bool    g_isVbusConnected = false;
bool    g_powerSaveMode = true;

bool lastB1 = HIGH;
bool lastB2 = HIGH;

// ════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════
void setup() {
  Serial0.begin(115200);
  delay(500);
  // ── Load config from LittleFS ─────────────────────────
  init_fs();
  rtcNvBootCount++;

  Serial0.println("\n=== ★ e-ink Mini Clock ★ ===");
  Serial0.printf("Firmware version: v%s\n", FW_VERSION);
  Serial0.printf("build: %s %s\n", __DATE__, __TIME__);
  Serial0.println("Github: https://github.com/0x1iii1ii");
  Serial0.println("Facebook: https://www.facebook.com/liisengxyz");
  Serial0.printf("\n=== ePaper Clock  wake #%u ===\n", rtcNvBootCount);
  Serial0.printf("Wakeup cause: %d\n", esp_sleep_get_wakeup_cause());

  // ── I2C initialization ────────────────────────────────
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Serial0.println("I2C initialized.");
  delay(100);

#ifdef DEBUG
  // Optional I2C scan (debug)
  Serial0.println("Scanning I2C sensor devices...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
      Serial0.printf("Found: 0x%02X\n", addr);
  }
#endif

  // ── Periperial initialization ───────────────────────────────
  aht20_init();
  delay(100);
  rtc_init();
  delay(100);
  epd_init();

  // I/O pin setup
  pinMode(B1_PIN, INPUT);
  pinMode(B2_PIN, INPUT);
  pinMode(BZ_PIN, OUTPUT);
  pinMode(VBUS_PIN, INPUT);

  // deepsleep pin trigger to wakeup or wake on both user button press
  esp_deep_sleep_enable_gpio_wakeup(
    (1ULL << B1_PIN) | (1ULL << B2_PIN),
    ESP_GPIO_WAKEUP_GPIO_LOW
  );
  // Wake on VBUS going HIGH 
  esp_deep_sleep_enable_gpio_wakeup(
    (1ULL << VBUS_PIN),
    ESP_GPIO_WAKEUP_GPIO_HIGH
  );

  delay(100);
  // will only play startup sound if not waking from deep sleep
  if (getResetReason() != ESP_RST_DEEPSLEEP) {
    startUpSound();
  }

  // ── Optional boot splash screen ───────────────────────
  // if (g_enSplash) boot_splash();

  // ── Read sensors ──────────────────────────────────────
  g_batteryPct = readBattery();
  g_isVbusConnected = isVbusConnected();

  // ── First boot / factory state ────────────────────────
  if (strlen(cfg.wifi.wifi->ssid) == 0 || strlen(cfg.wifi.wifi->password) == 0) {
    Serial0.println("No WiFi configured — entering setup mode");
    enterPortalMode(true);
  }
  // both user buttons held down on boot => enter setup mode
  if (digitalRead(B1_PIN) == LOW && digitalRead(B2_PIN) == LOW) {
    Serial0.println("Both buttons held — entering setup mode");
    enterPortalMode(true, true);
  }

  // ── Power saving mode ───────────────────────────────────
  if (!cfg.device.powerSave) {
    Serial0.println("Power save mode: OFF — full features enabled");
    g_powerSaveMode = false;
    // ── WiFi initialization ──────────────────────────────
    if (wifi_init()) web_init(); // start web server only if WiFi connected
    // ── First draw ────────────────────────────────────────
    drawDisplay();
  }
  else {
    // ── NTP sync (WiFi only when needed) ─────────────────
    if (shouldSyncNtp()) {
      doNtpSync();
    }
    else {
      // Restore system clock from RTC (no WiFi needed)
      // restore_rtc();
      Serial0.println("Clock from RTC — skipped sync");
    }

    // ── Draw display ─────────────────────────────────────
    drawDisplay();

    // ── spawn the web for the first time power on ───────────────
    if (rtcNvBootCount == 1) {
      Serial0.println("First boot — entering portal mode");
      enterPortalMode();   // blocks for 60 s then sleeps
    }
    else {
      switch (esp_sleep_get_wakeup_cause()) {

        // wake by timer itself (normal operation)
      case ESP_SLEEP_WAKEUP_TIMER:
        Serial0.println("Wakeup cause: Timer");
        Serial0.println("going to sleep");
        goToDeepSleep();
        break;

        // user want to enter portal mode by pressing the button
      case ESP_SLEEP_WAKEUP_GPIO:
        Serial0.println("Wakeup cause: GPIO");
        enterPortalMode();   // blocks for 60 s then sleeps
        break;

        // nothing idk
      default:
        Serial0.println("going to sleep");
        goToDeepSleep();
        break;
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════
void loop() {
  if (!g_powerSaveMode) {
    maintainWifi();
    web_loop();

    // button
    bool b1 = digitalRead(B1_PIN);
    bool b2 = digitalRead(B2_PIN);

    // Normal mode button handling
    if (lastB1 == HIGH && b1 == LOW) {
      Serial0.println("B1 Pressed");
      beep(2500, 100);
    }

    if (lastB2 == HIGH && b2 == LOW) {
      Serial0.println("B2 Pressed");
      beep(1200, 80);
      delay(50);
      beep(1200, 80);
    }

    lastB1 = b1;
    lastB2 = b2;

    // Read sensors every second for smooth display updates
    unsigned long ms = millis();
    if (ms - lastSensorRead >= 60000UL) {
      lastSensorRead = ms;
      aht20_read();
      g_batteryPct = readBattery();
      g_isVbusConnected = isVbusConnected();
      Serial0.println("\nUpdated Sensor Values: \n");
      Serial0.printf("AHT20: %.1f °C, %.1f %%RH\n", g_temperature, g_humidity);
      Serial0.printf("Battery: %d%%  VBUS: %d\n", g_batteryPct, g_isVbusConnected);
    }

    // Refresh display on schedule
    if (ms - lastRefresh >= static_cast<unsigned long>(cfg.display.refreshMin) * 60000UL) {
      lastRefresh = ms;
      DateTime d = rtc.now();
      lastRefreshEpoch = d.unixtime();
      drawDisplay();
    }

    if (!isVbusConnected() && cfg.device.powerSave) {
      Serial0.println("USB power lost — entering power save mode");
      g_powerSaveMode = true;
      goToDeepSleep();
    }
  }
  delay(1);
}
