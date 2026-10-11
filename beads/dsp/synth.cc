// Copyright 2017 Emilie Gillet.
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
// The synth that is used when no external signal is available.

#include "beads/dsp/synth.h"

#include <algorithm>

namespace beads {

using namespace std;
using namespace stmlib;

void Synth::Init(uint16_t* reverb_buffer) {
  const float sr = 48000.0f;
  
  reverb_.Init(reverb_buffer, sr);

  dry_ = 0.0f;
  wet_ = 0.0f;
  
  trigger_generator_.Init(sr);
  pre_reverb_limiter_.Init(1.0f / (sr * 4.0f), 0.9f);
  output_limiter_.Init(1.0f / (sr * 6.0f), 0.9f);
  wavetable_granular_player_.Init(sr);
  wavetable_voice_.Init(sr);
  bank_quantizer_.Init(kNumBanks, 0.048f, false);
}

void Synth::Render(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    Resolution resolution,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const Processor::MixParameters& mix_parameters,
    FloatFrame* out,
    size_t size) {
  
  Parameters p = parameters;
  p.pitch.base = p.pitch.base + (p.pitch.has_cv ? p.pitch.cv : 0.0f);
  p.pitch.has_cv = false;
  
  float expo_fm_amount = 20.0f * (fabsf(density.base - 0.5f) - 0.225f);
  CONSTRAIN(expo_fm_amount, 0.0f, 1.0f);

  TriggerGenerator::Output triggers[size];
  trigger_generator_.Render(
      seed_button_state,
      seed_input,
      density.base,
      (1.0f - expo_fm_amount) * density.cv,
      expo_fm_amount * density.cv,
      triggers,
      size);
  
  int bank = bank_quantizer_.Process(mix_parameters.feedback);
  const int continuous_wavetable_bit_shift[4] = { 4, 16, 7, 16 };
  const int wavetable_bit_shift[4] = { 4, 6, 7, 16 };
  
  FloatFrame zero;
  zero = 0.0f;
  fill(&out[0], &out[size], zero);
  wavetable_voice_.Render(
      bank,
      continuous_wavetable_bit_shift[resolution],
      freeze,
      p,
      out,
      size);

  // Dry/wet balance.
  const float post_gain_dry = 0.5f; // To get a 10Vpp output for the dry osc.
  const float post_gain_wet = 1.0f;
  const float dry_wet = mix_parameters.dry_wet;
  const float dry = (1.0f - dry_wet) * (1.0f + dry_wet) * post_gain_dry;
  const float wet = dry_wet * (2.0f - dry_wet) * post_gain_wet;
  
  stmlib::ParameterInterpolator dry_modulation(
      &dry_, dry, size);
  stmlib::ParameterInterpolator wet_modulation(
      &wet_, wet, size);

  for (size_t i = 0; i < size; ++i) {
    FloatFrame w;
    w = 0.0f;
    wavetable_granular_player_.Render(
        bank,
        wavetable_bit_shift[resolution],
        triggers[i],
        freeze[i],
        p,
        &w);
    out[i] = dry_modulation.Next() * out[i] + w * wet_modulation.Next();
  }
  
  pre_reverb_limiter_.Process(out, size);
  
  // Reverb.
  const float reverb = mix_parameters.reverb_amount;
  const float reverb_mix = reverb * reverb * 0.8f;
  const float reverb_time = reverb * reverb * reverb;
  reverb_.set_mix(reverb_mix);
  reverb_.set_time(0.3f + 0.6f * reverb_time);
  reverb_.set_lp(0.6f);
  reverb_.Process(out, size);

  // Final clipping/limiting
  output_limiter_.Process(out, size);
  
  if (mix_parameters.output_gate) {
    for (size_t i = 0; i < size; ++i) {
      out[i].l = (out[i].l + out[i].r) * 0.5f;
      out[i].r = triggers[i].gate ? -0.8f : 0.0f;
    }
  }
}

}  // namespace beads