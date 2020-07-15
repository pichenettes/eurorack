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

#ifndef STAGES_OSCILLATOR_H_
#define STAGES_OSCILLATOR_H_

#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "stages/resources.h"

namespace stages {

enum OscillatorShape {
  OSCILLATOR_SHAPE_IMPULSE_TRAIN,
  OSCILLATOR_SHAPE_SAW,
  OSCILLATOR_SHAPE_SINE,
  OSCILLATOR_SHAPE_TRIANGLE,
  OSCILLATOR_SHAPE_SLOPE,
  OSCILLATOR_SHAPE_SQUARE,
  OSCILLATOR_SHAPE_SQUARE_BRIGHT,
  OSCILLATOR_SHAPE_SQUARE_DARK,
  OSCILLATOR_SHAPE_SQUARE_TRIANGLE,
};

const float kSampleRate = 31250.0f;
const float kMiddleCHz = 261.6255f;
const float kMaxFrequency = 0.25f;
const float kMinFrequency = 0.00001f;
float kScalingGainBasis = 0.66f;
float kScalingCoefficient = 0.78758f;

// TODO - experiment with these, originals below
//float ouroboros_ratios[] = {
//  0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 8.0f
//};
const float kHarmonicRatios[] = {
  0.25f, 0.5f, 0.75f, 1.0f,
  1.5f, 2.0f, 3.0f, 4.0f,
  5.0f, 6.0f, 7.0, 8.0f,
  9.0f, 10.0f, 12.0f, 12.0f,
};

inline float ThisBlepSample(float t) {
  return 0.5f * t * t;
}

inline float NextBlepSample(float t) {
  t = 1.0f - t;
  return -0.5f * t * t;
}

inline float NextIntegratedBlepSample(float t) {
  const float t1 = 0.5f * t;
  const float t2 = t1 * t1;
  const float t4 = t2 * t2;
  return 0.1875f - t1 + 1.5f * t2 - t4;
}

inline float ThisIntegratedBlepSample(float t) {
  return NextIntegratedBlepSample(1.0f - t);
}

// TODO - rename to HarmonicOscillator to signify extended functionality
class Oscillator {
 public:
  Oscillator() { }
  ~Oscillator() { }
  
  void Init() {
    fill(&phase_[0], &phase_[kMaxNumChannels], 0.5f);
    fill(&next_sample_[0], &next_sample_[kMaxNumChannels], 0.0f);
    fill(&lp_state_[0], &lp_state_[kMaxNumChannels], 1.0f);
    fill(&hp_state_[0], &hp_state_[kMaxNumChannels], 0.0f);
    fill(&high_[0], &high_[kMaxNumChannels], true);

    fill(&frequency_[0], &frequency_[kMaxNumChannels], 0.001f);
    fill(&pw_[0], &pw_[kMaxNumChannels], 0.5f);

    fill(&ratio_[0], &ratio_[kMaxNumChannels], 1.0f);
    fill(&amplitude_[0], &amplitude_[kMaxNumChannels], 1.0f);
    fill(&waveshape_[0], &waveshape_[kMaxNumChannels], 0x0);

    fundamental_ = 0.001f;
    num_channels_ = 1;
  }

  void Configure(size_t num_channels, uint8_t* harmosc_waveshapes) {
    if (num_channels_ != num_channels) {
      Init();
    }
    num_channels_ = num_channels;
    copy(&harmosc_waveshapes[0], &harmosc_waveshapes[num_channels], waveshape_);
  }

  void ConfigureSlave(float fundamental, uint8_t waveshape) {
    fundamental_ = fundamental;
    waveshape_[0] = waveshape;
    num_channels_ = 1;
  }

  inline float fundamental() {
    return fundamental_;
  }

  inline uint8_t num_channels() {
    return num_channels_;
  }

  void Render(float* out, size_t size);

  void RenderSingleHarmonic(uint8_t channel_index, float* out, size_t size);

  void RenderSingleHarmonicWaveshape(
      uint8_t channel_index,
      float pw,
      OscillatorShape shape,
      float* out,
      size_t size);

  inline void set_fundamental(float cv_slider_value, float pot_value) {
    const float coarse = (cv_slider_value - 0.5f) * 96.0f;
    const float fine = pot_value * 2.0f - 1.0f;
    fundamental_ = SemitonesToRatio(coarse + fine) * kMiddleCHz / kSampleRate;
  }

  inline void set_amplitude_and_harmonic_ratio(
      int8_t index,
      float cv_slider_value,
      float pot_value) {
    // TODO: I'm pretty sure this should be tied to the number of values in
    // kHarmonicRatios (ignoring the duplicated last one), but would be good
    // to verify for sure
    // const float harmonic = pot_value * 9.999f;
    const float harmonic = pot_value * 14.999f;
    MAKE_INTEGRAL_FRACTIONAL(harmonic);
    harmonic_fractional = 8.0f * (harmonic_fractional - 0.5f) + 0.5f;
    CONSTRAIN(harmonic_fractional, 0.0f, 1.0f);
    // TODO: why are these crossfaded instead of just either/or?
    ratio_[index] = Crossfade(
        kHarmonicRatios[harmonic_integral],
        kHarmonicRatios[harmonic_integral + 1],
        harmonic_fractional);
    amplitude_[index] = std::max(cv_slider_value, 0.0f);
  }

  inline float gain() {
    return kScalingGainBasis * std::pow(kScalingCoefficient, num_channels_ - 1);
  }

 private:
  // Oscillator state.
  float phase_[kMaxNumChannels];
  float next_sample_[kMaxNumChannels];
  float lp_state_[kMaxNumChannels];
  float hp_state_[kMaxNumChannels];
  bool high_[kMaxNumChannels];

  // For interpolation of parameters.
  float frequency_[kMaxNumChannels];
  float pw_[kMaxNumChannels];
  float previous_amplitude_[kMaxNumChannels];

  // Individual harmonic parameters.
  float ratio_[kMaxNumChannels];
  float amplitude_[kMaxNumChannels];
  uint8_t waveshape_[kMaxNumChannels];

  // Overall parameters.
  float fundamental_;
  uint8_t num_channels_;
  
  DISALLOW_COPY_AND_ASSIGN(Oscillator);
};

}  // namespace stages

#endif  // STAGES_OSCILLATOR_H_
