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
// Wavetable grain.

#ifndef BEADS_DSP_WAVETABLE_GRAIN_H_
#define BEADS_DSP_WAVETABLE_GRAIN_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/random.h"

#include "beads/dsp/grain.h"

namespace beads {

const size_t kTableSize = 256;
const size_t kNumWavesPerTable = 16;

inline float InterpolateWaveHermite(
    const int16_t* table,
    int32_t index_integral,
    float index_fractional) {
  const float xm1 = table[index_integral];
  const float x0 = table[index_integral + 1];
  const float x1 = table[index_integral + 2];
  const float x2 = table[index_integral + 3];
  const float c = (x1 - xm1) * 0.5f;
  const float v = x0 - x1;
  const float w = c + v;
  const float a = w + v + (x2 - x0) * 0.5f;
  const float b_neg = w + a;
  const float f = index_fractional;
  return (((a * f) - b_neg) * f + c) * f + x0;
}

class Differentiator {
 public:
  Differentiator() { }
  ~Differentiator() { }

  void Init(float value, float scale, float cutoff) {
    previous_ = value;
    lp_ = 0.0f;
    lp_2_ = 0.0f;
    scale_ = scale;
    cutoff_ = cutoff;
  }
  
  float Process(float s) {
    ONE_POLE(lp_, (s - previous_) * scale_, cutoff_);
    ONE_POLE(lp_2_, lp_, cutoff_);
    previous_ = s;
    return lp_2_;
  }
 private:
  float previous_;
  float lp_;
  float lp_2_;
  float cutoff_;
  float scale_;

  DISALLOW_COPY_AND_ASSIGN(Differentiator);
};

class WavetableGrain : public Grain {
 public:
  WavetableGrain() { }
  ~WavetableGrain() { }
  
  void Init(float sample_rate) {
    phase_ = 0.0f;
    frequency_ = 0.0f;
    envelope_sample_ = 0.0f;
    scale_up_ = 256.0f;
    scale_down_ = 1.0f / 256.0f;

    wave_integral_ = 0;
    wave_fractional_ = 0.0f;
    gain_l_ = 0.0f;
    gain_r_ = 0.0f;

    envelope_.Init(sample_rate);
    voice_stealing_envelope_.Init();
    differentiator_.Init(0.0f, 0.0f, 0.0f);

    remaining_samples_ = 0;
  }
  
  inline void Start(
      int bank,
      int bit_shift,
      float wave,
      float frequency,
      float duration,
      float shape,
      float gain,
      float pan,
      float fractional_sample) {
    wave = wave * float(kNumWavesPerTable - 1);
    MAKE_INTEGRAL_FRACTIONAL(wave);

    wave_integral_ = wave_integral;
    wave_fractional_ = wave_fractional;
    wave_start_ = wav_integrated_waves + \
        (bank * kNumWavesPerTable + wave_integral_) * (kTableSize + 4);
    
    frequency = std::min(frequency, 0.25f);

    remaining_samples_ = int(duration);
    envelope_.Start(duration, shape, fractional_sample);
    
    gain_l_ = gain * (1.0f - pan) * (1.0f + pan);
    gain_r_ = gain * pan * (2.0f - pan);
    
    voice_stealing_envelope_.ResetAndPause();
    
    scale_up_ = float(1 << bit_shift);
    scale_down_ = 1.0f / scale_up_;
    
    phase_ = fractional_sample * frequency;
    frequency_ = frequency;
    differentiator_.Init(
        ReadSample(phase_),
        1.0f / (frequency * 131072.0f),
        std::min(float(kTableSize) * frequency, 1.0f));
  }
  
  inline float ReadSample(float phase) const {
    const float p = phase * float(kTableSize);
    MAKE_INTEGRAL_FRACTIONAL(p);

    const float x0 = InterpolateWaveHermite(
        wave_start_, p_integral, p_fractional);
    const float x1 = InterpolateWaveHermite(
        wave_start_ + kTableSize + 4, p_integral, p_fractional);
    
    return x0 + (x1 - x0) * wave_fractional_;
  }

  inline void Render(FloatFrame* in_out) {
    Render(in_out, false);
  }
  
  inline void Render(FloatFrame* in_out, bool freeze) {
    if (done()) {
      return;
    }
    
    phase_ += frequency_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    
    float sample = differentiator_.Process(ReadSample(phase_));
    sample = float(int(sample * scale_up_)) * scale_down_;
    
    if (!freeze) {
      envelope_sample_ = envelope_.Render() * voice_stealing_envelope_.Render();
      --remaining_samples_;
      if (done()) {
        remaining_samples_ = 0;
      }
    }
    
    sample *= envelope_sample_;

    in_out->l += sample * gain_l_;
    in_out->r += sample * gain_r_;
  }
  
 private:
  float phase_;
  float frequency_;
  float envelope_sample_;
  float scale_up_;
  float scale_down_;
  float gain_l_;
  float gain_r_;
  
  int wave_integral_;
  float wave_fractional_;
  const int16_t* wave_start_;
  
  Differentiator differentiator_;
    
  DISALLOW_COPY_AND_ASSIGN(WavetableGrain);
};

}  // namespace beads

#endif  // BEADS_DSP_WAVETABLE_GRAIN_H_
