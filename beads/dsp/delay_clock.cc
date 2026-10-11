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
// Extract reliable clock information from taps or an external signal.

#include "beads/dsp/delay_clock.h"

#include <algorithm>

namespace beads {

using namespace std;
using namespace stmlib;

void DelayClock::Init(float sample_rate) {
  average_interval_ = interval_[0] = interval_[1] = sample_rate * 0.25f;
  max_interval_ = int(sample_rate * 4.0f);

  fade_ = 1.0f;
  min_fade_increment_ = 1.0f / (sample_rate * 0.25f);
  max_fade_increment_ = 1.0f / (sample_rate * 0.02f);
  fade_increment_ = max_fade_increment_;
  
  // The worst case scenario is a 1ms jitter on the clock signal
  acceptable_jitter_ = sample_rate * 0.001f;
  jitter_sensitivity_ = 1.0f;

  schedule_change_ = false;

  next_interval_ = 0;
  max_interval_ = int(sample_rate * 4.0f);
  
  rotate_ = 0;
}

void DelayClock::Process(bool tick, bool tap) {
  ++next_interval_;

  fade_ += fade_increment_;
  if (fade_ >= 1.0f) {
    fade_ = 1.0f;
  }

  if (tick || tap) {
    if (next_interval_ < max_interval_) {
      const float interval = float(next_interval_);
      
      const bool big_change = fabsf(interval - interval_[1]) > \
          acceptable_jitter_;
      const bool small_change = fabsf(average_interval_ - interval_[1]) > \
          jitter_sensitivity_ * acceptable_jitter_;
      
      if (big_change) {
        jitter_sensitivity_ = 1.0f;
        average_interval_ = interval;
      } else {
        ONE_POLE(average_interval_, interval, 0.1f);
        jitter_sensitivity_ = max(0.05f, jitter_sensitivity_ * 0.95f);
      }
      schedule_change_ = schedule_change_ || tap || big_change || small_change;
    }
    next_interval_ = 0;
  }

  const bool can_change = tap || (fade_ >= 1.0f);

  if (schedule_change_ && can_change) {
    ++rotate_;
    interval_[0] = interval_[1];
    interval_[1] = average_interval_;

    fade_ = 0.0f;
    fade_increment_ = 2.0f / average_interval_;
    CONSTRAIN(fade_increment_, min_fade_increment_, max_fade_increment_);

    schedule_change_ = false;
  }
}

}  // namespace beads