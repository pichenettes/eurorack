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

#include "beads/drivers/switches.h"

#include <algorithm>

namespace beads {

using namespace std;

struct SwitchDefinition {
  GPIO_TypeDef* gpio;
  uint16_t pin;
};

const SwitchDefinition switch_definitions[] = {
  { FREEZE_SW_GPIO_PORT, FREEZE_SW_GPIO_PIN },
  { SEED_SW_GPIO_PORT, SEED_SW_GPIO_PIN },
  { MEDIA_SW_GPIO_PORT, MEDIA_SW_GPIO_PIN },
  { ASSIGN_SW_GPIO_PORT, ASSIGN_SW_GPIO_PIN },
};

bool Switches::Init() {
  GPIO_InitTypeDef gpio_init;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_PULLUP;
  
  for (int i = 0; i < SWITCH_LAST; ++i) {
    const SwitchDefinition& definition = switch_definitions[i];
    gpio_init.Pin = definition.pin;
    HAL_GPIO_Init(definition.gpio, &gpio_init);
  }
  fill(&switch_state_[0], &switch_state_[SWITCH_LAST], 0xff);

  return true;
}

void Switches::Debounce() {
  for (int i = 0; i < SWITCH_LAST; ++i) {
    const SwitchDefinition& definition = switch_definitions[i];
    switch_state_[i] = (switch_state_[i] << 1) | \
        HAL_GPIO_ReadPin(definition.gpio, definition.pin);
  }
}

}  // namespace beads