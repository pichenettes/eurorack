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

#ifndef PLAITS_DSP_ENGINE_STRING_MACHINE_ENGINE_H_
#define PLAITS_DSP_ENGINE_STRING_MACHINE_ENGINE_H_

#include "plaits/dsp/chords/chord_bank.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "stmlib/dsp/filter.h"
#include "plaits/dsp/fx/ensemble.h"

namespace plaits {

class StringMachineEngine : public Engine {
 public:
  StringMachineEngine() { }
  ~StringMachineEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void ComputeRegistration(float registration, float* amplitudes);
  
  ChordBank chords_;
  
  Ensemble ensemble_;
  StringSynthOscillator divide_down_voice_[kChordNumNotes];
  stmlib::NaiveSvf svf_[2];
  
  float morph_lp_;
  float timbre_lp_;
  
  DISALLOW_COPY_AND_ASSIGN(StringMachineEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_STRING_MACHINE_ENGINE_H_