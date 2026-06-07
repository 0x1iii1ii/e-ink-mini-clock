//
// Created by Li on 5/1/2026.
//

#include "time_sync.h"
#include "global.h"
#include <time.h>

RTC_PCF8563    rtc;

// ── Deep-sleep state in RTC NVRAM ─────────────────────────
// Survives deep sleep, wiped on power loss (RTC battery keeps PCF8563 but
// not ESP32 NVRAM — so we persist the important bits to LittleFS too).

RTC_DATA_ATTR uint32_t  rtcNvBootCount = 0;  // total wake count
RTC_DATA_ATTR uint32_t  rtcNvLastNtpEpoch = 0;  // epoch of last successful NTP sync
RTC_DATA_ATTR bool      rtcNvNtpPending = true; // true = need a sync this wake
RTC_DATA_ATTR uint8_t   rtcNvRetryDays = 0;   // days since last failed sync

// ════════════════════════════════════════════════════════════
//  NTP
// ════════════════════════════════════════════════════════════

void rtc_init() {
    // ── RTC (PCF8563) ────────────────────────────────────
    Serial.print("Init RTC PCF8563... ");
    if (!rtc.begin()) {
        Serial.println("FAILED — check wiring");
    }
    else {
        Serial.println("OK");
        // Initialise RTC to compile time only if it has lost power
        if (rtc.lostPower()) {
            Serial.println("RTC lost power — setting compile-time date");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        else {
            Serial.println("RTC battery OK — time preserved");
        }
    }
}

/**
 * Restore the ESP32 system clock from the PCF8563 RTC.
 * Call this in AP-fallback mode when there is no WiFi / NTP.
 */
void restore_rtc() {
    DateTime now = rtc.now();
    // DateTime is valid if year is sane (RTClib returns 2000-01-01 on cold start)
    if (now.year() < 2020) {
        Serial.println("RTC time not set yet — skipping restore");
        return;
    }
    // Build a struct tm from the RTClib DateTime
    struct tm t = {};
    t.tm_year = now.year() - 1900;
    t.tm_mon = now.month() - 1;
    t.tm_mday = now.day();
    t.tm_hour = now.hour();
    t.tm_min = now.minute();
    t.tm_sec = now.second();
    t.tm_isdst = -1;

    time_t epoch = mktime(&t);
    timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
    Serial.printf("RTC restore: %04d-%02d-%02d %02d:%02d:%02d\n",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
}

bool getRtcTime(struct tm* timeinfo) {
    DateTime now = rtc.now();
    if (timeinfo == nullptr) {
        return 0;
    }
    if (now.year() < 2020) {
        Serial.println("RTC time not set yet — returning zero time");
        memset(timeinfo, 0, sizeof(struct tm));
        return 0;
    }
    timeinfo->tm_year = now.year() - 1900;
    timeinfo->tm_mon = now.month() - 1;
    timeinfo->tm_mday = now.day();
    timeinfo->tm_wday = now.dayOfTheWeek();
    timeinfo->tm_hour = now.hour();
    timeinfo->tm_min = now.minute();
    timeinfo->tm_sec = now.second();
    timeinfo->tm_isdst = -1;
    return 1;
}

bool sync_time() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected — cannot sync time");
        // restore_rtc();
        return 0;
    }
    Serial.print("NTP sync...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // Polls system time until it becomes valid
    time_t now = 0;
    for (int i = 0; i < 80 && now < 1000000000L; i++) {
        delay(100);
        time(&now);
        Serial.print('.');
    }

    if (now > 1000000000L) {
        Serial.println("Sync OK!");
        // Apply UTC offset to get local time
        now = now + (long) cfg.clockCfg.utcOffset * 3600L;
        // Push the synced time into the RTC so it survives a reboot without WiFi.
        rtc.adjust(DateTime((uint32_t) now));
        Serial.printf("RTC updated to: %04d-%02d-%02d %02d:%02d:%02d\n",
            rtc.now().year(), rtc.now().month(), rtc.now().day(),
            rtc.now().hour(), rtc.now().minute(), rtc.now().second());

        return 1;
    }
    else {
        Serial.println(" FAILED — keeping RTC time");
        return 0;
        // restore_rtc();
    }
}

// ════════════════════════════════════════════════════════════
//  NTP sync execution
// ════════════════════════════════════════════════════════════

void doNtpSync() {
    Serial.println("NTP: connecting WiFi...");

    // Brief WiFi connection — timeout 12 s
    WiFi.setHostname(cfg.hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi->ssid, cfg.wifi->password);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
        delay(200);
        Serial.print('.');
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nNTP: WiFi failed — keeping RTC time, retry in 24 h");
        // restore_rtc();
        rtcNvNtpPending = true;
        // Update last-attempt time so retry fires 24 h from now
        DateTime now = rtc.now();
        rtcNvLastNtpEpoch = now.unixtime();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return;
    }

    Serial.println("\nNTP: WiFi OK — syncing...");
    if (sync_time()) {
        Serial.println("NTP: sync successful");
        DateTime now = rtc.now();
        rtcNvLastNtpEpoch = (uint32_t) now.unixtime();
        rtcNvNtpPending = false;
        rtcNvRetryDays = 0;
    }
    else {
        Serial.println("NTP: sync failed — retry in 24 h");
        rtcNvNtpPending = true;
        DateTime d = rtc.now();
        rtcNvLastNtpEpoch = d.unixtime();
    }

    // Kill WiFi immediately to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
}

// ════════════════════════════════════════════════════════════
//  NTP sync decision
// ════════════════════════════════════════════════════════════

bool shouldSyncNtp() {
    // Always sync on very first boot (NVRAM wiped = no epoch stored)
    if (rtcNvLastNtpEpoch == 0) {
        Serial.println("NTP: first boot — sync needed");
        return true;
    }

    // Get current time from RTC to compute elapsed
    DateTime now = rtc.now();
    time_t nowEpoch = (time_t) now.unixtime();

    uint32_t elapsed = (nowEpoch > (time_t) rtcNvLastNtpEpoch)
        ? (uint32_t) (nowEpoch - rtcNvLastNtpEpoch)
        : NTP_INTERVAL_SEC;  // clock went backwards — force sync

    // In retry mode: try every NTP_RETRY_SEC
    if (rtcNvNtpPending) {
        if (elapsed >= NTP_RETRY_SEC) {
            Serial.printf("NTP: retry mode, %u h since last attempt — syncing\n", elapsed / 3600);
            return true;
        }
        Serial.printf("NTP: retry mode, only %u h elapsed — skip\n", elapsed / 3600);
        return false;
    }

    // Normal mode: sync every NTP_INTERVAL_SEC (1 week)
    if (elapsed >= NTP_INTERVAL_SEC) {
        Serial.printf("NTP: %u days since last sync — syncing\n", elapsed / 86400);
        return true;
    }

    Serial.printf("NTP: %u h since last sync — skip\n", elapsed / 3600);
    return false;
}
