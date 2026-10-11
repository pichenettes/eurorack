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
// Wavetable granular player.

#ifndef BEADS_DSP_WAVETABLE_GRANULAR_PLAYER_H_
#define BEADS_DSP_WAVETABLE_GRANULAR_PLAYER_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/granular_player.h"
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_voice.h"

namespace beads {

class WavetableGranularPlayer : public GranularPlayer<WavetableGrain, 32> {
 public:
  WavetableGranularPlayer() { }
  ~WavetableGranularPlayer() { }
  
  inline void Render(
      int bank,
      int bit_shift,
      const TriggerGenerator::Output& trigger,
      const bool freeze,
      const Parameters& p,
      FloatFrame* out) {
    if (trigger.trigger && !freeze) {
      float amplitude, pan;
      ComputeGrainAmplitudeAndPan(trigger, p, &amplitude, &pan);
      
#ifdef TEST
      if (!new_grain()->done()) {
        printf("stealing, duration = %d\n", new_grain()->remaining_samples());
      }
#endif  // TEST

      new_grain()->Start(
          bank,
          bit_shift,
          p.time.value(),
          p.pitch.pitch_value() * kWavetableRootNote * one_hertz_,
          ComputeGrainDuration(p.size.value()),
          p.shape.value(),
          amplitude,
          pan,
          trigger.fractional_sample);
      SortGrainList(p.monophonic_mode);
    }

    for (int i = 0; i < NUM_GRAINS; ++i) {
      grain_[i].Render(out, freeze);
    }
    
    Tick();  // Required to estimate the timing between grains.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WavetableGranularPlayer);
};

}  // namespace beads

#endif  // BEADS_DSP_GRANULAR_PLAYER_H_
