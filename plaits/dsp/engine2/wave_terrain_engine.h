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
// Wave terrain synthesis - a 2D function evaluated along an elliptical path of
// adjustable center and excentricity.
//
// This implementation initially used pre-computed terrains stored in flash
// memory, but even at a poor resolution of 64x64 with 8-bit samples, this
// takes 4kb per terrain! It turned out that directly evaluating the terrain
// function on the fly uses less flash, but is also faster than bicubic
// interpolation of the terrain data.

#ifndef PLAITS_DSP_ENGINE_WAVE_TERRAIN_ENGINE_H_
#define PLAITS_DSP_ENGINE_WAVE_TERRAIN_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {
  
class WaveTerrainEngine : public Engine {
 public:
  WaveTerrainEngine() { }
  ~WaveTerrainEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) {
    user_terrain_ = (const int8_t*)(user_data);
  }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  
 private:
  float Terrain(float x, float y, int terrain_index);
  
  FastSineOscillator path_;
  float offset_;
  float terrain_;
  
  float* temp_buffer_;
  const int8_t* user_terrain_;
  
  DISALLOW_COPY_AND_ASSIGN(WaveTerrainEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_WAVE_TERRAIN_ENGINE_H_