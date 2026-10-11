// Copyright 2019 Emilie Gillet.
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
// CV reader.

#ifndef BEADS_CV_READER_H_
#define BEADS_CV_READER_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/hysteresis_filter.h"

#include "beads/drivers/cv_adc.h"
#include "beads/drivers/pots_adc.h"
#include "beads/settings.h"

namespace beads {

class CvChannel {
 public:
  CvChannel();
  ~CvChannel();
  
  void Init(
      float lp_coefficient,
      float min,
      float max,
      const float* scale,
      const float* offset) {
    lp_coefficient_ = lp_coefficient;
    lp_ = 0.5f;
    value_ = (min + max) * 0.5f;
    min_ = min;
    max_ = max;
    scale_ = scale;
    offset_ = offset;
  }
  
  void Process(uint16_t raw_value) {
    float v = float(raw_value) / 65536.0f;
    ONE_POLE(lp_, v, lp_coefficient_);
    
    v = lp_;
    v *= *scale_;
    v += *offset_;
    CONSTRAIN(v, min_, max_)
    value_ = v;
  }
  
  inline float value() const { return value_; }
  inline float unscaled_value() const { return lp_; }
  
 private:
  float value_;
  float lp_;
  float lp_coefficient_;
  float min_;
  float max_;
  const float* scale_;
  const float* offset_;
  
  DISALLOW_COPY_AND_ASSIGN(CvChannel);
};

class PotChannel {
 public:
  PotChannel();
  ~PotChannel();
  
  enum Law {
    LAW_NORMAL,
    LAW_ATTENUVERTER,
    LAW_DETUNE
  };
  
  void Init(Law law, float lp_coefficient, float hysteresis) {
    law_ = law;

    lp_coefficient_ = lp_coefficient;
    lp_ = 0.0f;

    hysteresis_filter_.Init(hysteresis);
  }
  
  void Process(uint16_t raw_value) {
    float v = float(raw_value) / 65536.0f;
    if (law_ == LAW_ATTENUVERTER) {
      v -= 0.5f;
      v = (2.0f / 0.47f) * v * std::max(fabsf(v) - 0.03f, 0.0f);
      // v = v * v * v * 8.0f;
    } else if (law_ == LAW_DETUNE) {
      float detune = 2.0f * v - 1.0f;
      float sign = detune < 0.0f ? -1.0f : 1.0f;
      detune = detune * sign * 6.0f;
      MAKE_INTEGRAL_FRACTIONAL(detune);
      
      const float intervals[8] = {
        0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f
      };
      
      detune_fractional = std::max(
          (detune_fractional - 0.33333333f) * 1.5f, 0.0f);
  
      float a = intervals[detune_integral];
      float b = intervals[detune_integral + 1];
      v = stmlib::Crossfade(a, b, stmlib::SmoothStep(detune_fractional)) * sign;
    }
    ONE_POLE(lp_, v, lp_coefficient_);
    hysteresis_filter_.Process(lp_);
  }
  
  inline float value() const { return hysteresis_filter_.value(); }
  
 private:
  float lp_;
  float lp_coefficient_;
  stmlib::HysteresisFilter hysteresis_filter_;
  Law law_;
  
  DISALLOW_COPY_AND_ASSIGN(PotChannel);
};

class CvReader {
 public:
  CvReader() { }
  ~CvReader() { }
  
  void Init(const Settings* settings);
  void Read();
  
  inline const uint16_t* cv_values() { return cv_.values(); }
  
  inline float cv(CvAdcChannel channel) const {
    return cv_channel_[channel].value();
  }

  inline float unscaled_cv(CvAdcChannel channel) const {
    return cv_channel_[channel].unscaled_value();
  }

  inline float pot(PotsAdcChannel channel) const {
    return pot_channel_[channel].value();
  }

  inline float raw_pot(PotsAdcChannel channel) const {
    return pots_.float_value(channel);
  }

  inline float raw_cv(CvAdcChannel channel) const {
    return cv_.float_value(channel);
  }
  
 private:
  CvAdc cv_;
  PotsAdc pots_;
  
  PotChannel pot_channel_[POTS_ADC_CHANNEL_LAST];
  
  CvChannel cv_channel_[CV_ADC_CHANNEL_LAST];
  
  DISALLOW_COPY_AND_ASSIGN(CvReader);
};

}  // namespace beads

#endif  // BEADS_CV_READER_H_