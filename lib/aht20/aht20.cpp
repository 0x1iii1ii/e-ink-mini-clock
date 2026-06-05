//
// Created by Li on 5/1/2026.
//

#include "aht20.h"

bool AHT20::begin() {
    Wire.beginTransmission(AHT20_ADDR);
    Wire.write(AHT20_CMD_RESET);
    Wire.endTransmission();
    delay(20);

    Wire.beginTransmission(AHT20_ADDR);
    Wire.write(AHT20_CMD_INIT);
    Wire.write(0x08);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(10);

    // wait for calibration bit
    uint32_t t = millis();
    while (!(getStatus() & 0x08)) {
        if (millis() - t > 500) return false;
        delay(10);
    }
    return true;
}
bool AHT20::read() {
    Wire.beginTransmission(AHT20_ADDR);
    Wire.write(AHT20_CMD_MEAS);
    Wire.write(0x33);
    Wire.write(0x00);
    Wire.endTransmission();

    delay(80);  // measurement time

    Wire.requestFrom(AHT20_ADDR, 6);
    if (Wire.available() < 6) return false;

    uint8_t data[6];
    for (auto& b : data) b = Wire.read();

    if (data[0] & 0x80) return false;  // busy bit

    uint32_t raw_hum = ((uint32_t) data[1] << 12) | ((uint32_t) data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temp = ((uint32_t) (data[3] & 0x0F) << 16) | ((uint32_t) data[4] << 8) | data[5];

    humidity = (raw_hum / 1048576.0f) * 100.0f;
    temperature = (raw_temp / 1048576.0f) * 200.0f - 50.0f;
    return true;
}

uint8_t AHT20::getStatus() {
    Wire.requestFrom(AHT20_ADDR, 1);
    return Wire.available() ? Wire.read() : 0;
}