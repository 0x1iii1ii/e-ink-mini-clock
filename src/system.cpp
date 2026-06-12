//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "system.h"
#include "battery.h"

/*
- During normal operation: sleep refreshMin.
- When the device enters quiet hours from a timer wakeup: sleep until quietEnd.
- If the device was awakened manually (button, USB power detect, etc.),
continue using the normal refresh interval even during quiet hours
so the user can interact with it.
*/

// check if we're currently in quiet hours
bool isInQuietHours() {
    if (!cfg.clockCfg.quietEnabled)
        return false;

    DateTime now = rtc.now();
    uint8_t h = now.hour();

    return (cfg.clockCfg.quietStart > cfg.clockCfg.quietEnd)
        ? (h >= cfg.clockCfg.quietStart || h < cfg.clockCfg.quietEnd)
        : (h >= cfg.clockCfg.quietStart && h < cfg.clockCfg.quietEnd);
}

// calculate seconds until the end of quiet hours, used for adjusting sleep duration
uint32_t secondsUntilQuietEnd() {
    DateTime now = rtc.now();

    uint32_t nowSec =
        now.hour() * 3600UL +
        now.minute() * 60UL +
        now.second();

    uint32_t endSec =
        cfg.clockCfg.quietEnd * 3600UL;

    if (cfg.clockCfg.quietStart > cfg.clockCfg.quietEnd) {
        // spans midnight
        if (now.hour() >= cfg.clockCfg.quietStart) {
            return (24UL * 3600UL - nowSec) + endSec;
        }
        else {
            return endSec - nowSec;
        }
    }

    return endSec - nowSec;
}
// ════════════════════════════════════════════════════════════
//  Deep sleep
// ════════════════════════════════════════════════════════════

void goToDeepSleep() {
    if (isVbusConnected()) {
        Serial.println("USB power detected — staying awake");
        g_powerSaveMode = false;
        return;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    uint32_t sleepSec = effectiveRefreshSec();
    uint64_t sleepUs = (uint64_t) sleepSec * 1000000ULL;
    if (isInQuietHours() && sleepSec > (cfg.clockCfg.refreshMin * 60UL)) {
        Serial.printf(
            "Quiet hours active, sleeping until %02u:00 (%lu min)\n",
            cfg.clockCfg.quietEnd,
            sleepSec / 60UL
        );
    }
    else {
        Serial.printf("Sleeping for %lu min\n", sleepSec / 60UL);
    }
    Serial.flush();
    // Power down peripherals before sleep
    Wire.end();
    // Wake on timer
    esp_sleep_enable_timer_wakeup(sleepUs);
    // Wake on user button press or plugging in power
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
    unsigned long timeoutMs = (factory) ? WEB_SPAWN_SETUP_MODE_MS : WEB_SPAWN_TIMEOUT_MS;

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

    Serial.printf("Web Portal running for %lu s timeout...", timeoutMs / 1000);

    unsigned long portalStart = millis();
    while (millis() - portalStart < timeoutMs) {
        if (factory && isVbusConnected()) {
            portalStart = millis(); // reset the start time
        }
        web_loop();
        delay(10);
    }

    Serial.println("Portal timeout — going back to sleep");
    if (factory) {
        // after timeout during configuration, 
        // go to deep sleep to wait for next power on
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_deep_sleep_start();
    }
    goToDeepSleep();
}

uint32_t effectiveRefreshSec() {
    uint32_t base = (uint32_t) cfg.clockCfg.refreshMin * 60UL;

    if (!isInQuietHours())
        return base;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
    {
        uint32_t quietSleep = secondsUntilQuietEnd();

        // Prevent accidental very short sleeps
        if (quietSleep > base)
            return quietSleep;

        return base;
    }

    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
        return base;

    default:
        return base;
    }
}

void checkWakeupReason() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        Serial.println("Wakeup caused by GPIO");
        break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        Serial.println("Wakeup cause: undefined");
        break;
    }
}