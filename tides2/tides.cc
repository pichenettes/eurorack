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

#include <stm32f37x_conf.h>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/dsp/units.h"

#include "tides2/drivers/dac.h"
#include "tides2/drivers/debug_pin.h"
#include "tides2/drivers/gate_inputs.h"
#include "tides2/drivers/system.h"
#include "tides2/cv_reader.h"
#include "tides2/factory_test.h"
#include "tides2/io_buffer.h"
#include "tides2/poly_slope_generator.h"
#include "tides2/ramp/ramp_extractor.h"
#include "tides2/resources.h"
#include "tides2/settings.h"
#include "tides2/ui.h"

using namespace std;
using namespace stmlib;
using namespace tides2;

const bool skip_factory_test = false;
const bool test_adc_noise = false;
const size_t kDacBlockSize = 2;

// #define PROFILE_INTERRUPT

CvReader cv_reader;
Dac dac;
FactoryTest factory_test;
GateInputs gate_inputs;
HysteresisQuantizer2 ratio_index_quantizer;
IOBuffer io_buffer;
PolySlopeGenerator poly_slope_generator;
RampExtractor ramp_extractor;
Settings settings;
Ui ui;

// Default interrupt handlers.
extern "C" {

void NMI_Handler() { }
void HardFault_Handler() { while (1); }
void MemManage_Handler() { while (1); }
void BusFault_Handler() { while (1); }
void UsageFault_Handler() { while (1); }
void SVC_Handler() { }
void DebugMon_Handler() { }
void PendSV_Handler() { }

}

// SysTick and 48kHz handles
extern "C" {

void SysTick_Handler() {
  IWDG_ReloadCounter();
  ui.Poll();
  if (!skip_factory_test) {
    factory_test.Poll();
  }
}

}

IOBuffer::Slice FillBuffer(size_t size) {
  IOBuffer::Slice s = io_buffer.NextSlice(size);
  gate_inputs.Read(s);
  if (io_buffer.new_block()) {
    gate_inputs.ReadNormalization(s.block, cv_reader.fm_cv_thresholded());
    cv_reader.Read(s.block);
  }
  return s;
}

static const float kRoot[3] = {
  0.125f / kSampleRate,
  2.0f / kSampleRate,
  130.81f / kSampleRate
};

static Ratio kRatios[20] = {
  { 0.0625f, 16 },
  { 0.125f, 8 },
  { 0.1666666f, 6 },
  { 0.25f, 4 },
  { 0.3333333f, 3 },
  { 0.5f, 2 },
  { 0.6666666f, 3 },
  { 0.75f, 4 },
  { 0.8f, 5 },
  { 1.0f, 1 },
  { 1.0f, 1 },
  { 1.25f, 4 },
  { 1.3333333f, 3 },
  { 1.5f, 2 },
  { 2.0f, 1 },
  { 3.0f, 1 },
  { 4.0f, 1 },
  { 6.0f, 1 },
  { 8.0f, 1 },
  { 16.0f, 1 },
};

PolySlopeGenerator::OutputSample out[kBlockSize];
GateFlags no_gate[kBlockSize];
float ramp[kBlockSize];
OutputMode previous_output_mode;
bool must_reset_ramp_extractor;

void Process(IOBuffer::Block* block, size_t size) {
#ifdef PROFILE_INTERRUPT
  ScopedDebugPinToggler toggler;
#endif  // PROFILE_INTERRUPT
  const State& state = settings.state();
  const RampMode ramp_mode = RampMode(state.mode);
  const OutputMode output_mode = OutputMode(state.output_mode);
  const Range range = state.range < 2 ? RANGE_CONTROL : RANGE_AUDIO;
  const bool half_speed = output_mode >= OUTPUT_MODE_SLOPE_PHASE;
  float transposition = block->parameters.frequency + block->parameters.fm;
  CONSTRAIN(transposition, -128.0f, 127.0f);
  
  if (test_adc_noise) {
    static float note_lp = 0.0f;
    float note = block->parameters.frequency;
    ONE_POLE(note_lp, note, 0.0001f);
    float cents = (note - note_lp) * 100.0f;
    CONSTRAIN(cents, -8.0f, +8.0f);
    for (size_t j = 0; j < kNumCvOutputs; ++j) {
      for (size_t i = 0; i < size; ++i) {
        block->output[j][i] = settings.dac_code(j, cents);
      }
    }
    return;
  }

  // Because kDacBlockSize half of the samples from the TRIG and CLOCK inputs
  // in block->input are missing.
  if (half_speed) {
    // Not a problem because we're working at SR/2 anyway: just shift everything
    size >>= 1;
    for (size_t i = 0; i < size; ++i) {
      block->input[0][i] = block->input[0][2 * i];
      block->input[1][i] = block->input[1][2 * i];
    }
  } else {
    // Interpolate the missing GATE samples.
    for (size_t i = 0; i < size; i += 2) {
      block->input[0][i + 1] = block->input[0][i] & GATE_FLAG_HIGH;
      block->input[1][i + 1] = block->input[1][i] & GATE_FLAG_HIGH;
    }
  }

  float frequency;
  if (block->input_patched[1]) {
    if (must_reset_ramp_extractor) {
      ramp_extractor.Reset();
    }
    
    Ratio r = ratio_index_quantizer.Lookup(
        kRatios, 0.5f + transposition * 0.0105f);
    frequency = ramp_extractor.Process(
        range == RANGE_AUDIO,
        range == RANGE_AUDIO && ramp_mode == RAMP_MODE_AR,
        r,
        block->input[1],
        ramp,
        size);
    must_reset_ramp_extractor = false;
  } else {
    frequency = kRoot[state.range] * stmlib::SemitonesToRatio(transposition);
    if (half_speed) {
      frequency *= 2.0f;
    }
    must_reset_ramp_extractor = true;
  }
  
  if (output_mode != previous_output_mode) {
    poly_slope_generator.Reset();
    previous_output_mode = output_mode;
  }

  poly_slope_generator.Render(
      ramp_mode,
      output_mode,
      range,
      frequency,
      block->parameters.slope,
      block->parameters.shape,
      block->parameters.smoothness,
      block->parameters.shift,
      block->input_patched[0] ? block->input[0] : no_gate,
      !block->input_patched[0] && block->input_patched[1] ? ramp : NULL,
      out,
      size);
  
  if (half_speed) {
    for (size_t i = 0; i < size; ++i) {
      for (size_t j = 0; j < kNumCvOutputs; ++j) {
        block->output[j][2 * i] = block->output[j][2 * i + 1] =
            settings.dac_code(j, out[i].channel[j]);
      }
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      for (size_t j = 0; j < kNumCvOutputs; ++j) {
        block->output[j][i] = settings.dac_code(j, out[i].channel[j]);
      }
    }
  }
}

void Init() {
  System sys;
  sys.Init(true);
  dac.Init(int(kSampleRate), kDacBlockSize);
  gate_inputs.Init();
  io_buffer.Init();

  bool freshly_baked = !settings.Init();

  cv_reader.Init(&settings);
  
  previous_output_mode = OutputMode(settings.state().output_mode);

  ui.Init(&settings, &factory_test);
  factory_test.Init(&settings, &cv_reader, &gate_inputs, &ui.switches());

  if (freshly_baked && !skip_factory_test) {
    factory_test.Start();
    ui.set_factory_test(true);
  } else {
#ifdef PROFILE_INTERRUPT
    DebugPin::Init();
#endif  // PROFILE_INTERRUPT
  }
  
  poly_slope_generator.Init();
  ratio_index_quantizer.Init(20, 0.05f, false);
  ramp_extractor.Init(kSampleRate, 40.0f / kSampleRate);
  std::fill(&no_gate[0], &no_gate[kBlockSize], GATE_FLAG_LOW);

  sys.StartTimers();
  dac.Start(&FillBuffer);
}

int main(void) {
  Init();
  while (1) {
    io_buffer.Process(factory_test.running() ?
        FactoryTest::ProcessFn : Process);
    ui.DoEvents();
  }
}
