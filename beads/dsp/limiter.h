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
// Mono and stereo limiter.

#ifndef BEADS_DSP_LIMITER_H_
#define BEADS_DSP_LIMITER_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"

#include "beads/dsp/frame.h"

namespace beads {

class Limiter {
 public:
  Limiter() { }
  ~Limiter() { }

  void Init(float decay, float max_peak) {
    peak_ = 0.5f;
    peak_lp_ = 0.5f;
    decay_ = decay;
    max_peak_ = max_peak;
  }

  void Process(FloatFrame* in_out, size_t size) {
    while (size--) {
      const float peak = std::min(
          std::max(fabsf(in_out->l), fabsf(in_out->r)),
          40.0f);
      SLOPE(peak_, peak, 0.1f, decay_);
      SLOPE(peak_lp_, peak_, 0.5f, decay_);
      float gain = (peak_lp_ <= max_peak_ ? 1.0f : max_peak_ / peak_lp_);
      in_out->l *= gain;
      in_out->r *= gain;
      ++in_out;
    }
  }

 private:
  float peak_;
  float peak_lp_;
  float decay_;
  float max_peak_;

  DISALLOW_COPY_AND_ASSIGN(Limiter);
};

}  // namespace beads

#endif  // BEADS_DSP_LIMITER_H_
