/**
 * epd_gfx.cpp  —  GFX layer implementation (EPD_266 backend)
 */

#include "epd_gfx.h"
#include <Arduino.h>

EpdGfx::EpdGfx(EPD_266& epd)
  : _epd(epd), _textColor(EPD_BLACK), _textSize(1), _curX(0), _curY(0) {
}

void EpdGfx::fillScreen(UBYTE color) {
  if (color == EPD_WHITE) {
    _epd.clear(); // both layers -> white, fast path
    return;
  }
  fillRect(0, 0, EPD_WIDTH, EPD_HEIGHT, color);
}

// EPD_266 keeps two independent layers (BW + RED). A given screen
// pixel can only show ONE colour, so writing a pixel in one layer
// clears it in the other to keep them consistent.
void EpdGfx::drawPixel(int x, int y, UBYTE color) {
  y = EPD_HEIGHT - 1 - y;
  switch (color) {
  case EPD_BLACK:
    _epd.drawPixel(x, y, LAYER_BW, BLACK);
    _epd.drawPixel(x, y, LAYER_RED, WHITE);
    break;
  case EPD_RED:
    _epd.drawPixel(x, y, LAYER_RED, BLACK); // BLACK = "active" on the red layer
    _epd.drawPixel(x, y, LAYER_BW, WHITE);
    break;
  default: // EPD_WHITE
    _epd.drawPixel(x, y, LAYER_BW, WHITE);
    _epd.drawPixel(x, y, LAYER_RED, WHITE);
    break;
  }
}

void EpdGfx::drawHLine(int x, int y, int w, UBYTE color) {
  for (int i = 0; i < w; i++)
    drawPixel(x + i, y, color);
}

void EpdGfx::drawVLine(int x, int y, int h, UBYTE color) {
  for (int i = 0; i < h; i++)
    drawPixel(x, y + i, color);
}

void EpdGfx::drawRect(int x, int y, int w, int h, UBYTE color) {
  drawHLine(x, y, w, color);
  drawHLine(x, y + h - 1, w, color);
  drawVLine(x, y, h, color);
  drawVLine(x + w - 1, y, h, color);
}

void EpdGfx::fillRect(int x, int y, int w, int h, UBYTE color) {
  for (int row = y; row < y + h; row++)
    drawHLine(x, row, w, color);
}

void EpdGfx::drawLine(int x0, int y0, int x1, int y1, UBYTE color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;
  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y0 += sy;
    }
  }
}

void EpdGfx::setTextColor(UBYTE color) { _textColor = color; }
void EpdGfx::setTextSize(uint8_t size) { _textSize = (size < 1) ? 1 : size; }
void EpdGfx::setCursor(int x, int y) {
  _curX = x;
  _curY = y;
}

float EpdGfx::getScaleFactor(uint8_t size) {
  switch (size) {
    case 1: return 1.0f;
    case 2: return 1.5f;
    case 3: return 2.0f;
    default: return 1.0f * size;  // Linear scaling for larger sizes
  }
}

int EpdGfx::charHeight() { return (int)(8 * getScaleFactor(_textSize)); }

int EpdGfx::textWidth(const char* str) { return (int)(strlen(str) * 6 * getScaleFactor(_textSize)); }

// Draw a single 5x7 character scaled by custom factors
void EpdGfx::drawChar(int x, int y, char c, UBYTE color, uint8_t size) {
  if (c < 0x20 || c > 0x7E)
    c = '?';
  float scale = getScaleFactor(size);
  const uint8_t* glyph = font5x7[c - 0x20];
  for (int col = 0; col < 5; col++) {
    uint8_t line = pgm_read_byte(&glyph[col]);
    for (int row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        if (scale <= 1.0f) {
          // For size 1, just draw pixels (no scaling)
          drawPixel(x + col, y + row, color);
        }
        else {
          // For size > 1, use fillRect with scaled dimensions
          int scaledX = (int)(x + col * scale);
          int scaledY = (int)(y + row * scale);
          int scaledW = (int)(scale + 0.5f);
          int scaledH = (int)(scale + 0.5f);
          fillRect(scaledX, scaledY, scaledW, scaledH, color);
        }
      }
    }
  }
}

void EpdGfx::print(char c) {
  if (c == '\n') {
    _curY += charHeight();
    _curX = 0;
    return;
  }
  drawChar(_curX, _curY, c, _textColor, _textSize);
  _curX += (int)(6 * getScaleFactor(_textSize));
}

void EpdGfx::print(const char* str) {
  while (*str)
    print(*str++);
}

void EpdGfx::print(const String& s) { print(s.c_str()); }

void EpdGfx::print(int v) { print(String(v)); }

void EpdGfx::print(float v, int decimals) { print(String(v, decimals)); }

void EpdGfx::display() { 
  _epd.display();  // Non-blocking: just triggers update, returns immediately
}

void EpdGfx::displayWait() {
  _epd.display();  // Triggers update
  _epd.waitForRefresh();  // Then waits for completion
}

bool EpdGfx::isDisplayBusy() {
  return _epd.isBusy();  // Non-blocking: just checks BUSY pin
}