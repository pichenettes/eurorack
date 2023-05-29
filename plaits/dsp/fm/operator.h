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
// FM Operator.

#ifndef PLAITS_DSP_FM_OPERATOR_H_
#define PLAITS_DSP_FM_OPERATOR_H_

#include <algorithm>

#include "plaits/dsp/oscillator/sine_oscillator.h"

#include "stmlib/dsp/dsp.h"

namespace plaits {

namespace fm {

struct Operator {
  enum ModulationSource {
    MODULATION_SOURCE_EXTERNAL = -2,
    MODULATION_SOURCE_NONE = -1,
    MODULATION_SOURCE_FEEDBACK = 0
  };

  inline void Reset() {
    phase = 0;
    amplitude = 0.0f;
  }
  
  uint32_t phase;
  float amplitude;
};

typedef void (*RenderFn)(
    Operator* ops,
    const float* f,
    const float* a,
    float* fb_state,
    int fb_amount,
    const float* modulation,
    float* out,
    size_t size);

template<int n, int modulation_source, bool additive>
void RenderOperators(
    Operator* ops,
    const float* f,
    const float* a,
    float* fb_state,
    int fb_amount,
    const float* modulation,
    float* out,
    size_t size) {
  float previous_0, previous_1;
  
  if (modulation_source >= Operator::MODULATION_SOURCE_FEEDBACK) {
    previous_0 = fb_state[0];
    previous_1 = fb_state[1];
  }

  uint32_t frequency[n];
  uint32_t phase[n];
  float amplitude[n];
  float amplitude_increment[n];

  const float scale = 1.0f / float(size);
  for (int i = 0; i < n; ++i) {
    frequency[i] = static_cast<uint32_t>(std::min(f[i], 0.5f) * 4294967296.0f);
    phase[i] = ops[i].phase;
    amplitude[i] = ops[i].amplitude;
    amplitude_increment[i] = (std::min(a[i], 4.0f) - amplitude[i]) * scale;
  }
  
  const float fb_scale = fb_amount ? float(1 << fb_amount) / 512.0f : 0.0f;

  while (size--) {
    float pm = 0.0f;
    if (modulation_source >= Operator::MODULATION_SOURCE_FEEDBACK) {
      pm = (previous_0 + previous_1) * fb_scale;
    } else if (modulation_source == Operator::MODULATION_SOURCE_EXTERNAL) {
      pm = *modulation++;
    }
    for (int i = 0; i < n; ++i) {
      phase[i] += frequency[i];
      pm = SinePM(phase[i], pm) * amplitude[i];
      amplitude[i] += amplitude_increment[i];
      if (i == modulation_source) {
        previous_1 = previous_0;
        previous_0 = pm;
      }
    }
    if (additive) {
      *out++ += pm;
    } else {
      *out++ = pm;
    }
  }
  
  for (int i = 0; i < n; ++i) {
    ops[i].phase = phase[i];
    ops[i].amplitude = amplitude[i];
  }
  
  if (modulation_source >= Operator::MODULATION_SOURCE_FEEDBACK) {
    fb_state[0] = previous_0;
    fb_state[1] = previous_1;
  }
};

}  // namespace fm
  
}  // namespace plaits

#endif  // PLAITS_DSP_FM_OPERATOR_H_
