// Copyright 2016 Emilie Gillet.
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
// Triangle waveform approximated by 16 discrete steps.

#ifndef PLAITS_DSP_OSCILLATOR_NES_TRIANGLE_OSCILLATOR_H_
#define PLAITS_DSP_OSCILLATOR_NES_TRIANGLE_OSCILLATOR_H_

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "plaits/dsp/oscillator/wavetable_oscillator.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

#include "plaits/resources.h"

namespace plaits {

template<int num_bits=5>
class NESTriangleOscillator {
 public:
  NESTriangleOscillator() { }
  ~NESTriangleOscillator() { }
  
  inline void Init() {
    phase_ = 0.0f;
    step_ = 0;
    ascending_ = true;
    next_sample_ = 0.0f;
    frequency_ = 0.001f;
  }
  
  inline void Render(float frequency, float* out, size_t size) {
    // Compute all constants needed to scale the waveform and its
    // discontinuities.
    const int num_steps = 1 << num_bits;
    const int half = num_steps / 2;
    const int top = num_steps != 2 ? num_steps - 1 : 2;
    const float num_steps_f = static_cast<float>(num_steps);
    const float scale = num_steps != 2
        ? 4.0f / static_cast<float>(top - 1)
        : 2.0f;
    
    frequency = std::min(frequency, 0.25f);
    
    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    
    float next_sample = next_sample_;
    while (size--) {
      const float frequency = fm.Next();
      phase_ += frequency;
      
      // Compute the point at which we transition between the "full resolution"
      // NES triangle, and a naive band-limited triangle.
      float fade_to_tri = (frequency - 0.5f / num_steps_f) * 2.0f * num_steps_f;
      CONSTRAIN(fade_to_tri, 0.0f, 1.0f)
      
      const float nes_gain = 1.0f - fade_to_tri;
      const float tri_gain = fade_to_tri * 2.0f / scale;
      
      float this_sample = next_sample;
      next_sample = 0.0f;
      
      // Handle the discontinuity at the top of the naive triangle.
      if (ascending_ && phase_ >= 0.5f) {
        float discontinuity = 4.0f * frequency * tri_gain;
        if (discontinuity) {
          float t = (phase_ - 0.5f) / frequency;
          this_sample -= stmlib::ThisIntegratedBlepSample(t) * discontinuity;
          next_sample -= stmlib::NextIntegratedBlepSample(t) * discontinuity;
        }
        ascending_ = false;
      }

      int next_step = static_cast<int>(phase_ * num_steps_f);
      if (next_step != step_) {
        bool wrap = false;
        if (next_step >= num_steps) {
          phase_ -= 1.0f;
          next_step -= num_steps;
          wrap = true;
        }
        
        float discontinuity = next_step < half ? 1.0f : -1.0f;
        if (num_steps == 2) {
          discontinuity = -discontinuity;
        } else {
          if (next_step == 0 || next_step == half) {
            discontinuity = 0.0f;
          }
        }

        // Handle the discontinuity at each step of the NES triangle.
        discontinuity *= nes_gain;
        if (discontinuity) {
          float frac = (phase_ * num_steps_f - static_cast<float>(next_step));
          float t = frac / (frequency * num_steps_f);
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
        }
        
        // Handle the discontinuity at the bottom of the naive triangle.
        if (wrap) {
          float discontinuity = 4.0f * frequency * tri_gain;
          if (discontinuity) {
            float t = phase_ / frequency;
            this_sample += stmlib::ThisIntegratedBlepSample(t) * discontinuity;
            next_sample += stmlib::NextIntegratedBlepSample(t) * discontinuity;
          }
          ascending_ = true;
        }
      }
      step_ = next_step;
      
      // Contribution from NES triangle.
      next_sample += nes_gain * \
          static_cast<float>(step_ < half ? step_ : top - step_);

      // Contribution from naive triangle.
      next_sample += tri_gain * \
          (phase_ < 0.5f ? 2.0f * phase_ : 2.0f - 2.0f * phase_);
      
      *out++ = this_sample * scale - 1.0f;
    }
    next_sample_ = next_sample;
  }
 
 private:
  // Oscillator state.
  float phase_;
  float next_sample_;
  int step_;
  bool ascending_;

  // For interpolation of parameters.
  float frequency_;

  DISALLOW_COPY_AND_ASSIGN(NESTriangleOscillator);
};
  
}  // namespace plaits

#endif  // PLAITS_DSP_OSCILLATOR_NES_TRIANGLE_OSCILLATOR_H_
