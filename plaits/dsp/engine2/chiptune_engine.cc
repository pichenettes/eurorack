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
// Chiptune waveforms with arpeggiator.

#include "plaits/dsp/engine2/chiptune_engine.h"

#include <algorithm>

namespace plaits {

using namespace std;
using namespace stmlib;

void ChiptuneEngine::Init(BufferAllocator* allocator) {
  bass_.Init();
  for (int i = 0; i < kChordNumNotes; ++i) {
    voice_[i].Init();
  }
  
  chords_.Init(allocator);
  
  arpeggiator_.Init();
  
  arpeggiator_pattern_selector_.Init(12, 0.075f, false);
  
  envelope_shape_ = NO_ENVELOPE;
  envelope_state_ = 0.0f;
  aux_envelope_amount_ = 0.0f;
}

void ChiptuneEngine::Reset() {
  chords_.Reset();
}

void ChiptuneEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);
  const float shape = parameters.morph * 0.995f;
  const bool clocked = !(parameters.trigger & TRIGGER_UNPATCHED);
  float root_transposition = 1.0f;
  
  *already_enveloped = clocked;
  
  if (clocked) {
    if (parameters.trigger & TRIGGER_RISING_EDGE) {
      chords_.set_chord(parameters.harmonics);
      chords_.Sort();

      int pattern = arpeggiator_pattern_selector_.Process(parameters.timbre);
      arpeggiator_.set_mode(ArpeggiatorMode(pattern / 3));
      arpeggiator_.set_range(1 << (pattern % 3));
      arpeggiator_.Clock(chords_.num_notes());
      envelope_state_ = 1.0f;
    }
    const float octave = float(1 << arpeggiator_.octave());
    const float note_f0 = f0 * chords_.sorted_ratio(
        arpeggiator_.note()) * octave;
    root_transposition = octave;
    voice_[0].Render(note_f0, shape, out, size);
  } else {
    float ratios[kChordNumVoices];
    float amplitudes[kChordNumVoices];

    chords_.set_chord(parameters.harmonics);
    chords_.ComputeChordInversion(parameters.timbre, ratios, amplitudes);
    for (int j = 1; j < kChordNumVoices; j += 2) {
      amplitudes[j] = -amplitudes[j];
    }
  
    fill(&out[0], &out[size], 0.0f);
    for (int voice = 0; voice < kChordNumVoices; ++voice) {
      const float voice_f0 = f0 * ratios[voice];
      voice_[voice].Render(voice_f0, shape, aux, size);
      for (size_t j = 0; j < size; ++j) {
        out[j] += aux[j] * amplitudes[voice];
      }
    }
  }
  
  // Render bass note.
  bass_.Render(f0 * 0.5f * root_transposition, aux, size);
  
  // Apply envelope if necessary.
  if (envelope_shape_ != NO_ENVELOPE) {
    const float shape = fabsf(envelope_shape_);
    const float decay = 1.0f - \
        2.0f / kSampleRate * SemitonesToRatio(60.0f * shape) * shape;
    float aux_envelope_amount = envelope_shape_ * 20.0f;
    CONSTRAIN(aux_envelope_amount, 0.0f, 1.0f);
    
    for (size_t i = 0; i < size; ++i) {
      ONE_POLE(aux_envelope_amount_, aux_envelope_amount, 0.01f);
      envelope_state_ *= decay;
      out[i] *= envelope_state_;
      aux[i] *= 1.0f + aux_envelope_amount_ * (envelope_state_ - 1.0f);
    }
  }
}

}  // namespace plaits
