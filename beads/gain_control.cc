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
// Converts raw 32-bit data from the codec into a FloatFrame, by applying
// the correct gain constant. Also responsible for filling in right
// channel data with left channel data (or the opposite) when either channel
// is left unpatched.

#include "beads/gain_control.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

namespace beads {

using namespace std;
using namespace stmlib;

const float kIgnoreTime = 1.0f;  // second
const float kActiveTime = 5.0f;  // second

const float kMinGain = 1.0f;
const float kMaxGain = 40.0f;

void GainControl::Init(
    float sample_rate,
    SoftClippingMode soft_clipping_mode,
    bool reset) {
  if (reset) {
    stable_agc_peak_ = agc_peak_ = peak_ = 0.5f;
    gain_ = 1.0f;
    soft_clipping_amount_ = 0.0f;
  }
  soft_clipping_mode_ = soft_clipping_mode;

  ignore_time_ = size_t(sample_rate * kIgnoreTime);

  bool no_readjust = !reset && time_ && time_ >= active_time_;
  active_time_ = size_t(sample_rate * (kIgnoreTime + kActiveTime));
  time_ = no_readjust ? active_time_ : 0;
  one_hertz_ = 1.0f / sample_rate;
}

void GainControl::Process(
    bool active,
    uint8_t gain_setting,
    bool left_normalization,
    bool right_normalization,
    const Codec::Frame* in,
    FloatFrame* out,
    size_t size) {
  // Nothing is patched in the module, we can safely fill the output buffer with
  // zeroes and take care of the mess later.
  if (left_normalization && right_normalization) {
    FloatFrame silence;
    silence.l = 0.0f;
    silence.r = 0.0f;
    fill(&out[0], &out[size], silence);

    peak_ = 0.0f;
    agc_peak_ = stable_agc_peak_ = 0.5f;
    gain_ = 1.0f;
    time_ = 0;
    return;
  }
  
  const float hz = float(size) * one_hertz_;
  if (time_ < ignore_time_) {
    agc_peak_ = 0.5f;
  } else if (time_ < active_time_) {
    SLOPE(
        agc_peak_,
        peak_,
        10.0f * hz,  // Reduce gain in 0.1s if a peak is detected
        2.0f * hz);  // Adjust gain in 0.5s if the level is too low
    stable_agc_peak_ = agc_peak_;
  } else {
    SLOPE(
        agc_peak_,
        peak_,
        5.0f * hz,  // Reduce gain in 0.2s if a peak is detected
        // Recover from a peak reduction in 2.0s as long as the observed level
        // isn't lower than what we measured during the learning phase.
        // Otherwise, pump the gain in 100s.
        (peak_ > stable_agc_peak_ ? 0.5f : 0.01f) * hz);
  }
  
  if (active) {
    if (time_ > ignore_time_ || peak_ > 0.001f) {
      time_ += size;
    }
    if (time_ >= active_time_) {
      time_ = active_time_;
    }
  }
  
  auto_gain_ = gain_setting == 0;

  float manual_gain = static_cast<float>(gain_setting) / 255.0f;
  manual_gain *= manual_gain;
  manual_gain *= manual_gain;
  manual_gain = kMinGain + (kMaxGain - kMinGain) * manual_gain;
  ONE_POLE(manual_gain_, manual_gain, 0.01f);
  
  const float optimal_gain = min(0.9f / min(agc_peak_, 0.9f), kMaxGain);
  const float gain = auto_gain_ ? optimal_gain : manual_gain_;

  float soft_clipping_amount;
  if (soft_clipping_mode_ == SOFT_CLIPPING_MODE_OFF) {
    soft_clipping_amount = 0.0f;
  } else if (soft_clipping_mode_ == SOFT_CLIPPING_MODE_ON) {
    soft_clipping_amount = 1.0f;
  } else {
    soft_clipping_amount = 2.0f * (manual_gain_ / optimal_gain - 0.75f);
    CONSTRAIN(soft_clipping_amount, 0.0f, 1.0f)
  }
  
  ParameterInterpolator gain_modulation(&gain_, gain, size);
  ParameterInterpolator soft_clipping_amount_modulation(
      &soft_clipping_amount_,
      soft_clipping_amount,
      size);
  while (size--) {
    int32_t l_int32 = left_normalization ? in->r : in->l;
    int32_t r_int32 = right_normalization ? in->l : in->r;
    
    float l = static_cast<float>(l_int32) / 2147483648.0f;
    float r = static_cast<float>(r_int32) / 2147483648.0f;
    
    if (active) {
      const float peak = max(fabsf(l), fabsf(r));
      SLOPE(peak_, peak, 0.1f, 2.0f * one_hertz_);
    }
    
    const float gain = gain_modulation.Next();
    
    l *= gain;
    r *= gain;
    
    if (!auto_gain_) {
      if (soft_clipping_mode_ == SOFT_CLIPPING_MODE_OFF) {
        CONSTRAIN(l, -1.0f, +1.0f)
        CONSTRAIN(r, -1.0f, +1.0f)
      } else {
        // When the manual gain is enabled, crossfade between the clean, but
        // possibly hard-clipped signal, and the soft-clipped signal, depending
        // on the value of the soft_clipping_mode flag.
        const float amount = soft_clipping_amount_modulation.Next();
        const float bias = 0.05f;
        const float soft_clipping_l = (1.0f - bias) * SoftClip(l + bias) - bias;
        const float soft_clipping_r = (1.0f - bias) * SoftClip(r + bias) - bias;

        CONSTRAIN(l, -1.0f, +1.0f)
        CONSTRAIN(r, -1.0f, +1.0f)
        l = Crossfade(l, soft_clipping_l, amount);
        r = Crossfade(r, soft_clipping_r, amount);
      }
    }
    
    out->l = l;
    out->r = r;
    
    ++out;
    ++in;
  }
}

}  // namespace beads