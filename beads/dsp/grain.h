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
// Grain.

#ifndef BEADS_DSP_GRAIN_H_
#define BEADS_DSP_GRAIN_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/grain_envelope.h"
#include "beads/dsp/voice_stealing_envelope.h"

namespace beads {

class Grain {
 public:
  Grain() { }
  ~Grain() { }
  
  inline bool done() const {
    return envelope_.done() || voice_stealing_envelope_.done();
  }
  
  inline int remaining_samples() const {
    return remaining_samples_;
  }
  
  inline void FadeOut(int duration) {
    if (remaining_samples_ > duration) {
      remaining_samples_ = voice_stealing_envelope_.Start(float(duration));
    }
  }
  
  static inline bool Compare(Grain* const &lhs, Grain* const &rhs) {
    return lhs->remaining_samples() < rhs->remaining_samples();
  }

 protected:
  int remaining_samples_;
  
  GrainEnvelope envelope_;
  VoiceStealingEnvelope voice_stealing_envelope_;
    
 private:
  DISALLOW_COPY_AND_ASSIGN(Grain);
};

}  // namespace beads

#endif  // BEADS_DSP_GRAIN_H_
