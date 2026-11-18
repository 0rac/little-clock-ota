#pragma once

#include <Arduino.h>

// OTA state (shared)
extern volatile int otaProgress;

enum OtaState
{
  OTA_IDLE,
  OTA_DOWNLOADING,
  OTA_INSTALLING,
  OTA_DONE,
  OTA_ERROR
};
extern volatile OtaState otaState;

// OTA API
void handleStartUpdate();          // HTTP-safe: flag + redirect
void handleInfoInstallRequest();   // compatibility route
void handleInfoInstall();           // OTA engine (non-HTTP)
void drawOtaProgress();
