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
// Driver for the normalization probe.

#ifndef BEADS_DRIVERS_NORMALIZATION_PROBE_H_
#define BEADS_DRIVERS_NORMALIZATION_PROBE_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

namespace beads {

class NormalizationProbe {
 public:
  NormalizationProbe() { }
  ~NormalizationProbe() { }

  inline void Init() const {
    GPIO_InitTypeDef gpio_init;
    gpio_init.Pin = NORMALIZATION_PROBE_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(NORMALIZATION_PROBE_GPIO_PORT, &gpio_init);
  }
  
  inline void Set(bool value) const {
    if (value) {
      High();
    } else {
      Low();
    }
  }

  inline void High() const {
    NORMALIZATION_PROBE_GPIO_PORT->BSRR = NORMALIZATION_PROBE_GPIO_PIN;
  }

  inline void Low() const {
    NORMALIZATION_PROBE_GPIO_PORT->BSRR = NORMALIZATION_PROBE_GPIO_PIN << 16U;
  }
  
 private:
  DISALLOW_COPY_AND_ASSIGN(NormalizationProbe);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_NORMALIZATION_PROBE_H_