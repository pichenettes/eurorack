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
// Various "magic" conversion functions for DX7 patch data.

#ifndef PLAITS_DSP_DX_UNITS_H_
#define PLAITS_DSP_DX_UNITS_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/fm/patch.h"

namespace plaits {

namespace fm {

extern const float lut_cube_root[17];
extern const float lut_amp_mod_sensitivity[4];
extern const float lut_pitch_mod_sensitivity[8];
extern const float lut_coarse[32];

// Computes 2^x by using a polynomial approximation of 2^frac(x) and directly
// incrementing the exponent of the IEEE 754 representation of the result
// by int(x). Depending on the use case, the order of the polynomial
// approximation can be chosen.
template<int order>
inline float Pow2Fast(float x) {
  union {
    float f;
    int32_t w;
  } r;

  
  if (order == 1) {
    r.w = float(1 << 23) * (127.0f + x);
    return r.f;
  }
  
  int32_t x_integral = static_cast<int32_t>(x);
  if (x < 0.0f) {
    --x_integral;
  }
  x -= static_cast<float>(x_integral);

  if (order == 1) {
    r.f = 1.0f + x;
  } else if (order == 2) {
    r.f = 1.0f + x * (0.6565f + x * 0.3435f);
  } else if (order == 3) {
    r.f = 1.0f + x * (0.6958f + x * (0.2251f + x * 0.0791f));
  }
  r.w += x_integral << 23;
  return r.f;
}

// Convert an operator (envelope) level from 0-99 to the complement of the
// "TL" value.
//   0 =   0  (TL = 127)
//  20 =  48  (TL =  79)
//  50 =  78  (TL =  49)
//  99 = 127  (TL =   0)
inline int OperatorLevel(int level) {
  int tlc = int(level);
  if (level < 20) {
    tlc = tlc < 15 ? (tlc * (36 - tlc)) >> 3 : 27 + tlc;
  } else {
    tlc += 28;
  }
  return tlc;
}

// Convert an envelope level from 0-99 to an octave shift.
//  0 = -4 octave
// 18 = -1 octave
// 50 =  0
// 82 = +1 octave
// 99 = +4 octave
inline float PitchEnvelopeLevel(int level) {
  float l = (float(level) - 50.0f) / 32.0f;
  float tail = std::max(fabsf(l + 0.02f) - 1.0f, 0.0f);
  return l * (1.0f + tail * tail * 5.3056f);
}

// Convert an operator envelope rate from 0-99 to a frequency.
inline float OperatorEnvelopeIncrement(int rate) {
  int rate_scaled = (rate * 41) >> 6;
  int mantissa = 4 + (rate_scaled & 3);
  int exponent = 2 + (rate_scaled >> 2);
  return float(mantissa << exponent) / float(1 << 24);
}

// Convert a pitch envelope rate from 0-99 to a frequency.
inline float PitchEnvelopeIncrement(int rate) {
  float r = float(rate) * 0.01f;
  return (1.0f + 192.0f * r * (r * r * r * r + 0.3333f)) / (21.3f * 44100.0f);
}

const float kMinLFOFrequency = 0.005865f;

// Convert an LFO rate from 0-99 to a frequency.
inline float LFOFrequency(int rate) {
  int rate_scaled = rate == 0 ? 1 : (rate * 165) >> 6;
  rate_scaled *= rate_scaled < 160 ? 11 : (11 + ((rate_scaled - 160) >> 4));
  return float(rate_scaled) * kMinLFOFrequency;
}

// Convert an LFO delay from 0-99 to the two increments.
inline void LFODelay(int delay, float increments[2]) {
  if (delay == 0) {
    increments[0] = increments[1] = 100000.0f;
  } else {
    int d = 99 - delay;
    d = (16 + (d & 15)) << (1 + (d >> 4));
    increments[0] = float(d) * kMinLFOFrequency;
    increments[1] = float(std::max(0x80, d & 0xff80)) * kMinLFOFrequency;
  }
}

// Pre-process the velocity to easily compute the velocity scaling.
inline float NormalizeVelocity(float velocity) {
  // float cube_root = stmlib::Sqrt(
  //     0.7f * stmlib::Sqrt(velocity) + 0.3f * velocity);
  const float cube_root = stmlib::Interpolate(lut_cube_root, velocity, 16);
  return 16.0f * (cube_root - 0.918f);
}

// MIDI note to envelope increment ratio.
inline float RateScaling(float note, int rate_scaling) {
  return Pow2Fast<1>(
      float(rate_scaling) * (note * 0.33333f - 7.0f) * 0.03125f);
}

// Operator amplitude modulation sensitivity (0-3).
inline float AmpModSensitivity(int amp_mod_sensitivity) {
  return lut_amp_mod_sensitivity[amp_mod_sensitivity];
}

// Pitch modulation sensitivity (0-7).
inline float PitchModSensitivity(int pitch_mod_sensitivity) {
  return lut_pitch_mod_sensitivity[pitch_mod_sensitivity];
}

// Keyboard tracking to TL adjustment.
inline float KeyboardScaling(float note, const Patch::KeyboardScaling& ks) {
  const float x = note - float(ks.break_point) - 15.0f;
  const int curve = x > 0.0f ? ks.right_curve : ks.left_curve;

  float t = fabsf(x);
  if (curve == 1 || curve == 2) {
    t = std::min(t * 0.010467f, 1.0f);
    t = t * t * t;
    t *= 96.0f;
  }
  if (curve < 2) {
    t = -t;
  }

  float depth = float(x > 0.0f ? ks.right_depth : ks.left_depth);
  return t * depth * 0.02677f;
}

inline float FrequencyRatio(const Patch::Operator& op) {
  const float detune = op.mode == 0 && op.fine
      ? 1.0f + 0.01f * float(op.fine)
      : 1.0f;

  float base = op.mode == 0
      ? lut_coarse[op.coarse]
      : float(int(op.coarse & 3) * 100 + op.fine) * 0.39864f;
  base += (float(op.detune) - 7.0f) * 0.015f;

  return stmlib::SemitonesToRatioSafe(base) * detune;
}

}  // namespace fm
  
}  // namespace plaits

#endif  // PLAITS_DSP_DX_UNITS_H_
