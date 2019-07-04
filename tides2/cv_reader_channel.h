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
// CV reader channel.

#ifndef TIDES_CV_READER_CHANNEL_H_
#define TIDES_CV_READER_CHANNEL_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

namespace tides {

class CvReaderChannel {
 public:
  CvReaderChannel() { }
  ~CvReaderChannel() { }

  void Init() {
    pot_lp_ = 0.0f;
    attenuverter_lp_ = 0.0f;
    cv_lp_ = 0.0f;
  }
  
  template<bool has_pot, bool has_attenuverter>
  inline float Process(
      float pot,
      float pot_scale,
      float pot_offset,
      float pot_lp_coefficient,
      float cv,
      float cv_scale,
      float cv_offset,
      float cv_lp_coefficient,
      float attenuverter,
      float min,
      float max) {
    ONE_POLE(cv_lp_, cv, cv_lp_coefficient);
      
    float a = 1.0f;
    if (has_attenuverter) {
      ONE_POLE(attenuverter_lp_, attenuverter, pot_lp_coefficient);
      a = attenuverter_lp_ - 0.5f;
      a = a * a * a * 8.0f;
    }
    
    float value = (cv_lp_ * cv_scale + cv_offset) * a;
    
    if (has_pot) {
      ONE_POLE(pot_lp_, pot, pot_lp_coefficient);
      value += pot_lp_ * pot_scale + pot_offset;
    }
    
    CONSTRAIN(value, min, max);
    return value;
  }
  
  inline float cv_lp() const { return cv_lp_; }
  inline float attenuverter_lp() const { return attenuverter_lp_; }
  inline float pot_lp() const { return pot_lp_; }

 private:
  float pot_lp_;
  float attenuverter_lp_;
  float cv_lp_;

  DISALLOW_COPY_AND_ASSIGN(CvReaderChannel);
};


}  // namespace tides

#endif  // TIDES_CV_READER_CHANNEL_H_
