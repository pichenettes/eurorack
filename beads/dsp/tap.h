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
// Delay tap (delay + pitch-shifting).

#ifndef BEADS_DSP_TAP_H_
#define BEADS_DSP_TAP_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"

#include "beads/dsp/delay_line.h"
#include "beads/dsp/frame.h"

namespace beads {

template<int32_t window_size>
class Tap {
 public:
  Tap() { }
  ~Tap() { }
  
  void Init() {
    phase_ = 0;
  }
  
  template<typename DelayLineType>
  inline void Render(
      const DelayLineType* delay_line,
      int32_t delay,
      float gain,
      float speed,
      FloatFrame* out) {
    if (delay == 0) {
      return;
    }
    if (window_size == 1) {
      *out += gain * delay_line->template Read<INTERPOLATION_HERMITE>(delay);
      return;
    }
    
    const int32_t full_window_size = 1024 * window_size;
    const int32_t half_window_size = full_window_size >> 1;
    
    phase_ += static_cast<int32_t>(int(1024.0f * (speed - 1.0f)));
    if (phase_ >= full_window_size) {
      phase_ -= full_window_size;
    } else if (phase_ < 0) {
      phase_ += full_window_size;
    }
    
    delay = std::max(full_window_size + 1, delay + half_window_size);
    
    const float phase = static_cast<float>(phase_) / float(full_window_size);
    const float triangle = phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
    
    const FloatFrame head_1 = delay_line->template Read<INTERPOLATION_HERMITE>(
          -phase_ + delay);
    const FloatFrame head_2 = delay_line->template Read<INTERPOLATION_HERMITE>(
          -((phase_ + half_window_size) % full_window_size) + delay);
    
    *out += (head_2 + (head_1 - head_2) * triangle) * gain;
  }
  
 private:
  int32_t phase_;
  
  DISALLOW_COPY_AND_ASSIGN(Tap);
};

}  // namespace beads

#endif  // BEADS_DSP_TAP_H_
