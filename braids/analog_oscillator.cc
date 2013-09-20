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

#include "braids/analog_oscillator.h"

#include "stmlib/utils/dsp.h"

#include "braids/resources.h"
#include "braids/parameter_interpolation.h"

namespace braids {
  
using namespace stmlib;

static const uint8_t kNumZones = 19;

static const uint16_t kHighestNote = 140 * 128;
static const uint16_t kPitchTableStart = 128 * 128;
static const uint16_t kOctave = 12 * 128;

static const uint16_t kBlepTransitionStart = 104 << 7;
static const uint16_t kBlepTransitionEnd = 112 << 7;

#define ACCUMULATE_BLEP(index) \
if (state_.blep_pool[index].scale) { \
  int16_t value = lut_blep[state_.blep_pool[index].phase]; \
  output += (value * state_.blep_pool[index].scale) >> 15; \
  state_.blep_pool[index].phase += 256; \
  if (state_.blep_pool[index].phase >= LUT_BLEP_SIZE) { \
    state_.blep_pool[index].scale = 0; \
  } \
}

uint32_t AnalogOscillator::ComputePhaseIncrement(int16_t midi_pitch) {
  if (midi_pitch >= kHighestNote) {
    midi_pitch = kHighestNote - 1;
  }
  
  int32_t ref_pitch = midi_pitch;
  ref_pitch -= kPitchTableStart;
  
  size_t num_shifts = 0;
  while (ref_pitch < 0) {
    ref_pitch += kOctave;
    ++num_shifts;
  }
  
  uint32_t a = lut_oscillator_increments[ref_pitch >> 4];
  uint32_t b = lut_oscillator_increments[(ref_pitch >> 4) + 1];
  uint32_t phase_increment = a + \
      (static_cast<int32_t>(b - a) * (ref_pitch & 0xf) >> 4);
  phase_increment >>= num_shifts;
  return phase_increment;
}

void AnalogOscillator::Render(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  RenderFn fn = fn_table_[shape_];
  
  if (shape_ != previous_shape_) {
    Init();
    previous_shape_ = shape_;
  }
  
  phase_increment_ = ComputePhaseIncrement(pitch_);
  
  if (pitch_ > kHighestNote) {
    pitch_ = kHighestNote;
  } else if (pitch_ < 0) {
    pitch_ = 0;
  }
  
  (this->*fn)(sync_in, buffer, sync_out, size);
}

void AnalogOscillator::RenderSaw(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  uint32_t aux_phase = state_.aux_phase;
  int32_t previous_sample = phase_ >> 18;
  int32_t previous_sample_aux = aux_phase >> 18;
  while (size--) {
    phase_ += phase_increment_;
    if (*sync_in++) {
      phase_ = 0;
    }
    
    bool wrap = phase_ < phase_increment_;
    if (pitch_ < kBlepTransitionEnd) {
      // Servo control of the dephasing between the two sawtooth waves.
      uint32_t error = phase_ + (parameter_ << 16) - aux_phase;
      uint32_t aux_phase_increment = phase_increment_;
      if (error >= 0x80000000) {
        error = ~error;
        if (error > phase_increment_) {
          error = phase_increment_;
        }
        aux_phase_increment -= (error >> 1);
      } else {
        if (error > phase_increment_) {
          error = phase_increment_;
        }
        aux_phase_increment += (error >> 1);
      }

      aux_phase += aux_phase_increment;
      if (aux_phase < aux_phase_increment) {
        AddBlep(aux_phase, aux_phase_increment, previous_sample_aux);
      }
      if (wrap) {
        AddBlep(phase_, phase_increment_, previous_sample);
      }

      previous_sample = phase_ >> 18;
      previous_sample_aux = aux_phase >> 18;
      int32_t output = previous_sample + previous_sample_aux - 16384;
      ACCUMULATE_BLEP(0)
      ACCUMULATE_BLEP(1)
      *buffer = output;
    }
    if (pitch_ > kBlepTransitionStart) {
      uint16_t sine_gain = (pitch_ - kBlepTransitionStart) << 6;
      if (pitch_ >= kBlepTransitionEnd) {
        sine_gain = 65535;
      }
      int16_t sine = wav_sine[phase_ >> 24] >> 1;
      *buffer = Mix(*buffer, sine, sine_gain);
    }
    buffer++;
  }    
  state_.aux_phase = aux_phase;
}

void AnalogOscillator::RenderCSaw(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  while (size--) {
    phase_ += phase_increment_;
    if (*sync_in++) {
      phase_ = 0;
    }
    bool wrap = phase_ < phase_increment_;
    if (pitch_ < kBlepTransitionEnd) {
      if (wrap) {
        state_.aux_phase = parameter_;
        state_.phase_remainder = phase_;
        state_.aux_shift = aux_parameter_;
        AddBlep(phase_, phase_increment_, 16384 + state_.aux_shift);
      }
      int32_t output = -8192;
      if (state_.aux_phase) {
        --state_.aux_phase;
        if (state_.aux_phase == 0) {
          AddBlep(
              state_.phase_remainder,
              phase_increment_,
              -(phase_ >> 18) - state_.aux_shift);
          output += (phase_ >> 18);
        } else if (phase_ > (1UL << 30)) {
          AddBlep(
              phase_ - (1UL << 30),
              phase_increment_,
              -(phase_ >> 18) - state_.aux_shift);
          output += (phase_ >> 18);
          state_.aux_phase = 0;
        } else {
          output -= state_.aux_shift;
        }
      } else {
        output += (phase_ >> 18);
      }
      ACCUMULATE_BLEP(0)
      ACCUMULATE_BLEP(1)
      *buffer = output;
    }
    if (pitch_ > kBlepTransitionStart) {
      uint16_t sine_gain = (pitch_ - kBlepTransitionStart) << 6;
      if (pitch_ >= kBlepTransitionEnd) {
        sine_gain = 65535;
      }
      int16_t sine = wav_sine[phase_ >> 24] >> 1;
      *buffer = Mix(*buffer, sine, sine_gain);
    }
    buffer++;
  }
}

void AnalogOscillator::RenderSquare(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  if (parameter_ > 32384) {
    parameter_ = 32384;
  }
  uint32_t pw = static_cast<uint32_t>(32768 - parameter_) << 16;
  while (size--) {
    phase_ += phase_increment_;
    if (sync_out) {
      *sync_out++ = phase_ < phase_increment_;
    }
    if (*sync_in++) {
      phase_ = 0;
    }

    bool wrap = phase_ < phase_increment_;

    // Render this with BLEP.
    if (pitch_ < kBlepTransitionEnd) {
      if (state_.up) {
        // Add a blep from up to down when the phase exceeds the pulse width.
        if (phase_ >= pw) {
          AddBlep(phase_ - pw, phase_increment_, 32767);
          state_.up = false;
        }
      } else {
        // Add a blep from down to up when there is a phase reset.
        if (wrap) {
          AddBlep(phase_, phase_increment_, -32767);
          state_.up = true;
        }
      }
      int32_t output = state_.up ? 16383 : -16383;
      ACCUMULATE_BLEP(0)
      ACCUMULATE_BLEP(1)
      *buffer = output;
    }
    if (pitch_ > kBlepTransitionStart) {
      uint16_t sine_gain = (pitch_ - kBlepTransitionStart) << 6;
      if (pitch_ >= kBlepTransitionEnd) {
        sine_gain = 65535;
      }
      int16_t sine = wav_sine[phase_ >> 24] >> 1;
      *buffer = Mix(*buffer, sine, sine_gain);
    }
    *buffer = -*buffer;
    buffer++;
  }
}

void AnalogOscillator::RenderTriangle(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  uint32_t increment = phase_increment_ >> 1;
  uint32_t phase = phase_;
  while (size--) {
    int16_t triangle;
    uint16_t phase_16;
    
    if (*sync_in++) {
      phase = 0;
    }
    
    phase += increment;
    phase_16 = phase >> 16;
    triangle = (phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000);
    triangle += 32768;
    *buffer = triangle >> 1;
    
    phase += increment;
    phase_16 = phase >> 16;
    triangle = (phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000);
    triangle += 32768;
    *buffer++ += triangle >> 1;
  }
  phase_ = phase;
}

void AnalogOscillator::RenderSine(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  uint32_t phase = phase_;
  uint32_t increment = phase_increment_;
  while (size--) {
    phase += increment;
    if (*sync_in++) {
      phase = 0;
    }
    *buffer++ = Interpolate824(wav_sine, phase);
  }
  phase_ = phase;
}

void AnalogOscillator::RenderTriangleFold(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  uint32_t increment = phase_increment_ >> 1;
  uint32_t phase = phase_;
  
  BEGIN_INTERPOLATE_PARAMETER
  
  while (size--) {
    INTERPOLATE_PARAMETER
    
    uint16_t phase_16;
    int16_t triangle;
    int16_t gain = 2048 + (parameter * 30720 >> 15);
    
    if (*sync_in++) {
      phase = 0;
    }
    
    // 2x oversampled WF.
    phase += increment;
    phase_16 = phase >> 16;
    triangle = (phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000);
    triangle += 32768;
    triangle = triangle * gain >> 15;
    triangle = Interpolate88(ws_tri_fold, triangle + 32768);
    *buffer = triangle >> 1;
    
    phase += increment;
    phase_16 = phase >> 16;
    triangle = (phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000);
    triangle += 32768;
    triangle = triangle * gain >> 15;
    triangle = Interpolate88(ws_tri_fold, triangle + 32768);
    *buffer++ += triangle >> 1;
  }
  
  END_INTERPOLATE_PARAMETER
  
  phase_ = phase;
}

void AnalogOscillator::RenderSineFold(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  uint32_t increment = phase_increment_ >> 1;
  uint32_t phase = phase_;
  
  BEGIN_INTERPOLATE_PARAMETER
  
  while (size--) {
    INTERPOLATE_PARAMETER
    
    int16_t sine;
    int16_t gain = 2048 + (parameter * 30720 >> 15);
    
    if (*sync_in++) {
      phase = 0;
    }
    
    // 2x oversampled WF.
    phase += increment;
    sine = Interpolate824(wav_sine, phase);
    sine = sine * gain >> 15;
    sine = Interpolate88(ws_sine_fold, sine + 32768);
    *buffer = sine >> 1;
    
    phase += increment;
    sine = Interpolate824(wav_sine, phase);
    sine = sine * gain >> 15;
    sine = Interpolate88(ws_sine_fold, sine + 32768);
    *buffer++ += sine >> 1;
  }
  
  END_INTERPOLATE_PARAMETER
  
  phase_ = phase;
}

void AnalogOscillator::RenderBuzz(
    const uint8_t* sync_in,
    int16_t* buffer,
    uint8_t* sync_out,
    uint8_t size) {
  int32_t shifted_pitch = pitch_ + ((32767 - parameter_) >> 1);
  uint16_t crossfade = shifted_pitch << 6;
  size_t index = (shifted_pitch >> 10);
  if (index >= kNumZones) {
    index = kNumZones;
  }
  const int16_t* wave_1 = waveform_table[WAV_BANDLIMITED_COMB_0 + index];
  index += 1;
  if (index >= kNumZones) {
    index = kNumZones;
  }
  const int16_t* wave_2 = waveform_table[WAV_BANDLIMITED_COMB_0 + index];
  while (size--) {
    phase_ += phase_increment_;
    if (*sync_in++) {
      phase_ = 0;
    }
    *buffer++ = Crossfade(wave_1, wave_2, phase_, crossfade);
  }
}

/* static */
AnalogOscillator::RenderFn AnalogOscillator::fn_table_[] = {
  &AnalogOscillator::RenderSaw,
  &AnalogOscillator::RenderCSaw,
  &AnalogOscillator::RenderSquare,
  &AnalogOscillator::RenderTriangle,
  &AnalogOscillator::RenderSine,
  &AnalogOscillator::RenderTriangleFold,
  &AnalogOscillator::RenderSineFold,
  &AnalogOscillator::RenderBuzz,
};

}  // namespace braids
