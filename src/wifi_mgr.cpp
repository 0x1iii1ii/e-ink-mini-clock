//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "wifi_mgr.h"

config_t cfg;
// Declared in eink_clock.ino — we write them back on load
// so the weekly NTP schedule survives a full power cycle.
extern uint32_t rtcNvLastNtpEpoch;
extern bool     rtcNvNtpPending;

static uint32_t lastWifiCheck = 0;
static uint32_t lastSensorRead = 0;
static uint32_t lastRefresh = 0;
static uint8_t  wifiRetryCount = 0;

void init_fs() {
    if (!LittleFS.begin()) {
        Serial0.println("LittleFS failed, formatting...");
        LittleFS.format();
        LittleFS.begin();
    }
    load_config();
}

void load_config() {

    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial0.println("Config file not found");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        Serial0.println("Failed to open config file");
        return;
    }

    Serial0.println("Raw file content:");
    while (f.available()) {
        Serial0.write(f.read());
    }
    Serial0.println("\n--- end of file ---");
    // Rewind file for parsing
    f.seek(0);

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);

    if (err) {
        Serial0.print("JSON parse failed: ");
        Serial0.println(err.c_str());
        f.close();
        return;
    }
    Serial0.println("JSON parsed successfully");

    // --- Print parsed JSON values ---
    Serial0.println("Parsed JSON:");
    serializeJsonPretty(doc, Serial);
    Serial0.println();

    // --- Load into cfg ---
    strlcpy(cfg.wifi->ssid, doc["ssid"] | "", sizeof cfg.wifi->ssid);
    strlcpy(cfg.wifi->password, doc["password"] | "", sizeof cfg.wifi->password);
    strlcpy(cfg.hostname, doc["hostname"] | "eink-clock", sizeof cfg.hostname);
    cfg.clockCfg.utcOffset = doc["utcOffset"] | 7;
    cfg.clockCfg.refreshMin = doc["refreshMin"] | 3;
    cfg.clockCfg.ntpSyncDays = doc["ntpSyncDays"] | 7;
    cfg.clockCfg.ntpReSyncDays = doc["ntpReSyncDays"] | 1;
    cfg.clockCfg.quietStart = doc["quietStart"] | 23;
    cfg.clockCfg.quietEnd = doc["quietEnd"] | 7;
    cfg.clockCfg.hour12 = doc["hour12"] | false;
    cfg.clockCfg.quietEnabled = doc["quietEnabled"] | false;
    cfg.clockCfg.powerSave = doc["powerSave"] | false;
    cfg.clockCfg.showBattPct = doc["showBattPct"] | false;
    cfg.clockCfg.showHum = doc["showHum"] | true;
    cfg.clockCfg.showTemp = doc["showTemp"] | true;
    cfg.clockCfg.showRssi = doc["showRSSI"] | false;
    cfg.clockCfg.alarm.hour = doc["alarmHour"] | 7;
    cfg.clockCfg.alarm.minute = doc["alarmMinute"] | 0;
    cfg.clockCfg.alarm.enabled = doc["alarmEnabled"] | false;

    // --- Print final cfg struct ---
    Serial0.println("Final cfg values:");
    Serial0.printf("ssid: %s\n", cfg.wifi->ssid);
    Serial0.printf("password: %s\n", cfg.wifi->password);
    Serial0.printf("hostname: %s\n", cfg.hostname);
    Serial0.printf("utcOffset: %d\n", cfg.clockCfg.utcOffset);
    Serial0.printf("refreshMin: %d\n", cfg.clockCfg.refreshMin);
    Serial0.printf("ntpSyncDays: %d\n", cfg.clockCfg.ntpSyncDays);
    Serial0.printf("ntpReSyncDays: %d\n", cfg.clockCfg.ntpReSyncDays);
    Serial0.printf("quietStart: %d\n", cfg.clockCfg.quietStart);
    Serial0.printf("quietEnd: %d\n", cfg.clockCfg.quietEnd);
    Serial0.printf("hour12: %s\n", cfg.clockCfg.hour12 ? "true" : "false");
    Serial0.printf("quietEnabled: %s\n", cfg.clockCfg.quietEnabled ? "true" : "false");
    Serial0.printf("powerSave: %s\n", cfg.clockCfg.powerSave ? "true" : "false");
    Serial0.printf("showBattPct: %s\n", cfg.clockCfg.showBattPct ? "true" : "false");
    Serial0.printf("showHum: %s\n", cfg.clockCfg.showHum ? "true" : "false");
    Serial0.printf("showTemp: %s\n", cfg.clockCfg.showTemp ? "true" : "false");
    Serial0.printf("showRssi: %s\n", cfg.clockCfg.showRssi ? "true" : "false");

    Serial0.println("=== load_config() done ===");
    // // Restore NTP schedule into RTC NVRAM so deep-sleep state
    // // is correct even after a full power cycle.
    // uint32_t savedEpoch = doc["ntpEpoch"] | 0;
    // bool     savedPending = doc["ntpPending"] | true;
    // if (savedEpoch > 0 && rtcNvLastNtpEpoch == 0) {
    //     // Only restore if NVRAM was wiped (cold boot)
    //     rtcNvLastNtpEpoch = savedEpoch;
    //     rtcNvNtpPending = savedPending;
    //     Serial0.printf("Config: restored NTP epoch %u, pending=%d\n",
    //         savedEpoch, savedPending);
    // }

    f.close();
}

void save_config() {
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) return;
    StaticJsonDocument<512> doc;
    doc["ssid"] = cfg.wifi->ssid;
    doc["password"] = cfg.wifi->password;
    doc["hostname"] = cfg.hostname;
    doc["utcOffset"] = cfg.clockCfg.utcOffset;
    doc["refreshMin"] = cfg.clockCfg.refreshMin;
    doc["ntpSyncDays"] = cfg.clockCfg.ntpSyncDays;
    doc["ntpReSyncDays"] = cfg.clockCfg.ntpReSyncDays;
    doc["quietStart"] = cfg.clockCfg.quietStart;
    doc["quietEnd"] = cfg.clockCfg.quietEnd;
    doc["hour12"] = cfg.clockCfg.hour12;
    doc["quietEnabled"] = cfg.clockCfg.quietEnabled;
    doc["powerSave"] = cfg.clockCfg.powerSave;
    doc["showBattPct"] = cfg.clockCfg.showBattPct;
    doc["showHum"] = cfg.clockCfg.showHum;
    doc["showTemp"] = cfg.clockCfg.showTemp;
    doc["showRSSI"] = cfg.clockCfg.showRssi;
    doc["alarmHour"] = cfg.clockCfg.alarm.hour;
    doc["alarmMinute"] = cfg.clockCfg.alarm.minute;
    doc["alarmEnabled"] = cfg.clockCfg.alarm.enabled;
    // // Persist NTP schedule so it survives a full power cycle
    // doc["ntpEpoch"] = rtcNvLastNtpEpoch;
    // doc["ntpPending"] = rtcNvNtpPending;
    Serial0.println("json config updated:");
    serializeJsonPretty(doc, Serial);

    serializeJson(doc, f);
    f.close();
}

void factory_reset() {
    LittleFS.end();

    if (!LittleFS.format()) {
        Serial0.println("Format failed");
    }
    else {
        Serial0.println("Format OK");
    }

    LittleFS.begin(true);
}

void erase_config() {
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
        Serial0.println("Config erased");
    }
}

// ── WiFi reconnect ────────────────────────────────────
void maintainWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiRetryCount = 0;   // reset on success
        return;
    }

    uint32_t now = millis();

    // Back off longer after repeated failures
    uint32_t interval = (wifiRetryCount >= WIFI_RETRY_MAX)
        ? WIFI_RETRY_LONG
        : WIFI_CHECK_INTERVAL;

    if (now - lastWifiCheck < interval) return;
    lastWifiCheck = now;

    Serial0.println("[WiFi] Disconnected — reconnecting (attempt " +
        String(wifiRetryCount + 1) + ")");

    // WiFi.disconnect(false);
    WiFi.setHostname(cfg.hostname);
    WiFi.begin();   // uses saved credentials

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
        millis() - start < WIFI_CONNECT_TIMEOUT) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial0.println("mDNS started");
        Serial0.println("[WiFi] Reconnected — IP: " + WiFi.localIP().toString());
        wifiRetryCount = 0;
    }
    else {
        wifiRetryCount++;
        Serial0.println("[WiFi] Failed — retry count: " + String(wifiRetryCount));
    }
}

bool wifi_init() {
    WiFi.setHostname(cfg.hostname);
    if (strlen(cfg.wifi->ssid) > 0 && strlen(cfg.wifi->password) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi->ssid, cfg.wifi->password);
        unsigned long t0 = millis();
        Serial0.print("Connecting to: " + String(cfg.wifi->ssid) + "...");
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
            delay(300);
            Serial0.print('.');
        }
    }
    else {
        Serial0.println("No WiFi credentials, going offline");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial0.println("\nWiFi: " + WiFi.localIP().toString());
        Serial0.println(String("Hostname: ") + WiFi.getHostname());
        MDNS.begin(cfg.hostname);
        sync_time();
        return true; // connected
    }
    else {
        Serial0.println("\nWiFi connection failed, going offline");
        return false; // not connected
    }
}