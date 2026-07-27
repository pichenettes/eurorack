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
// 6-operator FM synth.

#ifndef PLAITS_DSP_ENGINE_SIX_OP_ENGINE_H_
#define PLAITS_DSP_ENGINE_SIX_OP_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/fm/algorithms.h"
#include "plaits/dsp/fm/lfo.h"
#include "plaits/dsp/fm/voice.h"
#include "plaits/dsp/fm/patch.h"

namespace plaits {

const int kNumSixOpVoices = 2;

class FMVoice {
 public:
  FMVoice() { }
  ~FMVoice() { }
  
  void Init(fm::Algorithms<6>* algorithms, float sample_rate);
  void LoadPatch(const fm::Patch* patch);
  void Render(float* buffer, size_t size);
  
  inline void UnloadPatch() {
    patch_ = NULL;
  }
  
  inline const fm::Patch* patch() const {
    return patch_;
  }
  
  inline fm::Voice<6>::Parameters* mutable_parameters() {
    return &parameters_;
  }
  
  inline fm::Lfo* mutable_lfo() {
    return &lfo_;
  }
  inline const fm::Lfo& lfo() const {
    return lfo_;
  }
  
  inline void set_modulations(const fm::Lfo& lfo) {
    parameters_.pitch_mod = lfo.pitch_mod();
    parameters_.amp_mod = lfo.amp_mod();
  }
  
 private:
  const fm::Patch* patch_;

  fm::Lfo lfo_;
  fm::Voice<6> voice_;
  fm::Voice<6>::Parameters parameters_;
  
  DISALLOW_COPY_AND_ASSIGN(FMVoice);
};

class SixOpEngine : public Engine {
 public:
  SixOpEngine() { }
  ~SixOpEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data);
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
      
  void LoadBank(int bank);
  
 private:
  stmlib::HysteresisQuantizer2 patch_index_quantizer_;
  fm::Algorithms<6> algorithms_;
  fm::Patch* patches_;
  FMVoice voice_[kNumSixOpVoices];
  float* temp_buffer_;
  float* acc_buffer_;
  int active_voice_;
  int rendered_voice_;
  
  DISALLOW_COPY_AND_ASSIGN(SixOpEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_SIX_OP_ENGINE_H_