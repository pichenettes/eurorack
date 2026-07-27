// Copyright 2022 Emilie Gillet.
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
// Continuously variable waveform: triangle > saw > square. Both square and
// triangle have variable slope / pulse-width.
//
// Taken from Plaits and simplified to remove all unused sync stuff + remove
// interpolation of PW and shape modulation

#ifndef STAGES_VARIABLE_SHAPE_OSCILLATOR_H_
#define STAGES_VARIABLE_SHAPE_OSCILLATOR_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

#include "plaits/dsp/oscillator/oscillator.h"

#include <algorithm>

namespace stages {

class VariableShapeOscillator {
 public:
  VariableShapeOscillator() { }
  ~VariableShapeOscillator() { }

  void Init() {
    phase_ = 0.0f;
    next_sample_ = 0.0f;
    high_ = false;
  }

  void Render(
      float frequency,
      float macro,
      float* out,
      size_t size) {
    float shape = macro * 1.5f;
    CONSTRAIN(shape, 0.0f, 1.0f);

    float pw = 0.5f + (macro - 0.66f) * 1.46f;
    CONSTRAIN(pw, 0.5f, 0.995f);

    Render(frequency, pw, shape, out, size);
  }

  void Render(
      float frequency,
      float pw,
      float waveshape,
      float* out,
      size_t size) {
    if (frequency >= plaits::kMaxFrequency) {
      frequency = plaits::kMaxFrequency;
    }
    
    if (frequency >= 0.25f) {
      pw = 0.5f;
    } else {
      CONSTRAIN(pw, frequency * 2.0f, 1.0f - 2.0f * frequency);
    }
    
    float next_sample = next_sample_;

    const float square_amount = std::max(waveshape - 0.5f, 0.0f) * 2.0f;
    const float triangle_amount = std::max(1.0f - waveshape * 2.0f, 0.0f);
    const float slope_up = 1.0f / (pw);
    const float slope_down = 1.0f / (1.0f - pw);

    while (size--) {
      float this_sample = next_sample;
      next_sample = 0.0f;
      phase_ += frequency;
      if (!high_ && phase_ >= pw) {
        float t = (phase_ - pw) / frequency;
        float triangle_step = (slope_up + slope_down) * frequency;
        triangle_step *= triangle_amount;
        this_sample += square_amount * stmlib::ThisBlepSample(t);
        next_sample += square_amount * stmlib::NextBlepSample(t);
        this_sample -= triangle_step * stmlib::ThisIntegratedBlepSample(t);
        next_sample -= triangle_step * stmlib::NextIntegratedBlepSample(t);
        high_ = true;
      } else if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
        float t = phase_ / frequency;
        float triangle_step = (slope_up + slope_down) * frequency;
        triangle_step *= triangle_amount;

        this_sample -= (1.0f - triangle_amount) * stmlib::ThisBlepSample(t);
        next_sample -= (1.0f - triangle_amount) * stmlib::NextBlepSample(t);
        this_sample += triangle_step * stmlib::ThisIntegratedBlepSample(t);
        next_sample += triangle_step * stmlib::NextIntegratedBlepSample(t);
        high_ = false;
      }
    
      next_sample += ComputeNaiveSample(
          phase_,
          pw,
          slope_up,
          slope_down,
          triangle_amount,
          square_amount);
      *out++ = (2.0f * this_sample - 1.0f);
    }
    next_sample_ = next_sample;
  }
  
 private:
  inline float ComputeNaiveSample(
      float phase,
      float pw,
      float slope_up,
      float slope_down,
      float triangle_amount,
      float square_amount) const {
    float saw = phase;
    float square = phase < pw ? 0.0f : 1.0f;
    float triangle = phase < pw
        ? phase * slope_up
        : 1.0f - (phase - pw) * slope_down;
    saw += (square - saw) * square_amount;
    saw += (triangle - saw) * triangle_amount;
    return saw;
  }

  // Oscillator state.
  float phase_;
  float next_sample_;
  bool high_;

  DISALLOW_COPY_AND_ASSIGN(VariableShapeOscillator);
};
  
}  // namespace stages

#endif  // STAGES_VARIABLE_SHAPE_OSCILLATOR_H_
