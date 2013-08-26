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
// A waveshaper adding waveform impurities, seeded by the serial number of
// the MCU.

#ifndef BRAIDS_SIGNATURE_WAVESHAPER_H_
#define BRAIDS_SIGNATURE_WAVESHAPER_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/dsp.h"

namespace braids {

class SignatureWaveshaper {
 public:
  SignatureWaveshaper() { }
  ~SignatureWaveshaper() { }
  
  inline void Init(uint32_t seed) {
    for (uint16_t i = 0; i < 256; ++i) {
      transfer_[i] = static_cast<int16_t>(i - 128) << 8;
    }
    
    // Shuffle bits of the seed.
    seed = seed * 1664525L + 1013904223L;
    
    // Upper clipping.
    uint16_t start = 192 + ((seed & 0xf) << 2);
    seed >>= 4;
    int16_t factor = static_cast<int16_t>(seed & 0xf) - 8;
    seed >>= 4;
    factor = ((factor < 0) ? -1 : 1) * factor * factor;
    for (uint16_t i = start; i < 256; ++i) {
      transfer_[i] += (i - start) * factor * 4;
    }
    
    // Lower clipping.
    uint16_t end = (seed & 0xf) << 2;
    seed >>= 4;
    factor = static_cast<int16_t>(seed & 0xf) - 8;
    seed >>= 4;
    factor = ((factor < 0) ? -1 : 1) * factor * factor;
    for (uint16_t i = 0; i < end; ++i) {
      transfer_[i] += (end - i) * factor * 4;
    }
    
    // Dead zone.
    int16_t dead_zone_size = seed & 0x3;
    seed >>= 2;
    for (uint16_t i = 128 - dead_zone_size; i < 128 + dead_zone_size; ++i) {
      transfer_[i] = 0;
    }
    
    // Glitch at origin;
    int16_t origin_glitch = seed & 0x03;
    seed >>= 2;
    transfer_[128] += ((origin_glitch * origin_glitch) << 8);
    
    // Bumps.
    for (uint16_t bump = 0; bump < 2; ++bump) {
      uint16_t position = 64 + ((seed & 0x7) << 4);
      seed >>= 3;
      int16_t intensity = (seed & 0x7) - 4;
      seed >>= 3;
      for (int16_t x = -32; x <= 32; ++x) {
        int16_t delta = x;
        if (delta < 0) {
          delta = -delta;
        }
        transfer_[x + position] += intensity * (32 - delta) * 32;
      }
    }
    
    // Finally, clip and switch to unsigned.
    for (uint16_t i = 0; i < 257; ++i) {
      int32_t value = transfer_[i];
      CLIP(value);
      transfer_[i] = value;
    }
    transfer_[256] = transfer_[255];
  }
  
  inline int32_t transfer(uint16_t i) {
    return transfer_[i];
  }
  
  inline int32_t Transform(int16_t sample) {
    uint16_t i = sample + 32768;
    int32_t a = transfer_[i >> 8];
    int32_t b = transfer_[(i >> 8) + 1];
    return a + ((b - a) * (i & 0xff) >> 8);
  }

 private:
  int32_t transfer_[257];
   
  DISALLOW_COPY_AND_ASSIGN(SignatureWaveshaper);
};

}  // namespace braids

#endif // BRAIDS_VCO_JITTER_SOURCE_H_
