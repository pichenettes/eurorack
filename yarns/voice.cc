// Copyright 2013 Olivier Gillet.
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
// Voice.

#include "yarns/voice.h"

#include <algorithm>
#include <cstdio>

#include "stmlib/midi/midi.h"
#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "yarns/resources.h"

namespace yarns {
  
using namespace stmlib;
using namespace stmlib_midi;

const int32_t kOctave = 12 << 7;
const int32_t kMaxNote = 120 << 7;

void Voice::Init() {
  note_ = -1;
  note_source_ = note_target_ = note_portamento_ = 60 << 7;
  gate_ = false;
  
  mod_velocity_ = 0;
  ResetAllControllers();
  
  modulation_rate_ = 0;
  pitch_bend_range_ = 2;
  vibrato_range_ = 0;
  
  lfo_phase_ = portamento_phase_ = 0;
  portamento_phase_increment_ = 1U << 31;
  
  trigger_duration_ = 2;
  for (uint8_t i = 0; i < kNumOctaves; ++i) {
    calibrated_dac_code_[i] = 54586 - 5133 * i;
  }
  dirty_ = false;
  
  audio_buffer_.Init();
}

void Voice::Calibrate(uint16_t* calibrated_dac_code) {
  std::copy(
      &calibrated_dac_code[0],
      &calibrated_dac_code[kNumOctaves],
      &calibrated_dac_code_[0]);
}

inline uint16_t Voice::NoteToDacCode(int32_t note) const {
  if (note <= 0) {
    note = 0;
  }
  if (note >= kMaxNote) {
    note = kMaxNote - 1;
  }
  uint8_t octave = 0;
  while (note >= kOctave) {
    note -= kOctave;
    ++octave;
  }
  
  // Note is now between 0 and kOctave
  // Octave indicates the octave. Look up in the DAC code table.
  int32_t a = calibrated_dac_code_[octave];
  int32_t b = calibrated_dac_code_[octave + 1];
  return a + ((b - a) * note / kOctave);
}

void Voice::ResetAllControllers() {
  mod_pitch_bend_ = 8192;
  mod_wheel_ = 0;
  std::fill(&mod_aux_[0], &mod_aux_[4], 0);
}

void Voice::Refresh() {
  // Compute base pitch with portamento.
  portamento_phase_ += portamento_phase_increment_;
  if (portamento_phase_ < portamento_phase_increment_) {
    portamento_phase_ = 0;
    portamento_phase_increment_ = 0;
    note_source_ = note_target_;
  }
  uint16_t portamento_level = Interpolate824(lut_env_expo, portamento_phase_);
  int32_t note = note_source_ + \
      ((note_target_ - note_source_) * portamento_level >> 16);

  note_portamento_ = note;
  
  // Add pitch-bend.
  note += static_cast<int32_t>(mod_pitch_bend_ - 8192) * pitch_bend_range_ >> 6;
  
  // Add transposition/fine tuning.
  note += tuning_;
  
  // Add vibrato.
  lfo_phase_ += lut_lfo_increments[modulation_rate_];
  int32_t lfo = lfo_phase_ < 1UL << 31
      ?  -32768 + (lfo_phase_ >> 15)
      : 0x17fff - (lfo_phase_ >> 15);
  note += lfo * mod_wheel_ * vibrato_range_ >> 15;
  mod_aux_[3] = (lfo * mod_wheel_ >> 15) + 128;
  
  if (retrigger_delay_) {
    --retrigger_delay_;
  }
  
  if (trigger_pulse_) {
    --trigger_pulse_;
  }
  
  if (trigger_phase_increment_) {
    trigger_phase_ += trigger_phase_increment_;
    if (trigger_phase_ < trigger_phase_increment_) {
      trigger_phase_ = 0;
      trigger_phase_increment_ = 0;
    }
  }
  if (note != note_ || dirty_) {
    note_dac_code_ = NoteToDacCode(note);
    note_ = note;
    dirty_ = false;
  }
}

void Voice::NoteOn(
    int16_t note,
    uint8_t velocity,
    uint8_t portamento,
    bool trigger) {
  note_source_ = note_portamento_;  
  note_target_ = note;
  if (!portamento) {
    note_source_ = note_target_;
  }
  portamento_phase_increment_ = lut_portamento_increments[portamento];
  portamento_phase_ = 0;

  mod_velocity_ = velocity;

  if (gate_ && trigger) {
    retrigger_delay_ = 2;
  }
  if (trigger) {
    trigger_pulse_ = trigger_duration_ * 8;
    trigger_phase_ = 0;
    trigger_phase_increment_ = lut_portamento_increments[trigger_duration_];
  }
  gate_ = true;
}

void Voice::NoteOff() {
  gate_ = false;
}

void Voice::ControlChange(uint8_t controller, uint8_t value) {
  switch (controller) {
    case kCCModulationWheelMsb:
      mod_wheel_ = value;
      break;
    
    case kCCBreathController:
      mod_aux_[1] = value << 1;
      break;
      
    case kCCFootPedalMsb:
      mod_aux_[2] = value << 1;
      break;
  }
}

uint16_t Voice::trigger_dac_code() const {
  if (trigger_phase_ <= trigger_phase_increment_) {
    return calibrated_dac_code_[3]; // 0V.
  } else {
    int32_t velocity_coefficient = trigger_scale_ ? mod_velocity_ << 8 : 32768;
    int32_t value = 0;
    switch(trigger_shape_) {
      case TRIGGER_SHAPE_SQUARE:
        value = 32767;
        break;
      case TRIGGER_SHAPE_LINEAR:
        value = 32767 - (trigger_phase_ >> 17);
        break;
      default:
        {
          const int16_t* table = waveform_table[
              trigger_shape_ - TRIGGER_SHAPE_EXPONENTIAL];
          value = Interpolate824(table, trigger_phase_);
        }
        break;
    }
    value = value * velocity_coefficient >> 15;
    int32_t max = calibrated_dac_code_[8];
    int32_t min = calibrated_dac_code_[3];
    return min + ((max - min) * value >> 15);
  }
}

static const uint16_t kHighestNote = 128 * 128;
static const uint16_t kPitchTableStart = 116 * 128;
static const uint8_t kNumZones = 6;

uint32_t Voice::ComputePhaseIncrement(int16_t midi_pitch) {
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

struct Wavetable {
  ResourceId first;
  uint16_t num_zones;
};

const Wavetable wavetables[] = {
  { WAV_BANDLIMITED_SAW_0, 7 },
  { WAV_BANDLIMITED_PULSE_0, 7 },
  { WAV_BANDLIMITED_SQUARE_0, 7 },
  { WAV_BANDLIMITED_TRIANGLE_0, 7 },
  { WAV_SINE, 1 },
  { WAV_SINE, 1 },
};

void Voice::FillAudioBuffer() {
  uint32_t phase = phase_;
  uint32_t phase_increment = ComputePhaseIncrement(note_);
  
  // Fill with blank if the note is off
  if ((audio_mode_ & 0x80) && !gate_) {
    size_t size = kAudioBlockSize;
    while (size--) {
      audio_buffer_.Overwrite(calibrated_dac_code_[3]);
    }
    return;
  }
  uint8_t wavetable_index = (audio_mode_ & 0x0f) - 1;
  const Wavetable& wavetable = wavetables[wavetable_index];
  
  int16_t note = note_ - (12 << 7);
  if (note < 0) {
    note = 0;
  }
  uint16_t crossfade = note << 5;
  int16_t first_zone = note >> 11;
  if (first_zone >= wavetable.num_zones) {
    first_zone = wavetable.num_zones - 1;
  }
  
  int16_t second_zone = first_zone + 1;
  if (second_zone >= wavetable.num_zones) {
    second_zone = wavetable.num_zones - 1;
  }
  
  const int16_t* wave_1 = waveform_table[wavetable.first + first_zone];
  const int16_t* wave_2 = waveform_table[wavetable.first + second_zone];

  int32_t scale = calibrated_dac_code_[3] - calibrated_dac_code_[8];
  int32_t offset = calibrated_dac_code_[3];
  size_t size = kAudioBlockSize;
  while (size--) {
    phase += phase_increment;
    int32_t sample = wavetable_index == 5 ?
        static_cast<int16_t>(Random::GetSample()) :
        Crossfade1022(wave_1, wave_2, phase, crossfade);
    uint16_t value = offset - (scale * sample >> 16);
    audio_buffer_.Overwrite(value);
  }
  phase_ = phase;
}

}  // namespace yarns