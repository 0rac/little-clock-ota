// ============================================================
//  ESP32 LITTLE CLOCK — CLEAN FINAL BASELINE
//  - Preserves all behaviour
//  - Fixes OTA redirect bug correctly (303 + flag, OTA outside HTTP)
//  - Removes provably dead/duplicate code only
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <HTTPClient.h>
#include <Update.h>
#include <stdlib.h>
#include <time.h>

#include "pins.h"           // single source of truth for all pin numbers
#include "version.h"
#include "PortalPage.h"
#include "InfoPortalPage.h"
#include "ota.h"
#include "reset.h"
#include "portal_info.h"
#include "display_ui.h"
#include "FontDigits.h"

// ============================================================
// SECTION 1 — DISPLAY SETUP
// ============================================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   4

#define OTA_VERSION_URL "https://raw.githubusercontent.com/0rac/little-clock-ota/main/version.json"

const char *OTA_BIN_URL =
  "https://raw.githubusercontent.com/0rac/little-clock-ota/main/"
  "Little-clock-baseline.ino.bin";

MD_Parola display =
  MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ============================================================
// SECTION 2 — PREFERENCES + WEB
// ============================================================
Preferences prefs;
WebServer server(80);

const byte DNS_PORT = 53;
DNSServer dnsServer;

// ============================================================
// SECTION 3 — FORWARD DECLARATIONS
// ============================================================

// Failsafe AP portal
void handleScanPage();
void handleFailRoot();
void handleFailSave();
void startApFailSafe();

// Info / Update portal
void handleInfoRoot();
void handleInfoCheck();
void handleInfoOptionsSave();
void handleInfoExit();
void startInfoPortal();

// OTA
void handleInfoInstall();
void drawOtaProgress();

// Reset / system
void serviceResetButton(unsigned long now);
void performSoftReboot();
void performFactoryReset();
void IRAM_ATTR onResetISR();

// Update start (redirect + flag)
void handleStartUpdate();
void handleInfoInstallRequest();

// Misc
String getDeviceSerial();
int compareVersions(const String &a, const String &b);

// ============================================================
// SECTION 4 — GLOBAL STATE / FLAGS
// ============================================================
volatile bool resetEdge         = false;
volatile bool exitInfoRequested = false;
volatile bool otaRequested      = false;
int setting_bold_animation      = 0;
FontStyle currentFont           = (FontStyle)255;

enum DisplayMode
{
  DISP_CLOCK,
  DISP_BLOCKING    // portals, scrolls, OTA, etc.
};

volatile DisplayMode displayMode = DISP_CLOCK;

// Update metadata
String pendingUpdateVersion = "";

// ============================================================
// SECTION 5 — NTP TIME SETUP
// ============================================================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);  // Keep the NTP clock in UTC.

// Mainland NZ: last Sunday in September at 02:00 NZST
// through the first Sunday in April at 03:00 NZDT.
static constexpr char NZ_TIME_ZONE[] = "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
static constexpr unsigned long NTP_POLL_MS = 60000UL;
static constexpr unsigned long NTP_RETRY_MS = 30000UL;
static constexpr unsigned long NTP_STALE_MS = 120000UL;

// ============================================================
// SECTION 6 — APP STATE
// ============================================================
bool colonVisible           = true;
bool ntpSyncOK              = false;

unsigned long lastSecond        = 0;
static unsigned long lastNtpSuccessMs = 0;
static unsigned long lastNtpAttempt   = 0;

const unsigned long RESET_DEBOUNCE_MS = 80;
unsigned long lastResetEdgeMs         = 0;

// ============================================================
// SECTION 7 — USER SETTINGS (FLASH)
// ============================================================
int  setting_time_format = 12;
bool setting_blink_colon = true;
bool setting_wifi_bar    = true;
int  setting_time_mode   = 0;

// ============================================================
// SECTION 7a — FONT SETTING (Info tab selectable)
// ============================================================
void loadFontSetting()
{
  prefs.begin("clock", true);

  uint8_t saved = prefs.getUChar("font", 255);

  if (saved <= FONT_BOLD)
  {
    currentFont = (FontStyle)saved;
  }
  else
  {
    currentFont = (FontStyle)255;   // default Parola
  }

  prefs.end();
}

// ============================================================
// SECTION 8 — SERIAL NUMBER (DEVICE ID)
// ============================================================
String getDeviceSerial()
{
  uint64_t mac  = ESP.getEfuseMac();
  uint32_t tail = mac & 0xFFFFFF;

  char buf[16];
  snprintf(buf, sizeof(buf), "LC-%07lu", (unsigned long)tail);

  return String(buf);
}

// ============================================================
// SECTION 9 — VERSION COMPARE
// ============================================================
int compareVersions(const String &a, const String &b)
{
  int a1 = 0, a2 = 0, a3 = 0;
  int b1 = 0, b2 = 0, b3 = 0;

  sscanf(a.c_str(), "%d.%d.%d", &a1, &a2, &a3);
  sscanf(b.c_str(), "%d.%d.%d", &b1, &b2, &b3);

  if (a1 != b1) return a1 - b1;
  if (a2 != b2) return a2 - b2;
  return a3 - b3;
}

// ============================================================
// SECTION 10 — NTP SYNCHRONIZATION
// ============================================================
static void attemptNtpSync()
{
  lastNtpAttempt = millis();
  // The caller controls the interval. forceUpdate reports the actual attempt.
  if (WiFi.status() == WL_CONNECTED && timeClient.forceUpdate())
  {
    lastNtpSuccessMs = millis();
    ntpSyncOK = true;
  }
}

// ============================================================
// SECTION 11 — RESET BUTTON LOGIC
// ============================================================
unsigned long resetPressStartMs = 0;

const unsigned long RESET_REBOOT_MS  = 1000;    // >1 second  → soft reboot
const unsigned long RESET_INFO_MS    = 5000;    // >5 seconds → info portal
const unsigned long RESET_FACTORY_MS = 20000;   // >20 seconds → factory reset

// ============================================================
// SECTION 12 — WIFI CONNECT WITH DOTS (BLOCKING)
// ============================================================
bool connectWiFiWithDots(const String &ssid, const String &pass)
{
  if (ssid.isEmpty()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start    = millis();
  unsigned long lastAnim = 0;
  uint8_t       dotPos   = 0;   // explicit counter — avoids millis() modulo drift

  while (millis() - start < 60000)
  {
    serviceResetButton(millis());

    if (WiFi.status() == WL_CONNECTED)
      return true;

    if (millis() - lastAnim > 800)
    {
      lastAnim = millis();

      char buf[9] = "        ";
      buf[dotPos % 8] = '.';
      dotPos++;

      display.displayClear();
      display.setTextAlignment(PA_CENTER);
      display.print(buf);
    }

    delay(10);
  }

  return false;
}

// ============================================================
// SECTION 13 — FAILSAFE AP MODE CONFIG PORTAL
// ============================================================
void startApFailSafe()
{
  displayMode = DISP_BLOCKING;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Grandfather-Clock");

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/",     handleScanPage);
  server.on("/scan", handleFailRoot);
  server.on("/save", HTTP_POST, handleFailSave);

  server.begin();

  display.displayClear();
  display.displayScroll(
    (char *)"Please connect to Grandfather-Clock and go to 192.168.4.1",
    PA_CENTER,
    PA_SCROLL_LEFT,
    30
  );
  display.displayReset();

  while (true)
  {
    serviceResetButton(millis());

    if (display.displayAnimate())
    {
      delay(300);
      display.displayReset();
    }

    dnsServer.processNextRequest();
    server.handleClient();
    yield();
  }
}

// ------------------------------------------------------------
// Scan splash page
// ------------------------------------------------------------
void handleScanPage()
{
  String page;
  page.reserve(900);

  page += F(
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='1;url=/scan'>"
    "<title>Little Clock Setup</title>"
    "<style>"
    "body{font-family:Arial,Helvetica,sans-serif;"
    "margin:0;padding:0;background:#f0f0f0;"
    "display:flex;justify-content:center;align-items:center;height:100vh;}"
    ".card{background:#fff;padding:30px 26px;border-radius:12px;"
    "box-shadow:0 2px 6px rgba(0,0,0,0.18);text-align:center;font-size:22px;}"
    "</style></head><body>"
    "<div class='card'>Scanning for WiFi<br>networks.</div></body></html>"
  );

  server.send(200, "text/html", page);
}

// ------------------------------------------------------------
// Failsafe main form page
// ------------------------------------------------------------
void handleFailRoot()
{
  WiFi.mode(WIFI_AP_STA);
  delay(120);

  String page = PORTAL_HEADER;
  page += PORTAL_BODY_TOP;

  WiFi.scanDelete();
  delay(200);

  int n = WiFi.scanNetworks(false, true);

  if (n <= 0)
  {
    page += F("<p>No networks found. Try again in a moment.</p>");
  }
  else
  {
    String shown[20];
    int shownCount = 0;

    for (int i = 0; i < n && shownCount < 20; i++)
    {
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;

      bool duplicate = false;
      for (int j = 0; j < shownCount; j++)
      {
        if (ssid == shown[j])
        {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      shown[shownCount++] = ssid;

      ssid.replace("'", "");
      page += "<button type='button' onclick=\"setSSID('";
      page += ssid;
      page += "')\">";
      page += ssid;
      page += "</button>";
    }
  }

  page += PORTAL_BODY_BOTTOM;
  server.send(200, "text/html", page);
}

// ------------------------------------------------------------
// Failsafe save handler
// ------------------------------------------------------------
void handleFailSave()
{
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length())
  {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
  }

  server.send(
    200,
    "text/html",
    "<html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body{font-family:Arial;text-align:center;padding-top:60px;font-size:24px;}"
    "</style></head><body>"
    "<b>Settings Saved!</b><br><br>Rebooting"
    "</body></html>"
  );

  delay(1500);
  ESP.restart();
}

// ============================================================
// SECTION 14 — START INFO / UPDATE PORTAL (BLOCKING)
// ============================================================
void startInfoPortal()
{
  displayMode = DISP_BLOCKING;
  display.displayClear();

  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  IPAddress ip = wifiOK ? WiFi.localIP() : IPAddress(192, 168, 4, 1);

  char msg[64];

  if (wifiOK)
  {
    snprintf(
      msg,
      sizeof(msg),
      "Info mode..... Please connect to  %d.%d.%d.%d",
      ip[0], ip[1], ip[2], ip[3]
    );
  }
  else
  {
    snprintf(
      msg,
      sizeof(msg),
      "Info mode.....Please connect to  192.168.4.1"
    );
  }

  display.displayScroll(msg, PA_CENTER, PA_SCROLL_LEFT, 30);
  display.displayReset();

  if (!wifiOK)
  {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("LittleClockInfo");
  }

  server.on("/", HTTP_GET, handleInfoRoot);

  server.on("/check",        HTTP_POST, handleInfoCheck);
  server.on("/update_start", HTTP_POST, handleStartUpdate);
  server.on("/install",      HTTP_ANY,  handleInfoInstallRequest);

  server.on("/info/options_save", HTTP_POST, handleInfoOptionsSave);
  server.on("/exit",              HTTP_POST, handleInfoExit);

  server.begin();

  while (true)
  {
    if (otaState != OTA_IDLE) return;

    if (exitInfoRequested)
    {
      exitInfoRequested = false;

      server.stop();
      WiFi.softAPdisconnect(true);

      display.displayClear();
      displayMode = DISP_CLOCK;
      return;
    }

    if (otaRequested)
    {
      otaRequested = false;

      server.stop();
      WiFi.softAPdisconnect(true);

      handleInfoInstall();   // OTA outside HTTP context
      return;                // OTA reboots on success
    }

    if (display.displayAnimate())
    {
      delay(300);
      display.displayReset();
    }

    server.handleClient();
    serviceResetButton(millis());
    yield();
  }
}

// ============================================================
// SECTION 14a — BLOCKING SCROLL TEXT (RESET / SETUP)
// ============================================================
void scrollText(const char *msg)
{
  displayMode = DISP_BLOCKING;

  display.displayClear();
  display.displayScroll((char *)msg, PA_CENTER, PA_SCROLL_LEFT, 40);

  while (!display.displayAnimate())
  {
    yield();
  }

  delay(200);
  display.displayReset();
}

// ============================================================
// SECTION 15 — SETUP
// ============================================================
void setup()
{
  // ----------------------------------------------------------
  // Reset button
  // ----------------------------------------------------------
  pinMode(RESET_PIN, INPUT_PULLUP);
  attachInterrupt(
    digitalPinToInterrupt(RESET_PIN),
    onResetISR,
    FALLING
  );

  // ----------------------------------------------------------
  // Display init
  // ----------------------------------------------------------
  display.begin();
  display.setFont(nullptr);
  display.displayClear();
  display.setIntensity(0);

  scrollText("Welcome  v" FW_VERSION);

  // ----------------------------------------------------------
  // Load clock preferences
  // ----------------------------------------------------------
  prefs.begin("clock", true);

  setting_time_format    = prefs.getInt("time_format", 12);
  setting_blink_colon    = prefs.getBool("blink_colon", true);
  setting_wifi_bar       = prefs.getBool("wifi_bar", true);
  setting_time_mode      = prefs.getInt("time_mode", 0);
  setting_bold_animation = prefs.getInt("bold_anim", 0);

  prefs.end();

  // ----------------------------------------------------------
  // Load font selection
  // ----------------------------------------------------------
  loadFontSetting();

  // ----------------------------------------------------------
  // Load WiFi credentials
  // ----------------------------------------------------------
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  bool wifiConnected = false;

  if (ssid.length())
  {
    scrollText("Connecting Wifi...");
    wifiConnected = connectWiFiWithDots(ssid, pass);
  }

  if (!wifiConnected)
  {
    scrollText("WiFi Failed. Starting Setup Mode...");
    delay(1000);
    startApFailSafe();   // blocking
  }

  scrollText("Wifi Connected");

  // ----------------------------------------------------------
  // Force reliable DNS (bypass Pi-hole or local DNS).
  // Remove these two lines if you want to use your router's DNS.
  // ----------------------------------------------------------
  WiFi.config(
    WiFi.localIP(),
    WiFi.gatewayIP(),
    WiFi.subnetMask(),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1)
  );

  // ----------------------------------------------------------
  // Set the display timezone and attempt the first UTC sync.
  // ----------------------------------------------------------
  setenv("TZ", NZ_TIME_ZONE, 1);
  tzset();
  timeClient.begin();
  attemptNtpSync();
  scrollText(ntpSyncOK ? "Time Synced" : "Time Not Synced");

  lastSecond  = millis();
  displayMode = DISP_CLOCK;
}

// ============================================================
// SECTION 16 — LOOP
// ============================================================
void loop()
{
  unsigned long now = millis();

  // ----------------------------------------------------------
  // Block normal clock updates during portals / OTA
  // ----------------------------------------------------------
  if (displayMode != DISP_CLOCK)
  {
    display.displayAnimate();
    return;
  }

  // ----------------------------------------------------------
  // Always service reset button
  // ----------------------------------------------------------
  serviceResetButton(now);

  // ----------------------------------------------------------
  // LDR brightness control
  //
  // The LDR is wired as a voltage divider: higher rawLdr value
  // = brighter ambient light = higher display brightness.
  // If your display behaves inversely (bright room = dim display),
  // swap the map() range to: map(rawLdr, 0, 4095, 15, 0)
  // ----------------------------------------------------------
  int rawLdr     = analogRead(LDR_PIN);
  int brightness = constrain(map(rawLdr, 0, 4095, 0, 15), 0, 15);
  display.setIntensity(brightness);

  // ----------------------------------------------------------
  // Keep rendering the last known time during a sync outage.
  // ----------------------------------------------------------
  now = millis();
  if (ntpSyncOK && now - lastNtpSuccessMs >= NTP_STALE_MS)
  {
    ntpSyncOK = false;
  }

  const unsigned long pollInterval = ntpSyncOK ? NTP_POLL_MS : NTP_RETRY_MS;
  if (now - lastNtpAttempt >= pollInterval)
  {
    attemptNtpSync();
    now = millis();
  }

  // ----------------------------------------------------------
  // Tick the display animation state machine (non-blocking)
  // ----------------------------------------------------------
  tickDisplayAnimation(now);

  // ----------------------------------------------------------
  // Trigger new animation frame once per second
  // ----------------------------------------------------------
  if (now - lastSecond >= 1000)
  {
    lastSecond   = now;
    colonVisible = !colonVisible;
    drawTime();
  }
}
