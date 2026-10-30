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
// Dumb sine oscillator, used for various internal modulations.

#ifndef BEADS_DSP_SINE_OSCILLATOR_H_
#define BEADS_DSP_SINE_OSCILLATOR_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"

#include "beads/resources.h"

namespace beads {

class SineOscillator {
 public:
  SineOscillator() { }
  ~SineOscillator() { }
  
  void Init() {
    Init(0.0f);
  }
  
  void Init(float frequency) {
    phase_ = 0.0f;
    frequency_ = frequency;
  }
  
  inline void set_frequency(float frequency) {
    frequency_ = frequency;
  }
  
  inline float phase() const { return phase_; }
  
  inline float Render() {
    phase_ += frequency_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    return 0.5f + 0.5f * stmlib::Interpolate(lut_sine, phase_, 1024.0f);
  }
  
  inline float Render(float frequency) {
    set_frequency(frequency);
    return Render();
  }
    
 private:
  float phase_;
  float frequency_;
  
  DISALLOW_COPY_AND_ASSIGN(SineOscillator);
};

}  // namespace beads

#endif  // BEADS_DSP_SINE_OSCILLATOR_H_
