// Copyright 2016 Emilie Gillet.
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
// Classic 2-op FM found in Braids, Rings and Elements.

#include "plaits/dsp/engine/fm_engine.h"

#include "stmlib/dsp/parameter_interpolator.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "plaits/dsp/downsampler/4x_downsampler.h"

namespace plaits {

using namespace stmlib;

void FMEngine::Init(BufferAllocator* allocator) {
  carrier_phase_ = 0;
  modulator_phase_ = 0;
  sub_phase_ = 0;

  previous_carrier_frequency_ = a0;
  previous_modulator_frequency_ = a0;
  previous_amount_ = 0.0f;
  previous_feedback_ = 0.0f;
  previous_sample_ = 0.0f;
}

void FMEngine::Reset() {
  
}

void FMEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  
  // 4x oversampling
  const float note = parameters.note - 24.0f;
  
  const float ratio = Interpolate(
      lut_fm_frequency_quantizer,
      parameters.harmonics,
      128.0f);
  
  float modulator_note = note + ratio;
  float target_modulator_frequency = NoteToFrequency(modulator_note);
  CONSTRAIN(target_modulator_frequency, 0.0f, 0.5f);

  // Reduce the maximum FM index for high pitched notes, to prevent aliasing.
  float hf_taming = 1.0f - (modulator_note - 72.0f) * 0.025f;
  CONSTRAIN(hf_taming, 0.0f, 1.0f);
  hf_taming *= hf_taming;
  
  ParameterInterpolator carrier_frequency(
      &previous_carrier_frequency_, NoteToFrequency(note), size);
  ParameterInterpolator modulator_frequency(
      &previous_modulator_frequency_, target_modulator_frequency, size);
  ParameterInterpolator amount_modulation(
      &previous_amount_,
      2.0f * parameters.timbre * parameters.timbre * hf_taming,
      size);
  ParameterInterpolator feedback_modulation(
      &previous_feedback_, 2.0f * parameters.morph - 1.0f, size);
  
  Downsampler carrier_downsampler(&carrier_fir_);
  Downsampler sub_downsampler(&sub_fir_);
  
  while (size--) {
    const float max_uint32 = 4294967296.0f;
    const float amount = amount_modulation.Next();
    const float feedback = feedback_modulation.Next();
    float phase_feedback = feedback < 0.0f ? 0.5f * feedback * feedback : 0.0f;
    const uint32_t carrier_increment = static_cast<uint32_t>(
        max_uint32 * carrier_frequency.Next());
    float _modulator_frequency = modulator_frequency.Next();

    for (size_t j = 0; j < kOversampling; ++j) {
      modulator_phase_ += static_cast<uint32_t>(max_uint32 * \
           _modulator_frequency * (1.0f + previous_sample_ * phase_feedback));
      carrier_phase_ += carrier_increment;
      sub_phase_ += carrier_increment >> 1;
      float modulator_fb = feedback > 0.0f ? 0.25f * feedback * feedback : 0.0f;
      float modulator = SinePM(
          modulator_phase_, modulator_fb * previous_sample_);
      float carrier = SinePM(carrier_phase_, amount * modulator);
      float sub = SinePM(sub_phase_, amount * carrier * 0.25f);
      ONE_POLE(previous_sample_, carrier, 0.05f);
      carrier_downsampler.Accumulate(j, carrier);
      sub_downsampler.Accumulate(j, sub);
    }
    
    *out++ = carrier_downsampler.Read();
    *aux++ = sub_downsampler.Read();
  }
}

}  // namespace plaits
