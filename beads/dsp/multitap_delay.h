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
// Multitap delay.

#ifndef BEADS_DSP_MULTITAP_DELAY_H_
#define BEADS_DSP_MULTITAP_DELAY_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/filter.h"

#include "beads/dsp/tap.h"
#include "beads/dsp/delay_controller.h"

#define ENABLE_BANDWIDTH_CONTROL

namespace beads {

class MultitapDelay {
 public:
  MultitapDelay() { }
  ~MultitapDelay() { }
  
  inline void Init(float sample_rate) {
    Reset();
    for (int i = 0; i < 2; ++i) {
      filter_[i].Init();
    }
  }
  
  inline void Reset() {
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
      tap_[i].Init();
      no_latency_tap_[i].Init();
    }
  }
  
  template<typename DelayLineType>
  inline void Render(
      const DelayLineType* delay_line,
      DelayController::Output taps,
      float speed,
      float bandwidth,
      FloatFrame* out) {
    
    // Cross-fade between the original and pitch-shifted delays.
    float gain = 100.0f * (fabsf(speed - 1.0f) - 0.01f);
    CONSTRAIN(gain, 0.0f, 1.0f);

    FloatFrame mix;
    mix.l = mix.r = 0.0f;
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
      tap_[i].Render(
          delay_line,
          taps.tap[i].delay,
          taps.tap[i].gain * gain,
          speed,
          &mix);
      no_latency_tap_[i].Render(
          delay_line,
          taps.tap[i].delay,
          taps.tap[i].gain * (1.0f - gain),
          speed,
          &mix);
    }

#ifdef ENABLE_BANDWIDTH_CONTROL
    const float amount = fabsf(2.0f * bandwidth - 1.0f);
    
    // LP: -7 to -2 octaves
    // HP: -8 to -3 octaves
    const float octave = bandwidth < 0.5f
        ? -7.0f + bandwidth * 10.0f
        : -8.0f + (bandwidth - 0.5f) * 10.0f;
    const float f = 0.49f * stmlib::SemitonesToRatio(12.0f * octave);
    const float q = 1.0f + 16.0f * amount * f;
    const float wet_amount_sqrt = amount * (2.0f - amount);
    const float wet_amount = wet_amount_sqrt * (2.0f - wet_amount_sqrt);
    
    filter_[0].set_f_q<stmlib::FREQUENCY_FAST>(f, q);
    filter_[1].set(filter_[0]);
    
    FloatFrame wet;
    if (bandwidth < 0.5f) {
      wet.l = filter_[0].Process<stmlib::FILTER_MODE_LOW_PASS>(mix.l);
      wet.r = filter_[1].Process<stmlib::FILTER_MODE_LOW_PASS>(mix.r);
    } else {
      wet.l = filter_[0].Process<stmlib::FILTER_MODE_HIGH_PASS>(mix.l);
      wet.r = filter_[1].Process<stmlib::FILTER_MODE_HIGH_PASS>(mix.r);
    }
    *out += mix + (wet - mix) * wet_amount;
#else
    *out += mix;
#endif  // ENABLE_BANDWIDTH_CONTROL
  }

 private:
  Tap<2048> tap_[DelayController::NUM_TAPS];
  Tap<1> no_latency_tap_[DelayController::NUM_TAPS];
  stmlib::Svf filter_[2];

  DISALLOW_COPY_AND_ASSIGN(MultitapDelay);
};

}  // namespace beads

#endif  // BEADS_DSP_MULTITAP_DELAY_H_
