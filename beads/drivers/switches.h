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
// Driver for the front panel switches.

#ifndef BEADS_DRIVERS_SWITCHES_H_
#define BEADS_DRIVERS_SWITCHES_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

namespace beads {

enum Switch {
  SWITCH_FREEZE,
  SWITCH_SEED,
  SWITCH_MEDIA,
  SWITCH_ASSIGN,
  SWITCH_LAST
};

class Switches {
 public:
  Switches() { }
  ~Switches() { }
  
  bool Init();
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
    switch (s) {
      case SWITCH_FREEZE:
        return !HAL_GPIO_ReadPin(FREEZE_SW_GPIO_PORT, FREEZE_SW_GPIO_PIN);
      case SWITCH_SEED:
        return !HAL_GPIO_ReadPin(SEED_SW_GPIO_PORT, SEED_SW_GPIO_PIN);
      case SWITCH_MEDIA:
        return !HAL_GPIO_ReadPin(MEDIA_SW_GPIO_PORT, MEDIA_SW_GPIO_PIN);
      default:
        return !HAL_GPIO_ReadPin(ASSIGN_SW_GPIO_PORT, ASSIGN_SW_GPIO_PIN);
    }
  }
  
 private:
  uint8_t switch_state_[SWITCH_LAST];
  
  DISALLOW_COPY_AND_ASSIGN(Switches);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_SWITCHES_H_