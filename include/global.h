//
// Created by Li on 3/12/2026.
//

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <HTTPUpdateServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <ctime>
#include "epd_266.h"
#include "clock_digit.h"
#include "epd_gfx.h"
#include <Adafruit_AHTX0.h>
#include <RTClib.h>

struct wifi_t {
    char ssid[64] = "";
    char password[64] = "";
};

struct alarm_t {
    uint8_t hour = 7;               // alarm hour (0-23)
    uint8_t minute = 0;            // alarm minute (0-59)
    bool    enabled = false;        // alarm enabled/disabled
};

struct clock_settings_t {

    // clock options
    int8_t  utcOffset = 7;          // default UTC+7 (ICT)
    uint8_t refreshMin = 2;         // refresh every 2 minutes by default
    uint8_t ntpSyncDays = 7;        // sync NTP every 7 days by default
    uint8_t ntpReSyncDays = 1;      // retry every 1 day if sync fails
    uint8_t quietStart = 23;        // quiet hours start (11 PM)
    uint8_t quietEnd = 7;           // quiet hours end (7 AM)
    bool    quietEnabled = false;   // quiet hours disable/enable
    bool    powerSave = false;      // power saving mode (WiFi off, web off)

    // display options
    bool    hour12 = true;
    bool    showHum = true;
    bool    showTemp = true;
    bool    showRssi = true;
    bool    showBattPct = false;
    
    // alarm options
    alarm_t alarm;                  // alarm settings
};

struct config_t {
    wifi_t              wifi[2];        // support 2 WiFi networks for backup
    clock_settings_t    clockCfg;  // clock and display settings
    char                hostname[12] = "eink-clock"; // mDNS hostname
};

extern String g_logBuf;
extern bool   g_serialEnabled;

extern config_t cfg;

// ===== HARDWARE =====
extern Adafruit_AHTX0 aht;
extern RTC_PCF8563 rtc;

// ===== NETWORK =====
extern WebServer server;
extern HTTPUpdateServer httpUpdater;

// ===== SENSOR DATA =====
extern float g_temperature;
extern float g_humidity;

// ===== IO =====
extern uint8_t g_batteryPct;
extern bool    g_isVbusConnected;
extern bool    g_powerSaveMode;

// ===== ALARM STATE =====
extern bool    g_alarmTriggered;    // alarm just triggered
extern bool    g_alarmSettingMode;  // in alarm set mode
extern uint8_t g_alarmEditIdx;      // 0=hour, 1=minute

// ===== SYSTEM STATE =====
extern time_t lastRefreshEpoch;
// extern unsigned long lastRefresh;
// extern unsigned long lastSensor;

// ===== FUNCTION PROTOTYPES =====
void webLog(const String& msg);
void webLogf(const char* fmt, ...);
uint32_t effectiveRefreshSec();

// ===== ALARM FUNCTIONS =====
void checkAlarm();                  // Check if alarm should trigger
void handleAlarmButton();           // Button handler for alarm setting
void startAlarmMode();              // Enter alarm setting mode
void exitAlarmMode();               // Exit alarm setting mode

// ===== Sleep Schedule =====
