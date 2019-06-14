// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Driver for the 3 front panel switches.

#ifndef TIDES_DRIVERS_SWITCHES_H_
#define TIDES_DRIVERS_SWITCHES_H_

#include "stmlib/stmlib.h"

#include <stm32f37x_conf.h>

namespace tides {

enum Switch {
  SWITCH_RANGE,
  SWITCH_MODE,
  SWITCH_SHIFT,
  SWITCH_LAST
};
  
class Switches {
 public:
  Switches() { }
  ~Switches() { }
  
  void Init();
  void Debounce();
  
  inline bool released(Switch s) const {
    return switch_state_[s] == 0x7f;
  }
  
  inline bool just_pressed(Switch s) const {
    return switch_state_[s] == 0x80;
  }

  inline bool pressed(Switch s) const {
    return switch_state_[s] == 0x00;
  }
  
  inline bool pressed_immediate(Switch s) const {
    if (s == SWITCH_RANGE) {
      return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2);
    } else if (s == SWITCH_SHIFT) {
      return !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
    } else {
      return false;
    }
  }
  
 private:
  uint8_t switch_state_[SWITCH_LAST];
  
  DISALLOW_COPY_AND_ASSIGN(Switches);
};

}  // namespace tides

#endif  // TIDES_DRIVERS_SWITCHES_H_
