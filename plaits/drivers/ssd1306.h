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
// Minimal driver for a 128x32 SSD1306 OLED over bit-banged I2C on the two
// GPIOs freed by desoldering the LED column: GPIOA12 (was LEDS_SCK) as
// SCL, GPIOF6 (was LEDS_SIN) as SDA. Bit-banged rather than the hardware
// I2C1 peripheral because neither pin sits on I2C1's AF mux (PB6/PB7,
// which are the two front-panel buttons on this board).
// Blocking / polled, since it is only redrawn on preset change. Only draws
// a single-line 5x7 bitmap-font string, which is all this UI mod needs.

#ifndef PLAITS_DRIVERS_SSD1306_H_
#define PLAITS_DRIVERS_SSD1306_H_

#include "stmlib/stmlib.h"

namespace plaits {

const int kSsd1306Width = 128;
const int kSsd1306Height = 32;
const int kSsd1306PageCount = kSsd1306Height / 8;  // 4 pages of 8 rows.

class Ssd1306 {
 public:
  Ssd1306() { }
  ~Ssd1306() { }

  // Sets up GPIO + I2C1 peripheral and the SSD1306 itself. Returns false
  // if the display never acknowledges its I2C address (e.g. not fitted).
  bool Init();

  // Clears the local frame buffer (does not push to the display).
  void Clear();

  // Draws a null-terminated string (only 'A'-'Z', '0'-'9', ' ', '-', ':'
  // are defined in the font) at pixel column x / page row (0-3), using
  // the built-in 5x7 font at 1x scale (6px pitch).
  void Print(int x, int page, const char* text);

  // Pushes the local frame buffer to the panel over I2C.
  void Flush();

  inline bool ready() const { return ready_; }

 private:
  bool WriteCommand(uint8_t command);
  bool WriteCommandSequence(const uint8_t* data, size_t size);
  bool WriteDataBlock(const uint8_t* data, size_t size);
  bool WriteByte(uint8_t control, uint8_t data);
  bool WriteBytes(uint8_t control, const uint8_t* data, size_t size);

  // Bit-bang primitives.
  void I2cStart();
  void I2cStop();
  bool I2cWriteByte(uint8_t byte);

  uint8_t frame_buffer_[kSsd1306Width * kSsd1306PageCount];
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(Ssd1306);
};

}  // namespace plaits

#endif  // PLAITS_DRIVERS_SSD1306_H_
