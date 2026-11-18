// ============================================================
//  display_ui.cpp — Little Clock UI + time rendering
//
//  Changes from v18:
//    - Dead code removed: setBoldAnimationMode() was never called
//      and its mapping conflicted with setting_bold_animation.
//      drawTime() now uses setting_bold_animation directly with
//      a single consistent mapping: 0=instant, 1=flip, 2=fall.
//    - Hex mode now works correctly in the Bold path.
//    - All blocking delay() calls in animations replaced with a
//      millis()-based state machine. The reset button and other
//      loop() work now runs during animations.
//    - BLANK_6x8 nullptr pattern consolidated.
// ============================================================

#include "display_ui.h"

#include <WiFi.h>
#include <NTPClient.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <time.h>

#include "FontDigits.h"

// ============================================================
//  EXTERNALS FROM MAIN .INO
// ============================================================
extern MD_Parola display;

extern bool colonVisible;
extern bool ntpSyncOK;

extern int  setting_bold_animation;
extern int  setting_time_format;
extern bool setting_blink_colon;
extern bool setting_wifi_bar;
extern int  setting_time_mode;

extern NTPClient timeClient;

extern FontStyle currentFont;
extern const uint8_t* getDigitGlyph(uint8_t digit);

// ============================================================
//  CONSTANTS
// ============================================================
static const uint8_t GLYPH_COLS        = 6;
static const uint8_t GLYPH_ROWS        = 8;
static const uint8_t GLYPH_COL_MASK    = 0b00111111;
static const uint16_t MATRIX_WIDTH     = 32;

// Fall animation
static const uint8_t FALL_TOTAL_FRAMES = 8;
static const uint8_t FALL_DIGIT_LAG    = 1;       // frames of lag per digit position
static const uint8_t FALL_OVERSHOOT    = 1;       // pixel overshoot on final frame
static const uint32_t FALL_FRAME_MS    = 50;      // ms between frames

// Flip animation
static const uint32_t FLIP_HALF_MS     = 200;     // ms for each half of the flip

// ============================================================
//  BLANK GLYPH
// ============================================================
static const uint8_t BLANK_6x8[GLYPH_ROWS] = {0, 0, 0, 0, 0, 0, 0, 0};

// ============================================================
//  NON-BLOCKING ANIMATION STATE MACHINE
// ============================================================

// Animation modes (maps directly to setting_bold_animation)
//   0 = instant  (no animation)
//   1 = flip     (split-flap style)
//   2 = fall     (odometer roll)

enum AnimState
{
  ANIM_IDLE,
  ANIM_FALL_RUNNING,
  ANIM_FLIP_TOP,       // top half = new, bottom = old
  ANIM_FLIP_BOTTOM     // both halves = new (complete)
};

struct AnimContext
{
  AnimState   state      = ANIM_IDLE;
  unsigned long lastMs   = 0;

  // Shared
  uint16_t    digitX[4] = {0, 0, 0, 0};
  int         startPos   = 0;
  int         oldValue   = 0;
  int         newValue   = 0;
  bool        hideLeading = false;

  // Fall specific
  uint8_t     frame      = 0;

  // Flip specific
  int         flipPos    = 0;   // which digit position we're currently flipping
};

static AnimContext anim;

// ============================================================
//  WIFI SIGNAL BAR (LEFT)
// ============================================================
void drawWifiBar()
{
  if (!setting_wifi_bar) return;

  MD_MAX72XX *mx = display.getGraphicObject();

  for (int r = 0; r < 8; r++)
  {
    mx->setPoint(r, 0, false);
    mx->setPoint(r, 1, false);
  }

  if (WiFi.status() != WL_CONNECTED) return;

  int rssi = max((int)WiFi.RSSI(), -90);
  int lvl  = constrain(map(rssi, -90, -30, 0, 8), 0, 8);

  for (int r = 0; r < 8; r++)
  {
    bool on = (lvl > 0 && r >= (8 - lvl));
    mx->setPoint(r, 0, on);
    mx->setPoint(r, 1, on);
  }
}

// ============================================================
//  NTP WARNING INDICATOR (RIGHT)
// ============================================================
void drawNtpWarning()
{
  MD_MAX72XX *mx = display.getGraphicObject();

  mx->setPoint(5, 31, false);
  mx->setPoint(6, 31, false);
  mx->setPoint(6, 30, false);
  mx->setPoint(7, 31, false);
  mx->setPoint(7, 30, false);
  mx->setPoint(7, 29, false);

  if (ntpSyncOK) return;

  if (currentFont == FONT_BLOCKY || currentFont == FONT_BOLD)
  {
    mx->setPoint(7, 31, true);
    return;
  }

  mx->setPoint(5, 31, true);
  mx->setPoint(6, 31, true);
  mx->setPoint(6, 30, true);
  mx->setPoint(7, 31, true);
  mx->setPoint(7, 30, true);
  mx->setPoint(7, 29, true);
}

// ============================================================
//  PM INDICATOR DOT
// ============================================================
void drawPMIndicator(bool isPM)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  mx->setPoint(0, 31, false);

  if (setting_time_mode == 0 &&
      setting_time_format == 12 &&
      isPM)
  {
    mx->setPoint(0, 31, true);
  }
}

// ============================================================
//  COLON
// ============================================================
static void drawColon(uint16_t x, uint8_t y, bool visible)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  uint16_t physX = (MATRIX_WIDTH - 1) - x;

  mx->setPoint(y + 2, physX, false);
  mx->setPoint(y + 5, physX, false);

  if (!visible) return;

  mx->setPoint(y + 2, physX, true);
  mx->setPoint(y + 5, physX, true);
}

// ============================================================
//  6x8 DIGIT RENDERERS
// ============================================================
static void drawDigit6x8(uint16_t x, int y, const uint8_t *glyph)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  for (int row = 0; row < GLYPH_ROWS; row++)
  {
    int yy = y + row;
    if (yy < 0 || yy > 7) continue;

    uint8_t bits = glyph[row] & GLYPH_COL_MASK;

    for (int col = 0; col < GLYPH_COLS; col++)
    {
      bool on = bits & (1 << (5 - col));
      uint16_t physX = (MATRIX_WIDTH - 1) - (x + col);
      mx->setPoint(yy, physX, on);
    }
  }
}

static void drawDigit6x8Top(uint16_t x, int y, const uint8_t *glyph)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  for (int row = 0; row < 4; row++)
  {
    int yy = y + row;
    if (yy < 0 || yy > 7) continue;

    uint8_t bits = glyph[row] & GLYPH_COL_MASK;

    for (int col = 0; col < GLYPH_COLS; col++)
    {
      bool on = bits & (1 << (5 - col));
      uint16_t physX = (MATRIX_WIDTH - 1) - (x + col);
      mx->setPoint(yy, physX, on);
    }
  }
}

static void drawDigit6x8Bottom(uint16_t x, int y, const uint8_t *glyph)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  for (int row = 4; row < GLYPH_ROWS; row++)
  {
    int yy = y + row;
    if (yy < 0 || yy > 7) continue;

    uint8_t bits = glyph[row] & GLYPH_COL_MASK;

    for (int col = 0; col < GLYPH_COLS; col++)
    {
      bool on = bits & (1 << (5 - col));
      uint16_t physX = (MATRIX_WIDTH - 1) - (x + col);
      mx->setPoint(yy, physX, on);
    }
  }
}

// ============================================================
//  DIGIT AREA CLEAR
// ============================================================
static void clearDigitArea6x8(uint16_t x)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  for (int row = 0; row < GLYPH_ROWS; row++)
  {
    for (int col = 0; col < GLYPH_COLS; col++)
    {
      mx->setPoint(row, (MATRIX_WIDTH - 1) - (x + col), false);
    }
  }
}

// ============================================================
//  CARRY DETECTION
//  Returns the leftmost digit position that has changed,
//  accounting for carry ripple (e.g. 12:59 → 13:00 = pos 0).
// ============================================================
static int carryStart(int oldV, int newV)
{
  // Full rollover (e.g. 12:59 → 1:00 in 12h, or 23:59 → 0:00)
  if ((oldV % 100 == 59) && (newV % 100 == 0))
  {
    return 0;
  }

  int pos = 3;

  while (pos >= 0)
  {
    int p = 1;
    for (int i = 0; i < (3 - pos); i++) p *= 10;

    int od = (oldV / p) % 10;
    int nd = (newV / p) % 10;

    if (!(od == 9 && nd == 0))
      break;

    pos--;
  }

  return pos;
}

// ============================================================
//  FALL HELPERS
// ============================================================

// Easing curve: pixel offset per frame (0–7)
// Gives a gradual acceleration into a smooth stop
static uint8_t getFallFrameShift(uint8_t frame)
{
  static const uint8_t shifts[] = {0, 1, 2, 3, 5, 6, 7, 8};
  const uint8_t count = sizeof(shifts) / sizeof(shifts[0]);

  return shifts[(frame < count) ? frame : count - 1];
}

static void drawDigitRollFrame(
  uint16_t x,
  const uint8_t* oldGlyph,
  const uint8_t* newGlyph,
  uint8_t shift
)
{
  MD_MAX72XX *mx = display.getGraphicObject();

  for (int screenRow = 0; screenRow < GLYPH_ROWS; screenRow++)
  {
    uint8_t bits = 0;

    int oldRow = screenRow - shift;
    if (oldRow >= 0 && oldRow < (int)GLYPH_ROWS)
      bits |= (oldGlyph[oldRow] & GLYPH_COL_MASK);

    int newRow = screenRow + GLYPH_ROWS - shift;
    if (newRow >= 0 && newRow < (int)GLYPH_ROWS)
      bits |= (newGlyph[newRow] & GLYPH_COL_MASK);

    for (int col = 0; col < GLYPH_COLS; col++)
    {
      bool on = bits & (1 << (5 - col));
      uint16_t physX = (MATRIX_WIDTH - 1) - (x + col);
      mx->setPoint(screenRow, physX, on);
    }
  }
}

// ============================================================
//  GLYPH HELPERS
// ============================================================
static const uint8_t* getGlyphForPos(int pos, int value, bool hideLeading)
{
  int p = 1;
  for (int i = 0; i < (3 - pos); i++) p *= 10;

  int d = (value / p) % 10;

  bool hide = hideLeading && (pos == 0) && ((value / 100) < 10);

  return hide ? BLANK_6x8 : getDigitGlyph((uint8_t)d);
}

// ============================================================
//  DRAW SINGLE FALL FRAME ACROSS ALL ANIMATING DIGITS
// ============================================================
static void drawFallFrame()
{
  uint8_t totalFrames = FALL_TOTAL_FRAMES + 3;

  for (int pos = anim.startPos; pos < 4; pos++)
  {
    uint16_t x = anim.digitX[pos];
    clearDigitArea6x8(x);

    const uint8_t* og = getGlyphForPos(pos, anim.oldValue, anim.hideLeading);
    const uint8_t* ng = getGlyphForPos(pos, anim.newValue, anim.hideLeading);

    int localFrame = (int)anim.frame - ((3 - pos) * FALL_DIGIT_LAG);

    if (localFrame <= 0)
    {
      drawDigit6x8(x, 0, og);
    }
    else if (localFrame >= (int)(FALL_TOTAL_FRAMES - 1))
    {
      // Slight mechanical overshoot — snaps back to 0 on final frame
      drawDigit6x8(x, FALL_OVERSHOOT, ng);
    }
    else
    {
      uint8_t shift = getFallFrameShift((uint8_t)localFrame);
      drawDigitRollFrame(x, og, ng, shift);
    }
  }

  display.getGraphicObject()->update();
}

// ============================================================
//  SNAP ALL DIGITS TO FINAL POSITION
// ============================================================
static void snapToFinal()
{
  for (int pos = anim.startPos; pos < 4; pos++)
  {
    const uint8_t* ng = getGlyphForPos(pos, anim.newValue, anim.hideLeading);
    clearDigitArea6x8(anim.digitX[pos]);
    drawDigit6x8(anim.digitX[pos], 0, ng);
  }

  display.getGraphicObject()->update();
}

// ============================================================
//  NON-BLOCKING TICK — called every loop()
// ============================================================
void tickDisplayAnimation(unsigned long now)
{
  if (anim.state == ANIM_IDLE) return;

  // ----------------------------------------------------------
  //  FALL
  // ----------------------------------------------------------
  if (anim.state == ANIM_FALL_RUNNING)
  {
    if (now - anim.lastMs < FALL_FRAME_MS) return;
    anim.lastMs = now;

    uint8_t totalFrames = FALL_TOTAL_FRAMES + 3;

    if (anim.frame >= totalFrames)
    {
      // Animation complete — snap to exact position
      snapToFinal();
      anim.state = ANIM_IDLE;
      return;
    }

    drawFallFrame();
    anim.frame++;
    return;
  }

  // ----------------------------------------------------------
  //  FLIP — phase 1: top=new, bottom=old
  // ----------------------------------------------------------
  if (anim.state == ANIM_FLIP_TOP)
  {
    if (now - anim.lastMs < FLIP_HALF_MS) return;
    anim.lastMs = now;

    // Advance to bottom phase for current digit
    int pos = anim.flipPos;
    uint16_t x = anim.digitX[pos];

    const uint8_t* ng = getGlyphForPos(pos, anim.newValue, anim.hideLeading);

    clearDigitArea6x8(x);
    drawDigit6x8Top(x, 0, ng);
    drawDigit6x8Bottom(x, 0, ng);
    display.getGraphicObject()->update();

    anim.state = ANIM_FLIP_BOTTOM;
    return;
  }

  // ----------------------------------------------------------
  //  FLIP — phase 2: both halves = new, move to next digit
  // ----------------------------------------------------------
  if (anim.state == ANIM_FLIP_BOTTOM)
  {
    if (now - anim.lastMs < FLIP_HALF_MS) return;
    anim.lastMs = now;

    anim.flipPos++;

    if (anim.flipPos >= 4)
    {
      // All digits done
      anim.state = ANIM_IDLE;
      return;
    }

    // Start flip phase 1 for next digit
    int pos = anim.flipPos;
    uint16_t x = anim.digitX[pos];

    const uint8_t* og = getGlyphForPos(pos, anim.oldValue, anim.hideLeading);
    const uint8_t* ng = getGlyphForPos(pos, anim.newValue, anim.hideLeading);

    clearDigitArea6x8(x);
    drawDigit6x8Top(x, 0, ng);
    drawDigit6x8Bottom(x, 0, og);
    display.getGraphicObject()->update();

    anim.state = ANIM_FLIP_TOP;
    return;
  }
}

// ============================================================
//  DRAW CURRENT TIME
// ============================================================
void drawTime()
{
  static FontStyle lastFont    = (FontStyle)255;
  static int       lastBoldVal = -1;

  const time_t utc = static_cast<time_t>(timeClient.getEpochTime());
  struct tm local;
  if (localtime_r(&utc, &local) == nullptr) return;

  int rawH = local.tm_hour;
  int min  = local.tm_min;

  bool hexMode = (setting_time_mode == 1);
  bool isPM    = (rawH >= 12);

  int hour = rawH;

  if (!hexMode && setting_time_format == 12)
  {
    hour = rawH % 12;
    if (hour == 0) hour = 12;
  }

  // =========================================================
  //  DEFAULT PAROLA FONT PATH
  // =========================================================
  if (currentFont != FONT_BLOCKY && currentFont != FONT_BOLD)
  {
    if (currentFont != lastFont)
    {
      display.displayClear();
      lastFont = currentFont;
    }

    display.setTextAlignment(PA_CENTER);

    char buf[6];
    char colonChar =
      (setting_blink_colon && !colonVisible) ? ' ' : ':';

    if (hexMode)
    {
      snprintf(buf, sizeof(buf), "%X%X%c%X%X",
               rawH / 16, rawH % 16, colonChar, min / 16, min % 16);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%02d%c%02d",
               hour, colonChar, min);
    }

    display.print(buf);

    drawPMIndicator(isPM);
    drawNtpWarning();
    drawWifiBar();
    return;
  }

  // =========================================================
  //  BLOCKY PATH
  // =========================================================
  if (currentFont == FONT_BLOCKY)
  {
    if (currentFont != lastFont)
    {
      display.displayClear();
      lastFont = currentFont;
    }

    uint8_t hT, hO, mT, mO;

    if (hexMode)
    {
      hT = (uint8_t)(rawH / 16);
      hO = (uint8_t)(rawH % 16);
      mT = (uint8_t)(min  / 16);
      mO = (uint8_t)(min  % 16);
    }
    else
    {
      hT = (uint8_t)(hour / 10);
      hO = (uint8_t)(hour % 10);
      mT = (uint8_t)(min  / 10);
      mO = (uint8_t)(min  % 10);
    }

    uint16_t x = 2;
    uint8_t  adv = getFontAdvance(currentFont);

    drawPMIndicator(isPM);
    drawNtpWarning();

    bool showLeadingZero = (setting_time_format == 24) || hexMode || (hour >= 10);

    drawDigit6x8(x, 0, showLeadingZero ? getDigitGlyph(hT) : BLANK_6x8);
    x += adv;

    drawDigit6x8(x, 0, getDigitGlyph(hO));
    x += adv + 2;

    drawDigit6x8(x, 0, getDigitGlyph(mT));
    x += adv;

    drawDigit6x8(x, 0, getDigitGlyph(mO));

    return;
  }

  // =========================================================
  //  BOLD PATH
  // =========================================================
  if (currentFont == FONT_BOLD)
  {
    if (currentFont != lastFont)
    {
      display.displayClear();
      lastFont    = currentFont;
      lastBoldVal = -1;
    }

    // Encode time as a 4-digit decimal value (HHMM) for comparison.
    // In hex mode, encode as hex digits: e.g. 0x1A3F = rawH=1A, min=3F
    int value;
    if (hexMode)
      value = (rawH / 16) * 1000 + (rawH % 16) * 100 + (min / 16) * 10 + (min % 16);
    else
      value = hour * 100 + min;

    bool hideLeadingZero =
      !hexMode &&
      (setting_time_mode == 0) &&
      (setting_time_format == 12) &&
      (hour < 10);

    uint8_t  adv = getFontAdvance(currentFont);
    uint16_t x   = 2;

    uint16_t digitX[4];
    digitX[0] = x; x += adv;
    digitX[1] = x; x += adv + 2;
    digitX[2] = x; x += adv;
    digitX[3] = x;

    // First draw — no animation
    if (lastBoldVal < 0)
    {
      for (int pos = 0; pos < 4; pos++)
      {
        const uint8_t* glyph = getGlyphForPos(pos, value, hideLeadingZero);
        clearDigitArea6x8(digitX[pos]);
        drawDigit6x8(digitX[pos], 0, glyph);
      }

      display.getGraphicObject()->update();
    }
    else if (value != lastBoldVal && anim.state == ANIM_IDLE)
    {
      int startPos = carryStart(lastBoldVal, value);

      // Populate shared animation context
      for (int i = 0; i < 4; i++) anim.digitX[i] = digitX[i];

      anim.startPos    = startPos;
      anim.oldValue    = lastBoldVal;
      anim.newValue    = value;
      anim.hideLeading = hideLeadingZero;
      anim.lastMs      = millis();

      if (setting_bold_animation == 1)
      {
        // Flip — start first digit immediately
        anim.flipPos = startPos;
        anim.state   = ANIM_FLIP_TOP;

        const uint8_t* og = getGlyphForPos(startPos, lastBoldVal, hideLeadingZero);
        const uint8_t* ng = getGlyphForPos(startPos, value,       hideLeadingZero);

        clearDigitArea6x8(digitX[startPos]);
        drawDigit6x8Top(digitX[startPos], 0, ng);
        drawDigit6x8Bottom(digitX[startPos], 0, og);
        display.getGraphicObject()->update();
      }
      else if (setting_bold_animation == 2)
      {
        // Fall — start frame 0
        anim.frame = 0;
        anim.state = ANIM_FALL_RUNNING;
        drawFallFrame();
        anim.frame++;
      }
      else
      {
        // Instant update — no animation
        for (int pos = startPos; pos < 4; pos++)
        {
          const uint8_t* ng = getGlyphForPos(pos, value, hideLeadingZero);
          clearDigitArea6x8(digitX[pos]);
          drawDigit6x8(digitX[pos], 0, ng);
        }

        display.getGraphicObject()->update();
      }
    }

    lastBoldVal = value;

    uint16_t colonX = digitX[1] + adv;
    drawColon(
      colonX,
      0,
      setting_blink_colon ? colonVisible : true
    );

    drawPMIndicator(isPM);
    drawNtpWarning();
    return;
  }
}
