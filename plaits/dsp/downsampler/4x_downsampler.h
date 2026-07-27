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
// FIR Downsampler.

#ifndef PLAITS_DSP_DOWNSAMPLER_4X_DOWNSAMPLER_H_
#define PLAITS_DSP_DOWNSAMPLER_4X_DOWNSAMPLER_H_

#include "stmlib/stmlib.h"

#include "plaits/resources.h"

namespace plaits {
  
const size_t kOversampling = 4;

class Downsampler {
 public:
  Downsampler(float* state) {
    head_ = *state;
    tail_ = 0.0f;
    state_ = state;
  }
  ~Downsampler() {
    *state_ = head_;
  }
  inline void Accumulate(int i, float sample) {
    head_ += sample * lut_4x_downsampler_fir[3 - (i & 3)];
    tail_ += sample * lut_4x_downsampler_fir[i & 3];
  }

  inline float Read() {
    float value = head_;
    head_ = tail_;
    tail_ = 0.0f;
    return value;
  }
 private:
  float head_;
  float tail_;
  float* state_;

  DISALLOW_COPY_AND_ASSIGN(Downsampler);
};

}  // namespace plaits

#endif  // PLAITS_DSP_DOWNSAMPLER_4X_DOWNSAMPLER_H_