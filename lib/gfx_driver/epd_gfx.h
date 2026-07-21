/**
 * epd_gfx.h  —  Lightweight GFX layer for the EPD_266 (296x152, B/W + RED)
 *
 * Provides text rendering and basic shapes on top of EPD_266's two
 * internal layer buffers (bufferBW / bufferR) without needing GxEPD2
 * or Adafruit GFX.
 *
 * This panel only has 3 colours (white / black / red) — anywhere the
 * old code used EPD_YELLOW, use EPD_RED instead.
 *
 * Built-in font: 5x7 pixel ASCII glyphs (classic Arduino font)
 * Text scaling is integer: size=1 → 6x8 px per char, size=2 → 12x16, etc.
 */

#ifndef EPD_GFX_H
#define EPD_GFX_H

#include <Arduino.h>
#include "epd_266.h"

#ifndef UBYTE
typedef uint8_t UBYTE;
#endif

// ── Tri-colour palette (this panel has no yellow) ─────────
#define EPD_WHITE 0
#define EPD_BLACK 1
#define EPD_RED   2

class EpdGfx {
public:
    explicit EpdGfx(EPD_266& epd);

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
    void setTextSize(uint8_t size);     // 1=6x8, 2=9x12, 3=12x16
    void setCursor(int x, int y);
    void print(const char* str);
    void print(const String& s);
    void print(int v);
    void print(float v, int decimals = 1);
    void print(char c);

    // Text metrics (pixels)
    int  textWidth(const char* str);
    int  charHeight();

    // ── Push framebuffer to display ───────────────────────
    void display();                     // Non-blocking: update and return immediately
    void displayWait();                 // Blocking: update and wait for completion
    bool isDisplayBusy();               // Non-blocking: check if display is refreshing

private:
    EPD_266& _epd;
    UBYTE   _textColor;
    uint8_t _textSize;
    int     _curX, _curY;

    void drawChar(int x, int y, char c, UBYTE color, uint8_t size);
    float getScaleFactor(uint8_t size);  // Returns: 1.0 for size 1, 1.5 for size 2, 2.0 for size 3
};

#endif