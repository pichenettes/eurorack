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
// Drivers for the UI LEDs.

#include "tides2/drivers/leds.h"

#include <algorithm>

#include <stm32f37x_conf.h>

namespace tides {

using namespace std;

struct LedDefinition {
  GPIO_TypeDef* gpio;
  uint16_t pin[3];  // pins for R, G, B - assumed to be on the same GPIO
};

const LedDefinition led_definition[] = {
  { GPIOB, { GPIO_Pin_5, GPIO_Pin_6, 0 } },  // LED_RANGE
  { GPIOF, { GPIO_Pin_7, GPIO_Pin_6, 0 } },  // LED_MODE
  { GPIOB, { GPIO_Pin_3, GPIO_Pin_4, 0 } },  // LED_SHIFT
};

void Leds::Init() {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE);
  
  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  
  for (int i = 0; i < LED_LAST; ++i) {
    const LedDefinition& d = led_definition[i];
    gpio_init.GPIO_Pin = d.pin[0] | d.pin[1] | d.pin[2];
    GPIO_Init(d.gpio, &gpio_init);
  }
  Clear();
}

void Leds::Clear() {
  fill(&colors_[0], &colors_[LED_LAST], LED_COLOR_OFF);
}

void Leds::Write() {
  for (int i = 0; i < LED_LAST; ++i) {
    const LedDefinition& d = led_definition[i];
    if (colors_[i] & 0x800000) {
      d.gpio->BSRR = d.pin[0];
    } else {
      d.gpio->BRR = d.pin[0];
    }
    if (colors_[i] & 0x008000) {
      d.gpio->BSRR = d.pin[1];
    } else {
      d.gpio->BRR = d.pin[1];
    }
  }
}

}  // namespace tides
