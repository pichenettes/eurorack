// Copyright 2019 Emilie Gillet.
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
// Driver for enabling the watchdog.

#ifndef BEADS_DRIVERS_WATCHDOG_H_
#define BEADS_DRIVERS_WATCHDOG_H_

#include "stmlib/stmlib.h"

#include <stm32h7xx_hal.h>

namespace beads {

class Watchdog {
 public:
  Watchdog() { }
  ~Watchdog() { }
  
  static void Start() {
    IWDG_HandleTypeDef iwdg;
    iwdg.Instance = IWDG1;
    // Auto-reset after a 2s crash.
    iwdg.Init.Prescaler = IWDG_PRESCALER_16;
    iwdg.Init.Reload = 0xfff;
    iwdg.Init.Window = 0;
    HAL_IWDG_Init(&iwdg);
  }
  
  static void Reload() {
    IWDG_HandleTypeDef iwdg;
    iwdg.Instance = IWDG1;
    HAL_IWDG_Refresh(&iwdg);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Watchdog);
};

} // namespace beads

#endif  // BEADS_DRIVERS_WATCHDOG_H_