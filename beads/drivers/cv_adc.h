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
// Driver for ADC1 - used for scanning CVs.
//
// Configured for a maximum sample rate of 10.66kHz, 8x oversampling
// with manually triggered conversion of a block of 6 channels.

#ifndef BEADS_DRIVERS_CV_ADC_H_
#define BEADS_DRIVERS_CV_ADC_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

#include <stm32h7xx_hal.h>

namespace beads {

enum CvAdcChannel {
  CV_ADC_CHANNEL_ASSIGN,
  CV_ADC_CHANNEL_DENSITY,
  CV_ADC_CHANNEL_PITCH,
  CV_ADC_CHANNEL_TIME,
  CV_ADC_CHANNEL_SIZE,
  CV_ADC_CHANNEL_SHAPE,
  CV_ADC_CHANNEL_LAST
};

class CvAdc {
 public:
  CvAdc() { }
  ~CvAdc() { }
  
  bool Init();
  void DeInit();
  void Convert();

  inline uint16_t value(CvAdcChannel channel) const { return values_[channel]; }
  
  inline float float_value(CvAdcChannel channel) const {
    return static_cast<float>(values_[channel]) / 65536.0f;
  }
  
  inline const uint16_t* values() { return values_; }
  
 private:
  bool InitializeADC();
  bool InitializeGPIO();

  static uint16_t values_[CV_ADC_CHANNEL_LAST] __attribute__((section (".dma")));
  
  ADC_HandleTypeDef adc_;
  DMA_HandleTypeDef dma_adc_;

  DISALLOW_COPY_AND_ASSIGN(CvAdc);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_CV_ADC_H_
