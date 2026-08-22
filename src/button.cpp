//
// Created by Li on 5/1/2026.
//

#include "global.h"
#include "button.h"

// ════════════════════════════════════════════════════════════
//  Portal button detection
//  Held LOW for > PORTAL_HOLD_MS ms = enter portal mode
// ════════════════════════════════════════════════════════════

void beep(int freq, int duration) {
    tone(BZ_PIN, freq);
    delay(duration);
    noTone(BZ_PIN);
}

void startUpSound() {
    beep(1200, 80);
    delay(30);
    beep(1800, 80);
    delay(30);
    beep(2400, 120);
}

// ════════════════════════════════════════════════════════════
//  ALARM FUNCTIONS
// ════════════════════════════════════════════════════════════
// void checkAlarm() {
//     // Check if current time matches alarm time
//     if (!cfg.clockCfg.alarm.enabled) return;

//     struct tm t;
//     if (!getRtcTime(&t)) return;

//     // Only trigger once per minute to avoid repeated alarms
//     unsigned long now = millis();
//     if (now - g_alarmLastTrigger < 60000UL) return;

//     if (t.tm_hour == cfg.clockCfg.alarm.hour &&
//         t.tm_min == cfg.clockCfg.alarm.minute) {
//         g_alarmTriggered = true;
//         g_alarmLastTrigger = now;
//         Serial0.println("=== ALARM TRIGGERED ===");
//         alarmSound2();  // Use existing alarm sound
//     }
// }

// void startAlarmMode() {
//     g_alarmSettingMode = true;
//     g_alarmEditIdx = 0;  // Start with hour
//     Serial0.println("Entering alarm set mode");
//     // drawDisplay();  // Redraw with alarm UI
// }

// void checkAlarm() {
//     static unsigned long lastAlarmCheck = 0;
//     unsigned long now = millis();
//     if (now - lastAlarmCheck < 1000UL) return;
//     lastAlarmCheck = now;

//     struct tm t;
//     if (!getRtcTime(&t)) return;

//     for (uint8_t i = 0; i < 5; i++) {
//         alarm_t& al = cfg.clock.alarm[i];
//         if (!al.enabled) continue;

//         if (t.tm_hour != al.hour || t.tm_min != al.minute) continue;

//         if (al.repeatDays != 0 && ((al.repeatDays & (1u << t.tm_wday)) == 0)) continue;

//         unsigned long snoozeMs = (unsigned long) max(al.snooze.minutes, (uint8_t) 1) * 60000UL;
//         if (g_alarmLastTrigger != 0 && now - g_alarmLastTrigger < snoozeMs) continue;

//         g_alarmTriggered = true;
//         g_alarmLastTrigger = now;
//         Serial0.printf("Alarm triggered: %s at %02u:%02u\n", al.name, al.hour, al.minute);
//         alarmSound2();
//         break;
//     }
// }

// void exitAlarmMode() {
//     g_alarmSettingMode = false;
//     g_alarmEditIdx = 0;
//     Serial0.println("Exiting alarm set mode");
//     // drawDisplay();  // Redraw normal display
// }

// void handleAlarmButton() {
//   if (!g_alarmSettingMode) return;

//   bool b1 = digitalRead(B1_PIN);
//   bool b2 = digitalRead(B2_PIN);

//   // B1 = Increment current field, B2 = Move to next field / confirm
//   if (b1 == LOW) {
//     beep(1500, 50);  // Click sound
//     if (g_alarmEditIdx == 0) {
//       // Hour editing
//       cfg.clockCfg.alarm.hour = (cfg.clockCfg.alarm.hour + 1) % 24;
//     }
//     else {
//       // Minute editing
//       cfg.clockCfg.alarm.minute = (cfg.clockCfg.alarm.minute + 1) % 60;
//     }
//     drawDisplay();
//     delay(200);  // Debounce
//   }

//   if (b2 == LOW) {
//     beep(2000, 50);  // Different click sound
//     if (g_alarmEditIdx == 0) {
//       g_alarmEditIdx = 1;  // Move to minute
//     }
//     else {
//       cfg.clockCfg.alarm.enabled = !cfg.clockCfg.alarm.enabled;  // Toggle enable
//       exitAlarmMode();  // Exit mode
//     }
//     drawDisplay();
//     delay(200);  // Debounce
//   }
// }

// void alarmSound() {
//     Serial0.println("=== ALARM ===");

//     unsigned long start = millis();

//     // Classic Casio F-91W alarm pattern:
//     // 4 rapid high-pitched beeps, short pause, repeat
//     while (millis() - start < 3000) {
//         // Four rapid beeps (1200 Hz - high metallic tone)
//         for (int i = 0; i < 4; i++) {
//             tone(BZ_PIN, 1200);
//             delay(120);
//             noTone(BZ_PIN);
//             delay(80);
//         }
//         // Longer pause before repeat
//         delay(200);
//     }
// }

// void alarmSound2() {
//     unsigned long start = millis();

//     while (millis() - start < 6000) {
//         beep(2500, 100);
//         delay(100);
//         beep(2500, 100);
//         delay(1000);
//     }
// }

// bool WakeButtonHeld() {
//     if (digitalRead(VBUS_PIN) == HIGH) return false;  // not pressed — fast path
//     // Debounce: must stay LOW for PORTAL_HOLD_MS
//     unsigned long t0 = millis();
//     while (millis() - t0 < PORTAL_HOLD_MS) {
//         if (digitalRead(VBUS_PIN) == HIGH) return false;
//         delay(10);
//     }
//     return true;
// }

// void handleAlarmConfig() {
//     // Both buttons held for 2 seconds = Enter alarm setting mode
//     if (digitalRead(B1_PIN) == LOW && digitalRead(B2_PIN) == LOW) {
//         unsigned long pressStart = millis();
//         while (digitalRead(B1_PIN) == LOW && digitalRead(B2_PIN) == LOW) {
//             if (millis() - pressStart > 2000) {
//                 // Both buttons held > 2 sec = alarm mode
//                 if (!g_alarmSettingMode) {
//                     startAlarmMode();
//                 }
//                 else {
//                     exitAlarmMode();
//                 }
//                 while (digitalRead(B1_PIN) == LOW || digitalRead(B2_PIN) == LOW) {
//                     delay(10);
//                 }
//                 last_B1 = HIGH;
//                 last_B2 = HIGH;
//                 return;
//             }
//             delay(10);
//         }

//         // Short press (< 2 sec) of both buttons = trigger test alarm
//         if (millis() - pressStart < 2000) {
//             alarmSound2();
//         }

//         // Wait until both buttons are released
//         while (digitalRead(B1_PIN) == LOW || digitalRead(B2_PIN) == LOW) {
//             delay(10);
//         }

//         last_B1 = HIGH;
//         last_B2 = HIGH;
//         return;
//     }
//     if (g_alarmSettingMode) {
//         // In alarm setting mode, buttons are used for editing
//         handleAlarmButton();
//     }
// }