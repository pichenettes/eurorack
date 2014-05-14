// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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
// Just intonation processor.

#include "yarns/just_intonation_processor.h"

#include <algorithm>

#include "yarns/resources.h"

namespace yarns {

const int16_t kOctave = 12 << 7;

void JustIntonationProcessor::Init() {
  write_ptr_ = 0;
  cached_note_ = 0xff;
  cached_pitch_ = 0;
  
  HistoryEntry e;
  e.note = 0;
  e.pitch = 0;
  e.weight = 0;
  std::fill(&history_[0], &history_[kHistorySize], e);
}

int16_t JustIntonationProcessor::TuneInternal(uint8_t note) {
  uint32_t best_score = 0xffffffff;
  int16_t best_pitch = 0;
  for (int16_t correction = -64; correction <= 64; ++correction) {
    uint32_t score = lut_consonance[
        correction >= 0 ? correction : (kOctave + correction)];
    int16_t pitch = correction + (note << 7);
    for (uint8_t i = 0; i < kHistorySize; ++i) {
      int16_t interval = pitch - history_[i].pitch;
      while (interval < 0) {
        interval += kOctave;
      }
      while (interval > kOctave) {
        interval -= kOctave;
      }
      score += lut_consonance[interval] * history_[i].weight;
      if (score > best_score) {
        break;
      }
    }
    if (score < best_score) {
      best_pitch = pitch;
      best_score = score;
    }
  }
  return best_pitch;
}

/* extern */
JustIntonationProcessor just_intonation_processor;

}  // namespace yarns