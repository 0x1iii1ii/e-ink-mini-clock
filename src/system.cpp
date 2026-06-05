//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "system.h"

// ════════════════════════════════════════════════════════════
//  Deep sleep
// ════════════════════════════════════════════════════════════

void goToDeepSleep() {
    uint64_t sleepUs = (uint64_t) effectiveRefreshSec() * 1000000ULL;

    Serial.printf("Sleeping for %u min...\n", cfg.clockCfg.refreshMin);
    Serial.flush();

    // Power down peripherals before sleep
    Wire.end();

    // Wake on timer
    esp_sleep_enable_timer_wakeup(sleepUs);

    // Wake on GPIO (USER_BUTTON LOW)
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t) VBUS_PIN, GPIO_INTR_HIGH_LEVEL);

    esp_deep_sleep_start();
}

// ════════════════════════════════════════════════════════════
//  Portal mode  (stays awake, serves web UI)
// ════════════════════════════════════════════════════════════

void enterPortalMode(bool factory) {
    Serial.println("Starting WiFi + web portal...");

    // Longer timeout for factory reset (no config) to give user more time to connect
    unsigned long timeoutMs = (factory) ? 6000000UL : 60000UL;

    // Start WiFi in AP mode and web server for configuration portal
    if (factory) {
        Serial.println("Starting WiFi AP...");
        startWiFiPortal();
        showSetupScreen();
    }
    Serial.println("Starting web server...");
    if (!factory) {
        wifi_init();
    }
    web_init();

    Serial.println("Portal running — 60 s timeout...");

    unsigned long portalStart = millis();
    while (millis() - portalStart < timeoutMs) {
        web_loop();
        delay(10);
    }

    Serial.println("Portal timeout — going back to sleep");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    goToDeepSleep();
}

uint32_t effectiveRefreshSec() {
    if (!cfg.clockCfg.quietEnabled) return (uint32_t) cfg.clockCfg.refreshMin * 60;

    DateTime now = rtc.now();
    uint8_t  h = now.hour();
    bool inQuiet = (cfg.clockCfg.quietStart > cfg.clockCfg.quietEnd)
        ? (h >= cfg.clockCfg.quietStart || h < cfg.clockCfg.quietEnd)   // spans midnight
        : (h >= cfg.clockCfg.quietStart && h < cfg.clockCfg.quietEnd);

    uint32_t base = (uint32_t) cfg.clockCfg.refreshMin * 60;
    return inQuiet ? base * 2 : base;
}