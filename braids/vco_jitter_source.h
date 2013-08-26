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
// A noise source used to add jitter to the VCO.

#ifndef BRAIDS_VCO_JITTER_SOURCE_H_
#define BRAIDS_VCO_JITTER_SOURCE_H_

#include "stmlib/stmlib.h"

#include <cstring>

#include "braids/resources.h"
#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

namespace braids {

using namespace stmlib;

class VcoJitterSource {
 public:
  VcoJitterSource() { }
  ~VcoJitterSource() { }
  
  inline void Init(uint32_t seed) {
    phase_ = 0;
    external_temperature_ = 0;
    room_temperature_ = 0;
    
    // Shuffle bits of the seed.
    seed = seed * 1664525L + 1013904223L;
    hum_intensity_ = 64 + ((seed >> 24) & 0x7f);
    lfo_bleed_intensity_ = 64 + ((seed >> 16) & 0x7f);
    noise_intensity_ = 64 + ((seed >> 8) & 0x7f);
    temperature_sensitivity_ = 64 + ((seed >> 0) & 0x7f);
  }
  
  inline int16_t Render(int16_t lfo_intensity) {
    phase_ += 53687091L;  // 50 * (1 << 32) / 4000
    
    // Rectification hum from 50 Hz AC.
    int32_t hum = Interpolate824(wav_sine, phase_);
    if (hum < 0) {
      hum = 0;
    }
    hum -= 11584;
    
    // Power supply drip due to a LFO/modulation LED.
    lfo_intensity -= 16384;
    if (lfo_intensity < 0) {
      lfo_intensity = 0;
    }
    
    // Noise.
    int16_t noise = Random::GetSample();
    
    // External temperature change, with 1-order filtering.
    int16_t external_temperature_toss = Random::GetSample();
    if (external_temperature_toss == 32767) {
      int32_t delta = Random::GetSample();
      if (noise & 1) {
        ++delta;
      }
      external_temperature_ += delta << 8;
    }
    room_temperature_ += (external_temperature_ - room_temperature_) >> 16;
    
    // Ad'hoc combination of these sources of VCO jitter.
    int32_t pitch_noise = (hum * hum_intensity_ >> 14) +
        (lfo_bleed_intensity_ * static_cast<int32_t>(lfo_intensity) >> 10) +
        (noise * noise_intensity_ >> 9) +
        ((room_temperature_ >> 10) * temperature_sensitivity_ >> 8);
    CLIP(pitch_noise);
    return pitch_noise;
  }
  
 private:
  uint32_t phase_;
  int32_t hum_intensity_;
  int32_t lfo_bleed_intensity_;
  int32_t noise_intensity_;
  int32_t temperature_sensitivity_;
  
  int32_t external_temperature_;
  int32_t room_temperature_;
   
  DISALLOW_COPY_AND_ASSIGN(VcoJitterSource);
};

}  // namespace braids

#endif // BRAIDS_VCO_JITTER_SOURCE_H_
