// Copyright 2021 Emilie Gillet.
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
// Multi-segment envelope generator.
//
// The classic constant-time design (as found in many other MI products) might
// cause differences in behavior from the original DX-series envelopes, in
// particular when jumping to the last segment before having reached the sustain
// phase.
//
// The unusual RenderAtSample() method allows the evaluation of the envelope at
// an arbitrary point in time, used in Plaits' "envelope scrubbing" feature.
//
// A couple of quirks from the DX-series' operator envelopes are implemented,
// namely:
// - vaguely logarithmic shape for ascending segments.
// - direct jump above a threshold for ascending segments.
// - specific logic and rates for plateaus.

#ifndef PLAITS_DSP_FM_ENVELOPE_H_
#define PLAITS_DSP_FM_ENVELOPE_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "plaits/dsp/fm/dx_units.h"

namespace plaits {

namespace fm {

template<int num_stages=4, bool reshape_ascending_segments=false>
class Envelope {
 public:
  Envelope() { }
  ~Envelope() { }
  
  enum {
    NUM_STAGES = num_stages,
    PREVIOUS_LEVEL = -100
  };
  
  inline void Init() {
    Init(1.0f);
  }
  
  inline void Init(float scale) {
    scale_ = scale;
    stage_ = num_stages - 1;
    phase_ = 1.0f;
    start_ = 0.0f;
    for (int i = 0; i < num_stages; ++i) {
      increment_[i] = 0.001f;
      level_[i] = 1.0f / float(1 << i);
    }
    level_[num_stages - 1] = 0.0f;
  }

  // Directly copy the variables.
  void Set(const float increment[num_stages], const float level[num_stages]) {
    std::copy(&increment[0], &increment[num_stages], &increment_[0]);
    std::copy(&level[0], &level[num_stages], &level_[0]);
  }
  
  inline float RenderAtSample(float t, const float gate_duration) {
    if (t > gate_duration) {
      // Check how far we are into the release phase.
      const float phase = (t - gate_duration) * increment_[num_stages - 1];
      return phase >= 1.0f
          ? level_[num_stages - 1]
          : value(num_stages - 1, phase,
                RenderAtSample(gate_duration, gate_duration));
    }

    int stage = 0;
    for (; stage < num_stages - 1; ++stage) {
      const float stage_duration = 1.0f / increment_[stage];
      if (t < stage_duration) {
        break;
      }
      t -= stage_duration;
    }

    if (stage == num_stages - 1) {
      t -= gate_duration;
      if (t <= 0.0f) {
        // TODO(pichenettes): this should always be true.
        return level_[num_stages - 2];
      } else if (t * increment_[num_stages - 1] > 1.0f) {
        return level_[num_stages - 1];
      }
    }
    return value(stage, t * increment_[stage], PREVIOUS_LEVEL);
  }
  
  inline float Render(bool gate) {
    return Render(gate, 1.0f, 1.0f, 1.0f);
  }
  
  inline float Render(
      bool gate,
      float rate,
      float ad_scale,
      float release_scale) {
    if (gate) {
      if (stage_ == num_stages - 1) {
        start_ = value();
        stage_ = 0;
        phase_ = 0.0f;
      }
    } else {
      if (stage_ != num_stages - 1) {
        start_ = value();
        stage_ = num_stages - 1;
        phase_ = 0.0f;
      }
    }
    phase_ += increment_[stage_] * rate * \
        (stage_ == num_stages - 1 ? release_scale : ad_scale);
    if (phase_ >= 1.0f) {
      if (stage_ >= num_stages - 2) {
        phase_ = 1.0f;
      } else {
        phase_ = 0.0f;
        ++stage_;
      }
      start_ = PREVIOUS_LEVEL;
    }
    
    return value();
  }
  
 private:
  inline float value() {
    return value(stage_, phase_, start_);
  }
  
  inline float value(int stage, float phase, float start_level) {
   float from = start_level == PREVIOUS_LEVEL
       ? level_[(stage - 1 + num_stages) % num_stages] : start_level;
   float to = level_[stage];
   
   if (reshape_ascending_segments && from < to) {
     from = std::max(6.7f, from);
     to = std::max(6.7f, to);
     phase *= (2.5f - phase) * 0.666667f;
   }
   
   return phase * (to - from) + from;
  }

  int stage_;
  float phase_;
  float start_;
  
 protected:
  float increment_[num_stages];
  float level_[num_stages];
  float scale_;
  
  DISALLOW_COPY_AND_ASSIGN(Envelope);
};

class OperatorEnvelope : public Envelope<4, true> {
 public:
  void Set(const uint8_t rate[NUM_STAGES], const uint8_t level[NUM_STAGES],
           uint8_t global_level) {
    // Configure levels.
    for (int i = 0; i < NUM_STAGES; ++i) {
      int level_scaled = OperatorLevel(level[i]);
      level_scaled = (level_scaled & ~1) + global_level - 133; // 125 ?
      level_[i] = 0.125f * \
          (level_scaled < 1 ? 0.5f : static_cast<float>(level_scaled));
    }
  
    // Configure increments.
    for (int i = 0; i < NUM_STAGES; ++i) {
      float increment = OperatorEnvelopeIncrement(rate[i]);
      float from = level_[(i - 1 + NUM_STAGES) % NUM_STAGES];
      float to = level_[i];
      
      if (from == to) {
        // Quirk: for plateaux, the increment is scaled.
        increment *= 0.6f;
        if (i == 0 && !level[i]) {
          // Quirk: the attack plateau is faster.
          increment *= 20.0f;
        }
      } else if (from < to) {
        from = std::max(6.7f, from);
        to = std::max(6.7f, to);
        if (from == to) {
          // Quirk: because of the jump, the attack might disappear.
          increment = 1.0f;
        } else {
          // Quirk: because of the weird shape, the rate is adjusted.
          increment *= 7.2f / (to - from);
        }
      } else {
        increment *= 1.0f / (from - to);
      }
      increment_[i] = increment * scale_;
    }
  }
};

class PitchEnvelope : public Envelope<4, false> {
 public:
  void Set(const uint8_t rate[NUM_STAGES], const uint8_t level[NUM_STAGES]) {
    // Configure levels.
    for (int i = 0; i < NUM_STAGES; ++i) {
      level_[i] = PitchEnvelopeLevel(level[i]);
    }
  
    // Configure increments.
    for (int i = 0; i < NUM_STAGES; ++i) {
      float from = level_[(i - 1 + NUM_STAGES) % NUM_STAGES];
      float to = level_[i];
      float increment = PitchEnvelopeIncrement(rate[i]);
      if (from != to) {
        increment *= 1.0f / fabsf(from - to);
      } else if (i != NUM_STAGES - 1) {
        increment = 0.2f;
      }
      increment_[i] = increment * scale_;
    }
  }
};

}  // namespace fm

}  // namespace plaits

#endif  // PLAITS_DSP_FM_ENVELOPE_H_
