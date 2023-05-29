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
// Virtual analog with VCF.

#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"

#include <algorithm>

#include "stmlib/dsp/parameter_interpolator.h"

using namespace std;

namespace plaits {

using namespace std;
using namespace stmlib;

void VirtualAnalogVCFEngine::Init(BufferAllocator* allocator) {
  oscillator_.Init();
  sub_oscillator_.Init();
  
  svf_[0].Init();
  svf_[1].Init();
  
  previous_sub_gain_ = 0.0f;
  previous_cutoff_ = 0.0f;
  previous_stage2_gain_ = 0.0f;
  previous_q_ = 0.0f;
  previous_gain_ = 0.0f;
}

void VirtualAnalogVCFEngine::Reset() {
  
}

void VirtualAnalogVCFEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  // VA Oscillator (saw or PW square) + sub
  const float f0 = NoteToFrequency(parameters.note);

  float shape = (parameters.morph - 0.25f) * 2.0f + 0.5f;
  CONSTRAIN(shape, 0.5f, 1.0f);

  float pw = (parameters.morph - 0.5f) * 2.0f + 0.5f;
  if (parameters.morph > 0.75f) {
    pw = 2.5f - parameters.morph * 2.0f;
  }
  CONSTRAIN(pw, 0.5f, 0.98f);
  
  float sub_gain = max(fabsf(parameters.morph - 0.5f) - 0.3f, 0.0f) * 5.0f;

  oscillator_.Render(f0, pw, shape, out, size);
  sub_oscillator_.Render(f0 * 0.501f, 0.5f, 1.0f, aux, size);
  
  const float cutoff = f0 * SemitonesToRatio(
      (parameters.timbre - 0.2f) * 120.0f);

  float stage2_gain = 1.0f - (parameters.harmonics - 0.4f) * 4.0f;
  CONSTRAIN(stage2_gain, 0.0f, 1.0f);
  
  const float resonance = 2.667f * \
      max(fabsf(parameters.harmonics - 0.5f) - 0.125f, 0.0f);
  const float resonance_sqr = resonance * resonance;
  const float q = resonance_sqr * resonance_sqr * 48.0f;
  float gain = (parameters.harmonics - 0.7f) + 0.85f;
  CONSTRAIN(gain, 0.7f - resonance_sqr * 0.3f, 1.0f);

  ParameterInterpolator sub_gain_modulation(
      &previous_sub_gain_, sub_gain, size);
  ParameterInterpolator cutoff_modulation(
      &previous_cutoff_, cutoff, size);
  ParameterInterpolator stage2_gain_modulation(
      &previous_stage2_gain_, stage2_gain, size);
  ParameterInterpolator q_modulation(
      &previous_q_, q, size);
  ParameterInterpolator gain_modulation(&previous_gain_, gain, size);
  
  for (size_t i = 0; i < size; ++i) {
    const float cutoff = min(cutoff_modulation.Next(), 0.25f);
    const float q = q_modulation.Next();
    const float stage2_gain = stage2_gain_modulation.Next();

    svf_[0].set_f_q<FREQUENCY_FAST>(cutoff, 0.5f + q);
    svf_[1].set_f_q<FREQUENCY_FAST>(cutoff, 0.5f + 0.025f * q);
    
    const float gain = gain_modulation.Next();
    const float input = SoftClip(
        (out[i] + aux[i] * sub_gain_modulation.Next()) * gain);
    
    float lp, hp;
    svf_[0].Process<FILTER_MODE_LOW_PASS, FILTER_MODE_HIGH_PASS>(
        input, &lp, &hp);

    lp = SoftClip(lp * gain);
    lp += stage2_gain * \
        (SoftClip(svf_[1].Process<FILTER_MODE_LOW_PASS>(lp)) - lp);
    
    out[i] = lp;
    aux[i] = SoftClip(hp * gain);
  }
}

}  // namespace plaits
