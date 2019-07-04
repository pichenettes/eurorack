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
// Generates several ramps, in lockstep or with various frequency/slope ratios.

#ifndef TIDES_RAMP_GENERATOR_H_
#define TIDES_RAMP_GENERATOR_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/utils/gate_flags.h"

#include <algorithm>

#include "tides2/ratio.h"

namespace tides {
  
enum RampMode {
  RAMP_MODE_AD,
  RAMP_MODE_LOOPING,
  RAMP_MODE_AR,
  RAMP_MODE_LAST
};

enum OutputMode {
  OUTPUT_MODE_GATES,
  OUTPUT_MODE_AMPLITUDE,
  OUTPUT_MODE_SLOPE_PHASE,
  OUTPUT_MODE_FREQUENCY,
  OUTPUT_MODE_LAST,
};

enum Range {
  RANGE_CONTROL,
  RANGE_AUDIO,
  RANGE_LAST
};

template<size_t num_channels=4>
class RampGenerator {
 public:
  RampGenerator() { }
  ~RampGenerator() { }
  
  inline float phase(size_t index) const {
    return phase_[index];
  }

  inline float frequency(size_t index) const {
    return frequency_[index];
  }
  
  inline void Init() {
    master_phase_ = 0.0f;
    std::fill(&phase_[0], &phase_[num_channels], 0.0f);
    std::fill(&frequency_[0], &frequency_[num_channels], 0.0f);
    std::fill(&wrap_counter_[0], &wrap_counter_[num_channels], 0);

    Ratio r;
    r.ratio = 1.0f;
    r.q = 1;
    std::fill(&ratio_[0], &ratio_[num_channels], r);
    next_ratio_ = &ratio_[0];
  }
  
  inline void set_next_ratio(const Ratio* next_ratio) {
    next_ratio_ = next_ratio;
  }
  
  template<
      RampMode ramp_mode,
      OutputMode output_mode,
      Range range,
      bool use_ramp>
  inline void Step(
      const float f0,
      const float* pw,
      stmlib::GateFlags gate_flags,
      float ramp) {

    const size_t n = output_mode == OUTPUT_MODE_FREQUENCY ||
        (output_mode == OUTPUT_MODE_SLOPE_PHASE && ramp_mode == RAMP_MODE_AR)
            ? num_channels : 1;

    if (ramp_mode == RAMP_MODE_AD) {
      if (gate_flags & stmlib::GATE_FLAG_RISING) {
        std::fill(&phase_[0], &phase_[n], 0.0f);
      }
      
      for (size_t i = 0; i < n; ++i) {
        frequency_[i] = std::min(f0 * next_ratio_[i].ratio, 0.25f);
        if (use_ramp) {
          phase_[i] = ramp * next_ratio_[i].ratio;
        } else {
          phase_[i] += frequency_[i];
        }
        phase_[i] = std::min(phase_[i], 1.0f);
      }
    }
    
    if (ramp_mode == RAMP_MODE_AR) {
      if (output_mode == OUTPUT_MODE_SLOPE_PHASE) {
        std::fill(&frequency_[0], &frequency_[n], f0);
      } else {
        for (size_t i = 0; i < n; ++i) {
          frequency_[i] = std::min(f0 * next_ratio_[i].ratio, 0.25f);
        }
      }
      
      const bool should_ramp_up = use_ramp
          ? ramp < 0.5f : gate_flags & stmlib::GATE_FLAG_HIGH;
    
      float clip_at = should_ramp_up ? 0.5f : 1.0f;
      for (size_t i = 0; i < n; ++i) {
        if (phase_[i] < 0.5f && !should_ramp_up) {
          phase_[i] = 0.5f;
        } else if (phase_[i] > 0.5f && should_ramp_up) {
          phase_[i] = 0.0f;
        }
        float this_pw = output_mode == OUTPUT_MODE_FREQUENCY ? pw[0] : pw[i];
        float slope = phase_[i] < 0.5f
            ? 0.5f / (1.0e-6f + this_pw)
            : 0.5f / (1.0f + 1.0e-6f - this_pw);
        phase_[i] += frequency_[i] * slope;
        phase_[i] = std::min(phase_[i], clip_at);
      }
    }
    
    if (ramp_mode == RAMP_MODE_LOOPING) {
      if (range == RANGE_AUDIO && output_mode == OUTPUT_MODE_FREQUENCY) {
        // Do not attempt to lock the phase of all outputs. This allows
        // smooth frequency changes.
        bool reset = false;
        if (gate_flags & stmlib::GATE_FLAG_RISING) {
          std::fill(&phase_[0], &phase_[n], 0.0f);
          reset = true;
        }
        for (size_t i = 0; i < n; ++i) {
          frequency_[i] = std::min(f0 * next_ratio_[i].ratio, 0.25f);
        }
        if (!reset) {
          for (size_t i = 0; i < n; ++i) {
            phase_[i] += frequency_[i];
            if (phase_[i] >= 1.0f) {
              phase_[i] -= 1.0f;
            }
          }
        }
      } else {
        if (use_ramp) {
          for (size_t i = 0; i < n; ++i) {
            frequency_[i] = std::min(f0 * ratio_[i].ratio, 0.25f);
          }
          if (ramp < master_phase_) {
            for (size_t i = 0; i < n; ++i) {
              ++wrap_counter_[i];
              if (wrap_counter_[i] >= ratio_[i].q) {
                ratio_[i] = next_ratio_[i];
                wrap_counter_[i] = 0;
              }
            }
          }
          master_phase_ = ramp;
        } else {
          bool reset = false;
          if (gate_flags & stmlib::GATE_FLAG_RISING) {
            master_phase_ = 0.0f;
            std::copy(&next_ratio_[0], &next_ratio_[n], &ratio_[0]);
            std::fill(&wrap_counter_[0], &wrap_counter_[n], 0);
            reset = true;
          }
          for (size_t i = 0; i < n; ++i) {
            frequency_[i] = std::min(f0 * ratio_[i].ratio, 0.25f);
          }
          if (!reset) {
            master_phase_ += f0;
          }
          if (master_phase_ >= 1.0f) {
            master_phase_ -= 1.0f;
            for (size_t i = 0; i < n; ++i) {
              ++wrap_counter_[i];
              if (wrap_counter_[i] >= ratio_[i].q) {
                ratio_[i] = next_ratio_[i];
                wrap_counter_[i] = 0;
              }
            }
          }
        }
        for (size_t i = 0; i < n; ++i) {
          float mult_phase = master_phase_ + float(wrap_counter_[i]);
          mult_phase *= ratio_[i].ratio;
          MAKE_INTEGRAL_FRACTIONAL(mult_phase);
          phase_[i] = mult_phase_fractional;
        }
      }
    }
  }
  
 private:
  const Ratio* next_ratio_;

  float master_phase_;
  int wrap_counter_[num_channels];

  float phase_[num_channels];
  float frequency_[num_channels];
  Ratio ratio_[num_channels];
  
  DISALLOW_COPY_AND_ASSIGN(RampGenerator);
};

}  // namespace tides

#endif  // TIDES_RAMP_GENERATOR_H_
