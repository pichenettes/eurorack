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

#include "beads/cv_reader.h"
#include "beads/settings.h"

namespace beads {

using namespace std;

void CvReader::Init(const Settings* settings) {
  cv_.Init();
  pots_.Init();
  pots_.StartMuxedPotsConversion();
  
  const AdcCalibrationData* c = settings->persistent_data().adc_calibration;
  
  for (int i = 0; i < POTS_ADC_CHANNEL_LAST; ++i) {
    PotChannel::Law law = PotChannel::LAW_NORMAL;
    if (i >= POTS_ADC_CHANNEL_TIME_AMOUNT && \
        i <= POTS_ADC_CHANNEL_PITCH_AMOUNT) {
      law = PotChannel::LAW_ATTENUVERTER;
    } else if (i == POTS_ADC_CHANNEL_PITCH) {
      law = PotChannel::LAW_DETUNE;
    }
    pot_channel_[i].Init(
        law,
        0.05f,
        i == POTS_ADC_CHANNEL_TIME ? 0.001f : 0.0f);
  }
  
  cv_channel_[CV_ADC_CHANNEL_ASSIGN].Init(
      0.1f, -1.0f, 1.0f,
      &c[CV_ADC_CHANNEL_ASSIGN].scale,
      &c[CV_ADC_CHANNEL_ASSIGN].offset);

  cv_channel_[CV_ADC_CHANNEL_DENSITY].Init(
      1.0f, -1.0f, 1.0f,
      &c[CV_ADC_CHANNEL_DENSITY].scale,
      &c[CV_ADC_CHANNEL_DENSITY].offset);

  cv_channel_[CV_ADC_CHANNEL_PITCH].Init(
      1.0f, -60.0f, 60.0f,
      &c[CV_ADC_CHANNEL_PITCH].scale,
      &c[CV_ADC_CHANNEL_PITCH].offset);
  
  cv_channel_[CV_ADC_CHANNEL_TIME].Init(
      0.5f, -1.6f, +1.6f,
      &c[CV_ADC_CHANNEL_TIME].scale,
      &c[CV_ADC_CHANNEL_TIME].offset);

  cv_channel_[CV_ADC_CHANNEL_SIZE].Init(
      0.5f, -1.6f, +1.6f,
      &c[CV_ADC_CHANNEL_SIZE].scale,
      &c[CV_ADC_CHANNEL_SIZE].offset);

  cv_channel_[CV_ADC_CHANNEL_SHAPE].Init(
      0.5f, -1.6f, +1.6f,
      &c[CV_ADC_CHANNEL_SHAPE].scale,
      &c[CV_ADC_CHANNEL_SHAPE].offset);
}

void CvReader::Read() {
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    cv_channel_[i].Process(cv_.value(CvAdcChannel(i)));
  }
  for (int i = 0; i < POTS_ADC_CHANNEL_LAST; ++i) {
    pot_channel_[i].Process(pots_.value(PotsAdcChannel(i)));
  }
  cv_.Convert();
  pots_.Convert();
}

}  // namespace beads