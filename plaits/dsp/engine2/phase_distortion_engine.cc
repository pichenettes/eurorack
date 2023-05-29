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
// Phase distortion and phase modulation with an asymmetric triangle as the
// modulator.

#include "plaits/dsp/engine2/phase_distortion_engine.h"

#include <algorithm>

#include "stmlib/dsp/parameter_interpolator.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "plaits/resources.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void PhaseDistortionEngine::Init(BufferAllocator* allocator) {
  modulator_.Init();
  shaper_.Init();
  temp_buffer_ = allocator->Allocate<float>(kMaxBlockSize * 4);
}

void PhaseDistortionEngine::Reset() {
  
}

void PhaseDistortionEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = 0.5f * NoteToFrequency(parameters.note);
  const float modulator_f = min(0.25f, f0 * SemitonesToRatio(Interpolate(
      lut_fm_frequency_quantizer,
      parameters.harmonics,
      128.0f)));
  const float pw = 0.5f + parameters.morph * 0.49f;
  const float amount = 8.0f * parameters.timbre * parameters.timbre * \
      (1.0f - modulator_f * 3.8f);
  
  // Upsample by 2x
  float* synced = &temp_buffer_[0];
  float* free_running = &temp_buffer_[2 * size];
  shaper_.Render<true, true>(
      f0, modulator_f, pw, 0.0f, amount, synced, 2 * size);
  modulator_.Render<false, true>(
      f0, modulator_f, pw, 0.0f, amount, free_running, 2 * size);
  
  for (size_t i = 0; i < size; ++i) {
    // Naive 0.5x downsampling.
    out[i] = 0.5f * Sine(*synced++ + 0.25f);
    out[i] += 0.5f * Sine(*synced++ + 0.25f);
    
    aux[i] = 0.5f * Sine(*free_running++ + 0.25f);
    aux[i] += 0.5f * Sine(*free_running++ + 0.25f);
  }
}

}  // namespace plaits
