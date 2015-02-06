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
// Tidal generator.

#include "tides/generator.h"

#include <algorithm>

#include "stmlib/utils/dsp.h"

#include "tides/resources.h"

// #define CORE_ONLY

namespace tides {

using namespace stmlib;

const int16_t kOctave = 12 * 128;
const uint16_t kSlopeBits = 12;
const uint32_t kSyncCounterMaxTime = 8 * 48000;

const int32_t kDownsampleCoefficient[4] = { 17162, 19069, 17162, 12140 };


/* static */
const FrequencyRatio Generator::frequency_ratios_[] = {
  { 1, 1 },
  { 5, 4 },
  { 4, 3 },
  { 3, 2 },
  { 5, 3 },
  { 2, 1 },
  { 3, 1 },
  { 4, 1 },
  { 6, 1 },
  { 8, 1 },
  { 12, 1 },
  { 16, 1 },
};

/* static */
const int16_t Generator::num_frequency_ratios_ = \
    sizeof(Generator::frequency_ratios_) / sizeof(FrequencyRatio);

void Generator::Init() {
  mode_ = GENERATOR_MODE_LOOPING;
  range_ = GENERATOR_RANGE_HIGH;
  clock_divider_ = 1;
  phase_ = 0;
  set_pitch(60 << 7);
  output_buffer_.Init();
  input_buffer_.Init();
  pattern_predictor_.Init();
  for (uint16_t i = 0; i < kBlockSize; ++i) {
    GeneratorSample s;
    s.flags = 0;
    s.unipolar = 0;
    s.bipolar = 0;
    output_buffer_.Overwrite(s);
    input_buffer_.Overwrite(0);
  }
  
  antialiasing_ = true;
  shape_ = 0;
  slope_ = 0;
  smoothed_slope_ = 0;
  smoothness_ = 0;
  
  previous_sample_.unipolar = previous_sample_.bipolar = 0;
  running_ = false;
  
  ClearFilterState();
  
  sync_counter_ = kSyncCounterMaxTime;
  frequency_ratio_.p = 1;
  frequency_ratio_.q = 1;
  sync_ = false;
  phase_increment_ = 9448928;
  local_osc_phase_increment_ = phase_increment_;
  target_phase_increment_ = phase_increment_;
}

void Generator::ComputeFrequencyRatio(int16_t pitch) {
  int16_t delta = previous_pitch_ - pitch;
  // Hysteresis for preventing glitchy transitions.
  if (delta < 96 && delta > -96) {
    return;
  }
  previous_pitch_ = pitch;
  // Corresponds to a 0V CV after calibration
  pitch -= (36 << 7);
  // The range of the control panel knob is 4 octaves.
  pitch = pitch * 12 / (48 << 7);
  bool swap = false;
  if (pitch < 0) {
    pitch = -pitch;
    swap = true;
  }
  if (pitch >= num_frequency_ratios_) {
    pitch = num_frequency_ratios_ - 1;
  }
  frequency_ratio_ = frequency_ratios_[pitch];
  if (swap) {
    frequency_ratio_.q = frequency_ratio_.p;
    frequency_ratio_.p = frequency_ratios_[pitch].q;
  }
}

uint32_t Generator::ComputePhaseIncrement(int16_t pitch) {
  int16_t num_shifts = 0;
  while (pitch < 0) {
    pitch += kOctave;
    --num_shifts;
  }
  while (pitch >= kOctave) {
    pitch -= kOctave;
    ++num_shifts;
  }
  // Lookup phase increment
  uint32_t a = lut_increments[pitch >> 4];
  uint32_t b = lut_increments[(pitch >> 4) + 1];
  uint32_t phase_increment = a + ((b - a) * (pitch & 0xf) >> 4);
  // Compensate for downsampling
  phase_increment *= clock_divider_;
  return num_shifts >= 0
      ? phase_increment << num_shifts
      : phase_increment >> -num_shifts;
}

int16_t Generator::ComputePitch(uint32_t phase_increment) {
  uint32_t first = lut_increments[0];
  uint32_t last = lut_increments[LUT_INCREMENTS_SIZE - 2];
  int16_t pitch = 0;
  
  if (phase_increment == 0) {
    phase_increment = 1;
  }
  
  phase_increment /= clock_divider_;
  while (phase_increment > last) {
    phase_increment >>= 1;
    pitch += kOctave;
  }
  while (phase_increment < first) {
    phase_increment <<= 1;
    pitch -= kOctave;
  }
  pitch += (std::lower_bound(
      lut_increments,
      lut_increments + LUT_INCREMENTS_SIZE,
      phase_increment) - lut_increments) << 4;
  return pitch;
}

int32_t Generator::ComputeCutoffFrequency(int16_t pitch, int16_t smoothness) {
  uint8_t shifts = clock_divider_;
  while (shifts > 1) {
    shifts >>= 1;
    pitch += kOctave;
  }
  int32_t frequency;
  if (smoothness > 0) {
    frequency = 256 << 7;
  } else if (smoothness > -16384) {
    int32_t start = pitch + (36 << 7);
    int32_t end = 256 << 7;
    frequency = start + ((end - start) * (smoothness + 16384) >> 14);
  } else {
    int32_t start = pitch - (36 << 7);
    int32_t end = pitch + (36 << 7);
    frequency = start + ((end - start) * (smoothness + 32768) >> 14);
  }
  frequency += 32768;
  if (frequency < 0) {
    frequency = 0;
  }
  return frequency;
}

int32_t Generator::ComputeAntialiasAttenuation(
    int16_t pitch,
    int16_t slope,
    int16_t shape,
    int16_t smoothness) {
  pitch += 128;
  if (pitch < 0) pitch = 0;
  if (slope < 0) slope = -slope;
  if (shape < 0) shape = -shape;
  if (smoothness < 0) smoothness = 0;

  int32_t p = 252059;
  p += -76 * smoothness >> 5;
  p += -30 * shape >> 5;
  p += -102 * slope >> 5;
  p += -664 * pitch >> 5;
  p += 31 * (smoothness * shape >> 16) >> 5;
  p += 12 * (smoothness * slope >> 16) >> 5;
  p += 14 * (shape * slope >> 16) >> 5;
  p += 219 * (pitch * smoothness >> 16) >> 5;
  p += 50 * (pitch * shape >> 16) >> 5;
  p += 425 * (pitch * slope >> 16) >> 5;
  p += 13 * (smoothness * smoothness >> 16) >> 5;
  p += 1 * (shape * shape >> 16) >> 5;
  p += -11 * (slope * slope >> 16) >> 5;
  p += 776 * (pitch * pitch >> 16) >> 5;
  if (p < 0) p = 0;
  if (p > 32767) p = 32767;
  return p;
}

// There are to our knowledge three ways of generating an "asymmetric" ramp:
//
// 1. Use the difference between two parabolic waves.
//
// + Anti-aliasing is easy with wavetables of band-limited parabolic waves.
// + Slope modulation does not cause discontinuities.
// - Does not allow a different waveshape to be used for the A and D segments.
// - Needs gain compensation at the extreme settings of the slope parameter.
// - Does not traverse the full 0 .. 65535 range due to inaccuracies in gain
//   factor.
//
// 2. Use different phase increments for the A and D segments.
//
// + Slope modulation does not cause discontinuities.
// + Traverses the full 0 .. 65535 range.
// - Due to rounding errors, the duration of the A+D segment is not preserved
//   exactly when the slope is modulated.
// - No anti-aliasing.
// 
// 3. Generate a ramp and waveshape it (phase distortion).
//
// + Duration of A+D segment is preserved.
// + Traverses the full 0 .. 65535 range.
// - No anti-aliasing.
// - Slope modulations causes waveform discontinuities.
//
// 
// We use 1. for the highest range (audio rates); and 3 for the two other ranges
// (control rates extending into audio territory). To compensate for the slope
// modulation discontinuities, we low-pass filter digitally the slope value.
// 2. has a terrible behaviour in the audio range, because it causes audible FM
// when the slope parameter is modulated by a LFO.

void Generator::FillBufferAudioRate() {
  uint8_t size = kBlockSize;
  
  GeneratorSample sample = previous_sample_;
  if (sync_) {
    pitch_ = ComputePitch(phase_increment_);
  } else {
    phase_increment_ = ComputePhaseIncrement(pitch_);
    local_osc_phase_increment_ = phase_increment_;
    target_phase_increment_ = phase_increment_;
  }
  if (pitch_ < 0) {
    pitch_ = 0;
  }

#ifndef CORE_ONLY
  // Load wavetable pointers for bandlimiting - they depend on pitch value.
  uint16_t xfade = pitch_ << 6;
  uint16_t index = pitch_ >> 10;
  if (pitch_ < 0) {
    index = 0;
    xfade = 0;
  }
  
  const int16_t* wave_1 = waveform_table[WAV_BANDLIMITED_PARABOLA_0 + index];
  const int16_t* wave_2 = waveform_table[WAV_BANDLIMITED_PARABOLA_0 + index + 1];

  int32_t gain = slope_;
  gain = (32768 - (gain * gain >> 15)) * 3 >> 1;
  gain = 32768 * 1024 / gain;
  
  uint32_t phase_offset_a_bi = (slope_ - (slope_ >> 1)) << 16;
  uint32_t phase_offset_b_bi = (32768 - (slope_ >> 1)) << 16;
  uint32_t phase_offset_a_uni = 49152 << 16;
  uint32_t phase_offset_b_uni = (32768 + 49152 - slope_) << 16;
  
  int32_t attenuation = 32767;
  if (antialiasing_) {
    attenuation = ComputeAntialiasAttenuation(
          pitch_,
          slope_,
          shape_,
          smoothness_);
  }

  uint16_t shape = static_cast<uint16_t>((shape_ * attenuation >> 15) + 32768);
  uint16_t wave_index = WAV_INVERSE_TAN_AUDIO + (shape >> 14);
  const int16_t* shape_1 = waveform_table[wave_index];
  const int16_t* shape_2 = waveform_table[wave_index + 1];
  uint16_t shape_xfade = shape << 2;
  
  int32_t frequency = ComputeCutoffFrequency(pitch_, smoothness_);
  int32_t f_a = lut_cutoff[frequency >> 7] >> 16;
  int32_t f_b = lut_cutoff[(frequency >> 7) + 1] >> 16;
  int32_t f = f_a + ((f_b - f_a) * (frequency & 0x7f) >> 7);
  int32_t wf_gain = 2048;
  int32_t wf_balance = 0;
  if (smoothness_ > 0) {
    int16_t attenuated_smoothness = smoothness_ * attenuation >> 15;
    wf_gain += attenuated_smoothness * (32767 - 1024) >> 14;
    wf_balance = attenuated_smoothness;
  }
#endif  // CORE_ONLY  
  
  uint32_t end_of_attack = (static_cast<uint32_t>(slope_ + 32768) << 16);
  
  // Load state into registers - saves some memory load/store inside the
  // rendering loop.
  uint32_t phase = phase_;
  uint32_t phase_increment = phase_increment_;
  bool wrap = wrap_;
  int32_t uni_lp_state_0 = uni_lp_state_[0];
  int32_t uni_lp_state_1 = uni_lp_state_[1];
  int32_t bi_lp_state_0 = bi_lp_state_[0];
  int32_t bi_lp_state_1 = bi_lp_state_[1];
  
  // Enforce that the EOA pulse is at least 1 sample wide.
  if (end_of_attack >= phase_increment) {
    end_of_attack -= phase_increment;
  }
  if (end_of_attack < phase_increment) {
    end_of_attack = phase_increment;
  }
  
  while (size--) {
    ++sync_counter_;
    uint8_t control = input_buffer_.ImmediateRead();

    // When freeze is high, discard any start/reset command.
    if (!(control & CONTROL_FREEZE)) {
      if (control & CONTROL_GATE_RISING) {
        phase = 0;
        running_ = true;
      } else if (mode_ != GENERATOR_MODE_LOOPING && wrap) {
        phase = 0;
        running_ = false;
      }
    }
    
    if (sync_) {
      if (control & CONTROL_CLOCK_RISING) {
        ++sync_edges_counter_;
        if (sync_edges_counter_ >= frequency_ratio_.q) {
          sync_edges_counter_ = 0;
          if (sync_counter_ < kSyncCounterMaxTime && sync_counter_) {
            uint64_t increment = frequency_ratio_.p * static_cast<uint64_t>(
                0xffffffff / sync_counter_);
            if (increment > 0x80000000) {
              increment = 0x80000000;
            }
            target_phase_increment_ = static_cast<uint32_t>(increment);
            local_osc_phase_ = 0;
          }
          sync_counter_ = 0;
        }
      }
      // Fast tracking of the local oscillator to the external oscillator.
      local_osc_phase_increment_ += static_cast<int32_t>(
          target_phase_increment_ - local_osc_phase_increment_) >> 8;
      local_osc_phase_ += local_osc_phase_increment_;
      
      // Slow phase realignment between the master oscillator and the local
      // oscillator.
      int32_t phase_error = local_osc_phase_ - phase;
      phase_increment = local_osc_phase_increment_ + (phase_error >> 13);
    }
    
    if (control & CONTROL_FREEZE) {
      output_buffer_.Overwrite(sample);
      continue;
    }
    
    bool sustained = mode_ == GENERATOR_MODE_AR
        && phase >= (1UL << 31)
        && control & CONTROL_GATE;

    if (sustained) {
      phase = 1L << 31;
    }

#ifndef CORE_ONLY
    // Bipolar version ---------------------------------------------------------
    int32_t ramp_a, ramp_b, saw;
    int32_t original, folded;
    ramp_a = Crossfade1022(wave_1, wave_2, phase + phase_offset_a_bi, xfade);
    ramp_b = Crossfade1022(wave_1, wave_2, phase + phase_offset_b_bi, xfade);
    saw = (ramp_b - ramp_a) * gain >> 10;
    CLIP(saw);
    
    // Appy shape waveshaper.
    saw = Crossfade115(shape_1, shape_2, saw + 32768, shape_xfade);
    if (!running_ && !sustained) {
      saw = 0;
    }

    // Run through LPF.
    bi_lp_state_0 += f * (saw - bi_lp_state_0) >> 15;
    bi_lp_state_1 += f * (bi_lp_state_0 - bi_lp_state_1) >> 15;
    
    // Fold.
    original = bi_lp_state_1;
    folded = Interpolate1022(wav_bipolar_fold, original * wf_gain + (1UL << 31));
    sample.bipolar = original + ((folded - original) * wf_balance >> 15);

    // Unipolar version --------------------------------------------------------
    ramp_a = Crossfade1022(wave_1, wave_2, phase + phase_offset_a_uni, xfade);
    ramp_b = Crossfade1022(wave_1, wave_2, phase + phase_offset_b_uni, xfade);
    saw = (ramp_b - ramp_a) * gain >> 10;
    CLIP(saw)
    
    // Appy shape waveshaper.
    saw = Crossfade115(shape_1, shape_2, (saw >> 1) + 32768 + 16384,
                       shape_xfade);
    if (!running_ && !sustained) {
      saw = 0;
    }
    // Run through LPF.
    uni_lp_state_0 += f * (saw - uni_lp_state_0) >> 15;
    uni_lp_state_1 += f * (uni_lp_state_0 - uni_lp_state_1) >> 15;
    
    // Fold.
    original = uni_lp_state_1 << 1;
    folded = Interpolate1022(wav_unipolar_fold, original * wf_gain) << 1;
    sample.unipolar = original + ((folded - original) * wf_balance >> 15);
#else    
    sample.bipolar = (phase >> 16) - 32768;
    sample.unipolar = phase >> 16;
#endif  // CORE_ONLY
    
    sample.flags = 0;
    bool looped = mode_ == GENERATOR_MODE_LOOPING && wrap;
    if (phase >= end_of_attack || !running_) {
      sample.flags |= FLAG_END_OF_ATTACK;
    }
    if (!running_ || looped) {
      eor_counter_ = phase_increment < 44739242 ? 48 : 1;
    }
    if (eor_counter_) {
      sample.flags |= FLAG_END_OF_RELEASE;
      --eor_counter_;
    }
    output_buffer_.Overwrite(sample);
    
    if (running_ && !sustained) {
      phase += phase_increment;
      wrap = phase < phase_increment;
    }
  }
  
  uni_lp_state_[0] = uni_lp_state_0;
  uni_lp_state_[1] = uni_lp_state_1;
  bi_lp_state_[0] = bi_lp_state_0;
  bi_lp_state_[1] = bi_lp_state_1;
  
  previous_sample_ = sample;
  phase_ = phase;
  phase_increment_ = phase_increment;
  wrap_ = wrap;
}

void Generator::FillBufferControlRate() {
  uint8_t size = kBlockSize;
  
  if (sync_) {
    pitch_ = ComputePitch(phase_increment_);
  } else {
    phase_increment_ = ComputePhaseIncrement(pitch_);
    local_osc_phase_increment_ = phase_increment_;
    target_phase_increment_ = phase_increment_;
  }
  
  GeneratorSample sample = previous_sample_;

#ifndef CORE_ONLY  
  uint16_t shape = static_cast<uint16_t>(shape_ + 32768);
  shape = (shape >> 2) * 3;
  uint16_t wave_index = WAV_REVERSED_CONTROL + (shape >> 13);
  const int16_t* shape_1 = waveform_table[wave_index];
  const int16_t* shape_2 = waveform_table[wave_index + 1];
  uint16_t shape_xfade = shape << 3;
  
  int64_t frequency = ComputeCutoffFrequency(pitch_, smoothness_);
  int64_t f_a = lut_cutoff[frequency >> 7];
  int64_t f_b = lut_cutoff[(frequency >> 7) + 1];
  int64_t f = f_a + ((f_b - f_a) * (frequency & 0x7f) >> 7);
  int32_t wf_gain = 2048;
  int32_t wf_balance = 0;
  if (smoothness_ > 0) {
    wf_gain += smoothness_ * (32767 - 1024) >> 14;
    wf_balance = smoothness_;
  }
#endif  // CORE_ONLY  

  // Load state into registers - saves some memory load/store inside the
  // rendering loop.
  uint32_t phase = phase_;
  uint32_t phase_increment = phase_increment_;
  bool wrap = wrap_;
  int32_t smoothed_slope = smoothed_slope_;
  int64_t uni_lp_state_0 = uni_lp_state_[0];
  int64_t uni_lp_state_1 = uni_lp_state_[1];
  int64_t bi_lp_state_0 = bi_lp_state_[0];
  int64_t bi_lp_state_1 = bi_lp_state_[1];
  int32_t previous_smoothed_slope = 0x7fffffff;
  uint32_t end_of_attack = 1UL << 31;
  uint32_t attack_factor = 1 << kSlopeBits;
  uint32_t decay_factor = 1 << kSlopeBits;
  
  while (size--) {
    sync_counter_++;
    // Low-pass filter the slope parameter.
    smoothed_slope += (slope_ - smoothed_slope) >> 4;
    
    uint8_t control = input_buffer_.ImmediateRead();

    // When freeze is high, discard any start/reset command.
    if (!(control & CONTROL_FREEZE)) {
      if (control & CONTROL_GATE_RISING) {
        phase = 0;
        running_ = true;
      } else if (mode_ != GENERATOR_MODE_LOOPING && wrap) {
        running_ = false;
        phase = 0;
      }
    }
    
    if ((control & CONTROL_CLOCK_RISING) && sync_ && sync_counter_) {
      if (sync_counter_ >= kSyncCounterMaxTime) {
        phase = 0;
      } else {
        uint32_t predicted_period = pattern_predictor_.Predict(sync_counter_);
        uint64_t increment = frequency_ratio_.p * static_cast<uint64_t>(
            0xffffffff / (predicted_period * frequency_ratio_.q));
        if (increment > 0x80000000) {
          increment = 0x80000000;
        }
        phase_increment = static_cast<uint32_t>(increment);
      }
      sync_counter_ = 0;
    }
    
    if (control & CONTROL_FREEZE) {
      output_buffer_.Overwrite(sample);
      continue;
    }
    
    // Recompute the waveshaping parameters only when the slope has changed.
    if (smoothed_slope != previous_smoothed_slope) {
       uint32_t slope_offset = Interpolate88(
            lut_slope_compression, smoothed_slope + 32768);
      if (slope_offset <= 1) {
        decay_factor = 32768 << kSlopeBits;
        attack_factor = 1 << (kSlopeBits - 1);
      } else {
        decay_factor = (32768 << kSlopeBits) / slope_offset;
        attack_factor = (32768 << kSlopeBits) / (65536 - slope_offset);
      }
      previous_smoothed_slope = smoothed_slope;
      end_of_attack = slope_offset << 16;
    }
    
    uint32_t skewed_phase = phase;
    if (phase <= end_of_attack) {
      skewed_phase = (phase >> kSlopeBits) * decay_factor;
    } else {
      skewed_phase = ((phase - end_of_attack) >> kSlopeBits) * attack_factor;
      skewed_phase += 1L << 31;
    }

    bool sustained = mode_ == GENERATOR_MODE_AR
        && phase >= end_of_attack
        && control & CONTROL_GATE;

    if (sustained) {
      skewed_phase = 1L << 31;
      phase = end_of_attack + 1;
    }

#ifndef CORE_ONLY  
    int32_t original, folded;
    int32_t unipolar = Crossfade115(
        shape_1,
        shape_2,
        skewed_phase >> 16, shape_xfade);
    uni_lp_state_0 += f * ((unipolar << 16) - uni_lp_state_0) >> 31;
    uni_lp_state_1 += f * (uni_lp_state_0 - uni_lp_state_1) >> 31;
    
    original = uni_lp_state_1 >> 15;
    folded = Interpolate1022(wav_unipolar_fold, original * wf_gain) << 1;
    sample.unipolar = original + ((folded - original) * wf_balance >> 15);
    
    int32_t bipolar = Crossfade115(
        shape_1,
        shape_2,
        skewed_phase >> 15, shape_xfade);
    if (skewed_phase >= (1UL << 31)) {
      bipolar = -bipolar;
    }
    
    bi_lp_state_0 += f * ((bipolar << 16) - bi_lp_state_0) >> 31;
    bi_lp_state_1 += f * (bi_lp_state_0 - bi_lp_state_1) >> 31;
    
    original = bi_lp_state_1 >> 16;
    folded = Interpolate1022(wav_bipolar_fold, original * wf_gain + (1UL << 31));
    sample.bipolar = original + ((folded - original) * wf_balance >> 15);

#else    
    sample.bipolar = (skewed_phase >> 16) - 32768;
    sample.unipolar = skewed_phase >> 16;
#endif  // CORE_ONLY

    uint32_t adjusted_end_of_attack = end_of_attack;
    if (adjusted_end_of_attack >= phase_increment) {
      adjusted_end_of_attack -= phase_increment;
    }
    if (adjusted_end_of_attack < phase_increment) {
      adjusted_end_of_attack = phase_increment;
    }

    sample.flags = 0;
    bool looped = mode_ == GENERATOR_MODE_LOOPING && wrap;
    if (phase >= adjusted_end_of_attack || !running_ || sustained) {
      sample.flags |= FLAG_END_OF_ATTACK;
    }
    if (!running_ || looped) {
      eor_counter_ = phase_increment < 44739242 ? 48 : 1;
    }
    if (eor_counter_) {
      sample.flags |= FLAG_END_OF_RELEASE;
      --eor_counter_;
    }
    // Two special cases for the "pure decay" scenario:
    // END_OF_ATTACK is always true except at the initial trigger.
    if (end_of_attack == 0) {
      sample.flags |= FLAG_END_OF_ATTACK;
    }
    bool triggered = control & CONTROL_GATE_RISING;
    if ((sustained || end_of_attack == 0) && (triggered || looped)) {
      sample.flags &= ~FLAG_END_OF_ATTACK;
    }
    
    output_buffer_.Overwrite(sample);
    if (running_ && !sustained) {
      phase += phase_increment;
      wrap = phase < phase_increment;
    } else {
      wrap = false;
    }
  }

  uni_lp_state_[0] = uni_lp_state_0;
  uni_lp_state_[1] = uni_lp_state_1;
  bi_lp_state_[0] = bi_lp_state_0;
  bi_lp_state_[1] = bi_lp_state_1;
  
  previous_sample_ = sample;
  phase_ = phase;
  phase_increment_ = phase_increment;
  wrap_ = wrap;
  smoothed_slope_ = smoothed_slope;
}


void Generator::FillBufferWavetable() {
  uint8_t size = kBlockSize;
  
  GeneratorSample sample = previous_sample_;
  if (sync_) {
    pitch_ = ComputePitch(phase_increment_);
  } else {
    phase_increment_ = ComputePhaseIncrement(pitch_);
  }

  uint32_t phase = phase_;
  uint32_t sub_phase = sub_phase_;
  uint32_t phase_increment = phase_increment_;
  
  // The grid is only 8x8 rather than 9x9 so we need to scale by 7/8.0
  uint16_t target_x = static_cast<uint16_t>(slope_ + 32768);
  target_x = target_x * 57344 >> 16;
  uint16_t x = x_;
  uint16_t x_increment = (target_x - x) / size;

  uint16_t target_y = static_cast<uint16_t>(shape_ + 32768);
  target_y = target_y * 57344 >> 16;
  uint16_t y = y_;
  uint16_t y_increment = (target_y - y) / size;

  int32_t wf_gain = smoothness_ > 0 ? smoothness_ : 0;
  wf_gain = wf_gain * wf_gain >> 15;
  
  int32_t frequency = ComputeCutoffFrequency(pitch_, smoothness_);
  int32_t f_a = lut_cutoff[frequency >> 7] >> 16;
  int32_t f_b = lut_cutoff[(frequency >> 7) + 1] >> 16;
  int32_t f = f_a + ((f_b - f_a) * (frequency & 0x7f) >> 7);
  int32_t lp_state_0 = bi_lp_state_[0];
  int32_t lp_state_1 = bi_lp_state_[1];
  
  const int16_t* bank = wt_waves + mode_ * 64 * 257 - (mode_ & 2) * 4 * 257;
  while (size--) {
    ++sync_counter_;
    uint8_t control = input_buffer_.ImmediateRead();
    
    // When freeze is high, discard any start/reset command.
    if (!(control & CONTROL_FREEZE)) {
      if (control & CONTROL_GATE_RISING) {
        phase = 0;
        sub_phase = 0;
      }
    }
    
    if (control & CONTROL_CLOCK_RISING) {
      if (sync_) {
        if (range_ == GENERATOR_RANGE_HIGH) {
          ++sync_edges_counter_;
          if (sync_edges_counter_ >= frequency_ratio_.q) {
            sync_edges_counter_ = 0;
            if (sync_counter_ < kSyncCounterMaxTime && sync_counter_) {
              uint64_t increment = frequency_ratio_.p * static_cast<uint64_t>(
                  0xffffffff / sync_counter_);
              if (increment > 0x80000000) {
                increment = 0x80000000;
              }
              target_phase_increment_ = static_cast<uint32_t>(increment);
              local_osc_phase_ = 0;
            }
            sync_counter_ = 0;
          }
        } else {
          if (sync_counter_ >= kSyncCounterMaxTime) {
            phase = 0;
          } else if (sync_counter_) {
            uint32_t predicted_period = sync_counter_ < 480
                ? sync_counter_
                : pattern_predictor_.Predict(sync_counter_);
            uint64_t increment = frequency_ratio_.p * static_cast<uint64_t>(
                0xffffffff / (predicted_period * frequency_ratio_.q));
            if (increment > 0x80000000) {
              increment = 0x80000000;
            }
            phase_increment = static_cast<uint32_t>(increment);
          }
          sync_counter_ = 0;
        }
      } else {
        // Normal behaviour: switch banks.
        uint8_t bank_index = mode_ + 1;
        if (bank_index > 2) {
          bank_index = 0;
        }
        mode_ = static_cast<GeneratorMode>(bank_index);
        bank = wt_waves + mode_ * 64 * 257 - (mode_ & 2) * 4 * 257;
      }
    }
    
    // PLL stuff
    if (sync_ && range_ == GENERATOR_RANGE_HIGH) {
      // Fast tracking of the local oscillator to the external oscillator.
      local_osc_phase_increment_ += static_cast<int32_t>(
          target_phase_increment_ - local_osc_phase_increment_) >> 8;
      local_osc_phase_ += local_osc_phase_increment_;
    
      // Slow phase realignment between the master oscillator and the local
      // oscillator.
      int32_t phase_error = local_osc_phase_ - phase;
      phase_increment = local_osc_phase_increment_ + (phase_error >> 13);
    }
    
    x += x_increment;
    y += y_increment;
  
    if (control & CONTROL_FREEZE) {
      output_buffer_.Overwrite(sample);
      continue;
    }
    
    uint16_t x_integral = x >> 13;
    uint16_t y_integral = y >> 13;
    const int16_t* wave_1 = &bank[(x_integral + y_integral * 8) * 257];
    const int16_t* wave_2 = wave_1 + 257 * 8;
    uint16_t x_fractional = x << 3;
    int32_t y_fractional = (y << 2) & 0x7fff;

    int32_t s = 0;
    for (int32_t subsample = 0; subsample < 4; ++subsample) {
      int32_t y_1 = Crossfade(wave_1, wave_1 + 257, phase, x_fractional);
      int32_t y_2 = Crossfade(wave_2, wave_2 + 257, phase, x_fractional);
      int32_t y_mix = y_1 + ((y_2 - y_1) * y_fractional >> 15);
      int32_t folded = Interpolate1022(
          ws_smooth_bipolar_fold, (y_mix + 32768) << 16);
      y_mix = y_mix + ((folded - y_mix) * wf_gain >> 15);
      s += y_mix * kDownsampleCoefficient[subsample];
      phase += (phase_increment >> 2);
    }
    
    lp_state_0 += f * ((s >> 16) - lp_state_0) >> 15;
    lp_state_1 += f * (lp_state_0 - lp_state_1) >> 15;
    
    sample.bipolar = lp_state_1;
    sample.unipolar = sample.bipolar + 32768;
    sample.flags = 0;
    if (sample.unipolar & 0x8000) {
      sample.flags |= FLAG_END_OF_ATTACK;
    }
    if (sub_phase & 0x80000000) {
      sample.flags |= FLAG_END_OF_RELEASE;
    }
    output_buffer_.Overwrite(sample);
    sub_phase += phase_increment >> 1;
  }
  previous_sample_ = sample;
  phase_ = phase;
  sub_phase_ = sub_phase;
  phase_increment_ = phase_increment;
  x_ = x;
  y_ = y;
  bi_lp_state_[0] = lp_state_0;
  bi_lp_state_[1] = lp_state_1;
}

}  // namespace tides
