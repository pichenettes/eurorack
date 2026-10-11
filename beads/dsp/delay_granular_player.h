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
// Granular player.

#ifndef BEADS_DSP_DELAY_GRANULAR_PLAYER_H_
#define BEADS_DSP_DELAY_GRANULAR_PLAYER_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/delay_grain.h"
#include "beads/dsp/granular_player.h"

namespace beads {

class DelayGranularPlayer : public GranularPlayer<DelayGrain, 32> {
 public:
  DelayGranularPlayer() { }
  ~DelayGranularPlayer() { }
  
  template<typename DelayLineType>
  inline void Render(
      const DelayLineType* delay_line,
      const TriggerGenerator::Output& trigger,
      const Parameters& p,
      FloatFrame* out) {
    if (trigger.trigger) {
      const float duration = p.size.value();
      const bool reverse = duration < 0.3333333f;

      float amplitude, pan;
      ComputeGrainAmplitudeAndPan(trigger, p, &amplitude, &pan);
      
      const int safety = DelayGrain::head_crossing_safety_zone + 1;
      const int max_delay = delay_line->duration() - 2 * safety;

      new_grain()->Start(
          delay_line,
          p.time.value() * float(max_delay) + float(safety),
          (reverse ? -1.0f : 1.0f) * p.pitch.pitch_value(),
          ComputeGrainDuration(duration),
          p.shape.value(),
          amplitude, pan,
          trigger.fractional_sample);
      SortGrainList(p.monophonic_mode);
    }

    for (int i = 0; i < NUM_GRAINS; ++i) {
      grain_[i].Render<HEAD_CROSSING_POLICY_STOP, true>(delay_line, out);
    }

    Tick();  // Required to estimate the timing between grains.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DelayGranularPlayer);
};

}  // namespace beads

#endif  // BEADS_DSP_DELAY_GRANULAR_PLAYER_H_
