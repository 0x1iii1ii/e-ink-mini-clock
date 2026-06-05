/**
 * epd_font_big.h  —  Large rectangle segment font for e-ink clock
 *
 * Each digit is drawn as thick filled rectangles mimicking a
 * 7-segment style but with solid blocks — looks great on e-ink.
 *
 * Digit cell: 36 wide × 56 tall (including 2px gap on right)
 * Colon cell: 12 wide × 56 tall
 *
 * Segment layout per digit (like 7-segment):
 *
 *   [─ A ─]
 *   F     B
 *   [─ G ─]
 *   E     C
 *   [─ D ─]
 *
 * Thickness: 6px  Segment length: 24px
 */

#ifndef EPD_FONT_BIG_H
#define EPD_FONT_BIG_H

#include "epd_gfx.h"

#define LINE_TOP_BAR 20
#define LINE_BOTTOM_BAR 170

 // ── Clock area constants (must match drawDisplay dividers) ──
#define CLOCK_Y_TOP         LINE_TOP_BAR            // first pixel below top dividers
#define CLOCK_Y_BOT         (LINE_BOTTOM_BAR + 1)   // first pixel of bottom dividers
#define CLOCK_MARGIN        3                       // px margin inside the clock area

// ── Derived font dimensions (auto-fit to area) ─────────────
#define BF_H   (CLOCK_Y_BOT - CLOCK_Y_TOP - CLOCK_MARGIN * 2)   // 139 digit height
#define BF_W   (BF_H * 55 / 100)                                // digit width, 55% of height keeps a natural tall/narrow 7-seg proportion
#define BF_T   (BF_H * 14 / 100)                                // segment thickness, 14% of height scales with size
#define BF_GAP  6                                               // pixels between digits
#define BF_COLON 18                                             // width reserved for the : character

// ── Segment table: 7 bools per digit [A B C D E F G] ──────
static const bool SEGS[10][7] PROGMEM = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}, // 9
};

class BigFont {
public:
    BigFont(EpdGfx& gfx) : _gfx(gfx) {
    }

    // ── Draw HH:MM centred in clock area, style 0=sharp 1=bold
    void drawTime(int h, int m, UBYTE color, uint8_t style = 0) {
        int totalW = 4 * (BF_W + BF_GAP) + BF_COLON;

        // init position by centre horizontally in 360px
        int x = (360 - totalW) / 2;
        int y = CLOCK_Y_TOP + CLOCK_MARGIN;

        // draw each digit
        drawDigit(x, y, h / 10, color, style);
        drawDigit(x + BF_W + BF_GAP, y, h % 10, color, style);
        drawColon(x + 2 * (BF_W + BF_GAP), y, color, style);
        drawDigit(x + 2 * (BF_W + BF_GAP) + BF_COLON + BF_GAP, y, m / 10, color, style);
        drawDigit(x + 3 * (BF_W + BF_GAP) + BF_COLON + BF_GAP, y, m % 10, color, style);
    }

private:
    EpdGfx& _gfx;

    bool seg(uint8_t d, uint8_t s) {
        return pgm_read_byte(&SEGS[d][s]);
    }

    // ── SHARP style: solid filled rects ───────────────────
    void drawDigitSharp(int x, int y, uint8_t d, UBYTE col) {
        // int vsg = (BF_T / 2); // vert segment gab
        int sh = (BF_H / 2); // vert segment inner height
        int my = BF_H / 2 - BF_T / 2; // middle y offset
        int rx = BF_W - BF_T; // right column x offset

        // A top
        if (seg(d, 0)) _gfx.fillRect(x, y, BF_W, BF_T, col);
        // B top-right
        if (seg(d, 1)) _gfx.fillRect(x + rx, y, BF_T, sh, col);
        // C bot-right
        if (seg(d, 2)) _gfx.fillRect(x + rx, y + sh, BF_T, sh, col);
        // D bottom
        if (seg(d, 3)) _gfx.fillRect(x, y + BF_H - BF_T, BF_W, BF_T, col);
        // E bot-left
        if (seg(d, 4)) _gfx.fillRect(x, y + sh, BF_T, sh, col);
        // F top-left
        if (seg(d, 5)) _gfx.fillRect(x, y, BF_T, sh, col);
        // G middle
        if (seg(d, 6)) _gfx.fillRect(x, y + my, BF_W, BF_T, col);

        // Corner fill — plugs the junction gaps
        if (seg(d, 0) && seg(d, 5)) _gfx.fillRect(x, y, BF_T, BF_T, col);
        if (seg(d, 0) && seg(d, 1)) _gfx.fillRect(x + rx, y, BF_T, BF_T, col);
        if (seg(d, 3) && seg(d, 4)) _gfx.fillRect(x, y + BF_H - BF_T, BF_T, BF_T, col);
        if (seg(d, 3) && seg(d, 2)) _gfx.fillRect(x + rx, y + BF_H - BF_T, BF_T, BF_T, col);

        // Middle junctions
        if (seg(d, 5) && seg(d, 6)) _gfx.fillRect(x, y + BF_H / 2 - BF_T / 2, BF_T, BF_T, col);
        if (seg(d, 1) && seg(d, 6)) _gfx.fillRect(x + rx, y + BF_H / 2 - BF_T / 2, BF_T, BF_T, col);
    }

    // ── BOLD style: thick segment + inset contrast line ───
    // Draws sharp first, then draws a 2px white inset line
    // down the centre of each segment for a "grooved" look
    void drawDigitBold(int x, int y, uint8_t d, UBYTE col) {
        drawDigitSharp(x, y, d, col); // draw filled base

        // Inset groove: 2px white line through centre of each segment
        int sw = BF_W - BF_T * 2;
        int sh = BF_H / 2 - BF_T;
        int my = BF_H / 2 - BF_T / 2;
        int rx = BF_W - BF_T;
        int gc = BF_T / 2 - 1; // groove centre offset within segment

        // A top — horizontal groove
        if (seg(d, 0)) _gfx.drawHLine(x + BF_T, y + gc, sw, EPD_WHITE);
        // B top-right — vertical groove
        if (seg(d, 1)) _gfx.drawVLine(x + rx + gc, y + BF_T, sh, EPD_WHITE);
        // C bot-right
        if (seg(d, 2)) _gfx.drawVLine(x + rx + gc, y + BF_H / 2 + BF_T / 2, sh, EPD_WHITE);
        // D bottom
        if (seg(d, 3)) _gfx.drawHLine(x + BF_T, y + BF_H - BF_T + gc, sw, EPD_WHITE);
        // E bot-left
        if (seg(d, 4)) _gfx.drawVLine(x + gc, y + BF_H / 2 + BF_T / 2, sh, EPD_WHITE);
        // F top-left
        if (seg(d, 5)) _gfx.drawVLine(x + gc, y + BF_T, sh, EPD_WHITE);
        // G middle
        if (seg(d, 6)) _gfx.drawHLine(x + BF_T, y + my + gc, sw, EPD_WHITE);
    }

    void drawDigit(int x, int y, uint8_t d, UBYTE col, uint8_t style) {
        if (d > 9) return;
        if (style == 1) drawDigitBold(x, y, d, col);
        else drawDigitSharp(x, y, d, col);
    }

    void drawColon(int x, int y, UBYTE col, uint8_t style) {
        int dot = BF_T + 3;
        int cx = x + BF_COLON / 2 - dot / 2;
        int q = BF_H / 4;
        _gfx.fillRect(cx, y + q - dot / 2, dot, dot, col);
        _gfx.fillRect(cx, y + BF_H * 3 / 4 - dot / 2, dot, dot, col);
        if (style == 1) {
            // groove the dots too
            _gfx.drawHLine(cx + 1, y + q - dot / 2 + dot / 2, dot - 2, EPD_WHITE);
            _gfx.drawHLine(cx + 1, y + BF_H * 3 / 4 - dot / 2 + dot / 2, dot - 2, EPD_WHITE);
        }
    }
};

#endif
