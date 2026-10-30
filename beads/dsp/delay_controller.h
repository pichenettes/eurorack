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
// Generates the delay time map.

#ifndef BEADS_DSP_DELAY_CONTROLLER_H_
#define BEADS_DSP_DELAY_CONTROLLER_H_

#include "stmlib/dsp/hysteresis_filter.h"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/utils/gate_flags.h"

#include "beads/dsp/delay_clock.h"
#include "beads/dsp/sine_oscillator.h"
#include "beads/dsp/trigger_generator.h"
#include "beads/dsp/randomized_parameter.h"

namespace beads {

struct DelayTap {
  int32_t delay;
  float gain;
};

class PlayHead {
 public:
  PlayHead() { }
  ~PlayHead() { }
  
  inline void Init() {
    start_ = 0;
    remaining_ = 0;
    mid_point_ = 0;
    phase_ = 0.0f;
    phase_increment_ = 0.0f;
    gain_ = 0.0f;
  }
  
  inline bool done() const { return remaining_ == 0; }
  inline bool half_way() const {
    return remaining_ && (remaining_ == mid_point_);
  }
  inline float phase() const { return phase_; }

  inline void Start(int start, int duration, float gain) {
    start_ = start;
    remaining_ = duration;
    mid_point_ = duration >> 1;
    phase_ = 0.0f;
    phase_increment_ = 1.0f / float(duration);
    gain_ = gain;
  }

  static inline float Triangle(float phase, float pw) {
    const float slope_up = 1.0f / pw;
    const float slope_down = 1.0f / (1.0f - pw);
    return phase < pw ? phase * slope_up : 1.0f - (phase - pw) * slope_down;
  }

  inline void Render(DelayTap* tap, float shape, bool slave) {
    if (done()) {
      tap->delay = 0;
      tap->gain = 0.0f;
      return;
    }
    float envelope;
    if (slave) {
      const float slave_envelopes[6] = {
          0.0f, 0.0f, 0.0f, Triangle(phase_, 0.5f), 0.0f, 0.0f,
      };
      envelope = stmlib::Interpolate(slave_envelopes, shape, 5.0f);
    } else {
      const float pw[6] = {
        0.0f, 0.001f, 0.01f, 0.5f, 0.75f, 0.999f
      };
      if (shape < 0.2f) {
        const float square = std::min(100.0f * Triangle(phase_, 0.5f), 1.0f);
        const float triangle = Triangle(phase_, pw[1]);
        envelope = square + (triangle - square) * shape * 5.0f;
      } else {
        envelope = Triangle(phase_, stmlib::Interpolate(pw, shape, 5.0f));
      }
    }
    tap->delay = int32_t(start_ + remaining_) << 10;
    tap->gain = gain_ * envelope;
    --remaining_;
    phase_ += phase_increment_;
  }

 private:
  int start_;
  int remaining_;
  int mid_point_;
  float phase_;
  float phase_increment_;
  float gain_;

  DISALLOW_COPY_AND_ASSIGN(PlayHead);
};

class TapEnvelope {
 public:
  TapEnvelope() { }
  ~TapEnvelope() { }
  
  inline void Init() {
    phase_ = 0.0f;
    phase_increment_ = 0.0f;
  }
  
  inline void Reset() {
    phase_ = 0.0f;
  }  
  inline void set_phase_increment(float phase_increment) {
    phase_increment_ = phase_increment;
  }
  
  inline float phase() const { return phase_; }
  
  inline float Render(float shape) {
    float envelope;
    const float pw[6] = {
      0.0f, 0.001f, 0.01f, 0.5f, 0.75f, 0.999f
    };
    if (shape < 0.2f) {
      const float square = 1.0f;
      const float triangle = PlayHead::Triangle(phase_, pw[1]);
      envelope = square + (triangle - square) * shape * 5.0f;
    } else {
      envelope = PlayHead::Triangle(
          phase_, stmlib::Interpolate(pw, shape, 5.0f));
    }
    phase_ += phase_increment_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    return envelope;
  }
  
 private:
  float phase_;
  float phase_increment_;
  
  DISALLOW_COPY_AND_ASSIGN(TapEnvelope);
};

class DelayController {
 public:
  DelayController() { }
  ~DelayController() { }
  
  enum {
    NUM_INTERVALS = 2,
    NUM_RATIOS = 2,
    NUM_HEADS = NUM_INTERVALS * NUM_RATIOS,
    NUM_TAPS = NUM_HEADS * 2,
  };
  
  struct Output {
    DelayTap tap[NUM_TAPS];
    bool gate;
  };
  
  void Init(float sample_rate, int max_delay);
  
  void Render(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* external_input,
      const bool* freeze,
      float density,
      float linear_fm,
      float exponential_fm,
      const RandomizedParameter& position,
      const RandomizedParameter& shape,
      Output* out,
      size_t size);
  
 private:
  template<bool synchronized>
  void RenderInternal(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* external_input,
      const bool* freeze,
      float density,
      float linear_fm,
      float exponential_fm,
      const RandomizedParameter& position,
      const RandomizedParameter& shape,
      Output* out,
      size_t size);
  
  int max_delay_;
  
  DelayClock clock_;
  
  PlayHead play_head_[NUM_HEADS];
  PlayHead slave_play_head_[NUM_HEADS];
  stmlib::HysteresisQuantizer2 numerator_quantizer_[NUM_HEADS];

  TapEnvelope tap_envelope_[NUM_TAPS];
  
  RandomWalkModulation position_modulation_;
  RandomWalkModulation shape_modulation_;
  SineOscillator tap_modulation_;
  
  stmlib::HysteresisFilter density_filter_;
  stmlib::HysteresisFilter fm_filter_;
  stmlib::HysteresisFilter slice_index_filter_;
  
  float multiplier_lp_;
  float shape_;
  float linear_fm_;
  float fm_;
  float density_;
  
  bool freeze_;
  SeedButtonState seed_button_state_;

  DISALLOW_COPY_AND_ASSIGN(DelayController);
};


}  // namespace beads

#endif  // BEADS_DSP_DELAY_CONTROLLER_H_
