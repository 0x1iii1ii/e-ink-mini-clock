//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "display.h"

 // ════════════════════════════════════════════════════════════
 //  Frame buffer
 //  184 × 360 px, 2 bits/pixel → 184/4 × 360 = 46 × 360 = 16560 bytes
 // ════════════════════════════════════════════════════════════
static UBYTE frameBuf[EPD_BUFFER_SIZE];

Epd     epd;
EpdGfx  gfx(epd, frameBuf);
BigFont bigFont(gfx);

const char* days[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
const char* months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

void epd_init() {
  // ── e-Paper ──────────────────────────────────────────
  Serial.println("Init e-paper...");
  if (epd.Init() != 0) {
    Serial.println("FAILED — check wiring and PWR pin!");
  }
  else {
    Serial.println("e-Paper OK.");
  }
}

// ════════════════════════════════════════════════════════════
//  Drawing helpers
// ════════════════════════════════════════════════════════════
String zp(int v) { return (v < 10 ? "0" : "") + String(v); }

/**
 * Battery icon — 28 × 16 px (body 25 × 16 + nub 3 × 8)
 *
 *  pct      : 0-100 fill level
 *  charging : true  → draw yellow lightning bolt over the body
 *             false → normal colour-coded fill only
 *
 *  Fill colour thresholds:
 *    > 50 %  → BLACK   (normal)
 *    > 20 %  → YELLOW  (warning)
 *    ≤ 20 %  → RED     (critical)
 */

void drawBattery(int x, int y, uint8_t pct, bool charging) {
  // ── Outer body ───────────────────────────────────────────
  gfx.drawRect(x, y, 25, 16, EPD_BLACK);
  // ── Nub (positive terminal) ──────────────────────────────
  gfx.fillRect(x + 25, y + 4, 3, 8, EPD_BLACK);

  // ── Charge fill ──────────────────────────────────────────
  uint8_t fillW = (uint8_t) (pct * 23 / 100);
  UBYTE   col = (pct > 50) ? EPD_BLACK
    : (pct > 20) ? EPD_YELLOW
    : EPD_RED;
  if (fillW > 0)
    gfx.fillRect(x + 1, y + 1, fillW, 14, col);

  // ── Charging bolt overlay ─────────────────────────────────
  if (charging) {
    const UBYTE BLT = EPD_YELLOW;
    int yoff = 1;  // move bolt down
    int xoff = -2;  // move bolt left
    for (int dy = 0; dy <= 1; dy++) {
      gfx.drawHLine(x + 14 + xoff, y + 1 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 13 + xoff, y + 2 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 12 + xoff, y + 3 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 11 + xoff, y + 4 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 10 + xoff, y + 5 + dy + yoff, 4, BLT);

      gfx.drawHLine(x + 14 + xoff, y + 6 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 13 + xoff, y + 7 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 12 + xoff, y + 8 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 11 + xoff, y + 9 + dy + yoff, 3, BLT);
      gfx.drawHLine(x + 10 + xoff, y + 10 + dy + yoff, 3, BLT);
    }
  }
}

// WiFi signal bars — 4 bars, ~26 x 20 px at (x,y)
void drawWifiBars(int x, int y, int rssi) {
  int bars = 0;
  if (rssi > -55) bars = 4;
  else if (rssi > -66) bars = 3;
  else if (rssi > -77) bars = 2;
  else if (rssi > -88) bars = 1;
  for (int i = 0; i < 4; i++) {
    int bh = 4 + i * 4;
    int bx = x + i * 7;
    int by = y + 20 - bh;
    if (i < bars) gfx.fillRect(bx, by, 6, bh, EPD_BLACK);
    else          gfx.drawRect(bx, by, 6, bh, EPD_BLACK);
  }
}

// ════════════════════════════════════════════════════════════
//  Main draw
// ════════════════════════════════════════════════════════════
void drawDisplay() {

  struct tm t;
  if (!getRtcTime(&t)) {
    Serial.println("RTC read failed");
    return;
  }

  int hr = t.tm_hour;
  int min = t.tm_min;
  bool pm = (hr >= 12);

  if (cfg.clockCfg.hour12) {
    if (hr > 12) hr -= 12;
    if (hr == 0) hr = 12;
  }

  String dateStr = String(days[t.tm_wday]) + " " + zp(t.tm_mday) + "-" + months[t.tm_mon] + "-" + String(1900 + t.tm_year);

  Serial.println("dateStr: " + dateStr);

  gfx.fillScreen(EPD_WHITE);

  // ── TOP BAR ────────────────────────────────────────────
  gfx.setTextSize(2);
  gfx.setTextColor(EPD_BLACK);
  gfx.setCursor(4, 3);
  gfx.print(dateStr);

  // battery icon always there
  drawBattery(330, 2, g_batteryPct, g_isVbusConnected);

  // Auto right-align items based on enabled settings
  // Order: temp -> hum -> wifi bar -> battery percentage
  // Item approximate widths (in pixels at textSize 2)
  int ITEM_SPACING = 10;
  int WIFI_WIDTH = 26;
  int BATT_WIDTH = 35;
  int TEMP_WIDTH = 35;  // e.g., "25C"
  int HUM_WIDTH = 48;   // e.g., "65%H"

  int rightX = 330 - 3;  // Start from right edge

  // Draw items in reverse order (battery, wifi, humidity, temperature)
  // so we can calculate positions from right to left

  // // Battery percentage (rightmost)
  // if (cfg.clockCfg.showBattPct) {
  //   rightX -= BATT_WIDTH;
  //   gfx.setTextColor(EPD_BLACK);
  //   gfx.setTextSize(2);
  //   gfx.setCursor(rightX, 3);
  //   gfx.print(String(g_batteryPct).c_str());
  //   gfx.print("%");
  //   rightX -= ITEM_SPACING;
  // }

  // WiFi signal bars
  if (cfg.clockCfg.showRssi) {
    rightX -= WIFI_WIDTH;
    drawWifiBars(rightX, -2, WiFi.RSSI());
    rightX -= ITEM_SPACING;
  }

  // Humidity from AHT20
  if (cfg.clockCfg.showHum) {
    rightX -= HUM_WIDTH;
    gfx.setTextColor(EPD_BLACK);
    gfx.setTextSize(2);
    gfx.setCursor(rightX, 3);
    gfx.print(String(g_humidity, 0).c_str());
    gfx.print("%H");
    rightX -= ITEM_SPACING;
  }

  // Temperature from AHT20
  if (cfg.clockCfg.showTemp) {
    rightX -= TEMP_WIDTH;
    gfx.setTextColor(EPD_RED);
    gfx.setTextSize(2);
    gfx.setCursor(rightX, 3);
    gfx.print(String(g_temperature, 0).c_str());
    gfx.print("C");
  }

  // AM/PM top-right corner of clock area (yellow)
  // if (cfg.hour12) {
  //   gfx.setTextColor(EPD_BLACK);
  //   gfx.setTextSize(2);
  //   gfx.setCursor(260, 3);
  //   gfx.print(pm ? "PM" : "AM");
  // }

  // Double divider
  gfx.drawHLine(0, LINE_TOP_BAR, 360, EPD_BLACK);

  // ── BIG CLOCK ──────────────────────────────────────────
  bigFont.drawTime(hr, min, EPD_BLACK);
  gfx.drawHLine(0, LINE_BOTTOM_BAR, 360, EPD_BLACK);

  // ── BOTTOM BAR ─────────────────────────────────────────
  gfx.setTextColor(EPD_RED);
  gfx.setTextSize(1);
  const String ip = "IP:" + WiFi.localIP().toString();
  gfx.setCursor(4, 175);
  gfx.print(ip.c_str());

  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setCursor(135, 175);
  gfx.print("UTC");
  if (cfg.clockCfg.utcOffset >= 0) gfx.print("+");
  gfx.print((int) cfg.clockCfg.utcOffset);
  gfx.print(" | ~");
  gfx.print(cfg.clockCfg.refreshMin);
  gfx.print("min | web: ");
  gfx.print(cfg.hostname);
  gfx.print(".local");

  Serial.println("Display updated.");
  gfx.display();
}

void showSetupScreen() {
  gfx.fillScreen(EPD_WHITE);

  // Red title bar
  gfx.fillRect(0, 0, 360, 34, EPD_RED);
  gfx.setTextColor(EPD_WHITE);
  gfx.setTextSize(2);
  gfx.setCursor(8, 9);
  gfx.print("SETUP MODE | e-Ink Clock");

  gfx.drawHLine(0, 34, 360, EPD_BLACK);

  // Step 1
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setCursor(8, 44);
  gfx.print("1. Connect to WiFi:");
  gfx.setTextColor(EPD_RED);
  gfx.setTextSize(2);
  gfx.setCursor(8, 56);
  gfx.print("eInkClock");
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setCursor(8, 76);
  gfx.print("password: 12345678");

  // Vertical divider
  gfx.drawVLine(180, 34, 140, EPD_BLACK);

  // Step 2
  gfx.setCursor(192, 44);
  gfx.print("2. Open in browser:");
  gfx.setTextColor(EPD_RED);
  gfx.setTextSize(2);
  gfx.setCursor(192, 56);
  gfx.print("192.168.4.1");
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setCursor(192, 76);
  gfx.print("Set WiFi, timezone.");

  gfx.drawHLine(0, 100, 360, EPD_BLACK);

  // Bottom hint
  // gfx.setTextColor(EPD_BLACK);
  // gfx.setCursor(8, 110);
  // gfx.print("");

  // Yellow accent bar at bottom
  gfx.fillRect(0, 130, 360, 54, EPD_YELLOW);
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setCursor(8, 140);
  gfx.print(
    "After saving config, device will restart and connect automatically.");
  gfx.setCursor(8, 155);
  gfx.print(
    "Access config anytime at  http://eink-clock.local  on your network.");

  gfx.display();
}

void boot_splash() {
  // ── Boot splash ───────────────────────────────────────
  gfx.fillScreen(EPD_WHITE);
  gfx.fillRect(0, 0, 360, 40, EPD_BLACK);
  gfx.setTextColor(EPD_WHITE);
  gfx.setTextSize(2);
  gfx.setCursor(8, 12);
  gfx.print("e-Ink Clock  —  ESP8266");
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(2);
  gfx.setCursor(8, 52);
  gfx.print("Connecting to WiFi...");
  gfx.setTextSize(1);
  gfx.setCursor(8, 74);
  gfx.print(cfg.wifi->ssid);
  gfx.setTextColor(EPD_YELLOW);
  gfx.setCursor(8, 90);
  gfx.print("Waveshare 2.66\" (G)  360x184  4-colour");
  gfx.display();
}
