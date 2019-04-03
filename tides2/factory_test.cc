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
// Factory test mode.

#include "tides2/factory_test.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "tides2/drivers/gate_inputs.h"
#include "tides2/cv_reader.h"
#include "tides2/resources.h"
#include "tides2/settings.h"
#include "tides2/ui.h"

namespace tides {
  
using namespace std;
using namespace stmlib;

/* static */
FactoryTest* FactoryTest::instance_;

void FactoryTest::Init(
    Settings* settings,
    CvReader* cv_reader,
    GateInputs* gate_inputs,
    const Switches* switches) {
  settings_ = settings;
  cv_reader_ = cv_reader;
  gate_inputs_ = gate_inputs;
  switches_ = switches;
  
  calibration_data_ = 0;
  calibration_first_adc_value_ = 0.0f;
  
  fill(
      &forced_dac_code_[0],
      &forced_dac_code_[kNumCvOutputs],
      0);
}

void FactoryTest::Start() {
  instance_ = this;
  debug_port_.Init();
}

/* static */
void FactoryTest::ProcessFn(IOBuffer::Block* block, size_t size) {
  FactoryTest::GetInstance()->Process(block, size);
}

void FactoryTest::Process(IOBuffer::Block* block, size_t size) {
  static float phase = 0.0f;
  for (size_t i = 0; i < size; ++i) {
    phase += 100.0f / kSampleRate;
    if (phase >= 1.0f) {
      phase -= 1.0f;
    }
    
    float x[4];
    x[0] = 4.0f * stmlib::Interpolate(lut_sine, phase, 1024.0f);
    x[1] = -8.0f * phase + 4.0f;
    x[2] = (phase < 0.5f ? phase : 1.0f - phase) * 16.0f - 4.0f;
    x[3] = phase < 0.5f ? -4.0f : 4.0f;
    
    for (size_t j = 0; j < kNumCvOutputs; ++j) {
      uint16_t dac_code = forced_dac_code_[j];
      block->output[j][i] = dac_code ? dac_code : settings_->dac_code(j, x[j]);
    }
  }
}

bool FactoryTest::Calibrate(int step, float v1, float v2) {
  bool success = true;
  
  if (step == 0) {
    forced_dac_code_[0] = settings_->dac_code(0, v1);
    gate_inputs_->DisableNormalizationProbe();
  } else if (step == 1) {
    calibration_first_adc_value_ = cv_reader_->channel(0).cv_lp();
    // Acquire the zero for the other channels.
    for (size_t i = 1; i < CV_ADC_CHANNEL_LAST; ++i) {
      AdcCalibrationData* c = settings_->mutable_adc_calibration_data(i);
      c->offset = 0.0f - cv_reader_->channel(i).cv_lp() * c->scale;
    }
    forced_dac_code_[0] = settings_->dac_code(0, v2);
  } else if (step == 2) {
    float calibration_v1 = calibration_first_adc_value_;
    float calibration_v2 = cv_reader_->channel(0).cv_lp();
    float scale = (v2 - v1) * 12.0f / (calibration_v2 - calibration_v1);
    float offset = v2 * 12.0f - calibration_v2 * scale;
    if (scale > -65.0f && scale < -55.0f) {
      AdcCalibrationData* c = settings_->mutable_adc_calibration_data(0);
      c->scale = scale;
      c->offset = offset;
      settings_->SavePersistentData();
    } else {
      success = false;
    }
    gate_inputs_->Init();
  }
  return success;
}

uint8_t FactoryTest::HandleRequest(uint8_t command) {
  uint8_t argument = command & 0x1f;
  command = command >> 5;
  uint8_t reply = 0;
  switch (command) {
    case FACTORY_TEST_READ_POT:
      reply = static_cast<uint8_t>(256.0f * \
          (argument < 6
              ? cv_reader_->channel(argument).pot_lp()
              : cv_reader_->channel(argument - 6).attenuverter_lp()));
      break;

    case FACTORY_TEST_READ_CV:
      reply = static_cast<uint8_t>(
          cv_reader_->channel(argument).cv_lp() * 127.0f + 128.0f);
      break;
    
    case FACTORY_TEST_READ_NORMALIZATION:
      reply = gate_inputs_->is_normalized(argument) ? 255 : 0;
      break;      
    
    case FACTORY_TEST_READ_GATE:
      reply = argument < 2
          ? gate_inputs_->value(argument)
          : switches_->pressed(Switch(argument - 2));
      break;
      
    case FACTORY_TEST_GENERATE_TEST_SIGNALS:
      fill(
          &forced_dac_code_[0],
          &forced_dac_code_[kNumCvOutputs],
          0);
      break;
      
    case FACTORY_TEST_CALIBRATE:
      {
        int step = argument & 0x3;
        Calibrate(step, -2.0f, +4.0f);
      }
      break;
      
    case FACTORY_TEST_FORCE_DAC_CODE:
      {
        int channel = argument >> 2;
        int step = argument & 0x3;
        if (step == 0) {
          forced_dac_code_[channel] = 0x9ff1;
        } else if (step == 1) {
          forced_dac_code_[channel] = 0x416b;
        } else {
          DacCalibrationData* c = settings_->mutable_dac_calibration_data(
              channel);
          c->offset = static_cast<float>(calibration_data_ & 0xffff);
          c->scale = -static_cast<float>(calibration_data_ >> 16) * 0.125f;
          forced_dac_code_[channel] = settings_->dac_code(
              channel, 1.0f);
          settings_->SavePersistentData();
        }
      }
      break;
      
    case FACTORY_TEST_WRITE_CALIBRATION_DATA_NIBBLE:
      calibration_data_ <<= 4;
      calibration_data_ |= argument & 0xf;
      break;
  }
  return reply;
}

}  // namespace tides
