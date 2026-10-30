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
// Generate the random or periodic pulse train used to trigger grains.

#ifndef BEADS_DSP_TRIGGER_GENERATOR_H_
#define BEADS_DSP_TRIGGER_GENERATOR_H_

#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/utils/gate_flags.h"

#include <algorithm>

namespace beads {

enum SeedButtonState {
  SEED_BUTTON_STATE_OFF,
  SEED_BUTTON_STATE_ON,
  SEED_BUTTON_STATE_LOCKED,
};

class TriggerGenerator {
 public:
  TriggerGenerator() { }
  ~TriggerGenerator() { }
  
  struct Output {
    bool trigger;
    bool gate;
    float fractional_sample;
    float regularity;
    float phase;
  };

  void Init(float sample_rate);
  
  void Render(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* external_input,
      float density,
      float linear_fm,
      float exponential_fm,
      Output* out,
      size_t size);
  
 private:
  void RenderPulseTrain(
      const stmlib::GateFlags* gate_flags,
      float frequency,
      bool randomize,
      Output* out,
      size_t size);
  
  void RenderDivider(
      const stmlib::GateFlags* gate_flags,
      float density,
      Output* out,
      size_t size);

  float phase_;
  float frequency_;
  float target_frequency_;
  float frequency_lp_coefficient_;
  float ratio_;
  float one_hertz_;
  int division_;
  bool audio_rate_;
  
  stmlib::HysteresisQuantizer2 divider_quantizer_;

  float max_pulse_frequency_;
  int audio_rate_interval_;
  
  bool accept_pulse_;
  int pulse_count_;
  int elapsed_since_last_pulse_;

  int divider_reset_interval_;
  int default_reset_interval_;
  
  stmlib::GateFlags previous_flags_;

  DISALLOW_COPY_AND_ASSIGN(TriggerGenerator);
};

}  // namespace beads

#endif  // BEADS_DSP_TRIGGER_GENERATOR_H_
