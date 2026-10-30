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
// DX7-compatible LFO.

#ifndef PLAITS_DSP_FM_LFO_H_
#define PLAITS_DSP_FM_LFO_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/random.h"

#include "plaits/dsp/fm/dx_units.h"
#include "plaits/dsp/fm/patch.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

namespace fm {

class Lfo {
 public:
  Lfo() { };
  ~Lfo() { };
  
  enum Waveform {
    WAVEFORM_TRIANGLE,
    WAVEFORM_RAMP_DOWN,
    WAVEFORM_RAMP_UP,
    WAVEFORM_SQUARE,
    WAVEFORM_SINE,
    WAVEFORM_S_AND_H
  };
  
  inline void Init(float sample_rate) {
    phase_ = 0.0f;
    frequency_ = 0.1f;
    delay_phase_ = 0.0f;
    delay_increment_[0] = delay_increment_[1] = 0.1f;
    random_value_ = value_ = 0.0f;
    
    one_hz_ = 1.0f / sample_rate;

    amp_mod_depth_ = 0.0f;
    pitch_mod_depth_ = 0.0f;
    
    waveform_ = WAVEFORM_TRIANGLE;
    reset_phase_ = false;
    
    phase_integral_ = 0;
  }
  
  inline void Set(const Patch::ModulationParameters& modulations) {
    frequency_ = LFOFrequency(modulations.rate) * one_hz_;

    LFODelay(modulations.delay, delay_increment_);
    delay_increment_[0] *= one_hz_;
    delay_increment_[1] *= one_hz_;
    
    waveform_ = Waveform(modulations.waveform);
    reset_phase_ = modulations.reset_phase != 0;
    
    amp_mod_depth_ = float(modulations.amp_mod_depth) * 0.01f;

    pitch_mod_depth_ = float(modulations.pitch_mod_depth) * 0.01f * \
        PitchModSensitivity(modulations.pitch_mod_sensitivity);
  }
  
  inline void Reset() {
    if (reset_phase_) {
      phase_ = 0.0f;
    }
    delay_phase_ = 0.0f;
  }
  
  inline void Step(float scale) {
    phase_ += scale * frequency_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      random_value_ = stmlib::Random::GetFloat();
    }
    value_ = value();
    
    delay_phase_ += scale * delay_increment_[(delay_phase_ < 0.5f) ? 0 : 1];
    if (delay_phase_ >= 1.0f) {
      delay_phase_ = 1.0f;
    }
  }
  
  inline void Scrub(float sample) {
    float phase = sample * frequency_;
    MAKE_INTEGRAL_FRACTIONAL(phase)
    phase_ = phase_fractional;
    if (phase_integral != phase_integral_) {
      phase_integral_ = phase_integral;
      random_value_ = stmlib::Random::GetFloat();
    }
    value_ = value();
    
    delay_phase_ = sample * delay_increment_[0];
    if (delay_phase_ > 0.5f) {
      sample -= 0.5f / delay_increment_[0];
      delay_phase_ = 0.5f + sample * delay_increment_[1];
      if (delay_phase_ >= 1.0f) {
        delay_phase_ = 1.0f;
      }
    }
  }
  
  inline float value() const {
    switch (waveform_) {
      case WAVEFORM_TRIANGLE:
        return 2.0f * (phase_ < 0.5f ? 0.5f - phase_ : phase_ - 0.5f);

      case WAVEFORM_RAMP_DOWN:
        return 1.0f - phase_;

      case WAVEFORM_RAMP_UP:
        return phase_;

      case WAVEFORM_SQUARE:
        return phase_ < 0.5f ? 0.0f : 1.0f;

      case WAVEFORM_SINE:
        return 0.5f + 0.5f * Sine(phase_ + 0.5f);

      case WAVEFORM_S_AND_H:
        return random_value_;
    }
    return 0.0f;
  }
  
  inline float delay_ramp() const {
    return delay_phase_ < 0.5f ? 0.0f : (delay_phase_ - 0.5f) * 2.0f;
  }
  
  inline float pitch_mod() const {
    return (value_ - 0.5f) * delay_ramp() * pitch_mod_depth_;
  }
  
  inline float amp_mod() const {
    return (1.0f - value_) * delay_ramp() * amp_mod_depth_;
  }
  
 private:
  float phase_;
  float frequency_;  
  float delay_phase_;
  float delay_increment_[2];
  float value_;
  
  float random_value_;
  float one_hz_;
  
  float amp_mod_depth_;
  float pitch_mod_depth_;
  
  Waveform waveform_;
  bool reset_phase_;
  
  int phase_integral_;
  
  DISALLOW_COPY_AND_ASSIGN(Lfo);
};

}  // namespace fm
  
}  // namespace plaits

#endif  // PLAITS_DSP_FM_LFO_H_
