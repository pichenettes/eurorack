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
// Settings storage.

#include "beads/settings.h"

#include <algorithm>
#include <cmath>

#include "stmlib/system/storage.h"

namespace beads {

using namespace std;

#define FIX_OUTLIER(destination, expected_value) if (fabsf(destination / expected_value - 1.0f) > 0.25f) { destination = expected_value; }
#define FIX_OUTLIER_ABSOLUTE(destination, expected_value) if (fabsf(destination - expected_value) > 0.1f) { destination = expected_value; }

bool Settings::Init(bool recovery_mode) {
  AdcCalibrationData calibration;
  calibration.scale = -2.0f;
  calibration.offset = 1.0f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_ASSIGN] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_DENSITY] = calibration;

  calibration.scale = -3.2f;
  calibration.offset = 1.6f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_TIME] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_SIZE] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_SHAPE] = calibration;

  calibration.scale = -120.0f;
  calibration.offset = 60.0f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_PITCH] = calibration;
  
  state_.media = 3;
  state_.assignable_cv_destination = 0;
  state_.assignable_cv_amount[0] = 255;
  state_.assignable_cv_amount[1] = 0;
  state_.assignable_cv_amount[2] = 128;
  state_.gain = 0;
  state_.gate_output_mode = 0;
  state_.frozen = 0;
  state_.disable_autosave = 0;
  state_.monophonic_mode = 0;
  
  if (recovery_mode) {
    chunk_storage_.Recover(&persistent_data_, &state_);
  }
  
  bool success = chunk_storage_.Init(&persistent_data_, &state_);
  
  if (success) {
    // Sanitize settings read from flash.
    CONSTRAIN(state_.media, 0, 3);
    CONSTRAIN(state_.assignable_cv_destination, 0, 3);
  }
  
  return success;
}

void Settings::SavePersistentData() {
  fill(
      &persistent_data_.padding[0],
      &persistent_data_.padding[sizeof(persistent_data_.padding)],
      0);
  chunk_storage_.SavePersistentData();
}

void Settings::SaveState() {
  fill(&state_.padding[0], &state_.padding[sizeof(state_.padding)], 0);
  chunk_storage_.SaveState();
}

}  // namespace beads
