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
// Oscillator - analog style waveforms.

#ifndef BRAIDS_ANALOG_OSCILLATOR_H_
#define BRAIDS_ANALOG_OSCILLATOR_H_

#include "stmlib/stmlib.h"

#include <cstring>

#include "braids/resources.h"

namespace braids {

static const size_t kNumBleps = 2;

enum AnalogOscillatorShape {
  OSC_SHAPE_SAW,
  OSC_SHAPE_CSAW,
  OSC_SHAPE_SQUARE,
  OSC_SHAPE_TRIANGLE,
  OSC_SHAPE_SINE,
  OSC_SHAPE_TRIANGLE_FOLD,
  OSC_SHAPE_SINE_FOLD,
  OSC_SHAPE_BUZZ
};

enum SyncMode {
  OSCILLATOR_SYNC_MODE_OFF,
  OSCILLATOR_SYNC_MODE_MASTER,
  OSCILLATOR_SYNC_MODE_SLAVE
};

struct Blep {
  uint32_t phase;  // The blep table is not interpolated.
  int32_t scale;
};

struct AnalogOscillatorState {
  size_t lru_blep;
  bool up;
  Blep blep_pool[kNumBleps];
  uint32_t aux_phase;
  uint32_t phase_remainder;
  int16_t aux_shift;
};

class AnalogOscillator {
 public:
  typedef void (AnalogOscillator::*RenderFn)(
      const uint8_t*,
      int16_t*,
      uint8_t*,
      uint8_t);

  AnalogOscillator() { }
  ~AnalogOscillator() { }
  
  inline void Init() {
    memset(&state_, 0, sizeof(state_));
    phase_ = 0;
  }
  
  inline void set_shape(AnalogOscillatorShape shape) {
    shape_ = shape;
  }
  
  inline void set_pitch(int16_t pitch) {
    pitch_ = pitch;
  }

  inline void set_parameter(int16_t parameter) {
    parameter_ = parameter;
  }

  inline void set_aux_parameter(int16_t parameter) {
    aux_parameter_ = parameter;
  }
  
  inline uint32_t phase_increment() const {
    return phase_increment_;
  }
  
  inline void Reset() {
    phase_ = -phase_increment_;
  }

  void Render(
      const uint8_t* sync_in,
      int16_t* buffer,
      uint8_t* sync_out,
      uint8_t size);
  
 private:
  void RenderSquare(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderSaw(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderCSaw(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderTriangle(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderSine(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderTriangleFold(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderSineFold(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  void RenderBuzz(const uint8_t*, int16_t*, uint8_t*, uint8_t);
  
  uint32_t ComputePhaseIncrement(int16_t midi_pitch);
   
  inline void AddBlep(
      uint32_t phase_residue,
      uint32_t phase_increment,
      int32_t scale)
  __attribute__((always_inline)) {
    uint32_t blep_phase = phase_residue / (phase_increment >> 8);
    if (blep_phase < LUT_BLEP_SIZE) {
      state_.lru_blep = (state_.lru_blep + 1) % kNumBleps;
      Blep& blep = state_.blep_pool[state_.lru_blep];
      blep.phase = blep_phase;
      blep.scale = scale;
    }
  }

  uint32_t phase_;
  uint32_t phase_increment_;
  uint32_t delay_;

  int16_t parameter_;
  int16_t previous_parameter_;
  int16_t aux_parameter_;
  int16_t pitch_;
  
  AnalogOscillatorShape shape_;
  AnalogOscillatorShape previous_shape_;
  AnalogOscillatorState state_;
  
  static RenderFn fn_table_[];
  
  DISALLOW_COPY_AND_ASSIGN(AnalogOscillator);
};

}  // namespace braids

#endif // BRAIDS_ANALOG_OSCILLATOR_H_
