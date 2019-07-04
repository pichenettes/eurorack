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
// Drivers for the 12-bit ADC scanning pots and sliders.

#ifndef TIDES_DRIVERS_POTS_ADC_H_
#define TIDES_DRIVERS_POTS_ADC_H_

#include "stmlib/stmlib.h"

namespace tides {

enum PotsAdcChannel {
  POTS_ADC_CHANNEL_POT_FREQUENCY,
  POTS_ADC_CHANNEL_POT_SHAPE,
  POTS_ADC_CHANNEL_POT_SLOPE,
  POTS_ADC_CHANNEL_POT_SMOOTHNESS,
  POTS_ADC_CHANNEL_POT_SHIFT,
  POTS_ADC_CHANNEL_ATTENUVERTER_FREQUENCY,
  POTS_ADC_CHANNEL_ATTENUVERTER_SHAPE,
  POTS_ADC_CHANNEL_ATTENUVERTER_SLOPE,
  POTS_ADC_CHANNEL_ATTENUVERTER_SMOOTHNESS,
  POTS_ADC_CHANNEL_ATTENUVERTER_SHIFT,
  POTS_ADC_CHANNEL_LAST,
};

class PotsAdc {
 public:
  PotsAdc() { }
  ~PotsAdc() { }
  
  void Init();
  void Convert();

  inline int32_t value(PotsAdcChannel channel) const {
    return static_cast<int32_t>(values_[channel]);
  }
  
  inline float float_value(PotsAdcChannel channel) const {
    return static_cast<float>(value(channel)) / 65536.0f;
  }
  
 private:
  uint16_t adc_values_[3];
  uint16_t values_[POTS_ADC_CHANNEL_LAST];
  int mux_address_;
  bool conversion_done_;
  
  static uint8_t mux_address_to_channel_index_[8];
  
  DISALLOW_COPY_AND_ASSIGN(PotsAdc);
};

}  // namespace tides

#endif  // TIDES_DRIVERS_POTS_ADC_H_
