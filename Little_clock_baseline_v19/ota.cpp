#include "ota.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>

#include <MD_Parola.h>
#include <MD_MAX72XX.h>

// ============================================================
//  EXTERNALS FROM MAIN SKETCH
// ============================================================
extern WebServer server;
extern MD_Parola display;
extern volatile bool otaRequested;

// ============================================================
//  OTA GLOBALS
// ============================================================
volatile int      otaProgress = 0;
volatile OtaState otaState    = OTA_IDLE;

// ============================================================
//  HTTP-SAFE OTA START (REDIRECT + FLAG ONLY)
// ============================================================
void handleStartUpdate()
{
  otaRequested = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------------------------------------------------
// Compatibility route (never runs OTA here)
// ------------------------------------------------------------
void handleInfoInstallRequest()
{
  otaRequested = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
//  OTA ENGINE (RUNS OUTSIDE HTTP CONTEXT)
// ============================================================
void handleInfoInstall()
{
  display.displayClear();

  otaState    = OTA_DOWNLOADING;
  otaProgress = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    otaState = OTA_ERROR;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  extern const char *OTA_BIN_URL;

  if (!http.begin(client, OTA_BIN_URL))
  {
    otaState = OTA_ERROR;
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    otaState = OTA_ERROR;
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0)
  {
    http.end();
    otaState = OTA_ERROR;
    return;
  }

  if (!Update.begin(contentLength))
  {
    http.end();
    otaState = OTA_ERROR;
    return;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written     = 0;

  uint8_t buffer[1024];

  while (http.connected() && written < (size_t)contentLength)
  {
    size_t available = stream->available();

    if (available)
    {
      size_t toRead = (available > sizeof(buffer))
                        ? sizeof(buffer)
                        : available;

      int bytes = stream->readBytes(buffer, toRead);

      if (bytes > 0)
      {
        Update.write(buffer, bytes);
        written += bytes;

        otaProgress = (written * 100) / contentLength;
        drawOtaProgress();
      }
    }

    yield();
  }

  http.end();

  if (!Update.end(true))
  {
    otaState = OTA_ERROR;
    return;
  }

  otaState = OTA_DONE;
  delay(1000);
  ESP.restart();
}

// ============================================================
//  OTA PROGRESS DISPLAY
// ============================================================
void drawOtaProgress()
{
  if (otaState == OTA_IDLE)
    return;

  MD_MAX72XX *mx = display.getGraphicObject();
  mx->clear();

  int bars = map(otaProgress, 0, 100, 0, 8);

  for (int i = 0; i < bars; i++)
  {
    mx->setPoint(7, i + 12, true);
    mx->setPoint(6, i + 12, true);
  }

  display.displayAnimate();
}
