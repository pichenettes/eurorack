// Copyright 2015 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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
// Limiter.

#ifndef RINGS_DSP_LIMITER_H_
#define RINGS_DSP_LIMITER_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"

namespace rings {

class Limiter {
 public:
  Limiter() { }
  ~Limiter() { }

  void Init() {
    peak_ = 0.5f;
  }

  void Process(
      float* l,
      float* r,
      size_t size,
      float pre_gain) {
    while (size--) {
      float l_pre = *l * pre_gain;
      float r_pre = *r * pre_gain;
    
      float l_peak = fabs(l_pre);
      float r_peak = fabs(r_pre);
      float s_peak = fabs(r_pre - l_pre);

      float peak = std::max(std::max(l_peak, r_peak), s_peak);
      SLOPE(peak_, peak, 0.05f, 0.00002f);

      // Clamp to 8Vpp, clipping softly towards 10Vpp
      float gain = (peak_ <= 1.0f ? 1.0f : 1.0f / peak_);
      *l++ = stmlib::SoftLimit(l_pre * gain * 0.8f);
      *r++ = stmlib::SoftLimit(r_pre * gain * 0.8f);
    }
  }

 private:
  float peak_;

  DISALLOW_COPY_AND_ASSIGN(Limiter);
};

}  // namespace rings

#endif  // RINGS_DSP_LIMITER_H_
