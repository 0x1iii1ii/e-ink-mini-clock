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
#include "config.h"

// ===== CONFIG STRUCTS =====

typedef enum {
    CS_DEFAULT = 0,
    CS_KHMER,
    CS_RETRO
} clock_style_t;

typedef struct {
    char ssid[64];              // WiFi SSID
    char password[64];          // WiFi password
} wifi_t;

typedef struct {
    uint8_t minutes;            // snooze duration in minutes
    uint8_t repeat;             // snooze repeat
    uint8_t sound;              // snooze sound
} snooze_t;

typedef struct {
    uint8_t hour;               // alarm hour (0-23)
    uint8_t minute;             // alarm minute (0-59)
    uint8_t repeatDays;         // bitmask for repeat days (0=none, 1=Sun, 2=Mon, 4=Tue, 8=Wed, 16=Thu, 32=Fri, 64=Sat)  
    char    name[16];           // alarm names
    bool    enabled;            // alarm enabled/disabled
    snooze_t snooze;
} alarm_t;

typedef struct {
    // clock options
    bool    hour12;
    int8_t  utcOffset;          // default UTC+7 (ICT)
    uint8_t ntpSyncDays;        // sync NTP every 7 days by default
    uint8_t ntpReSyncDays;      // retry every 1 day if sync fails

    // alarm options
    alarm_t alarm[MAX_ALARMS];  // alarm settings
} clock_settings_t;

typedef struct {
    uint8_t refreshMin;         // refresh every 2 minutes by default
    uint8_t clockStyle;         // clock style (0=default, 1=khmer, 2=retro)
    bool    showBattPct;
    bool    showHum;
    bool    showTemp;
    bool    showRssi;
} display_settings_t;

typedef struct {
    uint8_t quietStart;         // quiet hours start (12 AM)
    uint8_t quietEnd;           // quiet hours end (5 AM)
    bool    quietEnabled;       // quiet hours disable/enable
    bool    powerSave;          // power saving mode (WiFi off, web off)
} device_settings_t;

typedef struct {
    wifi_t wifi[2];             // support 2 WiFi networks for backup
    char hostname[12];          // mDNS hostname
} wifi_settings_t;

typedef struct {
    wifi_settings_t wifi;
    clock_settings_t clock;
    device_settings_t device;
    display_settings_t display;
} config_t;

extern config_t cfg;
extern String g_logBuf;
extern bool   g_serialEnabled;

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

// ===== SYSTEM STATE =====
extern time_t lastRefreshEpoch;
extern uint32_t rtcNvLastNtpEpoch;
extern bool     rtcNvNtpPending;
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
