//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "system.h"
#include "battery.h"

#include <esp_system.h>

esp_reset_reason_t getResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();

    Serial0.print("Reset reason: ");

    switch (reason) {
    case ESP_RST_UNKNOWN:    Serial0.println("Unknown"); break;
    case ESP_RST_POWERON:    Serial0.println("Power-on"); break;
    case ESP_RST_EXT:        Serial0.println("External reset"); break;
    case ESP_RST_SW:         Serial0.println("Software reset (ESP.restart())"); break;
    case ESP_RST_PANIC:      Serial0.println("Exception/Panic"); break;
    case ESP_RST_INT_WDT:    Serial0.println("Interrupt Watchdog"); break;
    case ESP_RST_TASK_WDT:   Serial0.println("Task Watchdog"); break;
    case ESP_RST_WDT:        Serial0.println("Other Watchdog"); break;
    case ESP_RST_DEEPSLEEP:  Serial0.println("Wake from Deep Sleep"); break;
    case ESP_RST_BROWNOUT:   Serial0.println("Brownout"); break;
    case ESP_RST_SDIO:       Serial0.println("SDIO reset"); break;
    default:                 Serial0.printf("Other (%d)\n", reason); break;
    }
    return reason;
}

/*
- During normal operation: sleep refreshMin.
- When the device enters quiet hours from a timer wakeup: sleep until quietEnd.
- If the device was awakened manually (button, USB power detect, etc.),
continue using the normal refresh interval even during quiet hours
so the user can interact with it.
*/

// check if we're currently in quiet hours
bool isInQuietHours() {
    if (!cfg.device.quietEnabled)
        return false;

    DateTime now = rtc.now();
    uint8_t h = now.hour();

    return (cfg.device.quietStart > cfg.device.quietEnd)
        ? (h >= cfg.device.quietStart || h < cfg.device.quietEnd)
        : (h >= cfg.device.quietStart && h < cfg.device.quietEnd);
}

// calculate seconds until the end of quiet hours, used for adjusting sleep duration
uint32_t secondsUntilQuietEnd() {
    DateTime now = rtc.now();

    uint32_t nowSec =
        now.hour() * 3600UL +
        now.minute() * 60UL +
        now.second();

    uint32_t endSec =
        cfg.device.quietEnd * 3600UL;

    if (cfg.device.quietStart > cfg.device.quietEnd) {
        // spans midnight
        if (now.hour() >= cfg.device.quietStart) {
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
        Serial0.println("USB power detected — staying awake");
        g_powerSaveMode = false;
        return;
    }
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    uint32_t sleepSec = effectiveRefreshSec();
    // Adjust sleep time for first boot to avoid waking up too late
    if (rtcNvBootCount == 1 && cfg.display.refreshMin > 1) {
        sleepSec = sleepSec - 90;
    }
    uint64_t sleepUs = (uint64_t) sleepSec * 1000000ULL;
    if (isInQuietHours() && sleepSec > (cfg.display.refreshMin * 60UL)) {
        Serial0.printf(
            "Quiet hours active, sleeping until %02u:00 (%lu min)\n",
            cfg.device.quietEnd,
            sleepSec / 60UL
        );
    }
    else {
        Serial0.printf("Sleeping for %lu min\n", sleepSec / 60UL);
    }
    Serial0.flush();
    // Power down peripherals before sleep
    Wire.end();
    // Wake on timer
    esp_sleep_enable_timer_wakeup(sleepUs);
    Serial0.printf("B1=%d B2=%d\n",
        digitalRead(B1_PIN),
        digitalRead(B2_PIN));
    esp_deep_sleep_start();
}

// ════════════════════════════════════════════════════════════
//  Portal mode  (stays awake, serves web UI)
// ════════════════════════════════════════════════════════════

void enterPortalMode(bool factory, bool user) {
    Serial0.println("Starting WiFi + web portal...");

    // Longer timeout for factory reset (no config) to give user more time to connect
    unsigned long timeoutMs = (factory) ? WEB_SPAWN_SETUP_MODE_MS : WEB_SPAWN_TIMEOUT_MS;

    // Start WiFi in AP mode and web server for configuration portal
    if (factory) {
        Serial0.println("Starting WiFi AP...");
        startWiFiPortal();
        user ? showSetupScreen(SETUP_USER) : showSetupScreen(SETUP_FACTORY);
    }
    Serial0.println("Starting web server...");
    if (!factory && rtcNvBootCount != 1) {
        if (!wifi_init()) {
            Serial0.println("WiFi init failed — starting portal in AP mode");
            startWiFiPortal();
        }
    }
    web_init();

    Serial0.printf("Web Portal running for %lu s timeout...", timeoutMs / 1000);

    unsigned long portalStart = millis();
    while (millis() - portalStart < timeoutMs) {
        if (factory && isVbusConnected()) {
            portalStart = millis(); // reset the start time
        }
        // Keep alive if something important is happening
        // bool webBusy = isWebClientActive();   // user is browsing/submitting
        // if (webBusy) {
        //     portalStart = millis(); // extend deadline
        // }
        web_loop();
        delay(10);
    }

    Serial0.println("Portal timeout — going back to sleep");
    if (factory) {
        // after timeout during configuration, 
        // go to deep sleep to wait for next power on
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_deep_sleep_start();
    }
    goToDeepSleep();  // first boot +1.5min
}

uint32_t effectiveRefreshSec() {
    uint32_t base = (uint32_t) cfg.display.refreshMin * 60UL;

    if (!isInQuietHours())
        return base;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    Serial0.printf("Quiet hours active, wakeup cause: %d\n", cause);

    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
    case ESP_SLEEP_WAKEUP_GPIO:
    {
        uint32_t quietSleep = secondsUntilQuietEnd();

        // Prevent accidental very short sleeps
        if (quietSleep > base)
            return quietSleep;

        return base;
    }

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
        Serial0.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        Serial0.println("Wakeup caused by GPIO");
        break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        Serial0.println("Wakeup cause: undefined");
        break;
    }
}