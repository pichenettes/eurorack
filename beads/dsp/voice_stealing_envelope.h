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
// Envelope triggered when a grain must shut down ASAP to make room for a
// newly seeded grain.

#ifndef BEADS_DSP_VOICE_STEALING_ENVELOPE_H_
#define BEADS_DSP_VOICE_STEALING_ENVELOPE_H_

#include "stmlib/stmlib.h"

#include <algorithm>

namespace beads {

class VoiceStealingEnvelope {
 public:
  VoiceStealingEnvelope() { }
  ~VoiceStealingEnvelope() { }

  inline void Init() {
    ResetAndPause();
  }

  inline void ResetAndPause() {
    value_ = 1.0f;
    decrement_ = 0.0f;
  }

  inline int Start(float duration) {
    decrement_ = 1.0f / duration;
    return static_cast<int>(value_ * duration);
  }

  inline float Render() {
    value_ -= decrement_;
    return std::max(value_, 0.0f);
  }

  inline bool done() const {
    return value_ <= 0.0f;
  }

 private:
  float value_;
  float decrement_;

  DISALLOW_COPY_AND_ASSIGN(VoiceStealingEnvelope);
};

}  // namespace beads

#endif  // BEADS_DSP_VOICE_STEALING_ENVELOPE_H_
