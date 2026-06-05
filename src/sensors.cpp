//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "sensors.h"
#include "aht20.h"

// ════════════════════════════════════════════════════════════
//  Sensors
// ════════════════════════════════════════════════════════════
// Adafruit_AHTX0 aht;
AHT20 aht20;

// Sensor calibration offsets
static constexpr float TEMP_OFFSET = -1.5f;  // °C
static constexpr float HUM_OFFSET = +3.0f;  // %RH

// Global sensor variables
float   g_temperature = 0.0f;
float   g_humidity = 0.0f;

// ════════════════════════════════════════════════════════════
//  Sensor
// ════════════════════════════════════════════════════════════

void aht20_init() {
    // ── AHT20 ────────────────────────────────────────────
    Serial.print("Init AHT20... ");
    if (!aht20.begin()) {
        Serial.println("FAILED");
        return;
    }
    Serial.println("OK");
    aht20_read();
}

void aht20_read() {
    if (!aht20.read()) return;
    g_temperature = aht20.temperature + TEMP_OFFSET;
    g_humidity = constrain(aht20.humidity + HUM_OFFSET, 0.0f, 100.0f);
}
