// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xmmintrin.h>

#include "tides2/poly_slope_generator.h"
#include "tides2/resources.h"
#include "tides2/ramp_generator.h"
#include "tides2/ramp_shaper.h"
#include "tides2/test/fixtures.h"

#include "stmlib/test/wav_writer.h"

using namespace std;
using namespace stmlib;
using namespace tides;

const size_t kBlockSize = 6;
const float kSampleRate = 48000.0f;

void TestRampGenerator() {
  WavWriter wav_writer(6, kSampleRate, 10);
  wav_writer.Open("tides2_ramp_generator.wav");
  
  PulseGenerator p;
  p.CreateTestPattern();
  
  RampGenerator<4> g;
  g.Init();
  
  Ratio r[4];
  r[0].ratio = 1.0f;
  r[0].q = 1;

  r[1].ratio = 0.5f;
  r[1].q = 2;

  r[2].ratio = 0.333333f;
  r[2].q = 3;

  r[3].ratio = 4.0f;
  r[3].q = 1;
  
  g.set_next_ratio(r);
  
  RampShaper shaper;
  shaper.Init();
  
  for (size_t i = 0; i < kSampleRate * 10; i += kBlockSize) {
    GateFlags gate_flags[kBlockSize];
    p.Render(gate_flags, kBlockSize);
    
    float pw[4] = { 0.5f, 0.0f, 0.25f, 0.75f };
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      g.Step<RAMP_MODE_AD, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL, false>(
          0.0001f, pw, gate_flags[j], 0.0f);

      float s[6];
      s[0] = gate_flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
      s[1] = shaper.Slope<RAMP_MODE_AD, RANGE_CONTROL>(
          g.phase(0), 0.0f, g.frequency(0), 0.0f);
      s[2] = g.phase(0);
      s[3] = g.phase(1);
      s[4] = g.phase(2);
      s[5] = g.phase(3);
      wav_writer.Write(s, 6, 32767.0f);
    }
  }
}

const char* ramp_source_name[] = {
  "internal",
  "external"
};

const char* ramp_mode_name[] = {
  "ad",
  "loop",
  "ar"
};

const char* output_mode_name[] = {
  "gates",
  "amplitude",
  "slope_phase",
  "frequency"
};

void TestPolySlopeGenerator() {
  for (int ramp_source = 0; ramp_source < 2; ++ramp_source) {
    for (int ramp_mode = 0; ramp_mode < RAMP_MODE_LAST; ++ramp_mode) {
      for (int output_mode = 0; output_mode < OUTPUT_MODE_LAST; ++output_mode) {
        WavWriter wav_writer(5, kSampleRate, 10);
        char file_name[80];
        sprintf(
            file_name,
            "tides2_%s_%s_%s.wav",
            ramp_source_name[ramp_source],
            ramp_mode_name[ramp_mode],
            output_mode_name[output_mode]);
        wav_writer.Open(file_name);
      
        PulseGenerator pulses;
        if (ramp_mode != RAMP_MODE_LOOPING) {
          pulses.CreateTestPattern();
        } else {
          pulses.AddPulses(kSampleRate, 100, 10);
        }
      
        PolySlopeGenerator poly_slope;
        poly_slope.Init();
        
        float phase = 0.0f;
      
        for (size_t i = 0; i < kSampleRate * 10; i += kBlockSize) {
          GateFlags gate_flags[kBlockSize];
          float ramp[kBlockSize];
          pulses.Render(gate_flags, kBlockSize);
        
          PolySlopeGenerator::OutputSample out[kBlockSize];
          const float f0 = (ramp_mode == RAMP_MODE_LOOPING ? 0.5f * 261.5f : 4.0f) / kSampleRate;
          
          for (size_t j = 0; j < kBlockSize; ++j) {
            ramp[j] = phase;
            phase += f0;
            if (phase >= 1.0f) {
              phase -= 1.0f;
            }
          }
          
          if (ramp_source == 1) {
            fill(&gate_flags[0], &gate_flags[kBlockSize], GATE_FLAG_LOW);
          }
        
          poly_slope.Render(
              RampMode(ramp_mode),
              OutputMode(output_mode),
              ramp_mode == RAMP_MODE_LOOPING ? RANGE_AUDIO : RANGE_CONTROL,
              f0,
              0.0f,  // pw
              0.1f,  // shape
              0.5f + wav_writer.triangle() * 0.0f,  // smoothness
              wav_writer.triangle(3),  // shift
              gate_flags,
              ramp_source == 1 ? ramp : NULL,
              out,
              kBlockSize);
            
          for (size_t j = 0; j < kBlockSize; ++j) {
            float s[5];
            s[0] = gate_flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
            s[1] = out[j].channel[0] * 0.1f;
            s[2] = out[j].channel[1] * 0.1f;
            s[3] = out[j].channel[2] * 0.1f;
            s[4] = out[j].channel[3] * 0.1f;
            wav_writer.Write(s, 5, 32767.0f);
          }
        }
      }
    }
  }
}

int main(void) {
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  TestRampGenerator();
  TestPolySlopeGenerator();
}
