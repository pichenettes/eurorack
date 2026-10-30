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
// String machine emulation with filter and chorus.

#include "plaits/dsp/engine2/string_machine_engine.h"

#include <algorithm>

#include "plaits/resources.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void StringMachineEngine::Init(BufferAllocator* allocator) {
  for (int i = 0; i < kChordNumNotes; ++i) {
    divide_down_voice_[i].Init();
  }
  chords_.Init(allocator);
  morph_lp_ = 0.0f;
  timbre_lp_ = 0.0f;
  svf_[0].Init();
  svf_[1].Init();
  ensemble_.Init(allocator->Allocate<Ensemble::E::T>(1024));
}

void StringMachineEngine::Reset() {
  chords_.Reset();
  ensemble_.Reset();
}

const int kRegistrationTableSize = 11;
const float registrations[kRegistrationTableSize][kChordNumHarmonics * 2] = {
  { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },    // Saw
  { 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f },    // Saw + saw
  { 0.4f, 0.0f, 0.2f, 0.0f, 0.4f, 0.0f },    // Full saw
  { 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.4f },    // Full saw + square hybrid
  { 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f },    // Saw + high square harmo
  { 0.2f, 0.0f, 0.0f, 0.2f, 0.0f, 0.6f },    // Weird hybrid
  { 0.0f, 0.2f, 0.1f, 0.0f, 0.2f, 0.5f },    // Sawsquare high harmo
  { 0.0f, 0.3f, 0.0f, 0.3f, 0.0f, 0.4f },    // Square high armo
  { 0.0f, 0.4f, 0.0f, 0.3f, 0.0f, 0.3f },    // Full square
  { 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f },    // Square + Square
  { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },    // Square
};

void StringMachineEngine::ComputeRegistration(
    float registration,
    float* amplitudes) {
  registration *= (kRegistrationTableSize - 1.001f);
  MAKE_INTEGRAL_FRACTIONAL(registration);
  
  for (int i = 0; i < kChordNumHarmonics * 2; ++i) {
    float a = registrations[registration_integral][i];
    float b = registrations[registration_integral + 1][i];
    amplitudes[i] = a + (b - a) * registration_fractional;
  }
}

void StringMachineEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  ONE_POLE(morph_lp_, parameters.morph, 0.1f);
  ONE_POLE(timbre_lp_, parameters.timbre, 0.1f);

  chords_.set_chord(parameters.harmonics);

  float harmonics[kChordNumHarmonics * 2 + 2];
  float registration = max(morph_lp_, 0.0f);
  ComputeRegistration(registration, harmonics);
  harmonics[kChordNumHarmonics * 2] = 0.0f;

  // Render string/organ sound.
  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);
  const float f0 = NoteToFrequency(parameters.note) * 0.998f;
  for (int note = 0; note < kChordNumNotes; ++note) {
    const float note_f0 = f0 * chords_.ratio(note);
    float divide_down_gain = 4.0f - note_f0 * 32.0f;
    CONSTRAIN(divide_down_gain, 0.0f, 1.0f);
    divide_down_voice_[note].Render(
        note_f0,
        harmonics,
        0.25f * divide_down_gain,
        note & 1 ? aux : out,
        size);
  }
  
  // Pass through VCF.
  const float cutoff = 2.2f * f0 * SemitonesToRatio(120.0f * parameters.timbre);
  svf_[0].set_f_q<FREQUENCY_DIRTY>(cutoff, 1.0f);
  svf_[1].set_f_q<FREQUENCY_DIRTY>(cutoff * 1.5f, 1.0f);

  // Mixdown.
  for (size_t i = 0; i < size; ++i) {
    const float l = svf_[0].Process<FILTER_MODE_LOW_PASS>(out[i]);
    const float r = svf_[1].Process<FILTER_MODE_LOW_PASS>(aux[i]);
    out[i] = 0.66f * l + 0.33f * r;
    aux[i] = 0.66f * r + 0.33f * l;
  }

  // Ensemble FX.
  const float amount = fabsf(parameters.timbre - 0.5f) * 2.0f;
  const float depth = 0.35f + 0.65f * parameters.timbre;
  ensemble_.set_amount(amount);
  ensemble_.set_depth(depth);
  ensemble_.Process(out, aux, size);
}

}  // namespace plaits
