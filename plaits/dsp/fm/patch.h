// Copyright 2021 Emilie Gillet.
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
// DX7 patch.

#ifndef PLAITS_DSP_FM_PATCH_H_
#define PLAITS_DSP_FM_PATCH_H_

#include "stmlib/stmlib.h"

namespace plaits {

namespace fm {

struct Patch {
  enum {
    SYX_SIZE = 128
  };
  
  // The layout conveniently matches the 156 bytes SysEx format.
  
  struct Envelope {
    uint8_t rate[4];
    uint8_t level[4];
  };
  
  struct KeyboardScaling {
    uint8_t break_point;
    uint8_t left_depth;
    uint8_t right_depth;
    uint8_t left_curve;
    uint8_t right_curve;
  };

  struct Operator {
    Envelope envelope;    
    KeyboardScaling keyboard_scaling;
    
    uint8_t rate_scaling;
    uint8_t amp_mod_sensitivity;
    uint8_t velocity_sensitivity;
    uint8_t level;

    uint8_t mode;
    uint8_t coarse;
    uint8_t fine;   // x frequency by 1 + 0.01 x fine
    uint8_t detune; 
  } op[6];
  
  Envelope pitch_envelope;

  uint8_t algorithm;
  uint8_t feedback;
  
  uint8_t reset_phase;
  
  struct ModulationParameters {
    uint8_t rate;
    uint8_t delay;
    uint8_t pitch_mod_depth;
    uint8_t amp_mod_depth;
    uint8_t reset_phase;
    uint8_t waveform;
    uint8_t pitch_mod_sensitivity;
  } modulations;

  uint8_t transpose;
  uint8_t name[10];
  uint8_t active_operators;
  
  inline uint8_t* bytes() {
    return static_cast<uint8_t*>(static_cast<void*>(this));
  }
  
  inline void Unpack(const uint8_t* data) {
    for (int i = 0; i < 6; ++i) {
      Operator* o = &op[i];
      const uint8_t* op_data = &data[i * 17];
      for (int j = 0; j < 4; ++j) {
        o->envelope.rate[j] = std::min(op_data[j] & 0x7f, 99);
        o->envelope.level[j] = std::min(op_data[4 + j] & 0x7f, 99);
      }
      o->keyboard_scaling.break_point = std::min(op_data[8] & 0x7f, 99);
      o->keyboard_scaling.left_depth = std::min(op_data[9] & 0x7f, 99);
      o->keyboard_scaling.right_depth = std::min(op_data[10] & 0x7f, 99);
      o->keyboard_scaling.left_curve = op_data[11] & 0x3;
      o->keyboard_scaling.right_curve = (op_data[11] >> 2) & 0x3;

      o->rate_scaling = op_data[12] & 0x7;
      o->amp_mod_sensitivity = op_data[13] & 0x3;
      o->velocity_sensitivity = (op_data[13] >> 2) & 0x7;
      o->level = std::min(op_data[14] & 0x7f, 99);
      o->mode = op_data[15] & 0x1;
      o->coarse = (op_data[15] >> 1) & 0x1f;
      o->fine = std::min(op_data[16] & 0x7f, 99);
      o->detune = std::min((op_data[12] >> 3) & 0xf, 14);
    }
    
    for (int j = 0; j < 4; ++j) {
      pitch_envelope.rate[j] = std::min(data[102 + j] & 0x7f, 99);
      pitch_envelope.level[j] = std::min(data[106 + j] & 0x7f, 99);
    }
    
    algorithm = data[110] & 0x1f;
    feedback = data[111] & 0x7;
    reset_phase = (data[111] >> 3) & 0x1;
    
    modulations.rate = std::min(data[112] & 0x7f, 99);
    modulations.delay = std::min(data[113] & 0x7f, 99);
    modulations.pitch_mod_depth = std::min(data[114] & 0x7f, 99);
    modulations.amp_mod_depth = std::min(data[115] & 0x7f, 99);
    modulations.reset_phase = data[116] & 0x1;
    modulations.waveform = std::min((data[116] >> 1) & 0x7, 5);
    modulations.pitch_mod_sensitivity = data[116] >> 4;
    
    transpose = std::min(data[117] & 0x7f, 48);
    
    for (size_t i = 0; i < sizeof(name); ++i) {
      name[i] = data[118 + i] & 0x7f;
    }
    active_operators = 0x3f;
  }
};

}  // namespace fm
  
}  // namespace plaits

#endif  // PLAITS_DSP_FM_PATCH_H_
