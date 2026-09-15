// Copyright 2026 Iain (East Coast Modular).
//
// Author: Iain
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// SSD1306 128x32 OLED driver, bit-banged I2C on GPIOA12 (SCL, was
// LEDS_SCK) / GPIOF6 (SDA, was LEDS_SIN) -- the two GPIOs freed by
// desoldering the LED column. Neither pin sits on the I2C1 hardware
// peripheral's AF mux (that's PB6/PB7, which are this board's two
// front-panel buttons), so this bit-bangs instead of using I2C1.

#include "plaits/drivers/ssd1306.h"

#include <string.h>

#include <stm32f37x_conf.h>

namespace plaits {

namespace {

const uint8_t kSsd1306Address = 0x3C;  // 7-bit address.

const uint16_t kSclPin = GPIO_Pin_12;  // GPIOA12, was LEDS_SCK.
const uint16_t kSdaPin = GPIO_Pin_6;   // GPIOF6, was LEDS_SIN.

// Both lines are configured once as open-drain outputs with pull-ups
// (internal, plus the 4.7k externals already on the board): driving
// "high" just releases the line for the pull-up to bring it up, so a
// slave (or a clock-stretching peripheral, not that the SSD1306 does
// that) can still pull it low. No need to flip direction between
// output and input -- reading IDR works the same either way.
inline void SclHigh() { GPIOA->BSRR = kSclPin; }
inline void SclLow() { GPIOA->BRR = kSclPin; }
inline void SdaHigh() { GPIOF->BSRR = kSdaPin; }
inline void SdaLow() { GPIOF->BRR = kSdaPin; }
inline bool SdaRead() { return (GPIOF->IDR & kSdaPin) != 0; }

// Bit-bang half-period delay. Not calibrated to a specific frequency --
// I2C tolerates an arbitrarily slow clock, so on a 72MHz core this loop
// count gives a comfortably slow (sub-100kHz) bus without needing to be
// precise about it.
inline void Delay() {
  for (volatile uint32_t i = 0; i < 200; ++i) { }
}

// A very small 5x7 font, columns are read bottom-to-top / LSB-first, one
// byte per column, 5 columns per glyph -- the classic layout used by most
// small OLED "u8g" -style fonts. Only the characters needed to spell out
// Plaits engine names are defined; anything else falls back to a blank.
struct Glyph {
  char c;
  uint8_t columns[5];
};

const Glyph kFont[] = {
  {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
  {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
  {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
  {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
  {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
  {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
  {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
  {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
  {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
  {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
  {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
  {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
  {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
  {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
  {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
  {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
  {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
  {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
  {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
  {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
  {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
  {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
  {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
  {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
  {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
  {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
  {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
  {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
  {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
  {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
  {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
  {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
  {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
  {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
  {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
  {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
  {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
  {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
  {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
};

const int kFontSize = sizeof(kFont) / sizeof(kFont[0]);

const uint8_t* FindGlyph(char c) {
  for (int i = 0; i < kFontSize; ++i) {
    if (kFont[i].c == c) {
      return kFont[i].columns;
    }
  }
  return kFont[0].columns;  // Blank.
}

}  // namespace

void Ssd1306::I2cStart() {
  SdaHigh();
  SclHigh();
  Delay();
  SdaLow();
  Delay();
  SclLow();
  Delay();
}

void Ssd1306::I2cStop() {
  SdaLow();
  Delay();
  SclHigh();
  Delay();
  SdaHigh();
  Delay();
}

bool Ssd1306::I2cWriteByte(uint8_t byte) {
  for (int i = 7; i >= 0; --i) {
    if (byte & (1 << i)) {
      SdaHigh();
    } else {
      SdaLow();
    }
    Delay();
    SclHigh();
    Delay();
    SclLow();
  }
  // Release SDA and sample it during the 9th clock for the slave's ACK
  // (it pulls SDA low to acknowledge).
  SdaHigh();
  Delay();
  SclHigh();
  Delay();
  bool ack = !SdaRead();
  SclLow();
  Delay();
  return ack;
}

bool Ssd1306::WriteBytes(uint8_t control, const uint8_t* data, size_t size) {
  I2cStart();
  bool ok = I2cWriteByte(kSsd1306Address << 1);
  ok = ok && I2cWriteByte(control);
  for (size_t i = 0; ok && i < size; ++i) {
    ok = I2cWriteByte(data[i]);
  }
  I2cStop();
  return ok;
}

bool Ssd1306::WriteByte(uint8_t control, uint8_t data) {
  return WriteBytes(control, &data, 1);
}

bool Ssd1306::WriteCommand(uint8_t command) {
  return WriteByte(0x00, command);
}

bool Ssd1306::WriteCommandSequence(const uint8_t* data, size_t size) {
  return WriteBytes(0x00, data, size);
}

bool Ssd1306::WriteDataBlock(const uint8_t* data, size_t size) {
  return WriteBytes(0x40, data, size);
}

bool Ssd1306::Init() {
  // GPIOA is already clocked (pots/audio/etc all use it), but GPIOF was
  // only clocked by Leds::Init(), which is no longer called -- enable it
  // ourselves.
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE);

  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_OD;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_UP;  // Internal pull-up in addition to
                                       // the 4.7k externals on the board.

  gpio_init.GPIO_Pin = kSclPin;
  GPIO_Init(GPIOA, &gpio_init);
  gpio_init.GPIO_Pin = kSdaPin;
  GPIO_Init(GPIOF, &gpio_init);

  SclHigh();
  SdaHigh();

  Clear();

  // Standard SSD1306 init sequence for a 128x32 panel.
  const uint8_t init_sequence[] = {
    0xAE,        // Display off.
    0xD5, 0x80,  // Clock divide / osc freq.
    0xA8, 0x1F,  // Multiplex ratio: 32 - 1.
    0xD3, 0x00,  // Display offset: 0.
    0x40,        // Display start line: 0.
    0x8D, 0x14,  // Charge pump: enable.
    0x20, 0x00,  // Memory addressing mode: horizontal.
    0xA1,        // Segment re-map: column 127 -> SEG0.
    0xC8,        // COM output scan direction: remapped.
    0xDA, 0x02,  // COM pins config for 128x32.
    0x81, 0x8F,  // Contrast.
    0xD9, 0xF1,  // Pre-charge period.
    0xDB, 0x40,  // VCOMH deselect level.
    0xA4,        // Resume to RAM content display.
    0xA6,        // Normal (not inverted) display.
    0xAF,        // Display on.
  };
  ready_ = WriteCommandSequence(init_sequence, sizeof(init_sequence));
  return ready_;
}

void Ssd1306::Clear() {
  memset(frame_buffer_, 0, sizeof(frame_buffer_));
}

void Ssd1306::Print(int x, int page, const char* text) {
  if (page < 0 || page >= kSsd1306PageCount) {
    return;
  }
  uint8_t* row = &frame_buffer_[page * kSsd1306Width];
  int column = x;
  while (*text && column <= kSsd1306Width - 5) {
    const uint8_t* glyph = FindGlyph(*text);
    memcpy(&row[column], glyph, 5);
    column += 6;  // 5px glyph + 1px spacing.
    ++text;
  }
}

void Ssd1306::Flush() {
  if (!ready_) {
    return;
  }
  const uint8_t addressing[] = {
    0x21, 0x00, kSsd1306Width - 1,       // Column address range.
    0x22, 0x00, kSsd1306PageCount - 1,   // Page address range.
  };
  if (!WriteCommandSequence(addressing, sizeof(addressing))) {
    return;
  }
  // Send in page-sized chunks -- comfortably under any I2C driver's
  // internal buffer limits, and each page is a natural boundary.
  for (int page = 0; page < kSsd1306PageCount; ++page) {
    WriteDataBlock(&frame_buffer_[page * kSsd1306Width], kSsd1306Width);
  }
}

}  // namespace plaits
