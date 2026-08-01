//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "wifi_mgr.h"

static uint32_t lastWifiCheck = 0;
static uint32_t lastSensorRead = 0;
static uint32_t lastRefresh = 0;
static uint8_t  wifiRetryCount = 0;

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
    WiFi.setHostname(cfg.wifi.hostname);
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
    WiFi.setHostname(cfg.wifi.hostname);
    if (strlen(cfg.wifi.wifi->ssid) > 0 && strlen(cfg.wifi.wifi->password) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi.wifi->ssid, cfg.wifi.wifi->password);
        unsigned long t0 = millis();
        Serial0.print("Connecting to: " + String(cfg.wifi.wifi->ssid) + "...");
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
        MDNS.begin(cfg.wifi.hostname);
        sync_time();
        return true; // connected
    }
    else {
        Serial0.println("\nWiFi connection failed, going offline");
        return false; // not connected
    }
}