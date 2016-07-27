// Copyright 2014 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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
// Ensemble FX.

#ifndef RINGS_DSP_FX_ENSEMBLE_H_
#define RINGS_DSP_FX_ENSEMBLE_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"

#include "rings/dsp/fx/fx_engine.h"
#include "rings/resources.h"

namespace rings {

class Ensemble {
 public:
  Ensemble() { }
  ~Ensemble() { }
  
  void Init(uint16_t* buffer) {
    engine_.Init(buffer);
    phase_1_ = 0;
    phase_2_ = 0;
  }
  
  void Process(float* left, float* right, size_t size) {
    typedef E::Reserve<2047, E::Reserve<2047> > Memory;
    E::DelayLine<Memory, 0> line_l;
    E::DelayLine<Memory, 1> line_r;
    E::Context c;
    
    while (size--) {
      engine_.Start(&c);
      float dry_amount = 1.0f - amount_ * 0.5f;
    
      // Update LFO.
      phase_1_ += 1.57e-05f;
      if (phase_1_ >= 1.0f) {
        phase_1_ -= 1.0f;
      }
      phase_2_ += 1.37e-04f;
      if (phase_2_ >= 1.0f) {
        phase_2_ -= 1.0f;
      }
      int32_t phi_1 = (phase_1_ * 4096.0f);
      float slow_0 = lut_sine[phi_1 & 4095];
      float slow_120 = lut_sine[(phi_1 + 1365) & 4095];
      float slow_240 = lut_sine[(phi_1 + 2730) & 4095];
      int32_t phi_2 = (phase_2_ * 4096.0f);
      float fast_0 = lut_sine[phi_2 & 4095];
      float fast_120 = lut_sine[(phi_2 + 1365) & 4095];
      float fast_240 = lut_sine[(phi_2 + 2730) & 4095];
      
      float a = depth_ * 1.0f;
      float b = depth_ * 0.1f;
      
      float mod_1 = slow_0 * a + fast_0 * b;
      float mod_2 = slow_120 * a + fast_120 * b;
      float mod_3 = slow_240 * a + fast_240 * b;
    
      float wet = 0.0f;
    
      // Sum L & R channel to send to chorus line.
      c.Read(*left, 1.0f);
      c.Write(line_l, 0.0f);
      c.Read(*right, 1.0f);
      c.Write(line_r, 0.0f);
    
      c.Interpolate(line_l, mod_1 + 1024, 0.33f);
      c.Interpolate(line_l, mod_2 + 1024, 0.33f);
      c.Interpolate(line_r, mod_3 + 1024, 0.33f);
      c.Write(wet, 0.0f);
      *left = wet * amount_ + *left * dry_amount;
      
      c.Interpolate(line_r, mod_1 + 1024, 0.33f);
      c.Interpolate(line_r, mod_2 + 1024, 0.33f);
      c.Interpolate(line_l, mod_3 + 1024, 0.33f);
      c.Write(wet, 0.0f);
      *right = wet * amount_ + *right * dry_amount;
      left++;
      right++;
    }
  }
  
  inline void set_amount(float amount) {
    amount_ = amount;
  }
  
  inline void set_depth(float depth) {
    depth_ = depth * 128.0f;
  }
  
 private:
  typedef FxEngine<4096, FORMAT_16_BIT> E;
  E engine_;
  
  float amount_;
  float depth_;
  
  float phase_1_;
  float phase_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Ensemble);
};

}  // namespace rings

#endif  // RINGS_DSP_FX_ENSEMBLE_H_