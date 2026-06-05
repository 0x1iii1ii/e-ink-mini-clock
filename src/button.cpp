//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "button.h"

// ════════════════════════════════════════════════════════════
//  Portal button detection
//  Held LOW for > PORTAL_HOLD_MS ms = enter portal mode
// ════════════════════════════════════════════════════════════

bool WakeButtonHeld() {
    if (digitalRead(VBUS_PIN) == HIGH) return false;  // not pressed — fast path
    // Debounce: must stay LOW for PORTAL_HOLD_MS
    unsigned long t0 = millis();
    while (millis() - t0 < PORTAL_HOLD_MS) {
        if (digitalRead(VBUS_PIN) == HIGH) return false;
        delay(10);
    }
    return true;
}