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
// Grain.

#ifndef BEADS_DSP_DELAY_GRAIN_H_
#define BEADS_DSP_DELAY_GRAIN_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/random.h"

#include "beads/dsp/delay_line.h"
#include "beads/dsp/grain.h"

namespace beads {

enum HeadCrossingPolicy {
  HEAD_CROSSING_POLICY_STOP,
  HEAD_CROSSING_POLICY_WRAP,
  HEAD_CROSSING_POLICY_PING_PONG,
  HEAD_CROSSING_POLICY_STUTTER,
  HEAD_CROSSING_POLICY_CROSSFADE,
};

class DelayGrain : public Grain {
 public:
  DelayGrain() { }
  ~DelayGrain() { }
  
  enum {
    // Length of the "safety zone" near the head.
    head_crossing_safety_zone = 256
  };
  
  void Init(float sample_rate) {
    initial_delay_ = delay_ = 0;
    speed_ = 1024;
    gain_[0][0] = gain_[0][1] = 0.5f;
    gain_[1][0] = gain_[1][1] = 0.5f;
    
    envelope_.Init(sample_rate);
    voice_stealing_envelope_.Init();
    remaining_samples_ = 0;
  }
  
  template<typename DelayLineType>
  inline void Start(
      const DelayLineType* delay_line,
      float delay,
      float speed,
      float duration,
      float shape,
      float gain,
      float pan,
      float fractional_sample) {
    float consumption_speed = speed - (delay_line->frozen() ? 0.0f : 1.0f);
    delay -= fractional_sample * consumption_speed;

    // Trim duration so that there's enough space in the buffer.
    if (consumption_speed != 0.0f) {
      duration = std::min(
          duration,
          DelayLineType::MAX_DELAY / fabsf(consumption_speed));
    }
    
    if (consumption_speed > 0.0f) {
      // Trim delay time so we don't hit the recording head.
      delay = std::max(delay, duration * consumption_speed);
    } else if (consumption_speed < 0.0f) {
      // Trim delay time so we don't loop and hit back of the recording head.
      delay = std::min(delay, 
          DelayLineType::MAX_DELAY + duration * consumption_speed);
    }
    
    initial_delay_ = delay_ = int32_t(delay * 1024.0f);
    speed_ = int32_t(speed * 1024.0f);
    
    // Roughly equal power. Uses first order approximation of square root.
    if (DelayLineType::NUM_CHANNELS == 2) {
      if (pan < 0.5f) {
        // Destination Source
        pan *= 2.0f;
        gain_[0][0] = gain;
        gain_[1][0] = 0.0f;
        gain_[1][1] = pan * (2.0f - pan) * gain;
        gain_[0][1] = (1.0f - pan) * (1.0f + pan) * gain;
      } else {
        // Destination Source
        pan = 1.0f - pan;
        pan *= 2.0f;
        gain_[1][1] = gain;
        gain_[0][1] = 0.0f;
        gain_[0][0] = pan * (2.0f - pan) * gain;
        gain_[1][0] = (1.0f - pan) * (1.0f + pan) * gain;
      }
    } else {
      gain_[1][0] = pan * (2.0f - pan) * gain;
      gain_[0][0] = (1.0f - pan) * (1.0f + pan) * gain;
      gain_[0][1] = gain_[1][1] = 0.0f;
    }
    
    envelope_.Start(duration, shape, fractional_sample);
    remaining_samples_ = int(duration);
    
    voice_stealing_envelope_.ResetAndPause();
  }
  
  template<
      HeadCrossingPolicy head_crossing_policy,
      bool fade_out,
      typename DelayLineType>
  inline void Render(const DelayLineType* delay_line, FloatFrame* in_out) {
    if (done()) {
      return;
    }
    
    delay_ -= speed_;
    if (!delay_line->frozen()) {
      delay_ += 1024;
    }
    
    const int32_t tail = DelayLineType::MAX_DELAY << 10;
    const int32_t fade_zone = head_crossing_safety_zone << 10;
    const int32_t xfade_tail = tail - fade_zone;

    switch (head_crossing_policy) {
      case HEAD_CROSSING_POLICY_STOP:
        if (delay_ < 0 || delay_ >= tail) {
          delay_ = 0;
          envelope_.Stop();
        }
        break;

      case HEAD_CROSSING_POLICY_WRAP:
        if (delay_ < 0) {
          delay_ += tail;
        } else if (delay_ >= tail) {
          delay_ -= tail;
        }
        break;

      case HEAD_CROSSING_POLICY_PING_PONG:
        if (delay_ < 0 || delay_ >= tail) {
          delay_ += speed_;
          speed_ = -speed_;
        }
        break;
      
      case HEAD_CROSSING_POLICY_STUTTER:
        if (delay_ < 0 || delay_ >= tail) {
          delay_ = initial_delay_;
        }
        break;
  
      case HEAD_CROSSING_POLICY_CROSSFADE:
        if (delay_ < 0) {
          delay_ += xfade_tail;
        } else if (delay_ >= tail) {
          delay_ -= xfade_tail;
        }
        break;
    }
    
    const int32_t d = abs(delay_ >= tail / 2 ? delay_ - tail : delay_);
    const float fade_ramp = d < fade_zone ? float(d) / float(fade_zone) : 1.0f;

    int32_t delay_integral = delay_ >> 10;
    float delay_fractional = float(delay_ & 1023) / 1024.0f;
    FloatFrame sample = delay_line->template Read<INTERPOLATION_HERMITE>(
        delay_integral, delay_fractional);
    
    // Crossfade with samples near the tail.
    if (head_crossing_policy == HEAD_CROSSING_POLICY_CROSSFADE) {
      sample *= fade_ramp;
      if (delay_integral < head_crossing_safety_zone) {
        sample += (1.0f - fade_ramp) * \
            delay_line->template Read<INTERPOLATION_HERMITE>(
                delay_integral + (xfade_tail >> 10), delay_fractional);
      } else if (delay_ >= xfade_tail) {
        sample += (1.0f - fade_ramp) * \
            delay_line->template Read<INTERPOLATION_HERMITE>(
                delay_integral - (xfade_tail >> 10), delay_fractional);
      }
    } else {
      if (fade_out) {
        sample *= stmlib::SmoothStep(fade_ramp);
      }
    }
    
    sample *= envelope_.Render() * voice_stealing_envelope_.Render();

    in_out->l += sample.l * gain_[0][0] + sample.r * gain_[0][1];
    in_out->r += sample.l * gain_[1][0] + sample.r * gain_[1][1];

    --remaining_samples_;
    if (done()) {
      remaining_samples_ = 0;
    }
  }
  
 private:
  float gain_[2][2];
  
  int32_t delay_;
  int32_t speed_;
  int32_t initial_delay_;

  DISALLOW_COPY_AND_ASSIGN(DelayGrain);
};

}  // namespace beads

#endif  // BEADS_DSP_DELAY_GRAIN_H_
