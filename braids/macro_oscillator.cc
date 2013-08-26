// Copyright 2012 Olivier Gillet.
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
// Macro-oscillator.

#include "braids/macro_oscillator.h"

#include <algorithm>

#include "stmlib/utils/dsp.h"

#include "braids/parameter_interpolation.h"
#include "braids/resources.h"

namespace braids {
  
using namespace stmlib;

void MacroOscillator::Render(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  RenderFn fn = fn_table_[shape_];
  (this->*fn)(sync, buffer, size);
}

void MacroOscillator::RenderCSaw(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_pitch(pitch_);
  analog_oscillator_[0].set_shape(OSC_SHAPE_CSAW);
  analog_oscillator_[0].set_parameter(std::max(parameter_[0] >> 9, 3));
  int16_t shift = (parameter_[1] - 16384) >> 1;
  analog_oscillator_[0].set_aux_parameter(shift);
  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  while (size--) {
    int32_t signal_amplified = *buffer;
    signal_amplified = (3 * signal_amplified + (shift >> 1) + 2048) >> 1;
    *buffer = Interpolate88(ws_moderate_overdrive, signal_amplified + 32768);
    buffer++;
  }
}

void MacroOscillator::RenderMorph(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_pitch(pitch_);
  analog_oscillator_[1].set_pitch(pitch_);
  
  uint16_t balance;
  if (parameter_[0] <= 10922) {
    analog_oscillator_[0].set_parameter(0);
    analog_oscillator_[1].set_parameter(0);
    analog_oscillator_[0].set_shape(OSC_SHAPE_TRIANGLE);
    analog_oscillator_[1].set_shape(OSC_SHAPE_SAW);
    balance = parameter_[0] * 6;
  } else if (parameter_[0] <= 21845) {
    analog_oscillator_[0].set_parameter(0);
    analog_oscillator_[1].set_parameter(0);
    analog_oscillator_[0].set_shape(OSC_SHAPE_SQUARE);
    analog_oscillator_[1].set_shape(OSC_SHAPE_SAW);
    balance = 65535 - (parameter_[0] - 10923) * 6;
  } else {
    analog_oscillator_[0].set_parameter((parameter_[0] - 21846) * 3);
    analog_oscillator_[1].set_parameter(0);
    analog_oscillator_[0].set_shape(OSC_SHAPE_SQUARE);
    analog_oscillator_[1].set_shape(OSC_SHAPE_SINE);
    balance = 0;
  }
  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  analog_oscillator_[1].Render(sync, temp_buffer_, NULL, size);
  int16_t* temp_buffer = temp_buffer_;
  
  int32_t lp_cutoff = pitch_ - (parameter_[1] >> 1) + 128 * 128;
  if (lp_cutoff < 0) {
    lp_cutoff = 0;
  } else if (lp_cutoff > 32767) {
    lp_cutoff = 32767;
  }
  int32_t f = Interpolate824(lut_svf_cutoff, lp_cutoff << 17);
  int32_t lp_state = lp_state_;
  
  while (size--) {
    int16_t sample = Mix(*buffer, *temp_buffer, balance);
    int32_t shifted_sample = sample;
    shifted_sample += (parameter_[1] >> 2) + (parameter_[0] >> 4);
    
    lp_state += (sample - lp_state) * f >> 15;
    CLIP(lp_state)
    shifted_sample = lp_state + 32768;
    
    int16_t fuzzed = Interpolate88(ws_violent_overdrive, shifted_sample);
    *buffer = Mix(sample, fuzzed, parameter_[1] << 1);
    buffer++;
    temp_buffer++;
  }
  lp_state_ = lp_state;
}

void MacroOscillator::RenderSawSquare(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_parameter(parameter_[0]);
  analog_oscillator_[1].set_parameter(parameter_[0]);
  analog_oscillator_[0].set_pitch(pitch_);
  analog_oscillator_[1].set_pitch(pitch_);

  analog_oscillator_[0].set_shape(OSC_SHAPE_SAW);
  analog_oscillator_[1].set_shape(OSC_SHAPE_SQUARE);
  
  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  analog_oscillator_[1].Render(sync, temp_buffer_, NULL, size);
  
  BEGIN_INTERPOLATE_PARAMETER_1
  
  int16_t* temp_buffer = temp_buffer_;
  while (size--) {
    INTERPOLATE_PARAMETER_1
    
    uint16_t balance = parameter_1 << 1;
    int16_t attenuated_square = static_cast<int32_t>(*temp_buffer) * 148 >> 8;
    *buffer = Mix(*buffer, attenuated_square, balance);
    buffer++;
    temp_buffer++;
  }

  END_INTERPOLATE_PARAMETER_1
}

void MacroOscillator::RenderSquareSync(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_parameter(0);
  analog_oscillator_[0].set_shape(OSC_SHAPE_SQUARE);
  analog_oscillator_[0].set_pitch(pitch_);

  analog_oscillator_[1].set_parameter(0);
  analog_oscillator_[1].set_shape(OSC_SHAPE_SQUARE);
  analog_oscillator_[1].set_pitch(pitch_ + (parameter_[0] >> 2));

  analog_oscillator_[0].Render(sync, buffer, sync_buffer_, size);
  analog_oscillator_[1].Render(sync_buffer_, temp_buffer_, NULL, size);
  
  BEGIN_INTERPOLATE_PARAMETER_1

  int16_t* temp_buffer = temp_buffer_;
  while (size--) {
    INTERPOLATE_PARAMETER_1
    uint16_t balance = parameter_1 << 1;
    
    *buffer = Mix(*buffer, *temp_buffer, balance);
    buffer++;
    temp_buffer++;
  }
  
  END_INTERPOLATE_PARAMETER_1
}

void MacroOscillator::RenderSineTriangle(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_parameter(parameter_[0]);
  analog_oscillator_[1].set_parameter(parameter_[0]);
  analog_oscillator_[0].set_pitch(pitch_);
  analog_oscillator_[1].set_pitch(pitch_);
  
  analog_oscillator_[0].set_shape(OSC_SHAPE_SINE_FOLD);
  analog_oscillator_[1].set_shape(OSC_SHAPE_TRIANGLE_FOLD);

  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  analog_oscillator_[1].Render(sync, temp_buffer_, NULL, size);

  int16_t* temp_buffer = temp_buffer_;
  
  BEGIN_INTERPOLATE_PARAMETER_1
  
  while (size--) {
    INTERPOLATE_PARAMETER_1
    uint16_t balance = parameter_1 << 1;
    
    *buffer = Mix(*buffer, *temp_buffer, balance);
    buffer++;
    temp_buffer++;
  }
  
  END_INTERPOLATE_PARAMETER_1
}

void MacroOscillator::RenderBuzz(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  analog_oscillator_[0].set_parameter(parameter_[0]);
  analog_oscillator_[0].set_shape(OSC_SHAPE_BUZZ);
  analog_oscillator_[0].set_pitch(pitch_);

  analog_oscillator_[1].set_parameter(parameter_[0]);
  analog_oscillator_[1].set_shape(OSC_SHAPE_BUZZ);
  analog_oscillator_[1].set_pitch(pitch_ + (parameter_[1] >> 8));

  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  analog_oscillator_[1].Render(sync, temp_buffer_, NULL, size);
  int16_t* temp_buffer = temp_buffer_;
  while (size--) {
    *buffer >>= 1;
    *buffer += *temp_buffer >> 1;
    buffer++;
    temp_buffer++;
  }
}

void MacroOscillator::RenderDigital(
    const uint8_t* sync,
    int16_t* buffer,
    uint8_t size) {
  digital_oscillator_.set_parameters(parameter_[0], parameter_[1]);
  digital_oscillator_.set_pitch(pitch_);
  digital_oscillator_.set_shape(static_cast<DigitalOscillatorShape>(
      shape_ - MACRO_OSC_SHAPE_TRIPLE_RING_MOD));
  digital_oscillator_.Render(sync, buffer, size);
}

void MacroOscillator::RenderSawComb(
  const uint8_t* sync,
  int16_t* buffer,
  uint8_t size) {
  analog_oscillator_[0].set_parameter(0);
  analog_oscillator_[0].set_pitch(pitch_);
  analog_oscillator_[0].set_shape(OSC_SHAPE_SAW);
  analog_oscillator_[0].Render(sync, buffer, NULL, size);
  
  digital_oscillator_.set_parameters(parameter_[0], parameter_[1]);
  digital_oscillator_.set_pitch(pitch_);
  digital_oscillator_.set_shape(OSC_SHAPE_COMB_FILTER);
  digital_oscillator_.Render(sync, buffer, size);
}

/* static */
MacroOscillator::RenderFn MacroOscillator::fn_table_[] = {
  &MacroOscillator::RenderCSaw,
  &MacroOscillator::RenderMorph,
  &MacroOscillator::RenderSawSquare,
  &MacroOscillator::RenderSquareSync,
  &MacroOscillator::RenderSineTriangle,
  &MacroOscillator::RenderBuzz,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderSawComb,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  &MacroOscillator::RenderDigital,
  // &MacroOscillator::RenderDigital
};

}  // namespace braids
