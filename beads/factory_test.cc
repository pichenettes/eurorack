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
// Factory test mode.

#include "beads/factory_test.h"

#include "beads/drivers/freeze_input.h"
#include "beads/drivers/seed_input.h"
#include "beads/drivers/switches.h"
#include "beads/calibrator.h"
#include "beads/cv_reader.h"
#include "beads/normalization_detector.h"

namespace beads {
  
using namespace std;
using namespace stmlib;

/* static */
FactoryTest* FactoryTest::instance_;

void FactoryTest::Init(
    CvReader* cv_reader,
    NormalizationDetector* normalization_detector,
    Calibrator* calibrator,
    const Switches* switches) {
  cv_reader_ = cv_reader;
  normalization_detector_ = normalization_detector;
  calibrator_ = calibrator;
  switches_ = switches;
}

void FactoryTest::Start() {
  instance_ = this;
  debug_port_.Init();
}

/* static */
void FactoryTest::ProcessFn(
    const Codec::Event* events,
    const Codec::Frame* in,
    Codec::Frame* out,
    size_t size) {
  FactoryTest::GetInstance()->Process(events, in, out, size);
}

void FactoryTest::Process(
    const Codec::Event* events,
    const Codec::Frame* in,
    Codec::Frame* out,
    size_t size) {
  normalization_detector_->Process(
      events[size - 1],
      in[size - 1],
      cv_reader_->cv_values());
  cv_reader_->Read();

  for (size_t i = 0; i < size; ++i) {
    NormalizationDetector* d = normalization_detector_;
    if (d->normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT)) {
      out[i].l = 0;
    } else {
      out[i].l = in[i].l;
    }
    if (d->normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT)) {
      out[i].r = 0;
    } else {
      out[i].r = in[i].r;
    }
  }
}

bool FactoryTest::Calibrate(int step) {
  switch (step) {
    case 3:
      calibrator_->Start();
      return true;
      break;
      
    case 4:
      calibrator_->CalibrateOffsets();
      return calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 0, 12.0f);
      break;
      
    case 5:
      return calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 1, 36.0f);
      break;

    case 6:
      return calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 0, 0.2f);
      break;
  
    case 7:
      {
        bool success = calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 1, 0.6f);
        if (success) {
          calibrator_->Save();
        }
        return success;
      }
      break;
    
    default:
      // Received by the CV generator side of the test bench.
      return true;
      break;
  }
}

uint8_t FactoryTest::HandleRequest(uint8_t command) {
  uint8_t argument = command & 0x1f;
  command = command >> 5;
  uint8_t reply = 0;
  switch (command) {
    case FACTORY_TEST_READ_POT:
      reply = static_cast<uint8_t>(
          256.0f * cv_reader_->raw_pot(PotsAdcChannel(argument)));
      break;

    case FACTORY_TEST_READ_CV:
      reply = static_cast<uint8_t>(
          256.0f * cv_reader_->raw_cv(CvAdcChannel(argument)));
      break;
    
    case FACTORY_TEST_READ_NORMALIZATION:
      reply = normalization_detector_->normalized(
          NormalizationDetectorChannel(argument)) ? 255 : 0;
      break;      
    
    case FACTORY_TEST_READ_GATE:
      if (argument < SWITCH_LAST) {
        return switches_->pressed(Switch(argument));
      } else if (argument == 4) {
        FreezeInput freeze;
        reply = freeze.Read();
      } else {
        SeedInput seed;
        reply = seed.Read();
      }
      break;
      
    case FACTORY_TEST_CALIBRATE:
      reply = Calibrate(argument);
      break;
  }
  return reply;
}

}  // namespace beads
