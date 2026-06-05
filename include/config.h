//
// Created by Li on 5/1/2026.
//

#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <Arduino.h>

// fimware configurations

// #define SAVE_POWER
#define DEBUG
#define SERIAL_BUF_SIZE 4096
#define LOG_BUF_SIZE 4096
// ============================================================================
// CONFIG — edit these defaults
// ============================================================================
#define DEFAULT_AP_SSID  "ESP AP"
#define DEFAULT_AP_PASS  "12345678" 
#define GIF_FOLDER       "gif"
#define DEFAULT_FPS      15
// #define FW_VERSION       "1.0.5"
#define DEVICE_NAME      "ESP32-C3 E-Ink Clock"
#define CONFIG_FILE      "/config.json"
#define AP_SSID          "Eink-Clock-Setup"
#define AP_PASS          "12345678"

// ============================================================================
// PINS
// ============================================================================

// I2C pins for AHT20 and RTC PCF8563T
#define I2C_SDA   9    // GPIO9
#define I2C_SCL   3    // GPIO3

// e-Paper pins (Waveshare 2.66" G)
#define TFT_CS  16  // D0
#define TFT_RST 12  // D6
#define TFT_DC  15  // D8

// Screen dimensions and offsets
#define COL_OFFSET 1
#define ROW_OFFSET 2

#define SCREEN_W 160
#define SCREEN_H 80

// ============================================================================
// MADCTL
// ============================================================================
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_BGR 0x08

// ADC pins for battery monitoring
#define BATTRY_ADC  0    // GPIO2, ADC channel — voltage divider (100k/100k) 
#define USER_BUTTON 8    // GPIO8, digital input — User button
#define VBUS_PIN    10    // GPIO0, digital input — USB charger connected

#endif //CONFIG_MGR_H
