#include "reset.h"
#include "pins.h"         // RESET_PIN defined here — do not use literal 23

#include <WiFi.h>
#include <Preferences.h>

// ============================================================
//  EXTERNALS FROM MAIN SKETCH
// ============================================================
extern Preferences prefs;

extern volatile bool resetEdge;
extern unsigned long lastResetEdgeMs;
extern const unsigned long RESET_DEBOUNCE_MS;

extern unsigned long resetPressStartMs;
extern const unsigned long RESET_REBOOT_MS;
extern const unsigned long RESET_INFO_MS;
extern const unsigned long RESET_FACTORY_MS;

extern void scrollText(const char *msg);
extern void startInfoPortal();

// ============================================================
//  RESET BUTTON ISR
// ============================================================
void IRAM_ATTR onResetISR()
{
  resetEdge = true;
}

// ============================================================
//  RESET ACTIONS
// ============================================================
void performSoftReboot()
{
  scrollText("Rebooting...");
  ESP.restart();
}

void performFactoryReset()
{
  scrollText("Factory Reset...");

  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  prefs.begin("clock", false);
  prefs.clear();
  prefs.end();

  WiFi.disconnect(true, true);
  delay(300);

  ESP.restart();
}

// ============================================================
//  UNIFIED RESET SERVICE
// ============================================================
void serviceResetButton(unsigned long now)
{
  bool held = (digitalRead(RESET_PIN) == LOW);   // uses define from pins.h

  // ----------------------------------------------------------
  // Start timing on ISR edge
  // ----------------------------------------------------------
  if (resetEdge && held && resetPressStartMs == 0)
  {
    if (now - lastResetEdgeMs > RESET_DEBOUNCE_MS)
    {
      resetPressStartMs = now;
      lastResetEdgeMs   = now;
    }

    resetEdge = false;
  }

  // ----------------------------------------------------------
  // Button released → decide action based on hold duration
  // ----------------------------------------------------------
  if (resetPressStartMs > 0 && !held)
  {
    unsigned long heldMs = now - resetPressStartMs;
    resetPressStartMs = 0;

    if (heldMs >= RESET_FACTORY_MS)
    {
      performFactoryReset();
    }
    else if (heldMs >= RESET_INFO_MS)
    {
      scrollText("Entering Options Menu...");
      startInfoPortal();
    }
    else if (heldMs >= RESET_REBOOT_MS)
    {
      performSoftReboot();
    }
  }
}
