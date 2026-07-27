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
// Arpeggiator.

#ifndef PLAITS_DSP_ENGINE_ARPEGGIATOR_H_
#define PLAITS_DSP_ENGINE_ARPEGGIATOR_H_

#include "stmlib/utils/random.h"

namespace plaits {

enum ArpeggiatorMode {
  ARPEGGIATOR_MODE_UP,
  ARPEGGIATOR_MODE_DOWN,
  ARPEGGIATOR_MODE_UP_DOWN,
  ARPEGGIATOR_MODE_RANDOM,
  ARPEGGIATOR_MODE_LAST
};

class Arpeggiator{
 public:
  Arpeggiator() { }
  ~Arpeggiator() { }
  
  void Init() {
    mode_ = ARPEGGIATOR_MODE_UP;
    Reset();
  }
  
  void Reset() {
    note_ = 0;
    octave_ = 0;
    direction_ = 1;
  }
  
  void set_mode(ArpeggiatorMode mode) {
    mode_ = mode;
  }
  
  void set_range(int range) {
    range_ = range;
  }
  
  inline int note() const { return note_; }
  inline int octave() const { return octave_; }
  
  void Clock(int num_notes) {
    if (num_notes == 1 && range_ == 1) {
      note_ = octave_ = 0;
      return;
    }
    
    if (mode_ == ARPEGGIATOR_MODE_RANDOM) {
      while (true) {
        uint32_t w = stmlib::Random::GetWord();
        int octave = (w >> 4) % range_;
        int note = (w >> 20) % num_notes;
        if (octave != octave_ || note != note_) {
          octave_ = octave;
          note_ = note;
          break;
        }
      }
      return;
    }
    
    if (mode_ == ARPEGGIATOR_MODE_UP) {
      direction_ = 1;
    }

    if (mode_ == ARPEGGIATOR_MODE_DOWN) {
      direction_ = -1;
    }
    
    note_ += direction_;
    
    bool done = false;
    while (!done) {
      done = true;
      if (note_ >= num_notes || note_ < 0) {
        octave_ += direction_;
        note_= direction_ > 0 ? 0 : num_notes - 1;
      }
      if (octave_ >= range_ || octave_ < 0) {
        octave_ = direction_ > 0 ? 0 : range_ - 1;
        if (mode_ == ARPEGGIATOR_MODE_UP_DOWN) {
          direction_ = -direction_;
          note_ = direction_ > 0 ? 1 : num_notes - 2;
          octave_ = direction_ > 0 ? 0 : range_ - 1;
          done = false;
        }
      }
    }
  }
  
 private:
  ArpeggiatorMode mode_;
  int range_;
  
  int note_;
  int octave_;
  int direction_;
  
  DISALLOW_COPY_AND_ASSIGN(Arpeggiator);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_ARPEGGIATOR_H_
