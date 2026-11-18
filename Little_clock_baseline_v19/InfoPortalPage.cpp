#include "portal_info.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <NTPClient.h>

#include "version.h"
#include "InfoPortalPage.h"
#include "FontDigits.h"

// ============================================================
//  EXTERNALS OWNED BY MAIN SKETCH
// ============================================================
extern WebServer   server;
extern Preferences prefs;
extern NTPClient   timeClient;

extern String pendingUpdateVersion;

extern int    compareVersions(const String &a, const String &b);
extern String getDeviceSerial();

// ============================================================
//  FALLBACK DEFINES (SAFE DEFAULTS)
// ============================================================
#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

#ifndef OTA_VERSION_URL
#define OTA_VERSION_URL ""
#endif

// ============================================================
//  INFO LANDING PAGE (/)
// ============================================================
void handleInfoRoot()
{
  String ssid =
    (WiFi.status() == WL_CONNECTED)
      ? WiFi.SSID()
      : String("Not connected");

  String ip =
    (WiFi.status() == WL_CONNECTED)
      ? WiFi.localIP().toString()
      : String("0.0.0.0");

  String mac = WiFi.macAddress();

  String page = INFO_PORTAL_PAGE;

  page.replace("%SSID%",   ssid);
  page.replace("%IP%",     ip);
  page.replace("%MAC%",    mac);
  page.replace("%VERSION%", String(FW_VERSION));
  page.replace("%BUILD%",  String(__DATE__) + " " + String(__TIME__));
  page.replace("%SERIAL%", getDeviceSerial());

  // ----------------------------------------------------------
  // Populate Options tab state
  // ----------------------------------------------------------
  prefs.begin("clock", true);

  int  timeMode   = prefs.getInt("time_mode", 0);
  int  timeFormat = prefs.getInt("time_format", 12);
  bool blinkColon = prefs.getBool("blink_colon", true);
  bool wifiBar    = prefs.getBool("wifi_bar", true);

  uint8_t font = prefs.getUChar("font", 255);
  bool hasFont = prefs.isKey("font");

  prefs.end();
  page.replace(
    "%TIME12_CHECKED%",
    (timeMode == 0 && timeFormat == 12) ? "checked" : ""
  );

  page.replace(
    "%TIME24_CHECKED%",
    (timeMode == 0 && timeFormat == 24) ? "checked" : ""
  );

  page.replace(
    "%TIMEHEX_CHECKED%",
    (timeMode == 1) ? "checked" : ""
  );

  page.replace("%BLINK_CHECKED%",  blinkColon         ? "checked"  : "");
  page.replace("%WIFIBAR_CHECKED%",wifiBar            ? "checked"  : "");

  page.replace(
    "%FONT_DEFAULT_SELECTED%",
    (!hasFont) ? "selected" : ""
  );
  page.replace(
    "%FONT_BLOCKY_SELECTED%",
    (hasFont && font == FONT_BLOCKY) ? "selected" : ""
  );
  page.replace(
    "%FONT_BOLD_SELECTED%",
    (hasFont && font == FONT_BOLD) ? "selected" : ""
  );

  server.send(200, "text/html", page);
}

// ============================================================
//  UPDATE CHECK (/check)
// ============================================================
void handleInfoCheck()
{
  String message;

  if (WiFi.status() != WL_CONNECTED)
  {
    message =
      "No WiFi connection.<br><br>"
      "Please connect to WiFi and try again.";
  }
  else
  {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code =
      http.begin(
        client,
        "https://raw.githubusercontent.com/0rac/little-clock-ota/main/version.json"
      )
      ? http.GET()
      : -1;

    if (code != HTTP_CODE_OK)
    {
      http.end();
      message =
        "CONTACT FAIL - BUILD TEST A<br><br>"
        "HTTP code: " + String(code);
    }
    else
    {
      String payload = http.getString();
      http.end();

      String latest = "";

      int p = payload.indexOf("\"version\"");
      if (p >= 0)
      {
        int q = payload.indexOf(":", p);
        int r = payload.indexOf("\"", q + 1);
        int s = payload.indexOf("\"", r + 1);

        if (r >= 0 && s >= 0)
        {
          latest = payload.substring(r + 1, s);
        }
      }

      if (latest.length() == 0)
      {
        message = "Invalid version data received.";
      }
      else
      {
        int cmp = compareVersions(String(FW_VERSION), latest);

        if (cmp >= 0)
        {
          message =
            "<b>Up to date</b><br><br>"
            "Installed version: " + String(FW_VERSION) + "<br>"
            "Available version: " + latest;
        }
        else
        {
          pendingUpdateVersion = latest;

          String page = INFO_UPDATE_CONFIRM_PAGE;
          page.replace("%NEW_VERSION%", latest);

          server.send(200, "text/html", page);
          return;
        }
      }
    }
  }

  // ----------------------------------------------------------
  // Render status page
  // ----------------------------------------------------------
  String page;
  page.reserve(900);

  page +=
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Update Status</title>"
    "<style>"
    "body{font-family:Arial,Helvetica,sans-serif;"
    "background:#f0f0f0;margin:0;padding:0;}"
    ".card{max-width:460px;margin:14px auto;background:#fff;"
    "padding:24px;border-radius:10px;"
    "box-shadow:0 2px 6px rgba(0,0,0,0.18);"
    "text-align:center;font-size:17px;}"
    "button{width:100%;padding:12px;margin-top:18px;"
    "font-size:17px;border-radius:6px;"
    "background:#0078d7;color:#fff;"
    "border:none;font-weight:bold;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h2>Update Status</h2>";

  page += message;

  page +=
    "<form action='/' method='GET'>"
    "<button type='submit'>Back</button>"
    "</form>"
    "</div></body></html>";

  server.send(200, "text/html", page);
}

// ============================================================
//  EXIT HANDLER (/exit)
// ============================================================
void handleInfoExit()
{
  server.send(
    200,
    "text/html",
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Exit</title>"
    "<style>"
    "body{font-family:Arial,Helvetica,sans-serif;"
    "background:#f0f0f0;margin:0;padding:0;}"
    ".card{max-width:460px;margin:40px auto;background:#fff;"
    "padding:24px;border-radius:10px;"
    "box-shadow:0 2px 6px rgba(0,0,0,0.18);"
    "text-align:center;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h3>Returning to clock</h3>"
    "</div>"
    "</body></html>"
  );

  extern volatile bool exitInfoRequested;
  exitInfoRequested = true;
}

// ============================================================
//  OPTIONS SAVE HANDLER (/info/options_save)
// ============================================================
void handleInfoOptionsSave()
{
  bool hexMode    = server.hasArg("hex_mode");
  bool hour24     = server.hasArg("hour_24");
  bool blinkColon = server.hasArg("blink_colon");
  bool wifiBar    = server.hasArg("wifi_bar");

  int selectedFont = -1;
  if (server.hasArg("font"))
  {
    selectedFont = server.arg("font").toInt();
  }

  // ----------------------------------------------------------
  // Enforce Blocky / Bold rules
  // ----------------------------------------------------------
  if (selectedFont == FONT_BLOCKY || selectedFont == FONT_BOLD)
  {
    wifiBar = false;   // Blocky and Bold always disable WiFi bar
  }

  prefs.begin("clock", false);

  if (server.hasArg("time_mode"))
  {
    String mode = server.arg("time_mode");

    if (mode == "hex")
    {
      prefs.putInt("time_mode", 1);   // hex
      // time_format not used in hex
    }
    else if (mode == "24")
    {
      prefs.putInt("time_mode", 0);   // decimal
      prefs.putInt("time_format", 24);
    }
    else // "12"
    {
      prefs.putInt("time_mode", 0);   // decimal
      prefs.putInt("time_format", 12);
    }
  }

  prefs.putBool("blink_colon", blinkColon);
  prefs.putBool("wifi_bar",    wifiBar);

  // ----------------------------------------------------------
  // Font save clamp
  // ----------------------------------------------------------
  if (selectedFont >= FONT_BLOCKY && selectedFont <= FONT_BOLD)
  {
    prefs.putUChar("font", (uint8_t)selectedFont);
  }
  else
  {
    // Default Parola
    prefs.remove("font");
  }

  prefs.end();

  server.send(
    200,
    "text/html",
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Saved</title>"
    "<style>"
    "body{font-family:Arial,Helvetica,sans-serif;"
    "background:#f0f0f0;margin:0;padding:0;}"
    ".card{max-width:460px;margin:40px auto;background:#fff;"
    "padding:24px;border-radius:10px;"
    "box-shadow:0 2px 6px rgba(0,0,0,0.18);"
    "text-align:center;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h3>Settings Saved</h3>"
    "<p>Rebooting</p>"
    "</div>"
    "</body></html>"
  );

  delay(800);
  ESP.restart();
}

