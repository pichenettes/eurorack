// Copyright 2018 Emilie Gillet.
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
// Wow and flutter simulation.

#ifndef BEADS_DSP_FX_WOW_FLUTTER_H_
#define BEADS_DSP_FX_WOW_FLUTTER_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/random.h"
#include "stmlib/dsp/delay_line.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/sine_oscillator.h"

namespace beads {

class WowAndFlutter {
 public:
  WowAndFlutter() { }
  ~WowAndFlutter() { }
  
  void Init(float sample_rate) {
    for (size_t i = 0; i < 2; ++i) {
      delay_line_[i].Init();
    }
    
    wow_.Init();
    flutter_.Init();
    random_modulation_.Init();

    one_hertz_ = 1.0f / sample_rate;
    random_frequency_ = 0.125f * one_hertz_;

    delay_ = 0.0f;
    amount_ = 1.0f;
    lp_ = 1.0f;
    lp_state_.l = lp_state_.r = 0.0f;
  }
  
  void set_amount(float amount) {
    amount_ = amount;
  }
  
  void set_lp(float lp) {
    lp_ = lp;
  }
  
  void Process(FloatFrame* in_out, size_t size) {
    const float previous_phase = random_modulation_.phase();
    const float random_modulation = random_modulation_.Render(
        random_frequency_ * float(size));
    
    if (random_modulation_.phase() >= 0.25f && previous_phase < 0.25f) {
      random_frequency_ = one_hertz_ * \
          (0.05f + stmlib::Random::GetFloat() * 0.2f);
    }
    
    const float fm = 0.5f + 1.5f * random_modulation * random_modulation;
    const float amount = (1.0f - 0.4f * random_modulation) * amount_;
    
    const float wow_frequency = one_hertz_ * 0.23f * float(size);
    const float wow = wow_.Render(wow_frequency * fm);
    
    const float flutter_frequency = one_hertz_ * 1.99f * float(size);
    const float flutter = flutter_.Render(flutter_frequency);

    const float delay = 1.0f + (105.0f * wow + 17.0f * flutter) * amount;
    stmlib::ParameterInterpolator delay_modulation(&delay_, delay, size);
    
    while (size--) {
      const float delay = delay_modulation.Next();
      ONE_POLE(lp_state_.l, delay_line_[0].WriteRead(in_out->l, delay), lp_);
      ONE_POLE(lp_state_.r, delay_line_[1].WriteRead(in_out->r, delay), lp_);
      *in_out++ = lp_state_;
    }
  }

 private:
  float one_hertz_;
  float delay_;
  float amount_;
  float lp_;
  float random_frequency_;
  FloatFrame lp_state_;
  
  SineOscillator random_modulation_;
  SineOscillator wow_;
  SineOscillator flutter_;
  
  stmlib::DelayLine<float, 128> delay_line_[2];
  DISALLOW_COPY_AND_ASSIGN(WowAndFlutter);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_WOW_FLUTTER_H_
