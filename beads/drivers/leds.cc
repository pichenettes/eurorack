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

#include "beads/drivers/leds.h"

#include <algorithm>

namespace beads {

using namespace std;

struct LedDefinition {
  GPIO_TypeDef* gpio;
  uint32_t pin;
  TIM_TypeDef* timer;
  uint32_t channel;
};

LedDefinition led_definition[LED_LAST] = {
  // LED_MEDIA_R, LED_MEDIA_G, LED_MEDIA_B
  { MEDIA_LED_R_GPIO_PORT, MEDIA_LED_R_GPIO_PIN, TIM1, TIM_CHANNEL_1 },
  { MEDIA_LED_G_GPIO_PORT, MEDIA_LED_G_GPIO_PIN, TIM1, TIM_CHANNEL_2 },
  { MEDIA_LED_B_GPIO_PORT, MEDIA_LED_B_GPIO_PIN, TIM1, TIM_CHANNEL_3 },
  
  // LED_FREEZE
  { FREEZE_LED_GPIO_PORT, FREEZE_LED_GPIO_PIN, TIM3, TIM_CHANNEL_1 },

  // LED_SEED
  { SEED_LED_GPIO_PORT, SEED_LED_GPIO_PIN, TIM3, TIM_CHANNEL_2 },
  
  // LED_VU_METER
  { INPUT_LED_GPIO_PORT, INPUT_LED_GPIO_PIN, TIM3, TIM_CHANNEL_3 },
  
  // LED_ASSIGN_1, LED_ASSIGN_2, LED_ASSIGN_3
  { ASSIGN_LED_1_GPIO_PORT, ASSIGN_LED_1_GPIO_PIN, TIM4, TIM_CHANNEL_2 },
  { ASSIGN_LED_2_GPIO_PORT, ASSIGN_LED_2_GPIO_PIN, TIM4, TIM_CHANNEL_3 },
  { ASSIGN_LED_3_GPIO_PORT, ASSIGN_LED_3_GPIO_PIN, TIM4, TIM_CHANNEL_4 },
};

bool Leds::Init() {
  bool result = InitializeGPIO() && InitializeTimers();
  Clear();
  return result;
}

bool Leds::InitializeGPIO() {
  for (int i = 0; i < LED_LAST; ++i) {
    const LedDefinition& definition = led_definition[i];
    
    GPIO_InitTypeDef gpio_init;
    gpio_init.Pin = definition.pin;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = definition.timer == TIM1 ? GPIO_AF1_TIM1 : GPIO_AF2_TIM3;
    HAL_GPIO_Init(definition.gpio, &gpio_init);
  }
  return true;
}

bool Leds::InitializeTimers() {
  __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM4_CLK_ENABLE();

  TIM_ClockConfigTypeDef clock_source_config;
  clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  
  TIM_MasterConfigTypeDef master_config;
  master_config.MasterOutputTrigger = TIM_TRGO_RESET;
  master_config.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  
  TIM_OC_InitTypeDef oc_config;
  oc_config.OCMode = TIM_OCMODE_PWM1;
  oc_config.Pulse = 0;
  oc_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc_config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  oc_config.OCFastMode = TIM_OCFAST_DISABLE;
  oc_config.OCIdleState = TIM_OCIDLESTATE_RESET;
  oc_config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  
  TIM_Base_InitTypeDef timer_config;
  timer_config.Prescaler = 0;
  timer_config.CounterMode = TIM_COUNTERMODE_UP;
  timer_config.Period = 255;
  timer_config.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer_config.RepetitionCounter = 0;
  timer_config.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  TIM_HandleTypeDef timer;
  TIM_TypeDef* previous_timer = NULL;

  for (int i = 0; i < LED_LAST; ++i) {
    const LedDefinition& definition = led_definition[i];
    if (definition.timer != previous_timer) {
      previous_timer = timer.Instance = definition.timer;
      
      timer.Init = timer_config;
      
      if (HAL_TIM_Base_Init(&timer) != HAL_OK)
        return false;
      
      if (HAL_TIM_ConfigClockSource(&timer, &clock_source_config) != HAL_OK)
        return false;
      
      if (HAL_TIM_OC_Init(&timer) != HAL_OK)
        return false;
      
      if (HAL_TIMEx_MasterConfigSynchronization(
          &timer, &master_config) != HAL_OK)
        return false;
    }
    
    if (HAL_TIM_OC_ConfigChannel(
        &timer, &oc_config, definition.channel) != HAL_OK)
      return false;

    HAL_TIM_PWM_Start(&timer, definition.channel);
  }

  return true;
}

#define DIM_RGB 0
#define DIM_WHITE 0

void Leds::Write() {
  for (int i = 0; i < LED_LAST; ++i) {
    TIM_HandleTypeDef timer;
    timer.Instance = led_definition[i].timer;
    
    uint8_t pwm_ratio = intensity_[i];
    if (i >= LED_MEDIA_R && i <= LED_MEDIA_B) {
      // The RGB LED needs a current sink instead of a current source.
      pwm_ratio = 255 - (intensity_[i] >> DIM_RGB);
    } else if (i == LED_VU_METER || (i >= LED_ASSIGN_1 && i <= LED_ASSIGN_3)) {
      // The white LEDs have a weird response to PWM.
      pwm_ratio = intensity_[i] ?
          64 + (intensity_[i] >> (1 + DIM_WHITE)) : 0;
    }
    __HAL_TIM_SET_COMPARE(&timer, led_definition[i].channel, pwm_ratio);
  }
}

void Leds::Clear() {
  fill(&intensity_[0], &intensity_[LED_LAST], 0);
}

}  // namespace beads