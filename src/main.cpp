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
#include "NimBLEDevice.h"
 // #include "WebSerial.h"

#define SERVICE_UUID     "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_CONFIG_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_STATUS_UUID "6e400005-b5a3-f393-e0a9-e50e24dcca9e"

unsigned long lastRefresh = 0;
unsigned long lastSensorRead = 0;

uint8_t g_batteryPct = 0;
bool    g_isVbusConnected = false;
bool    g_powerSaveMode = true;

// ===== ALARM STATE =====
bool    g_alarmTriggered = false;    // alarm just triggered
bool    g_alarmSettingMode = false;  // in alarm set mode
uint8_t g_alarmEditIdx = 0;          // 0=hour, 1=minute
unsigned long g_alarmLastTrigger = 0; // timestamp of last alarm trigger (prevent re-trigger)

// esp_sleep_wakeup_cause_t wakeUpCause = ESP_SLEEP_WAKEUP_UNDEFINED;

bool lastB1 = HIGH;
bool lastB2 = HIGH;

NimBLECharacteristic* statusChar;
String pendingSsid, pendingPass;
volatile bool connectRequested = false;

void setStatus(const String& state, const String& ip = "") {
  JsonDocument doc;
  doc["state"] = state;
  if (ip.length()) doc["ip"] = ip;
  String out;
  serializeJson(doc, out);
  statusChar->setValue(out.c_str());
  statusChar->notify();
}

class ConfigCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    JsonDocument doc;
    if (deserializeJson(doc, c->getValue().c_str())) {
      setStatus("bad_json");
      return;
    }
    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    String cmd = doc["cmd"] | "";
    if (ssid.length()) pendingSsid = ssid;
    if (pass.length()) pendingPass = pass;
    if (cmd == "connect") connectRequested = true;
  }
};
class ServerCB : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    NimBLEDevice::getAdvertising()->start(); // resume advertising after disconnect
  }
};

void bleConfigBegin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setMTU(247); // negotiated with the client, not guaranteed — see loop() note
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  NimBLEService* svc = server->createService(SERVICE_UUID);

  svc->createCharacteristic(CHAR_CONFIG_UUID, NIMBLE_PROPERTY::WRITE)
    ->setCallbacks(new ConfigCB());

  statusChar = svc->createCharacteristic(
    CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  setStatus("idle");

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->start();
}

void beep(int freq, int duration) {
  tone(BZ_PIN, freq);
  delay(duration);
  noTone(BZ_PIN);
}
void alarmSound() {
  Serial0.println("=== ALARM ===");

  unsigned long start = millis();

  // Classic Casio F-91W alarm pattern:
  // 4 rapid high-pitched beeps, short pause, repeat
  while (millis() - start < 3000) {
    // Four rapid beeps (1200 Hz - high metallic tone)
    for (int i = 0; i < 4; i++) {
      tone(BZ_PIN, 1200);
      delay(120);
      noTone(BZ_PIN);
      delay(80);
    }
    // Longer pause before repeat
    delay(200);
  }
}
void alarmSound2() {
  unsigned long start = millis();

  while (millis() - start < 6000) {
    beep(2500, 100);
    delay(100);
    beep(2500, 100);
    delay(1000);
  }
}

// ════════════════════════════════════════════════════════════
//  ALARM FUNCTIONS
// ════════════════════════════════════════════════════════════
// void checkAlarm() {
//   // Check if current time matches alarm time
//   if (!cfg.clockCfg.alarm.enabled) return;

//   struct tm t;
//   if (!getRtcTime(&t)) return;

//   // Only trigger once per minute to avoid repeated alarms
//   unsigned long now = millis();
//   if (now - g_alarmLastTrigger < 60000UL) return;

//   if (t.tm_hour == cfg.clockCfg.alarm.hour &&
//     t.tm_min == cfg.clockCfg.alarm.minute) {
//     g_alarmTriggered = true;
//     g_alarmLastTrigger = now;
//     Serial0.println("=== ALARM TRIGGERED ===");
//     alarmSound2();  // Use existing alarm sound
//   }
// }

void startAlarmMode() {
  g_alarmSettingMode = true;
  g_alarmEditIdx = 0;  // Start with hour
  Serial0.println("Entering alarm set mode");
  drawDisplay();  // Redraw with alarm UI
}

void exitAlarmMode() {
  g_alarmSettingMode = false;
  g_alarmEditIdx = 0;
  Serial0.println("Exiting alarm set mode");
  drawDisplay();  // Redraw normal display
}

// void handleAlarmButton() {
//   if (!g_alarmSettingMode) return;

//   bool b1 = digitalRead(B1_PIN);
//   bool b2 = digitalRead(B2_PIN);

//   // B1 = Increment current field, B2 = Move to next field / confirm
//   if (b1 == LOW) {
//     beep(1500, 50);  // Click sound
//     if (g_alarmEditIdx == 0) {
//       // Hour editing
//       cfg.clockCfg.alarm.hour = (cfg.clockCfg.alarm.hour + 1) % 24;
//     }
//     else {
//       // Minute editing
//       cfg.clockCfg.alarm.minute = (cfg.clockCfg.alarm.minute + 1) % 60;
//     }
//     drawDisplay();
//     delay(200);  // Debounce
//   }

//   if (b2 == LOW) {
//     beep(2000, 50);  // Different click sound
//     if (g_alarmEditIdx == 0) {
//       g_alarmEditIdx = 1;  // Move to minute
//     }
//     else {
//       cfg.clockCfg.alarm.enabled = !cfg.clockCfg.alarm.enabled;  // Toggle enable
//       exitAlarmMode();  // Exit mode
//     }
//     drawDisplay();
//     delay(200);  // Debounce
//   }
// }

void startUpSound() {
  beep(1200, 80);
  delay(30);
  beep(1800, 80);
  delay(30);
  beep(2400, 120);
}

// EPD_266 epd2;
// ════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════
void setup() {
  Serial0.begin(115200);
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
  // epd2.begin();
  epd_init();
  // if (!epd_init()) {
  //   Serial0.println("e-Paper init failed — going to sleep");
  //   goToDeepSleep();
  // }
  pinMode(B1_PIN, INPUT);
  pinMode(B2_PIN, INPUT);
  pinMode(BZ_PIN, OUTPUT);
  bleConfigBegin("Eink-Clock Setup BLE");
  // Wake on user button press or plugging in power
  esp_deep_sleep_enable_gpio_wakeup(
    (1ULL << B1_PIN) | (1ULL << B2_PIN),
    ESP_GPIO_WAKEUP_GPIO_LOW
  );

  // esp_sleep_enable_gpio_wakeup();

  delay(100);

  if (getResetReason() != ESP_RST_DEEPSLEEP) {
    startUpSound();
  }

  // ── GPIO (charger / VBUS sense) ──────────────────────
  // pinMode(USER_BUTTON, INPUT);
  pinMode(VBUS_PIN, INPUT);

  // ── Optional boot splash screen ───────────────────────
  // if (g_enSplash) boot_splash();

  // ── Read sensors (fast, no WiFi needed) ──────────────
  g_batteryPct = readBattery();
  g_isVbusConnected = isVbusConnected();

  // ── First boot / factory state ────────────────────────
  if (strlen(cfg.wifi.wifi->ssid) == 0 || strlen(cfg.wifi.wifi->password) == 0) {
    Serial0.println("No WiFi configured — entering setup mode");
    enterPortalMode(true);
  }

  if (digitalRead(B1_PIN) == LOW && digitalRead(B2_PIN) == LOW) {
    Serial0.println("Both buttons held — entering setup mode");
    enterPortalMode(true, true);
  }

  // ── Power saving mode ───────────────────────────────────
  if (!cfg.clock.powerSave) {
    Serial0.println("Power save mode: OFF — full features enabled");
    g_powerSaveMode = false;
    // ── WiFi initialization ──────────────────────────────
    if (wifi_init()) web_init(); // start web server only if WiFi connected
    // ── First draw ────────────────────────────────────────
    drawDisplay();
  }
  else {
    // ── Portal mode check (GPIO0/BOOT held LOW) ───────────
    // if (WakeButtonHeld()) {
    //   Serial0.println("Portal button held — entering setup mode");
    //   enterPortalMode();
    // }
    // ── NTP sync (WiFi only when needed) ─────────────────
    if (shouldSyncNtp()) {
      doNtpSync();
    }
    else {
      // // Restore system clock from RTC (no WiFi needed)
      // restore_rtc();
      Serial0.println("Clock from RTC — skipped sync");
    }
    // ── Draw display ─────────────────────────────────────
    drawDisplay();
    // ── spawn the web for the first power on ───────────────
    if (rtcNvBootCount == 1) {
      Serial0.println("First boot — entering portal mode");
      enterPortalMode();   // blocks for 60 s then sleeps
    }
    else {
      switch (esp_sleep_get_wakeup_cause()) {
      case ESP_SLEEP_WAKEUP_TIMER:
        Serial0.println("Wakeup cause: Timer");
        Serial0.println("going to sleep");
        goToDeepSleep();
        break;
      case ESP_SLEEP_WAKEUP_GPIO:
        Serial0.println("Wakeup cause: GPIO");
        enterPortalMode();   // blocks for 60 s then sleeps
        break;
      default:
        Serial0.println("going to sleep");
        goToDeepSleep();
        break;
      }
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

  // if (connectRequested) {
  //   connectRequested = false;
  //   setStatus("connecting");
  //   WiFi.begin(pendingSsid.c_str(), pendingPass.c_str());

  //   uint32_t start = millis();
  //   while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
  //     delay(250);
  //   }

  //   if (WiFi.status() == WL_CONNECTED) {
  //     setStatus(("connected:" + WiFi.localIP().toString()).c_str());
  //     // TODO: persist pendingSsid/pendingPass via Preferences, same as the web portal path
  //   }
  //   else {
  //     setStatus("failed");
  //   }
  // }


  bool b1 = digitalRead(B1_PIN);
  bool b2 = digitalRead(B2_PIN);

  // Both buttons held for 2 seconds = Enter alarm setting mode
  if (b1 == LOW && b2 == LOW) {
    unsigned long pressStart = millis();
    while (digitalRead(B1_PIN) == LOW && digitalRead(B2_PIN) == LOW) {
      if (millis() - pressStart > 2000) {
        // Both buttons held > 2 sec = alarm mode
        if (!g_alarmSettingMode) {
          startAlarmMode();
        }
        else {
          exitAlarmMode();
        }
        while (digitalRead(B1_PIN) == LOW || digitalRead(B2_PIN) == LOW) {
          delay(10);
        }
        lastB1 = HIGH;
        lastB2 = HIGH;
        return;
      }
      delay(10);
    }

    // Short press (< 2 sec) of both buttons = trigger test alarm
    if (millis() - pressStart < 2000) {
      alarmSound2();
    }

    // Wait until both buttons are released
    while (digitalRead(B1_PIN) == LOW || digitalRead(B2_PIN) == LOW) {
      delay(10);
    }

    lastB1 = HIGH;
    lastB2 = HIGH;
    return;
  }

  // Handle individual button presses
  if (g_alarmSettingMode) {
    // In alarm setting mode, buttons are used for editing
    // handleAlarmButton();
  }
  else {
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
  }

  lastB1 = b1;
  lastB2 = b2;

  // ── ALARM CHECK ──────────────────────────────────────
  if (g_alarmTriggered && !g_alarmSettingMode) {
    g_alarmTriggered = false;  // Reset trigger flag
    // Alarm will keep playing from beep() call in checkAlarm()
  }

  if (!g_powerSaveMode) {
    // checkAlarm();  // Check if alarm should trigger
    maintainWifi();
    web_loop();
    unsigned long ms = millis();
    // Read sensors every second for smooth display updates
    if (ms - lastSensorRead >= 60000UL) {
      lastSensorRead = ms;
      aht20_read();
      // drawScreen0();
      // drawDisplay();
      g_batteryPct = readBattery();
      g_isVbusConnected = isVbusConnected();
      Serial0.println("\nUpdated Sensor Values: \n");
      Serial0.printf("AHT20: %.1f °C, %.1f %%RH\n", g_temperature, g_humidity);
      Serial0.printf("Battery: %d%%  VBUS: %d\n", g_batteryPct, g_isVbusConnected);
    }

    // Refresh display on schedule
    if (ms - lastRefresh >= static_cast<unsigned long>(cfg.clock.refreshMin) * 60000UL) {
      lastRefresh = ms;
      // sync_time();
      DateTime d = rtc.now();
      lastRefreshEpoch = d.unixtime();
      drawDisplay();
    }

    if (!isVbusConnected() && cfg.clock.powerSave) {
      Serial0.println("USB power lost — entering power save mode");
      g_powerSaveMode = true;
      goToDeepSleep();
    }
  }
  delay(1);
}
