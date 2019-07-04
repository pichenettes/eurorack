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
// CV reader.

#include "tides2/cv_reader.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

namespace tides {
  
using namespace std;
using namespace stmlib;

void CvReader::Init(Settings* settings) {
  settings_ = settings;
  pots_adc_.Init();
  cv_adc_.Init();
  
  for (int i = 0; i < 6; ++i) {
    cv_reader_channel_[i].Init();
  }
  
  note_lp_ = 0.0f;
}

float kShapeBreakpoints[] = {
  0.0f, 0.26f, 0.34f, 0.42f, 0.5f, 0.58f, 0.66f, 0.74f, 1.0f, 1.0f
};

void CvReader::Read(IOBuffer::Block* block) {
  // Note.
  float note = cv_reader_channel_[0].Process<true, false>(
      CenterDetent(pots_adc_.float_value(POTS_ADC_CHANNEL_POT_FREQUENCY)),
      96.0f,
      -48.0f,
      0.003f,
  
      cv_adc_.float_value(CV_ADC_CHANNEL_V_OCT),
      settings_->adc_calibration_data(0).scale,
      settings_->adc_calibration_data(0).offset,
      0.2f,
  
      1.0f,
  
      -96.0f,
      +96.0f);

  ONE_POLE(note_lp_, note, 0.2f);
  block->parameters.frequency = note_lp_; 

  // FM
  block->parameters.fm = cv_reader_channel_[1].Process<false, true>(
      0.0f,
      0.0f,
      0.0f,
      0.003f,
      
      cv_adc_.float_value(CV_ADC_CHANNEL_FM),
      settings_->adc_calibration_data(1).scale,
      settings_->adc_calibration_data(1).offset,
      0.3f,
      
      pots_adc_.float_value(POTS_ADC_CHANNEL_ATTENUVERTER_FREQUENCY),
      
      -96.0f,
      +96.0f);

  if (!block->input_patched[2]) {
    block->parameters.fm = \
        cv_reader_channel_[1].attenuverter_lp() * 2.0f - 1.0f;
  }
  
  // All remaining parameters.
  for (size_t i = 2; i < kNumParameters; ++i) {
    size_t pot = i - 2;
    float attenuverter_value = pots_adc_.float_value(PotsAdcChannel(
        POTS_ADC_CHANNEL_ATTENUVERTER_SHAPE + pot));
    
    float pot_value = pots_adc_.float_value(
        PotsAdcChannel(POTS_ADC_CHANNEL_POT_SHAPE + pot));
    
    if (i == 2) {
      pot_value = Interpolate(kShapeBreakpoints, pot_value, 8.0f);
    } else if (i == 5) {
      pot_value = CenterDetent(pot_value);
    }
    
    (&block->parameters.fm)[i] = cv_reader_channel_[i].Process<true, true>(
        pot_value,
        1.0f,
        0.0f,
        0.003f,
    
        cv_adc_.float_value(CvAdcChannel(i)),
        settings_->adc_calibration_data(i).scale,
        settings_->adc_calibration_data(i).offset,
        0.1f,
    
        attenuverter_value,
    
        0.0f,
        1.0f);
  }
  
  cv_adc_.Convert();
  pots_adc_.Convert();
}

}  // namespace tides
