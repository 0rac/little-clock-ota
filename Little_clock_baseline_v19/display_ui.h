#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>

// ============================================================
//  Display UI — public interface
// ============================================================

// Draw the current time (call once per second from loop)
void drawTime();

// Tick the non-blocking animation state machine
// Call this every loop() iteration regardless of whether a
// new second has elapsed.
void tickDisplayAnimation(unsigned long now);

// Status overlays
void drawWifiBar();
void drawNtpWarning();
void drawPMIndicator(bool isPM);

#endif
