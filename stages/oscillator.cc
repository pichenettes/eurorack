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
// Harmonic oscillator.

#include "stages/oscillator.h"

namespace stages {

using namespace std;

void Oscillator::Render(float* out, size_t size) {
  std::fill(&out[0], &out[size], 0.0f);

  for (uint8_t channel_index = 0; channel_index < num_channels_; ++channel_index) {
    float this_channel[size];
    RenderSingleHarmonic(channel_index, &this_channel[0], size);
    ParameterInterpolator am(
        &previous_amplitude_[channel_index],
        amplitude_[channel_index] * amplitude_[channel_index],
        size);

    for (uint8_t i = 0; i < size; ++i) {
      out[i] += this_channel[i] * am.Next() * gain();
    }
  }
}

void Oscillator::RenderSingleHarmonic(
    uint8_t channel_index,
    float* out,
    size_t size) {
  switch (waveshape_[channel_index]) {
    case 0:
      RenderSingleHarmonicWaveshape(channel_index, 0.5f, OSCILLATOR_SHAPE_SINE, out, size);
      break;
    case 1:
      RenderSingleHarmonicWaveshape(channel_index, 0.5f, OSCILLATOR_SHAPE_TRIANGLE, out, size);
      break;
    case 2:
    case 3:
      RenderSingleHarmonicWaveshape(channel_index, 0.5f, OSCILLATOR_SHAPE_SQUARE, out, size);
      break;
    case 4:
      RenderSingleHarmonicWaveshape(channel_index, 0.5f, OSCILLATOR_SHAPE_SAW, out, size);
      break;
    case 5:
      RenderSingleHarmonicWaveshape(channel_index, 0.75f, OSCILLATOR_SHAPE_SQUARE, out, size);
      break;
    case 6:
    case 7:
      RenderSingleHarmonicWaveshape(channel_index, 0.9f, OSCILLATOR_SHAPE_SQUARE, out, size);
      break;
  }
}

void Oscillator::RenderSingleHarmonicWaveshape(
    uint8_t channel_index,
    float pw,
    OscillatorShape shape,
    float* out,
    size_t size) {
  float frequency = fundamental_ * ratio_[channel_index];
  CONSTRAIN(frequency, kMinFrequency, kMaxFrequency);
  CONSTRAIN(pw, fabsf(frequency) * 2.0f, 1.0f - 2.0f * fabsf(frequency))

  stmlib::ParameterInterpolator fm(&frequency_[channel_index], frequency, size);
  stmlib::ParameterInterpolator pwm(&pw_[channel_index], pw, size);

  float next_sample = next_sample_[channel_index];

  while (size--) {
    float this_sample = next_sample;
    next_sample = 0.0f;

    float frequency = fm.Next();
    float pw = (shape == OSCILLATOR_SHAPE_SQUARE_TRIANGLE ||
                shape == OSCILLATOR_SHAPE_TRIANGLE) ? 0.5f : pwm.Next();
    phase_[channel_index] += frequency;

    if (shape <= OSCILLATOR_SHAPE_SAW) {
      if (phase_[channel_index] >= 1.0f) {
        phase_[channel_index] -= 1.0f;
        float t = phase_[channel_index] / frequency;
        this_sample -= ThisBlepSample(t);
        next_sample -= NextBlepSample(t);
      }
      next_sample += phase_[channel_index];

      if (shape == OSCILLATOR_SHAPE_SAW) {
        *out++ = 2.0f * this_sample - 1.0f;
      } else {
        lp_state_[channel_index] += 0.25f *
            ((hp_state_[channel_index] - this_sample) - lp_state_[channel_index]);
        *out++ = 4.0f * lp_state_[channel_index];
        hp_state_[channel_index] = this_sample;
      }
    } else if (shape == OSCILLATOR_SHAPE_SINE) {
      if (phase_[channel_index] >= 1.0f) {
        phase_[channel_index] -= 1.0f;
      }
      next_sample = stmlib::Interpolate(lut_sine, phase_[channel_index], 1024.0f);
      *out++ = this_sample;
    } else if (shape <= OSCILLATOR_SHAPE_SLOPE) {
      float slope_up = 2.0f;
      float slope_down = 2.0f;
      if (shape == OSCILLATOR_SHAPE_SLOPE) {
        slope_up = 1.0f / (pw);
        slope_down = 1.0f / (1.0f - pw);
      }
      if (high_[channel_index] ^ (phase_[channel_index] < pw)) {
        float t = (phase_[channel_index] - pw) / frequency;
        float discontinuity = (slope_up + slope_down) * frequency;
        this_sample -= ThisIntegratedBlepSample(t) * discontinuity;
        next_sample -= NextIntegratedBlepSample(t) * discontinuity;
        high_[channel_index] = phase_[channel_index] < pw;
      }
      if (phase_[channel_index] >= 1.0f) {
        phase_[channel_index] -= 1.0f;
        float t = phase_[channel_index] / frequency;
        float discontinuity = (slope_up + slope_down) * frequency;
        this_sample += ThisIntegratedBlepSample(t) * discontinuity;
        next_sample += NextIntegratedBlepSample(t) * discontinuity;
        high_[channel_index] = true;
      }
      next_sample += high_[channel_index]
        ? phase_[channel_index] * slope_up
        : 1.0f - (phase_[channel_index] - pw) * slope_down;
      *out++ = 2.0f * this_sample - 1.0f;
    } else {
      if (high_[channel_index] ^ (phase_[channel_index] >= pw)) {
        float t = (phase_[channel_index] - pw) / frequency;
        float discontinuity = 1.0f;
        this_sample += ThisBlepSample(t) * discontinuity;
        next_sample += NextBlepSample(t) * discontinuity;
        high_[channel_index] = phase_[channel_index] >= pw;
      }
      if (phase_[channel_index] >= 1.0f) {
        phase_[channel_index] -= 1.0f;
        float t = phase_[channel_index] / frequency;
        this_sample -= ThisBlepSample(t);
        next_sample -= NextBlepSample(t);
        high_[channel_index] = false;
      }
      next_sample += phase_[channel_index] < pw ? 0.0f : 1.0f;

      if (shape == OSCILLATOR_SHAPE_SQUARE_TRIANGLE) {
        const float integrator_coefficient = frequency * 0.0625f;
        this_sample = 128.0f * (this_sample - 0.5f);
        lp_state_[channel_index] += integrator_coefficient *
            (this_sample - lp_state_[channel_index]);
        *out++ = lp_state_[channel_index];
      } else if (shape == OSCILLATOR_SHAPE_SQUARE_DARK) {
        const float integrator_coefficient = frequency * 2.0f;
        this_sample = 4.0f * (this_sample - 0.5f);
        lp_state_[channel_index] += integrator_coefficient *
            (this_sample - lp_state_[channel_index]);
        *out++ = lp_state_[channel_index];
      } else if (shape == OSCILLATOR_SHAPE_SQUARE_BRIGHT) {
        const float integrator_coefficient = frequency * 2.0f;
        this_sample = 2.0f * this_sample - 1.0f;
        lp_state_[channel_index] += integrator_coefficient *
            (this_sample - lp_state_[channel_index]);
        *out++ = (this_sample - lp_state_[channel_index]) * 0.5f;
      } else {
        this_sample = 2.0f * this_sample - 1.0f;
        *out++ = this_sample;
      }
    }
  }
  next_sample_[channel_index] = next_sample;
}

}  // namespace stages
