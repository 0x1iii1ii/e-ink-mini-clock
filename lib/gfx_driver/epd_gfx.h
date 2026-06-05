/**
 * epd_gfx.h  —  Lightweight GFX layer for Waveshare 2.66" (G) framebuffer
 *
 * Provides text rendering and basic shapes directly into the 2-bit
 * colour framebuffer without needing GxEPD2 or Adafruit GFX.
 *
 * Built-in font: 5x7 pixel ASCII glyphs (classic Arduino font)
 * Text scaling is integer: size=1 → 6x8 px per char, size=2 → 12x16, etc.
 */

#ifndef EPD_GFX_H
#define EPD_GFX_H

#include <Arduino.h>
#include "epd2in66g.h"

class EpdGfx {
public:
    EpdGfx(Epd &epd, UBYTE *buf);

    // ── Drawing primitives ────────────────────────────────
    void fillScreen(UBYTE color);
    void drawPixel(int x, int y, UBYTE color);
    void drawHLine(int x, int y, int w, UBYTE color);
    void drawVLine(int x, int y, int h, UBYTE color);
    void drawRect(int x, int y, int w, int h, UBYTE color);
    void fillRect(int x, int y, int w, int h, UBYTE color);
    void drawLine(int x0, int y0, int x1, int y1, UBYTE color);

    // ── Text ──────────────────────────────────────────────
    void setTextColor(UBYTE color);
    void setTextSize(uint8_t size);     // 1 = 6x8, 2 = 12x16, etc.
    void setCursor(int x, int y);
    void print(const char *str);
    void print(const String &s);
    void print(int v);
    void print(float v, int decimals = 1);
    void print(char c);

    // Text metrics (pixels)
    int  textWidth(const char *str);
    int  charHeight();

    // ── Push framebuffer to display ───────────────────────
    void display();

private:
    Epd    &_epd;
    UBYTE  *_buf;
    UBYTE   _textColor;
    uint8_t _textSize;
    int     _curX, _curY;

    void drawChar(int x, int y, char c, UBYTE color, uint8_t size);
};

#endif
