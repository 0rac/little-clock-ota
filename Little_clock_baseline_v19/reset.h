#pragma once

#include <Arduino.h>

// Public API
void serviceResetButton(unsigned long now);
void performSoftReboot();
void performFactoryReset();

// ISR
void IRAM_ATTR onResetISR();

// Reset timing constants (defined in main .ino)
extern const unsigned long RESET_DEBOUNCE_MS;
extern const unsigned long RESET_REBOOT_MS;
extern const unsigned long RESET_INFO_MS;
extern const unsigned long RESET_FACTORY_MS;
