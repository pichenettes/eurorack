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
// Wavetable grain with infinite duration.

#ifndef BEADS_DSP_WAVETABLE_VOICE_H_
#define BEADS_DSP_WAVETABLE_VOICE_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "stmlib/utils/random.h"

#include "beads/dsp/grain.h"

namespace beads {

const float kWavetableRootNote = 130.81f;

class WavetableVoice {
 public:
  WavetableVoice() { }
  ~WavetableVoice() { }
  
  void Init(float sample_rate) {
    phase_ = 0.0f;
    root_note_ = kWavetableRootNote / sample_rate;

    position_random_walk_.Init(1.01f / sample_rate);
    lp_ = lp_2_ = previous_ = 0.0f;
    frequency_ = root_note_;
    wave_lp_ = 0.0f;

    bank_ = next_bank_ = 0;

    xfade_counter_ = 0;
    // 25ms xfade when switching bank.
    xfade_duration_ = int(sample_rate * 0.025f);
  }
  
  inline void Render(
      int bank,
      int bit_shift,
      const bool* freeze,
      const Parameters& p,
      FloatFrame* out,
      size_t size) {
    const float scale_up = float(1 << bit_shift);
    const float scale_down = 1.0f / scale_up;
    const float gain = scale_down * 2.0f / float(xfade_duration_);
    
    if (bank != bank_ && !xfade_counter_) {
      // Initiate a crossfade to the new bank when the requested bank is
      // different from the current one.
      next_bank_ = bank;
      xfade_counter_ = xfade_duration_;
    }
    
    stmlib::ParameterInterpolator fm(
        &frequency_,
        std::min(stmlib::SemitonesToRatio(p.pitch.base) * root_note_, 0.25f),
        size);
    
    while (size--) {
      ONE_POLE(
          wave_lp_,
          p.time.value(&position_random_walk_, *freeze ? 0 : 1, 1.0f, true),
          0.025f);
      const float wave = wave_lp_ * float(kNumWavesPerTable - 1);
      const float frequency = fm.Next();
      
      phase_ += frequency;
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      }
      
      if (xfade_counter_) {
        --xfade_counter_;
        if (xfade_counter_ == xfade_duration_ / 2) {
          bank_ = next_bank_;
        }
      }
      
      MAKE_INTEGRAL_FRACTIONAL(wave);
      
      const int16_t* wave_start = wav_integrated_waves + \
          (wave_integral + bank_ * kNumWavesPerTable) * (kTableSize + 4);
      
      const float p = phase_ * float(kTableSize);
      MAKE_INTEGRAL_FRACTIONAL(p);
      const float x0 = InterpolateWaveHermite(
          wave_start, p_integral, p_fractional);
      const float x1 = InterpolateWaveHermite(
          wave_start + kTableSize + 4, p_integral, p_fractional);
      
      const float sample = x0 + (x1 - x0) * wave_fractional;
      
      const float scale = 1.0f / (frequency * 131072.0f);
      const float cutoff = std::min(float(kTableSize) * frequency, 1.0f);
      
      ONE_POLE(lp_, (sample - previous_) * scale, cutoff);
      ONE_POLE(lp_2_, lp_, cutoff);
      previous_ = sample;

      float out_sample = lp_2_;
      
      const float xfade_gain = fabsf(
          float(xfade_counter_ - (xfade_duration_ / 2)));
      
      out_sample = float(int(out_sample * scale_up)) * xfade_gain * gain;
      CONSTRAIN(out_sample, -1.0f, +1.0f)
      out->l += out_sample;
      out->r += out_sample;
      
      ++out;
      ++freeze;
    }
  }
  
 private:
  float frequency_;
  float phase_;
  float root_note_;
  
  float lp_;
  float lp_2_;
  float previous_;
  float wave_lp_;
  
  int bank_;
  int next_bank_;
  int xfade_counter_;
  int xfade_duration_;
  
  RandomWalkModulation position_random_walk_;
  
  DISALLOW_COPY_AND_ASSIGN(WavetableVoice);
};

}  // namespace beads

#endif  // BEADS_DSP_WAVETABLE_VOICE_H_
