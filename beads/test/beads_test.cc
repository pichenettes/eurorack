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


#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "stmlib/dsp/polyblep.h"
#include "stmlib/test/wav_writer.h"

#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dsp/delay_controller.h"
#include "beads/dsp/delay_line.h"
#include "beads/dsp/fx/wow_and_flutter.h"
#include "beads/dsp/grain.h"
#include "beads/dsp/granular_player.h"
#include "beads/dsp/granular_processor.h"
#include "beads/dsp/randomized_parameter.h"
#include "beads/dsp/multitap_delay.h"
#include "beads/dsp/trigger_generator.h"
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_granular_player.h"
#include "beads/dsp/wavetable_voice.h"

using namespace beads;
using namespace std;
using namespace stmlib;

const size_t kSampleRate = 48000;
const size_t kBlockSize = 12;

class AudioSource {
 public:
  AudioSource(const string& filename) {
    fp_ = NULL;
    phase_ = 0.0;

    if (filename.length() != 0) {
      fp_ = fopen(filename.c_str(), "rb");
      fseek(fp_, 48, SEEK_SET);
    }
  }
  
  ~AudioSource() {
    if (fp_) {
      fclose(fp_);
    }
  }
  
  void Read(FloatFrame* out, size_t size) {
    if (!fp_) {
      while (size--) {
        phase_ += 261.626f / kSampleRate;
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
        }
        out->l = phase_ - 0.5f;
        out->r = sinf(phase_ * 2.0 * M_PI) * 0.5f;
        ++out;
      }
    } else {
      int16_t buffer[size * 2];
      if (fread(buffer, sizeof(int16_t) * 2, size, fp_) != size) {
        FloatFrame null;
        null = 0.0f;
        fill(&out[0], &out[size], null);
      } else {
        for (size_t i = 0; i < size; ++i) {
          out[i].l = static_cast<float>(buffer[2 * i]) / 32768.0f;
          out[i].r = static_cast<float>(buffer[2 * i + 1]) / 32768.0f;
        }
      }
    }
  }
  
 private:
  FILE* fp_;
  float phase_;
};

// Same memory layout as on the hardware.
uint8_t block_1[512 * 1024];
uint8_t block_2[256 * 1024];
uint8_t* buffers[3];

uint16_t reverb_buffer[32768];

void TestDelayLine() {
  WavWriter wav_writer(14, kSampleRate, 30);
  wav_writer.Open("beads_delay_line.wav");
  AudioSource audio_source("audio_samples/kettel.wav");
  
  size_t total_size = 65536 * 4;
  
  uint8_t* delay_buffer[6];
  for (int i = 0; i < 6; ++i) {
    delay_buffer[i] = new uint8_t[total_size];
  }

  beads::DelayLine<RESOLUTION_16_BIT, 65000, 2, 4096, 64> stereo_16;
  beads::DelayLine<RESOLUTION_16_BIT, 130000, 1, 4096, 64> mono_16;
  beads::DelayLine<RESOLUTION_12_BIT, 86666, 2, 4096, 64> stereo_12;
  beads::DelayLine<RESOLUTION_12_BIT, 173333, 1, 4096, 64> mono_12;
  beads::DelayLine<RESOLUTION_8_BIT, 130000, 2, 4096, 64> stereo_8;
  beads::DelayLine<RESOLUTION_8_BIT, 260000, 1, 4096, 64> mono_8;

  stereo_16.Init(delay_buffer[0], total_size);
  mono_16.Init(delay_buffer[1], total_size);
  stereo_12.Init(delay_buffer[2], total_size);
  mono_12.Init(delay_buffer[3], total_size);
  stereo_8.Init(delay_buffer[4], total_size);
  mono_8.Init(delay_buffer[5], total_size);

  float delay = 64999.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    FloatFrame out[6][kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t i = 0; i < kBlockSize; ++i) {
      stereo_16.Write(in[i]);
      out[0][i] = stereo_16.Read<INTERPOLATION_HERMITE>(delay);

      mono_16.Write(in[i]);
      out[1][i] = mono_16.Read<INTERPOLATION_HERMITE>(delay);

      stereo_12.Write(in[i]);
      out[2][i] = stereo_12.Read<INTERPOLATION_HERMITE>(delay);

      mono_12.Write(in[i]);
      out[3][i] = mono_12.Read<INTERPOLATION_HERMITE>(delay);

      stereo_8.Write(in[i]);
      out[4][i] = stereo_8.Read<INTERPOLATION_HERMITE>(delay);

      mono_8.Write(in[i]);
      out[5][i] = mono_8.Read<INTERPOLATION_HERMITE>(delay);
    }
    
    for (size_t i = 0; i < kBlockSize; ++i) {
      float s[14];
      s[0] = in[i].l;
      s[1] = in[i].r;
      for (int j = 0; j < 6; ++j) {
        s[2 + j * 2] = out[j][i].l;
        s[3 + j * 2] = out[j][i].r;
      }
      wav_writer.Write(s, 14);
    }
  }
  
  for (int i = 0; i < 6; ++i) {
    delete delay_buffer[i];
  }
}

void TestGrain() {
  WavWriter wav_writer(4, kSampleRate, 30);
  wav_writer.Open("beads_grain.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  // beads::DelayLine<RESOLUTION_16_BIT, 72000> delay_line;
  beads::DelayLine<RESOLUTION_12_BIT, 9600, 2> delay_line;
  // beads::DelayLine<RESOLUTION_8_BIT, 1440> delay_line;

  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 10.0f, +1.0f, 4800.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;
      delay_line.Write(in[j], i >= 552000 && ((i % 32768) >= 16384));

      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);

      if (g.done()) {
        g.Start(
            &delay_line,
            72000.0f,
            -1.0f,
            1200.0f * 10,
            wav_writer.triangle(10),
            1.0f,
            0.5f,
            0.0f);
      }
      
      float frame[4];
      frame[0] = in[j].l;
      frame[1] = in[j].r;
      frame[2] = o.l;
      frame[3] = o.r;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestPanMatrix() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_grain_pan_matrix.wav");
  AudioSource audio_source("");

  beads::DelayLine<RESOLUTION_16_BIT, 72000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 10.0f, +1.0f, 4800.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;
      delay_line.Write(in[j], false);
      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);
      if (g.done()) {
        g.Start(
            &delay_line,
            10.0f,
            +1.0f,
            8000.0f,
            0.25f,
            1.0f,
            wav_writer.triangle(5),
            0.0f);
      }
      wav_writer.Write(&o.l, 2);
    }
  }
}

void TestGrainFade() {
  WavWriter wav_writer(4, kSampleRate, 1);
  wav_writer.Open("beads_grain_fade.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 100.0f, +1.0f, 96000.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i == 24000) {
      assert(g.remaining_samples() == 72000);
      g.FadeOut(4096);
      assert(g.remaining_samples() == 4096);
    }
    if (i == 24000 + 24 * 100) {
      assert(g.remaining_samples() == 1696);
      g.FadeOut(64);
      assert(g.remaining_samples() == 26);
    }

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;

      delay_line.Write(in[j], false);
      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);

      float frame[4];
      frame[0] = in[j].l;
      frame[1] = in[j].r;
      frame[2] = o.l;
      frame[3] = o.r;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestHeadCrossingPolicies() {
  WavWriter wav_writer(8, kSampleRate, 30);
  wav_writer.Open("beads_head_crossing_policies.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 24000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g[8];
  for (int i = 0; i < 8; ++i) {
    g[i].Init(kSampleRate);
    g[i].Start(&delay_line, 800.0f, +1.0f, 9600000.0f, 0.0f, 1.0f, 0.5f, 0.0f);
  }

  for (size_t i = 0; i < kSampleRate * 30; ++i) {
    FloatFrame in;
    float out[8];

    audio_source.Read(&in, 1);
    delay_line.Write(in, i >= 600000);

    FloatFrame o;
    o = 0.0f;
    g[0].Render<HEAD_CROSSING_POLICY_STOP, false>(&delay_line, &o);
    out[0] = o.l;

    o = 0.0f;
    g[1].Render<HEAD_CROSSING_POLICY_STOP, true>(&delay_line, &o);
    out[1] = o.l;

    o = 0.0f;
    g[2].Render<HEAD_CROSSING_POLICY_WRAP, false>(&delay_line, &o);
    out[2] = o.l;

    o = 0.0f;
    g[3].Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);
    out[3] = o.l;

    o = 0.0f;
    g[4].Render<HEAD_CROSSING_POLICY_PING_PONG, false>(&delay_line, &o);
    out[4] = o.l;

    o = 0.0f;
    g[5].Render<HEAD_CROSSING_POLICY_PING_PONG, true>(&delay_line, &o);
    out[5] = o.l;

    o = 0.0f;
    g[6].Render<HEAD_CROSSING_POLICY_STUTTER, true>(&delay_line, &o);
    out[6] = o.l;

    o = 0.0f;
    g[7].Render<HEAD_CROSSING_POLICY_CROSSFADE, false>(&delay_line, &o);
    out[7] = o.l;

    wav_writer.Write(out, 8);
  }
}

void TestTriggerGenerator() {
  for (int conditions = 0; conditions < 4; ++conditions) {
    const bool locked = conditions & 1;
    const bool has_external_input = conditions & 2;
    const float external_clock_rate = locked ? 60.0f : 1.3f;

    WavWriter wav_writer(7, kSampleRate, 20);

    string name = "beads_trigger_generator";
    name += locked ? "_locked" : "_unlocked";
    name += has_external_input ? "_external" : "_free_running";
    name += ".wav";

    wav_writer.Open(name.c_str());

    TriggerGenerator g;
    g.Init(48000.0f);

    GateFlags previous_flags = GATE_FLAG_LOW;
    float phase = 0.0f;

    for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
      TriggerGenerator::Output out[kBlockSize];
      GateFlags flags[kBlockSize];

      for (size_t j = 0; j < kBlockSize; ++j) {
        phase += external_clock_rate / kSampleRate;
        if (phase >= 1.0f) {
          phase -= 1.0f;
        }
        flags[j] = previous_flags = ExtractGateFlags(
            previous_flags, phase < 0.25f);
      }

      SeedButtonState seed_button_state;
      if (locked) {
        seed_button_state = SEED_BUTTON_STATE_LOCKED;
      } else {
        seed_button_state = wav_writer.triangle(2) < 0.5f ?
            SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
      }

      g.Render(
          seed_button_state,
          has_external_input ? flags : NULL,
          wav_writer.triangle(20), 0.0f, 0.0f,
          out,
          kBlockSize);

      for (size_t j = 0; j < kBlockSize; ++j) {
        float frame[7];
        frame[0] = seed_button_state == SEED_BUTTON_STATE_LOCKED
            ? -0.8f
            : (seed_button_state == SEED_BUTTON_STATE_ON ? 0.8f : 0.0f);
        frame[1] = has_external_input
            ? (flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f)
            : -0.8f;
        frame[2] = out[j].trigger ? 0.8f : 0.0f;
        frame[3] = out[j].gate ? 0.8f : 0.0f;
        frame[4] = out[j].fractional_sample;
        frame[5] = out[j].regularity * 0.8f;
        frame[6] = out[j].phase;
        wav_writer.Write(frame, 7);
      }
    }
  }
}

void TestTriggerGeneratorExpoFM() {
  for (int conditions = 0; conditions < 2; ++conditions) {
    const bool linear_fm = conditions == 0;
    WavWriter wav_writer(5, kSampleRate, 20);
    string name = "beads_trigger_generator";
    name += linear_fm ? "_linear_fm" : "_expo_fm";
    name += ".wav";
    wav_writer.Open(name.c_str());

    TriggerGenerator g;
    g.Init(48000.0f);

    for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
      TriggerGenerator::Output out[kBlockSize];
      g.Render(
          SEED_BUTTON_STATE_LOCKED,
          NULL,
          linear_fm ? 0.5f : 0.0f,
          linear_fm ? wav_writer.triangle(10) - 0.5f : 0.0f,
          linear_fm ? 0.0f : wav_writer.triangle(10) - 0.5f,
          out,
          kBlockSize);

      for (size_t j = 0; j < kBlockSize; ++j) {
        float frame[6];
        frame[0] = out[j].trigger ? 0.8f : 0.0f;
        frame[1] = out[j].gate ? 0.8f : 0.0f;
        frame[2] = out[j].fractional_sample;
        frame[3] = out[j].regularity * 0.8f;
        frame[4] = out[j].phase;
        wav_writer.Write(frame, 5);
      }
    }
  }
}

void TestRandomizedParameter() {
  RandomizedParameter p[42];
  RandomizedParameter p_pitch[42];
  RandomWalkModulation w[42];

  for (int i = 0; i < 42; ++i) {
    p[i].base = 0.5f + 0.5f * ((i % 21) / 7) / 2.0f;
    p[i].attenuverter = -1.0f + (i % 7) * 0.333333f;
    p[i].has_cv = i >= 21;
    
    p_pitch[i].base = -12.0f + 24.0f * ((i % 21) / 7) / 2.0f;
    p_pitch[i].attenuverter = -1.0f + (i % 7) * 0.333333f;
    p_pitch[i].has_cv = i >= 21;
    w[i].Init(0.001f);
  }

  for (int sample = 0; sample < 10000; ++sample) {
    float cv = (float(sample % 1000) / 1000.0f) - 0.5f;
    for (int i = 0; i < 42; ++i) {
      p[i].cv = cv;
      printf("%f\t", p[i].value());
    }
    
    for (int i = 0; i < 42; ++i) {
      p[i].cv = cv;
      printf("%f\t", p[i].value(&w[i], 1, 0.5f, false));
    }

    cv = 24.0f * ((float(sample % 1000) / 1000.0f) - 0.5f);
    for (int i = 0; i < 42; ++i) {
      p_pitch[i].cv = cv;
      float semitones = 12.0f * logf(p_pitch[i].pitch_value()) / logf(2.0f);
      printf("%f\t", semitones);
    }
    printf("\n");
  }
}

void TestGranularPlayer() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_granular_player.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.5f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.33f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.00f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        /*wav_writer.triangle(20)*/ 0.85f, 0.0f, 0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      delay_line.Write(in[j], false);
      FloatFrame frame;
      frame.l = 0.0f;
      frame.r = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestMultitapDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_multitap_delay.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 96000, 2> delay_line;
  delay_line.Init(buffers);

  MultitapDelay multitap_delay;
  multitap_delay.Init(48000.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      bool freeze = (i % (kSampleRate * 8)) > kSampleRate * 4;
      delay_line.Write(in[j], freeze);
      int64_t delay_time = freeze
          ? (kSampleRate - ((i + j) % kSampleRate))
          : 24000;

      FloatFrame frame;
      frame = 0.0f;

      DelayController::Output delay;
      delay.tap[0].delay = delay_time << 16;
      delay.tap[0].gain = 1.0f;

      multitap_delay.Render(
          &delay_line,
          delay,
          1.0f,
          0.5f,
          &frame);
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestRandomPanning() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_random_panning.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.6f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        wav_writer.triangle(20), 0.0f, 0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      delay_line.Write(in[j], false);

      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      frame *= 0.25f;
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestSubsampleTriggering() {
  beads::DelayLine<RESOLUTION_16_BIT, 4800, 2> delay_line;
  delay_line.Init(buffers);

  float phase = 0.0f;
  for (int i = 0; i < 4800; ++i) {
    phase += 1356.0f / 48000.0f;
    if (phase >= 1.0f) {
      phase -= 1.0f;
    }
    FloatFrame s;
    s.r = s.l = sinf(phase * 2.0f * M_PI);
    delay_line.Write(s, i == 4799);
  }

  WavWriter wav_writer(2, kSampleRate, 10);
  wav_writer.Open("beads_subsample.wav");

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = +0.34f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    TriggerGenerator::Output trigger[kBlockSize];
    p.shape.base = wav_writer.triangle(10);
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        0.02f,
        0.0f,
        0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      frame *= 0.25f;
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestGranularProcessor() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_granular_processor.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  GranularProcessor<RESOLUTION_16_BIT, 48000, 1> proc;
  proc.Init(48000.0f, buffers);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.5f;
  p.time.attenuverter = 0.1f;
  p.time.has_cv = false;

  p.size.base = 0.8f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.5f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = +0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame in[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        0.8f,
        0.0f,
        0.0f,
        trigger, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);

    proc.Process(trigger, NULL, freeze, p, 0.5f, false, in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessor() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.75f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.3f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 0.9f;
  m.reverb_amount = 0.2f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.7f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.7f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.7f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.2f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i >= 15 * kSampleRate) {
      d.base = wav_writer.triangle(11);
    }
    
    if (i <= 11 * kSampleRate) {
      m.dry_wet = wav_writer.triangle(4);
      p.shape.base = 0.0f;
    } else {
      m.dry_wet = 1.0f;
      p.shape.base = 0.66f;
    }
    
    fill(&freeze[0], &freeze[kBlockSize], i >= 11.7 * kSampleRate);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayBW() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_bw.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.9f;
  m.dry_wet = 0.5f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    p.size.attenuverter = 2.0f * wav_writer.triangle(20) - 1.0f;

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayTimeXFade() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_time_xfade.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.has_cv = false;
  p.size.attenuverter = 0.0f;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    p.time.base = wav_writer.triangle(10);
    
    fill(&freeze[0], &freeze[kBlockSize], false);

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay.wav");
  AudioSource audio_source("audio_samples/talking_heads.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 21881;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    bool frz = (i >= 350108 && i <= 350108 * 3 / 2) || (i >= 350108 * 2);
    
    if (frz) {
      d.base = 0.35f;
      p.time.base = wav_writer.triangle();
      if (i >= kSampleRate * 15) {
        d.base = 0.15f + wav_writer.triangle(7) * 0.35f;
      }
      m.feedback = 0.0f;
      m.dry_wet = 1.0f;
      p.shape.base = wav_writer.triangle(8);
    } else {
      d.base = 0.5f;
      p.time.base = 0.0f;
      m.feedback = 0.3f;
      m.dry_wet = wav_writer.triangle(3);
      p.shape.base = 0.0f;
    }


    fill(&freeze[0], &freeze[kBlockSize], frz);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelayGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 480 + (rand() % 4);
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    p.time.base = 0.0f;
    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelayTempoChange() {
  WavWriter wav_writer(4, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_tempo_change.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    p.shape.base = wav_writer.triangle(2);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = ((i / kSampleRate) & 3) < 2 ? 2120 : 15343;
        next_tap_counter += ((i / kBlockSize) % 3) * 24;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    fill(&freeze[0], &freeze[kBlockSize], (i / kSampleRate) >= 15);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      float out[4];
      out[0] = frames[j].l;
      out[1] = frames[j].r;
      out[2] = tap_input[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
      out[3] = freeze[j] ? 0.8f : 0.0f;
      wav_writer.Write(out, 4);
    }
    
  }
}

void TestProcessorTapDelayDensityModulation() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_density_modulation.wav");
  AudioSource audio_source("audio_samples/talking_heads.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 1.0f;
  m.dry_wet = 0.7f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 21881;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    d.base = 1.0f - wav_writer.triangle(11) * 0.5f;
    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayGranularXFade() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_delay_granular_xfade.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.2f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.3f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.3f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    
    p.size.base = min(wav_writer.triangle(11) * 2, 1.0f);
    
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestWowAndFlutter() {
  WavWriter wav_writer(2, kSampleRate / 2, 30);
  wav_writer.Open("beads_wow_and_flutter.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  WowAndFlutter wow_and_flutter;
  wow_and_flutter.Init(kSampleRate / 2);

  SampleRateConverter<SRC_DOWN, 2, 48> src_down_;
  src_down_.Init();

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in_stereo[kBlockSize];
    float in[kBlockSize];
    float in_decimated[kBlockSize / 2];
    FloatFrame frames[kBlockSize / 2];
    audio_source.Read(in_stereo, kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) {
      in[i] = (in_stereo[i].l + in_stereo[i].r) * 0.5f;
    }
    src_down_.Process(in, in_decimated, kBlockSize);
    for (size_t i = 0; i < kBlockSize / 2; ++i) {
      frames[i].l = frames[i].r = in_decimated[i];
    }
    wow_and_flutter.Process(frames, kBlockSize / 2);
    wav_writer.Write(&(frames[0].l), kBlockSize / 2 * 2);
  }
}

void TestProcessorMonophonicMode() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_mono_mode.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.3f;
  p.time.has_cv = false;

  p.size.base = 0.8f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.1f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = true;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.8f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestWavetableGrain() {
  WavWriter wav_writer(2, 48000.0f, 10);
  wav_writer.Open("beads_wavetable_grain.wav");
  
  WavetableGrain g[2];
  g[0].Init(kSampleRate);
  g[1].Init(kSampleRate);
  
  int grain = 0;
  float phase = 0.0f;
  float frequency = 69.3f / kSampleRate;
  
  for (size_t i = 0; i < kSampleRate * 10; ++i) {
    phase += frequency;
    if (phase >= 1.0f) {
      phase -= 1.0f;
      float fractional_sample = phase / frequency;
      grain = (grain + 1) % 2;
      
      g[grain].Start(
          2, 16,
          wav_writer.triangle(5),
          4.0f * frequency,
          1.2f / frequency,
          wav_writer.triangle(7),
          0.5f,
          0.5f,
          fractional_sample);
    }
    FloatFrame s;
    s = 0.0f;
    g[0].Render(&s);
    g[1].Render(&s);
    wav_writer.Write(&s.l, 2);
  }
}

void TestWavetableGranularPlayer() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_wavetable_granular_player.wav");

  WavetableGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.attenuverter = 0.05f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.04f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    p.time.base = wav_writer.triangle(30);
    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        1.0f,
        0.0f,
        0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(
          2,
          16,
          trigger[j], false, p, &frame);
      frame *= 0.5f;
      wav_writer.Write(&frame.l, 2);
    }
  }
}

void TestSynth() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_synth.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.1f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.52f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 1.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    p.size.base = 0.7f + wav_writer.triangle(5) * 0.3f;
    p.time.base = wav_writer.triangle(25);

    fill(&freeze[0], &freeze[kBlockSize], ((i / kSampleRate) % 4) >= 3);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthWavetableScanGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_synth_scan.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.25f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.dry_wet = 0.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    m.feedback = wav_writer.triangle(5);

    fill(&freeze[0], &freeze[kBlockSize], ((i / kSampleRate) % 8) >= 6);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthSimpleSweep() {
  WavWriter wav_writer(2, kSampleRate, 68);
  wav_writer.Open("beads_synth_sweep.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.33f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = -12.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.25f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 68; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    m.feedback = float(i) / float(kSampleRate * 64);
    p.time.base = wav_writer.triangle(2);

    fill(&freeze[0], &freeze[kBlockSize], false);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthFrequencySweep() {
  WavWriter wav_writer(2, kSampleRate, 68);
  wav_writer.Open("beads_synth_frequency_sweep.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.40f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 0.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 68; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    float ramp = float(i % (2 * kSampleRate)) / float(2 * kSampleRate);
    p.pitch.base = ramp * 120.0f - 60.0f;

    fill(&freeze[0], &freeze[kBlockSize], false);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

template<int R, int N>
class CicDecimator {
 public:
  int32_t hi[N];
  int32_t hc[N];
  
  void Init() {
    fill(&hi[0], &hi[N], 0.0f);
    fill(&hc[0], &hc[N], 0.0f);
  }
  
  void Process(float* input, float* output, size_t size) {
    for (int i = 0; i < size / R; ++i) {
      // N integrators
      for (int r=0; r<R; r++) {
        hi[0] += (*input++) * 32768.0f;
        for(int n=1; n<N; n++) {
          hi[n] += hi[n-1];
        }
      }
      // N combs
      int32_t v = hi[N-1];
      for (int n=0; n<N; n++) {
        int32_t in = v;
        v -= hc[n];
        hc[n] = in;
      }
      *output++ = static_cast<float>(v / powf(R, N) / 32768.0f);
    }
  }
};

template<int R, int N>
class CicInterpolator {
 public:
  int32_t hi[N];
  int32_t hc[N];
  
  void Init() {
    fill(&hi[0], &hi[N], 0.0f);
    fill(&hc[0], &hc[N], 0.0f);
  }
  
  void Process(float* input, float* output, size_t size) {
    for (int i = 0; i < size; ++i) {
      int32_t v = static_cast<float>(*input++ * R * 32768.0f);
      for (int n=0; n<N; n++) {
        int32_t in = v;
        v -= hc[n];
        hc[n] = in;
      }
      for (int r=0; r<R; r++) {
        hi[0] += v;
        for(int n=1; n<N; n++) {
          hi[n] += hi[n-1];
        }
        *output++ = static_cast<float>(hi[N - 1] / powf(R, N) / 32768.0f);
        v = 0;
      }
    }
  }
};

void TestSRCDown(void) {
  int ratio = 3;
  
  WavWriter wav_writer(2, kSampleRate, 10);
  wav_writer.Open("beads_src_down.wav");
  AudioSource audio_source("");
  
  stmlib::SampleRateConverter<stmlib::SRC_DOWN, 3, 48> src_down_;
  stmlib::SampleRateConverter<stmlib::SRC_UP, 3, 48> src_up_;
  src_down_.Init();
  src_up_.Init();
  
  CicDecimator<3, 8> cic_down_;
  CicInterpolator<3, 4> cic_up_;
  cic_down_.Init();
  cic_up_.Init();
  
  float phase = 0.0f;
  for (size_t i = 0; i < 48000 * 10; i += kBlockSize) {
    float input[kBlockSize];
    
    float frequency = powf(2.0f, wav_writer.triangle(2) * -10.0f) * 0.5f;
    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += frequency;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      input[j] = sinf(M_PI * 2 * phase);
    }
    
    float down_fir[kBlockSize / ratio];
    float down_cic[kBlockSize / ratio];
    float up_fir[kBlockSize];
    float up_cic[kBlockSize];
    
    src_down_.Process(input, down_fir, kBlockSize);
    src_up_.Process(down_fir, up_fir, kBlockSize / ratio);
    
    cic_down_.Process(input, down_cic, kBlockSize);
    cic_up_.Process(down_cic, up_cic, kBlockSize / ratio);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      float s[2];
      s[0] = up_fir[j];
      s[1] = up_cic[j];
      wav_writer.Write(s, 2);
    }
  }
}

void TestGateOut() {
  WavWriter wav_writer(4, kSampleRate, 20);

  wav_writer.Open("beads_gate_out.wav");

  TriggerGenerator g;
  g.Init(48000.0f);

  GateFlags previous_flags = GATE_FLAG_LOW;
  float phase = 0.0f;

  for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
    TriggerGenerator::Output out[kBlockSize];
    GateFlags flags[kBlockSize];

    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += 1.0f / kSampleRate;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      flags[j] = previous_flags = ExtractGateFlags(
          previous_flags, phase < 0.5f);
    }

    g.Render(
        SEED_BUTTON_STATE_OFF,
        flags,
        wav_writer.triangle(10) * 0.7f + 0.15f, 0.0f, 0.0f,
        out,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      float frame[4];
      frame[0] = flags[j] & GATE_FLAG_HIGH;
      frame[1] = out[j].trigger ? 0.8f : 0.0f;
      frame[2] = out[j].gate ? 0.8f : 0.0f;
      frame[3] = out[j].phase;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestAudioRateTriggerGenerator() {
  WavWriter wav_writer(6, kSampleRate, 20);
  wav_writer.Open("beads_audio_rate_trigger.wav");

  TriggerGenerator g;
  g.Init(48000.0f);

  GateFlags previous_flags = GATE_FLAG_LOW;
  float phase = 0.0f;
  
  float this_sample = 0.0f;
  float next_sample;
  float previous_phase = 0.0f;

  for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
    const float frequency = ((i / (kSampleRate / 4)) % 2 == 0)
        ? 432.0f / kSampleRate
        : 865.3f / kSampleRate;
    GateFlags flags[kBlockSize];
    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += frequency;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      flags[j] = previous_flags = ExtractGateFlags(
          previous_flags, phase < 0.25f);
    }
    
    TriggerGenerator::Output out[kBlockSize];
    
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        flags,
        wav_writer.triangle(13) * 1.0f, 0.0f, 0.0f,
        out,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      float frame[6];
      frame[0] = flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
      frame[1] = out[j].trigger ? 0.8f : 0.0f;
      frame[2] = out[j].gate ? 0.8f : 0.0f;
      frame[3] = out[j].fractional_sample;
      frame[4] = out[j].regularity * 0.8f;
      
      next_sample = 0.0f;
      if (out[j].trigger) {
        float d = out[j].phase - previous_phase;
        this_sample += stmlib::ThisBlepSample(out[j].fractional_sample) * d;
        next_sample += stmlib::NextBlepSample(out[j].fractional_sample) * d;
      }
      next_sample += out[j].phase;
      previous_phase = out[j].phase;
      frame[5] = this_sample;
      wav_writer.Write(frame, 6);

      this_sample = next_sample;
    }
  }
}

void TestDelayModulation() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.base = 1.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.125f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i < kSampleRate * 15) {
      d.cv = 0.25f * (cosf(wav_writer.triangle(7) * 2 * M_PI) + 1.0f);
    } else {
      d.cv = wav_writer.triangle(1) > 0.5f ? 3.0f / 5.0f : 1.0f / 5.0f;
    }
    
    fill(&freeze[0], &freeze[kBlockSize], i > kSampleRate * 15);

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestFreezeGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_freeze_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.25f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.85f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.333f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.45f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], (i % 4000) <= 2000);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

int main(void) {
  buffers[0] = block_1;
  buffers[1] = block_1 + (256 * 1024);
  buffers[2] = block_2;
  
  // TestDelayLine();
  // TestGrain();
  // TestPanMatrix();
  
  // TestGrainFade();
  // TestHeadCrossingPolicies();
  // TestTriggerGenerator();
  // TestTriggerGeneratorExpoFM();
  
  // TestRandomizedParameter();
  
  // TestGranularPlayer();
  // TestMultitapDelay();
  // TestRandomPanning();
  // TestSubsampleTriggering();

  // TestGranularProcessor();
  // TestProcessor();
  // TestProcessorDelay();
  // TestProcessorDelayBW();
  // TestProcessorDelayTimeXFade();
  // TestProcessorTapDelay();
  // TestProcessorTapDelayDensityModulation();
  // TestProcessorTapDelayGlitch();
  // TestProcessorTapDelayTempoChange();
  
  // TestProcessorDelayGranularXFade();
  
  // TestWowAndFlutter();
  
  // TestProcessorMonophonicMode();
  
  // TestHysteresisFilter();
  
  // TestDelayModulation();
  // TestFreezeGlitch();
    
  // TestWavetableGrain();
  // TestWavetableGranularPlayer();
  // TestSynth();
  // TestSynthWavetableScanGlitch();
  // TestSynthSimpleSweep();
  // TestSynthFrequencySweep();
  
  // TestAudioRateTriggerGenerator();
  
  // TestSRCDown();

  // TestGateOut();
  
  //TestFreezeGlitch();
}
