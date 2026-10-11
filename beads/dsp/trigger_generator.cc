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

#include "beads/dsp/trigger_generator.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace beads {

using namespace std;
using namespace stmlib;

const float kMaxGrainFrequency = 130.81278f;  // Hz
const float kDividerResetInterval = 4.0f;  // s
const float kAudioRateInterval = 1 / 48.0f;  // Hz

void TriggerGenerator::Init(float sample_rate) {
  one_hertz_ = 1.0f / sample_rate;

  max_pulse_frequency_ = kMaxGrainFrequency / sample_rate;

  divider_reset_interval_ = default_reset_interval_ = static_cast<int>(
      sample_rate * kDividerResetInterval);
  audio_rate_interval_ = static_cast<int>(
      sample_rate * kAudioRateInterval);
  audio_rate_ = false;

  divider_quantizer_.Init(8, 0.075f, false);
  
  accept_pulse_ = true;
  pulse_count_ = 0;
  elapsed_since_last_pulse_ = int(sample_rate / 2.0f);
  
  phase_ = 0.0f;
  target_frequency_ = frequency_ = 1.0f / float(elapsed_since_last_pulse_);
  frequency_lp_coefficient_ = 1.0f;

  division_ = 1;
  ratio_ = 1.0f;

  previous_flags_ = GATE_FLAG_LOW;
}

inline float LogOneMinusX(float x) {
  return -x * (1.0f + x * (0.5f + x * (0.3333f + x * 0.25f)));
}

void TriggerGenerator::RenderPulseTrain(
    const stmlib::GateFlags* gate_flags,
    float frequency,
    bool randomize,
    TriggerGenerator::Output* out,
    size_t size) {
  if (!randomize) {
    ratio_ = 1.0f;
  }
  
  while (size--) {
    float f = frequency * ratio_;
    CONSTRAIN(f, 0.000001f, 0.25f);

    const GateFlags flags = *gate_flags++;
    const bool gate = flags & GATE_FLAG_HIGH;
    const bool trigger = flags & GATE_FLAG_RISING;
    
    phase_ += gate ? f : 0.0f;
    
    float t = 0.0f;
    bool reset = false;
    if (trigger) {
      phase_ = 0.0f;
      reset = true;
    } else if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      t = phase_ / f;
      reset = true;
    }
    
    if (reset) {
      if (randomize) {
        ratio_ = -1.0f / LogOneMinusX(0.1f + 0.9f * Random::GetFloat());
      }
    }
    
    out->fractional_sample = t;
    out->trigger = reset && gate;
    out->gate = phase_ < 0.5f && gate;
    out->regularity = randomize ? 0.0f : 1.0f;
    out->phase = phase_;
    ++out;
  }
}

const int kDivisionRatios[] = {
  1, 2, 3, 4, 6, 8, 12, 16
};

void TriggerGenerator::RenderDivider(
    const GateFlags* gate_flags,
    float density,
    TriggerGenerator::Output* out,
    size_t size) {
  const int division = density < 0.48f
      ? divider_quantizer_.Lookup(kDivisionRatios, density * 2.15f)
      : 1;
  const float probability_sqrt = density < 0.48f
      ? 1.01f
      : max((density - 0.52f) * 2.15f, 0.0f);
  const float probability = probability_sqrt * probability_sqrt;
  const float regularity = probability >= 1.0f ? 1.0f : 0.0f;
  
  while (size--) {
    ONE_POLE(frequency_, target_frequency_, frequency_lp_coefficient_);
    phase_ += frequency_;

    const GateFlags flags = *gate_flags++;
    const bool gate = flags & GATE_FLAG_HIGH;
    const bool trigger = flags & GATE_FLAG_RISING;

    if (trigger) {
      // If we haven't seen a pulse for a long time (4s), resynchronize
      // the pulse counter with the first pulse.
      if (elapsed_since_last_pulse_ >= divider_reset_interval_) {
        pulse_count_ = 0;
        phase_ = 0.0f;
      }
      divider_reset_interval_ = max(
          default_reset_interval_,
          4 * elapsed_since_last_pulse_);

      // If the pulses are coming fast (> 48 Hz), enable audio rate tracking.
      // When audio rate tracking is enabled, we no longer align with the phase
      // of the original signal. Instead, we use a free-running phasor,
      // whose frequency is smoothed.
      const int threshold = audio_rate_interval_ + (audio_rate_ ? 8 : 0);
      audio_rate_ = (elapsed_since_last_pulse_ < threshold) && \
          (probability >= 1.0f);

      accept_pulse_ = Random::GetFloat() < probability
          && (pulse_count_ % division) == 0;
      
      if (accept_pulse_) {
        const float period = float(elapsed_since_last_pulse_);
        const float frequency = 1.0f / period;

        if (probability >= 1.0f) {
          target_frequency_ = frequency / float(division);
        } else {
          target_frequency_ = frequency * probability;
        }

        if (audio_rate_) {
          bool no_glide = division != division_;
          division_ = division;
          
          float up_tolerance = (1.02f + 2.0f * frequency) * frequency_;
          float down_tolerance = (0.98f - 2.0f * frequency) * frequency_;
          no_glide |= target_frequency_ > up_tolerance ||
              target_frequency_ < down_tolerance;
          // Note: jitter is more audible on high frequencies, so the shorter
          // the period, the lower the frequency of the smoothing filter.
          frequency_lp_coefficient_ = no_glide
              ? 1.0f
              : period * one_hertz_;
        } else {
          phase_ = 0.0f;
          frequency_lp_coefficient_ = 1.0;  // frequency_ will change ASAP.
        }
      } else {
        if (probability < 1.0f) {
          target_frequency_ *= (1.0f - probability);
        }
      }
      pulse_count_ = (pulse_count_ + 1) % 48;
      elapsed_since_last_pulse_ = 0;
    }
    ++elapsed_since_last_pulse_;
    
    if (audio_rate_) {
      float t = 0.0f;
      const bool reset = phase_ >= 1.0f;
      if (reset) {
        phase_ -= 1.0f;
        t = phase_ / frequency_;
      }
      out->fractional_sample = t;
      out->trigger = reset;
      out->gate = phase_ < 0.5f;
    } else {
      if (phase_ >= 1.0f) {
        phase_ = 1.0f;
      }
      out->fractional_sample = 0.0f;
      out->trigger = accept_pulse_ && trigger;
      out->gate = accept_pulse_ && gate;
    }
    out->regularity = regularity;
    out->phase = phase_;
    ++out;
  }
}

void TriggerGenerator::Render(
    SeedButtonState seed_button_state,
    const GateFlags* external_input,
    float density,
    float linear_fm,
    float exponential_fm,
    TriggerGenerator::Output* out,
    size_t size) {
  if (external_input && seed_button_state == SEED_BUTTON_STATE_LOCKED) {
    // We don't make any difference between linear and expo FM when the CV
    // is only selecting a divider: the CV is swinging the knob, end of the
    // story.
    density += linear_fm + exponential_fm;
    CONSTRAIN(density, 0.0f, 1.0f);

    RenderDivider(external_input, density, out, size);
  } else {
    density += linear_fm;
    CONSTRAIN(density, 0.0f, 1.0f);

    const float fm = SemitonesToRatio(exponential_fm * 60.0f);
    stmlib::GateFlags merged_gate_flags[size];
    
    // Merge seed button and external gate input to get a single gate stream.
    for (size_t i = 0; i < size; ++i) {
      bool gate = seed_button_state != SEED_BUTTON_STATE_OFF;
      if (external_input) {
        gate |= *external_input++ & GATE_FLAG_HIGH;
      }
      merged_gate_flags[i] = previous_flags_ = \
          ExtractGateFlags(previous_flags_, gate);
    }
    
    if (density < 0.48f) {
      RenderPulseTrain(
          merged_gate_flags,
          max_pulse_frequency_ * SemitonesToRatio(-density * 192.0f) * fm,
          false,
          out,
          size);
    } else if (density > 0.52f) {
      RenderPulseTrain(
          merged_gate_flags,
          0.5f * max_pulse_frequency_ * \
              SemitonesToRatio(-(1.0f - density) * 192.0f) * fm,
          true,
          out,
          size);
    } else if (seed_button_state == SEED_BUTTON_STATE_LOCKED) {
      TriggerGenerator::Output o;
      o.fractional_sample = 0.0f;
      o.trigger = false;
      o.gate = false;
      o.regularity = 0.0f;
      o.phase = phase_;
      fill(&out[0], &out[size], o);
    } else {
      // Manual taps.
      for (size_t i = 0; i < size; ++i) {
        phase_ += frequency_;

        bool tap = merged_gate_flags[i] & GATE_FLAG_RISING;
        if (tap) {
          phase_ = 0.0f;
          frequency_ = 1.0f / float(elapsed_since_last_pulse_);
          elapsed_since_last_pulse_ = 0;
        }
        ++elapsed_since_last_pulse_;
        
        if (phase_ >= 1.0f) {
          phase_ = 1.0f;
        }

        out[i].fractional_sample = 0.0f;
        out[i].trigger = tap;
        out[i].gate = merged_gate_flags[i] & GATE_FLAG_HIGH;
        out[i].regularity = 1.0f - (density - 0.48f) * 25.0f;
        out[i].phase = phase_;
      }
    }
  }
}

}  // namespace beads