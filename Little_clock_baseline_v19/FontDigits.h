#ifndef FONT_DIGITS_H
#define FONT_DIGITS_H

#include <Arduino.h>
const uint8_t* getDigitGlyph(uint8_t d);

// ----------------------------------------------------------
// Font selector enum
// ----------------------------------------------------------
enum FontStyle
{
  FONT_BLOCKY = 0,
  FONT_BOLD
};


// ----------------------------------------------------------
// Font glyph tables (6×8, digits 0–9 + A–F)
// ----------------------------------------------------------
extern FontStyle currentFont;
extern const uint8_t blockyDigits[16][8];
extern const uint8_t boldDigits[16][8];

// ----------------------------------------------------------
// Per-font horizontal advance (proportional spacing)
// ----------------------------------------------------------
uint8_t getFontAdvance(FontStyle font);

#endif
