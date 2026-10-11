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
// Scale/offset computation for channel calibration.

#ifndef BEADS_CALIBRATOR_H_
#define BEADS_CALIBRATOR_H_

#include "stmlib/stmlib.h"

#include "beads/cv_reader.h"
#include "beads/normalization_detector.h"
#include "beads/settings.h"

namespace beads {

class CalibratorChannel {
 public:
  CalibratorChannel() { }
  ~CalibratorChannel() { }

  void Init() {
    measurement_[0] = measurement_[1] = 0.0f;
    expected_value_[0] = expected_value_[1] = 0.0f;
  }

  void Acquire(int step, float measurement, float expected_value) {
    measurement_[step] = measurement;
    expected_value_[step] = expected_value;
  }

  inline float scale() const {
    float d = measurement_[1] - measurement_[0];
    if (d == 0.0f) {
      return 0.0f;
    }
    return (expected_value_[1] - expected_value_[0]) / d;
  }

  inline float offset() const {
    return expected_value_[0] - scale() * measurement_[0];
  }

 private:
  float measurement_[2];
  float expected_value_[2];

  DISALLOW_COPY_AND_ASSIGN(CalibratorChannel);
};

class Calibrator {
 public:
  Calibrator() { }
  ~Calibrator() { }
  
  void Init(
      Settings* settings,
      CvReader* cv_reader,
      NormalizationDetector* normalization_detector) {
    calibration_data_ = settings->mutable_persistent_data()->adc_calibration;
    cv_reader_ = cv_reader;
    normalization_detector_ = normalization_detector;
    calibrator_[0].Init();
    calibrator_[1].Init();
    settings_ = settings;
  }
  
  inline void Start() {
    normalization_detector_->Stop();
  }
  
  inline void CalibrateOffsets() {
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      if (i != CV_ADC_CHANNEL_PITCH && i != CV_ADC_CHANNEL_DENSITY) {
        calibration_data_[i].offset = -calibration_data_[i].scale * \
            cv_reader_->unscaled_cv(CvAdcChannel(i));
      }
    }
    normalization_detector_->Start();
  }
  
  inline bool Calibrate(CvAdcChannel channel, int step, float expected_value) {
    CalibratorChannel* c = \
        &calibrator_[channel == CV_ADC_CHANNEL_PITCH ? 0 : 1];
    c->Acquire(step, cv_reader_->unscaled_cv(channel), expected_value);
    if (step == 1) {
      float scale = c->scale();
      float expected_scale = channel == CV_ADC_CHANNEL_PITCH ? -120.0f : -2.0f;
      if (fabsf((scale - expected_scale) / expected_scale) > 0.1f) {
        return false;
      }
      calibration_data_[channel].scale = c->scale();
      calibration_data_[channel].offset = c->offset();
    }
    return true;
  }
  
  inline void Save() {
    settings_->SavePersistentData();
  }
  
 private:
  CalibratorChannel calibrator_[2];
  
  AdcCalibrationData* calibration_data_;
  CvReader* cv_reader_;
  NormalizationDetector* normalization_detector_;
  Settings* settings_;
  
  DISALLOW_COPY_AND_ASSIGN(Calibrator);
};

}  // namespace beads

#endif  // BEADS_CALIBRATOR_H_
