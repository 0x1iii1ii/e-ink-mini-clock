//
// Created by Li on 5/1/2026.
//

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "wifi_mgr.h"
#include "time_sync.h"
#include "epd_266.h"
#include "epd_gfx.h"
#include "clock_digit.h"

bool epd_init();
void drawBattery(int x, int y, uint8_t pct, bool charging = false);
void drawWifiBars(int x, int y, int rssi);
void drawDisplay();
void showSetupScreen();
void boot_splash();

#endif //DISPLAY_H