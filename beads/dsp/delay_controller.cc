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
// Generates the delay time map.

#include "beads/dsp/delay_controller.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace beads {

using namespace std;
using namespace stmlib;

const int kMinDelay = 64 + 4;

void DelayController::Init(float sample_rate, int max_delay) {
  max_delay_ = max_delay;

  clock_.Init(sample_rate);
  
  for (int i = 0; i < NUM_HEADS; ++i) {
    numerator_quantizer_[i].Init(2, 0.05f, false);
    play_head_[i].Init();
    slave_play_head_[i].Init();
  }
  
  for (int i = 0; i < NUM_TAPS; ++i) {
    tap_envelope_[i].Init();
  }
  
  position_modulation_.Init(0.1f / sample_rate);
  shape_modulation_.Init(0.1f / sample_rate);
  tap_modulation_.Init(0.17f / sample_rate);
  
  
  const float kFilterCoefficient = 0.0025f;
  density_filter_.Init(kFilterCoefficient);
  slice_index_filter_.Init(kFilterCoefficient);
  fm_filter_.Init(kFilterCoefficient);

  multiplier_lp_ = 0.0f;
  shape_ = 0.0f;
  linear_fm_ = 0.0f;
  fm_ = 1.0f;
  density_ = 0.0f;
  
  freeze_ = false;
  seed_button_state_ = SEED_BUTTON_STATE_OFF;
}

const int kNumDelayRatios = 20;
const float kRatio[kNumDelayRatios + 2] = {
  // Powers of 2
  1.0f / 16.0f,
  1.0f / 16.0f,
  1.0f / 8.0f,
  1.0f / 8.0f,
  1.0f / 4.0f,
  1.0f / 4.0f,
  1.0f / 2.0f,
  1.0f / 2.0f,
  1.0f,
  1.0f,
  // Dividers
  1.0f,
  1.0f,
  1.0f / 2.0f,
  1.0f / 3.0f,
  1.0f / 4.0f,
  1.0f / 6.0f,
  1.0f / 8.0f,
  1.0f / 10.0f,
  1.0f / 12.0f,
  1.0f / 16.0f,
  1.0f / 16.0f,
  1.0f / 16.0f,
};

void DelayController::Render(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* external_input,
    const bool* freeze,
    float density,
    float linear_fm,
    float exponential_fm,
    const RandomizedParameter& position,
    const RandomizedParameter& shape,
    Output* out,
    size_t size) {
  if (seed_button_state == SEED_BUTTON_STATE_LOCKED && !external_input) {
    RenderInternal<false>(
        seed_button_state,
        external_input,
        freeze,
        density,
        linear_fm,
        exponential_fm,
        position,
        shape, out, size);
  } else {
    RenderInternal<true>(
        seed_button_state,
        external_input,
        freeze,
        density,
        linear_fm,
        exponential_fm,
        position,
        shape, out, size);
  }
}

template<bool synchronized>
void DelayController::RenderInternal(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* external_input,
    const bool* freeze,
    float density,
    float linear_fm,
    float exponential_fm,
    const RandomizedParameter& position,
    const RandomizedParameter& shape,
    Output* out,
    size_t size) {
  if (synchronized) {
    // We don't make any difference between linear and expo FM when the CV
    // is only selecting a divider: the CV is swinging the knob, end of the
    // story.
    linear_fm += exponential_fm;
    exponential_fm = 0.0f;
  }
  
  // Add more filtering to the CV. Because.
  ONE_POLE(linear_fm_, linear_fm, 0.05f);
  
  density += linear_fm_;
  CONSTRAIN(density, 0.0f, 1.0f);
  
  const float slice_modulation = 1.005f * position.value(
      &position_modulation_,
      size,
      1.0f,
      true);
  ParameterInterpolator modulated_shape(
      &shape_,
      shape.value(&shape_modulation_, size, 1.0f, true) * 0.995f,
      size);
  ParameterInterpolator modulated_fm(
      &fm_,
      SemitonesToRatio(-fm_filter_.Process(exponential_fm) * 60.0f),
      size);
  ParameterInterpolator modulated_density(
      &density_,
      density_filter_.Process(density),
      size);
  
  bool tap = (seed_button_state == SEED_BUTTON_STATE_ON) && \
      (seed_button_state_ == SEED_BUTTON_STATE_OFF);
  seed_button_state_ = seed_button_state;
  
  while (size--) {
    float tap_ratio[NUM_RATIOS];
    float tap_gain[NUM_RATIOS];

    const float density = modulated_density.Next();
    const float fm = modulated_fm.Next();
    
    if (synchronized) {
      // Record the interval between taps on the button or triggers
      // on the SEED input.
      clock_.Process(
          external_input && (*external_input & GATE_FLAG_RISING), tap);
      tap = false; // The tap is recorded only once!
      
      // Use density to select a ratio, and FM to further multiply/divide it.
      float ratio = density * float(kNumDelayRatios);
      MAKE_INTEGRAL_FRACTIONAL(ratio);
      ratio_fractional -= 0.5f;
      ratio_fractional *= density < 0.5f ? 4.0f : 1.5f;
      ratio_fractional += 0.5f;
      CONSTRAIN(ratio_fractional, 0.0f, 1.0f);
  
      int flip = ratio_integral & 1;
      tap_ratio[flip] = kRatio[ratio_integral];
      tap_ratio[1 - flip] = kRatio[ratio_integral + 1];
      tap_gain[flip] = 1.0f - ratio_fractional;
      tap_gain[1 - flip] = ratio_fractional;
      
      tap_ratio[0] *= fm;
      tap_ratio[1] *= fm;
    } else {
      const float new_interval = max(
          float(kMinDelay),
          fm * float(max_delay_) * \
              SemitonesToRatio(-fabsf(density - 0.5f) * 216.0f));
      float lp_coefficient = 10.0f / new_interval;
      CONSTRAIN(lp_coefficient, 0.005f, 0.7f);
      
      clock_.Update(new_interval, lp_coefficient);

      tap_ratio[0] = 1.00f;
      tap_ratio[1] = 0.77f;

      float fade_to_odd = (density - 0.5f) * 25.0f;
      CONSTRAIN(fade_to_odd, 0.0f, 1.0f);
      fade_to_odd = fade_to_odd * (0.25f + 0.75f * tap_modulation_.Render());
      tap_gain[0] = 1.0f - fade_to_odd * 0.2f;
      tap_gain[1] = fade_to_odd * 0.8f;
    }

    const float shape = modulated_shape.Next();
    
    if (*freeze) {
      // Reset all play heads when freezing.
      if (!freeze_) {
        for (int i = 0; i < NUM_HEADS; ++i) {
          play_head_[i].Init();
          slave_play_head_[i].Init();
        }
      }
      
      // Reset and resync the play heads that will be faded in.
      if (clock_.retrig()) {
        int start = clock_.new_interval_index() * NUM_RATIOS;
        for (int i = start; i < start + NUM_RATIOS; ++i) {
          play_head_[i].Init();
          slave_play_head_[i].Init();
        }
      }
      
      float slice_index_smooth = slice_index_filter_.Process(slice_modulation);
      
      for (int i = 0; i < NUM_HEADS; ++i) {
        PlayHead* new_head = NULL;
        
        if (play_head_[i].done()) {
          new_head = &play_head_[i];
        } else if (play_head_[i].half_way()) {
          new_head = &slave_play_head_[i];
        }
        
        const int interval = i / NUM_RATIOS;
        const int ratio = i % NUM_RATIOS;

        if (new_head) {
          const int slice_duration = int(
              clock_.interval(interval) * tap_ratio[ratio]);
          const int num_slices = max_delay_ / slice_duration;
          if (numerator_quantizer_[i].num_steps() != num_slices) {
            numerator_quantizer_[i].Init(
                num_slices,
                0.05f / 5.0f * float(num_slices),
                false);
          }
          
          const int slice_index = numerator_quantizer_[i].Process(
              slice_index_smooth);
          
          new_head->Start(
              slice_duration * slice_index + kMinDelay,
              slice_duration,
              tap_gain[ratio] * clock_.gain(interval));
        }
        play_head_[i].Render(&out->tap[2 * i], shape, false);
        slave_play_head_[i].Render(&out->tap[2 * i + 1], shape, true);
      }
      out->gate = \
          play_head_[clock_.new_interval_index() * NUM_RATIOS].phase() < 0.5f;
    } else {
      // Reset all envelopes when unfreezing.
      if (freeze_) {
        for (int i = 0; i < NUM_TAPS; ++i) {
          tap_envelope_[i].Init();
        }
      }

      // Reset and resync the envelopes that will be faded-in.
      if (clock_.retrig()) {
        int start = clock_.new_interval_index() * NUM_HEADS;
        for (int i = start; i < start + NUM_HEADS; ++i) {
          tap_envelope_[i].Init();
        }
      }
      
      ONE_POLE(multiplier_lp_, slice_modulation, 0.01f);
      float multiplier = multiplier_lp_ * 15.0f + 1.0f;
      CONSTRAIN(multiplier, 1.0f, 16.0f);
      
      MAKE_INTEGRAL_FRACTIONAL(multiplier);
      multiplier_fractional = 0.5f + 8.0f * (multiplier_fractional - 0.5f);
      CONSTRAIN(multiplier_fractional, 0.0f, 1.0f);
      
      float loudest = 0.0f;
      int loudest_index = 0;
      
      for (int t = 0; t < NUM_TAPS; ++t) {
        const int interval = t / NUM_HEADS;
        const int ratio = (t / NUM_INTERVALS) % NUM_RATIOS;
        const int tap = t & 1;
        const int i = (2 * ratio + ((multiplier_integral & 1)
            ? tap
            : (1 - tap))) + interval * NUM_HEADS;
          
        float delay_time = clock_.interval(interval) * tap_ratio[ratio] * float(
              multiplier_integral + tap);

        CONSTRAIN(delay_time, kMinDelay, max_delay_);

        tap_envelope_[i].set_phase_increment(1.0f / delay_time);
        out->tap[i].delay = int32_t(delay_time * 1024.0f);
        
        const float ratio_fade = tap
            ? multiplier_fractional
            : 1.0f - multiplier_fractional;
        const float base_gain = tap_gain[ratio] * ratio_fade * \
            clock_.gain(interval);
        out->tap[i].gain = base_gain * tap_envelope_[i].Render(shape);
        if (base_gain > loudest) {
          loudest = base_gain;
          loudest_index = i;
        }
      }
      out->gate = tap_envelope_[loudest_index].phase() < 0.5f;
    }

    freeze_ = *freeze;
    
    if (external_input) {
      ++external_input;
    }
    ++freeze;
    ++out;
  }
}

}  // namespace beads