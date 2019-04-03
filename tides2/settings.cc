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
// Settings storage.

#include "tides2/settings.h"

#include <algorithm>
#include <cmath>

#include "stmlib/system/storage.h"

namespace tides {

using namespace std;

#define FIX_OUTLIER(destination, expected_value) if (fabsf(destination / expected_value - 1.0f) > 0.25f) { destination = expected_value; }
#define FIX_OUTLIER_ABSOLUTE(destination, expected_value) if (fabsf(destination - expected_value) > 0.1f) { destination = expected_value; }

bool Settings::Init() {
  DacCalibrationData default_dac_calibration_data;
  default_dac_calibration_data.scale = -4032.9f;
  default_dac_calibration_data.offset = 32768.0f;
  
  AdcCalibrationData default_adc_calibration_data;
  default_adc_calibration_data.scale = -1.0f;
  default_adc_calibration_data.offset = 0.0f;
  
  fill(
      &persistent_data_.dac_calibration[0],
      &persistent_data_.dac_calibration[kNumCvOutputs],
      default_dac_calibration_data);

  fill(
      &persistent_data_.adc_calibration[0],
      &persistent_data_.adc_calibration[kNumParameters],
      default_adc_calibration_data);
  
  // Calibration data for the V/O and FM CV input.
  persistent_data_.adc_calibration[0].scale = -60.0f;
  persistent_data_.adc_calibration[0].offset = +25.68f;
  persistent_data_.adc_calibration[1].scale = -96.0f;
  persistent_data_.adc_calibration[1].offset = +0.0f;
  
  state_.mode = 1;
  state_.range = 2;
  state_.output_mode = 0;
  
  bool success = chunk_storage_.Init(&persistent_data_, &state_);
  
  if (success) {
    // Sanitize settings read from flash.
    FIX_OUTLIER(persistent_data_.adc_calibration[0].scale, -60.0f);
    FIX_OUTLIER(persistent_data_.adc_calibration[0].offset, +25.68f);
    FIX_OUTLIER(persistent_data_.adc_calibration[1].scale, -96.0f);
    
    if (fabsf(persistent_data_.adc_calibration[1].offset) > 10.0f) {
      persistent_data_.adc_calibration[1].offset = 0.0f;
    }
    for (int i = 0; i < 4; ++i) {
      FIX_OUTLIER(persistent_data_.adc_calibration[i + 2].scale, -1.0f);
      FIX_OUTLIER_ABSOLUTE(persistent_data_.adc_calibration[i + 2].offset, 0);
      FIX_OUTLIER(persistent_data_.dac_calibration[i].scale, -4032.9f);
      FIX_OUTLIER(persistent_data_.dac_calibration[i].offset, 32768.0f);
    }
    CONSTRAIN(state_.mode, 0, 2);
    CONSTRAIN(state_.range, 0, 2);
    CONSTRAIN(state_.output_mode, 0, 3);
  }
  
  return success;
}

void Settings::SavePersistentData() {
  chunk_storage_.SavePersistentData();
}

void Settings::SaveState() {
  chunk_storage_.SaveState();
}

}  // namespace tides
