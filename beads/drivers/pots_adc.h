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
// Driver for ADC1 and ADC2 - used for scanning pots.
//
// Non-muxed pots are scanned at a maximum rate of 13kHz, manually triggered.
// Muxed pots are scanned at a maximum rate of 10kHz, manually triggered.

#ifndef BEADS_DRIVERS_POTS_ADC_H_
#define BEADS_DRIVERS_POTS_ADC_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

namespace beads {

enum PotsAdcChannel {
  POTS_ADC_CHANNEL_FEEDBACK,
  POTS_ADC_CHANNEL_DRY_WET,
  POTS_ADC_CHANNEL_REVERB,
  POTS_ADC_CHANNEL_TIME_AMOUNT,
  POTS_ADC_CHANNEL_SIZE_AMOUNT,
  POTS_ADC_CHANNEL_SHAPE_AMOUNT,
  POTS_ADC_CHANNEL_PITCH_AMOUNT,

  POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL,
  POTS_ADC_CHANNEL_TIME = POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL,
  POTS_ADC_CHANNEL_DENSITY,
  POTS_ADC_CHANNEL_PITCH,
  POTS_ADC_CHANNEL_SIZE,
  POTS_ADC_CHANNEL_SHAPE,
  POTS_ADC_CHANNEL_LAST
};

class PotsAdc {
 public:
  PotsAdc() { }
  ~PotsAdc() { }
  
  bool Init();
  void DeInit();
  void Convert();
  void StartMuxedPotsConversion();
  void Cycle();

  inline uint16_t value(PotsAdcChannel channel) const {
    return values_[channel];
  }
  
  inline float float_value(PotsAdcChannel channel) const {
    return static_cast<float>(value(channel)) / 65536.0f;
  }
  
  static PotsAdc* GetInstance() { return instance_; }
  
 private:
  static const size_t kNumDirectlyScannedPots = POTS_ADC_CHANNEL_LAST - POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL;

  static const size_t kNumMuxedPots = POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL;
  
  bool InitializeADC2();
  bool InitializeADC3();
  bool InitializeGPIO();
  bool InitializeTimer();
  
  static uint16_t values_[POTS_ADC_CHANNEL_LAST] __attribute__((section (".dma")));
  static uint32_t mux_pin_map_[POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL] __attribute__((section (".dma")));

  ADC_HandleTypeDef adc2_;
  ADC_HandleTypeDef adc3_;
  DMA_HandleTypeDef dma_adc2_;
  DMA_HandleTypeDef dma_adc3_;
  TIM_HandleTypeDef tim8_;
  DMA_HandleTypeDef dma_tim8_;
  int muxed_channel_count_;
  
  static PotsAdc* instance_;

  DISALLOW_COPY_AND_ASSIGN(PotsAdc);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_POTS_ADC_H_
