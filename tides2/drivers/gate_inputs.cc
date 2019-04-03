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
// Driver for the two clock inputs and their normalization probe.

#include "tides2/drivers/gate_inputs.h"

#include <stm32f37x_conf.h>

namespace tides {

using namespace std;
using namespace stmlib;

struct GateInputDefinition {
  GPIO_TypeDef* gpio;
  uint16_t pin;
};

const GateInputDefinition gate_input_definition[] = {
  { GPIOB, GPIO_Pin_1 },
  { GPIOB, GPIO_Pin_2 },
};

void GateInputs::Init() {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
  
  // Initialize probe.
  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_init.GPIO_Pin = GPIO_Pin_9;
  GPIO_Init(GPIOA, &gpio_init);
  
  // Initialize inputs.
  gpio_init.GPIO_Mode = GPIO_Mode_IN;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_UP;

  for (size_t i = 0; i < kNumInputs; ++i) {
    gpio_init.GPIO_Pin = gate_input_definition[i].pin;
    GPIO_Init(gate_input_definition[i].gpio, &gpio_init);
  }
  

  for (size_t i = 0; i < kNumInputs + 1; ++i) {
    normalization_mismatch_count_[i] = 0;
    previous_flags_[i] = 0;
    normalized_[i] = false;
  }

  normalization_probe_state_ = 0;
  normalization_decision_count_ = 0;
}

void GateInputs::DisableNormalizationProbe() {
  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Mode = GPIO_Mode_IN;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_init.GPIO_Pin = GPIO_Pin_9;
  GPIO_Init(GPIOA, &gpio_init);
}

void GateInputs::ReadNormalization(
    IOBuffer::Block* block, bool fm_normalization_bit) {
  previous_flags_[kNumInputs] = fm_normalization_bit ? GATE_FLAG_HIGH : 0;

  ++normalization_decision_count_;
  if (normalization_decision_count_ >= kProbeSequenceDuration) {
    normalization_decision_count_ = 0;
    for (size_t i = 0; i < kNumInputs + 1; ++i) {
      normalized_[i] = normalization_mismatch_count_[i] < kProbeSequenceDuration / 8;
      normalization_mismatch_count_[i] = 0;
    }
  }

  int expected_value = normalization_probe_state_ >> 31;
  for (size_t i = 0; i < kNumInputs + 1; ++i) {
    int read_value = previous_flags_[i] & GATE_FLAG_HIGH;
    normalization_mismatch_count_[i] += read_value ^ expected_value;
    block->input_patched[i] = !normalized_[i];
  }

  normalization_probe_state_ = 1103515245 * normalization_probe_state_ + 12345;
  if (normalization_probe_state_ >> 31) {
    GPIOA->BSRR = GPIO_Pin_9;
  } else {
    GPIOA->BRR = GPIO_Pin_9;
  }
}

void GateInputs::Read(const IOBuffer::Slice& slice) {
  for (size_t i = 0; i < kNumInputs; ++i) {
    previous_flags_[i] = ExtractGateFlags(
        previous_flags_[i],
        !(gate_input_definition[i].gpio->IDR & gate_input_definition[i].pin));
    slice.block->input[i][slice.frame_index] = previous_flags_[i];
  }
}

}  // namespace tides
