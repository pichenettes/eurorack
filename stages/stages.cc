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

#include "chain_state.h"
#include "cv_reader.h"
#include "drivers/dac.h"
#include "drivers/gate_inputs.h"
#include "drivers/serial_link.h"
#include "drivers/system.h"
#include "factory_test.h"
#include "io_buffer.h"
#include "oscillator.h"
#include "segment_generator.h"
#include "settings.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "ui.h"

using namespace stages;
using namespace std;
using namespace stmlib;

const bool skip_factory_test = false;
const bool test_adc_noise = false;

ChainState chain_state;
CvReader cv_reader;
Dac dac;
FactoryTest factory_test;
GateFlags no_gate[kBlockSize];
GateInputs gate_inputs;
SegmentGenerator segment_generator[kNumChannels];
Oscillator oscillator[kNumChannels];
IOBuffer io_buffer;
SerialLink left_link;
SerialLink right_link;
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

// SysTick and 32kHz handles
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
  gate_inputs.Read(s, size);
  if (io_buffer.new_block()) {
    cv_reader.Read(s.block);
    gate_inputs.ReadNormalization(s.block);
  }
  return s;
}

SegmentGenerator::Output out[kBlockSize];
float harmosc_out[kBlockSize];

void Process(IOBuffer::Block* block, size_t size) {
  // NOTE: I don't think we need to pass an out equivalent for oscillator, since
  // that is just used for passing segment/phase info from R->L, which there is
  // no need for in the case of the oscillator.
  chain_state.Update(
      *block,
      &settings,
      &oscillator[0],
      &segment_generator[0],
      out);

  for (size_t channel = 0; channel < kNumChannels; ++channel) {
    if (chain_state.harmosc_status(channel) == ChainState::HARMOSC_STATUS_NONE) {
      bool led_state = segment_generator[channel].Process(
          block->input_patched[channel] ? block->input[channel] : no_gate,
          out,
          size);
      ui.set_slider_led(channel, led_state, 5);

      for (size_t i = 0; i < size; ++i) {
        block->output[channel][i] = settings.dac_code(channel, out[i].value);
      }
    } else {
      // TODO: UI changes here, or somewhere else?
      for (size_t i = 0; i < size; ++i) {
        oscillator[channel].Render(harmosc_out, size);
        block->output[channel][i] = settings.dac_code(channel, harmosc_out[i]);
      }
    }
  }
}

// TODO: test trigger logic in original ouroboros mode

void Init() {
  System sys;
  sys.Init(true);
  dac.Init(int(kSampleRate), 2);
  gate_inputs.Init();
  io_buffer.Init();
  
  bool freshly_baked = !settings.Init();
  for (size_t i = 0; i < kNumChannels; ++i) {
    segment_generator[i].Init();
    oscillator[i].Init();
  }
  std::fill(&no_gate[0], &no_gate[kBlockSize], GATE_FLAG_LOW);

  cv_reader.Init(&settings);

  ui.Init(&settings, &chain_state);

  if (freshly_baked && !skip_factory_test) {
    factory_test.Start(&settings, &cv_reader, &gate_inputs, &ui);
    ui.set_factory_test(true);
  } else {
    chain_state.Init(&left_link, &right_link);
  }
  
  sys.StartTimers();
  dac.Start(&FillBuffer);
}

int main(void) {
  Init();
  while (1) {
    io_buffer.Process(factory_test.running() ? &FactoryTest::ProcessFn : &Process);
  }
}
