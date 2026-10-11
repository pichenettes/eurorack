// Copyright 2018 Fred's Lab and Mutable Instruments.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Author: Frédéric Meslin (fred@fredslab.net)
// Author: Oliver Rockstedt (info@sourcebox.de)
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
// Driver for the LEDs

#ifndef BEADS_DRIVERS_LEDS_H_
#define BEADS_DRIVERS_LEDS_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

namespace beads {

const uint8_t kMaxLedIntensity = 255;

enum Led {
  LED_MEDIA_R,
  LED_MEDIA_G,
  LED_MEDIA_B,
  LED_FREEZE,
  LED_SEED,
  LED_VU_METER,
  LED_ASSIGN_1,
  LED_ASSIGN_2,
  LED_ASSIGN_3,
  LED_LAST
};

class Leds {
 public:
  Leds() { }
  ~Leds() { }

  struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  bool Init();

  void Write();
  void Clear();

  inline void set(Led led, uint8_t intensity) {
    intensity_[led] = intensity;
  }
  
  inline void set_media(Color color) {
    intensity_[LED_MEDIA_R] = color.r;
    intensity_[LED_MEDIA_G] = color.g;
    intensity_[LED_MEDIA_B] = color.b;
  }
  
  inline void set_meter(float value) {
    Color c;
    if (value < 0.25f) {
      c.r = 0;
      c.g = uint8_t(value * 1024.0f);
      c.b = 0;
    } else if (value < 0.5f) {
      c.r = uint8_t((value - 0.25f) * 1024.0f);
      c.g = 255;
      c.b = 0;
    } else {
      c.r = 255;
      c.g = 255 - uint8_t((value - 0.5f) * 512.0f);
      c.b = 0;
    }
    set_media(c);
  }

  inline void set_media(Color color, float a) {
    if (a != 1.0f) {
      color.r = uint8_t(float(color.r) * a * a);
      color.g = uint8_t(float(color.g) * a);
      color.b = uint8_t(float(color.b) * a);
    }
    set_media(color);
  }
  
 private:
  bool InitializeGPIO();
  bool InitializeTimers();
  
  uint8_t intensity_[LED_LAST];

  DISALLOW_COPY_AND_ASSIGN(Leds);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_LEDS_H_