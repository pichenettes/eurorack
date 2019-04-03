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
// Lightweight DAC driver used for the firmware update procedure.
// Initializes the I2S port as SPI, and relies on a timer for clock generation.

#ifndef TIDES_DRIVERS_FIRMWARE_UPDATE_DAC_H_
#define TIDES_DRIVERS_FIRMWARE_UPDATE_DAC_H_

#include "stmlib/stmlib.h"

#include <stm32f37x_conf.h>

namespace tides {

class FirmwareUpdateDac {
 public:
  FirmwareUpdateDac() { }
  ~FirmwareUpdateDac() { }
  
  typedef uint16_t (*NextSampleFn)();
  
  void Init(int sample_rate);
  
  void Start(NextSampleFn next_sample_fn) {
    next_sample_fn_ = next_sample_fn;
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);  
  }
  
  void Stop() {
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);  
  }
  
  template<int num_cycles>
  inline void Wait() {
    __asm__("nop");
    Wait<num_cycles - 1>();
  }
  
  void NextSample() {
    GPIOA->BSRR = GPIO_Pin_11;

    uint16_t sample = (*next_sample_fn_)();
    GPIOA->BRR = GPIO_Pin_11;

    uint16_t word_1 = 0x1000 | (0 << 9) | (sample >> 8);
    SPI2->DR = word_1;

    Wait<64>();
    uint16_t word_2 = sample << 8;
    SPI2->DR = word_2;
  }
  
  static FirmwareUpdateDac* GetInstance() { return instance_; }
  
 private:
  NextSampleFn next_sample_fn_;
  static FirmwareUpdateDac* instance_;
  
  DISALLOW_COPY_AND_ASSIGN(FirmwareUpdateDac);
};

template<>
inline void FirmwareUpdateDac::Wait<0>() { }

}  // namespace tides

#endif  // TIDES_DRIVERS_FIRMWARE_UPDATE_DAC_H_
