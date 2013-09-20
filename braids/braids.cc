// Copyright 2012 Olivier Gillet.
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

#include <stm32f10x_conf.h>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/ring_buffer.h"
#include "stmlib/system/system_clock.h"
#include "stmlib/system/uid.h"

#include "braids/drivers/adc.h"
#include "braids/drivers/dac.h"
#include "braids/drivers/debug_pin.h"
#include "braids/drivers/gate_input.h"
#include "braids/drivers/system.h"
#include "braids/envelope.h"
#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/ui.h"

using namespace braids;
using namespace stmlib;

const uint16_t kAudioBufferSize = 128;
const uint16_t kAudioBlockSize = 24;

RingBuffer<uint16_t, kAudioBufferSize> audio_samples;
RingBuffer<uint8_t, kAudioBufferSize> sync_samples;
MacroOscillator osc;
Envelope envelope;
Adc adc;
Dac dac;
DebugPin debug_pin;
GateInput gate_input;
SignatureWaveshaper ws;
System system;
VcoJitterSource jitter_source;
Ui ui;

int16_t render_buffer[kAudioBlockSize];
uint8_t sync_buffer[kAudioBlockSize];

bool trigger_detected_flag;
volatile bool trigger_flag;
uint16_t trigger_delay;

extern "C" {
  
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void NMI_Handler(void) { }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

}

extern "C" {

void SysTick_Handler() {
  system_clock.Tick();  // Tick global ms counter.
  ui.Poll();
}

void TIM1_UP_IRQHandler(void) {
  if (TIM_GetITStatus(TIM1, TIM_IT_Update) == RESET) {
    return;
  }
  
  TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
  
  dac.Write(audio_samples.ImmediateRead());

  bool trigger_detected = gate_input.raised();
  sync_samples.Overwrite(trigger_detected);
  trigger_detected_flag = trigger_detected_flag | trigger_detected;
  
  bool adc_scan_cycle_complete = adc.PipelinedScan();
  if (adc_scan_cycle_complete) {
    ui.UpdateCv(adc.channel(0), adc.channel(1), adc.channel(2), adc.channel(3));
    if (trigger_detected_flag) {
      trigger_delay = settings.trig_delay()
          ? (1 << settings.trig_delay()) : 0;
      ++trigger_delay;
      trigger_detected_flag = false;
    }
    if (trigger_delay) {
      --trigger_delay;
      if (trigger_delay == 0) {
        trigger_flag = true;
      }
    }
  }
}

}

void Init() {
  system.Init(F_CPU / 96000 - 1, true);
  settings.Init();
  ui.Init();
  system_clock.Init();
  adc.Init(false);
  gate_input.Init();
  debug_pin.Init();
  dac.Init();
  osc.Init();
  audio_samples.Init();
  sync_samples.Init();
  
  for (uint16_t i = 0; i < kAudioBufferSize / 2; ++i) {
    audio_samples.Overwrite(0);
    sync_samples.Overwrite(0);
  }
  
  envelope.Init();
  ws.Init(GetUniqueId(2));
  jitter_source.Init(GetUniqueId(1));
  system.StartTimers();
}

const uint16_t bit_reduction_masks[] = {
    0xc000,
    0xe000,
    0xf000,
    0xf800,
    0xff00,
    0xfff0,
    0xffff };

const uint16_t decimation_factors[] = { 24, 12, 6, 4, 3, 2, 1 };

struct TrigStrikeSettings {
  uint8_t attack;
  uint8_t decay;
  uint8_t amount;
};

const TrigStrikeSettings trig_strike_settings[] = {
  { 0, 0, 0 },
  { 0, 32, 30 },
  { 0, 40, 60 },
  { 0, 52, 96 },
  { 8, 72, 80 },
  { 40, 72, 60 },
  { 34, 60, 20 },
};

void RenderBlock() {
  static uint16_t previous_pitch_dac_code = 0;
  static int32_t previous_pitch = 0;

  debug_pin.High();
  
  const TrigStrikeSettings& trig_strike = \
      trig_strike_settings[settings.GetValue(SETTING_TRIG_ACTION)];
  envelope.Update(trig_strike.attack, trig_strike.decay, 0, 0);
  
  if (ui.paques()) {
    osc.set_shape(MACRO_OSC_SHAPE_QUESTION_MARK);
  } else if (settings.meta_modulation()) {
    int32_t shape = adc.channel(3);
    shape = shape > 2048 ? shape - 2048 : 0;
    shape = MACRO_OSC_SHAPE_LAST * shape >> 11;
    if (shape >= MACRO_OSC_SHAPE_DIGITAL_MODULATION) {
      shape = MACRO_OSC_SHAPE_DIGITAL_MODULATION;
    }
    osc.set_shape(static_cast<MacroOscillatorShape>(shape));
  } else {
    osc.set_shape(settings.shape());
  }
  uint16_t parameter_1 = adc.channel(0) << 3;
  parameter_1 += static_cast<uint32_t>(envelope.Render()) * \
      trig_strike.amount >> 9;
  if (parameter_1 > 32767) {
    parameter_1 = 32767;
  }
  osc.set_parameters(parameter_1, adc.channel(1) << 3);
  
  // Apply hysteresis to ADC reading to prevent a single bit error to move
  // the quantized pitch up and down the quantization boundary.
  uint16_t pitch_dac_code = adc.channel(2);
  if (settings.pitch_quantization()) {
    if ((pitch_dac_code > previous_pitch_dac_code + 4) ||
        (pitch_dac_code < previous_pitch_dac_code - 4)) {
      previous_pitch_dac_code = pitch_dac_code;
    } else {
      pitch_dac_code = previous_pitch_dac_code;
    }
  }
  int32_t pitch = settings.dac_to_pitch(pitch_dac_code);
  if (settings.pitch_quantization() == PITCH_QUANTIZATION_QUARTER_TONE) {
    pitch = (pitch + 32) & 0xffffffc0;
  } else if (settings.pitch_quantization() == PITCH_QUANTIZATION_SEMITONE) {
    pitch = (pitch + 64) & 0xffffff80;
  }
  if (!settings.meta_modulation()) {
    pitch += settings.dac_to_fm(adc.channel(3));
  }
  
  // Check if the pitch has changed to cause an auto-retrigger
  int32_t pitch_delta = pitch - previous_pitch;
  if (settings.data().auto_trig &&
      (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
    trigger_flag = true;
  }
  previous_pitch = pitch;
  
  if (settings.vco_drift()) {
    int16_t jitter = jitter_source.Render(adc.channel(1) << 3);
    pitch += (jitter >> 8);
  }

  if (pitch > 32767) {
    pitch = 32767;
  } else if (pitch < 0) {
    pitch = 0;
  }
  
  if (settings.vco_flatten()) {
    if (pitch > 16383) {
      pitch = 16383;
    }
    pitch = Interpolate88(lut_vco_detune, pitch << 2);
  }

  osc.set_pitch(pitch + settings.pitch_transposition());

  if (trigger_flag) {
    osc.Strike();
    envelope.Trigger(ENV_SEGMENT_ATTACK);
    ui.StepMarquee();
    trigger_flag = false;
  }
  
  if (settings.GetValue(SETTING_TRIG_ACTION) == 0) {
    for (size_t i = 0; i < kAudioBlockSize; ++i) {
      sync_buffer[i] = sync_samples.ImmediateRead();
    }
  } else {
    // Disable hardsync when the shaping envelopes are used.
    memset(sync_buffer, 0, sizeof(sync_buffer));
  }

  osc.Render(sync_buffer, render_buffer, kAudioBlockSize);
  
  // Copy to DAC buffer with sample rate and bit reduction applied.
  int16_t sample = 0;
  size_t decimation_factor = decimation_factors[settings.data().sample_rate];
  uint16_t bit_mask = bit_reduction_masks[settings.data().resolution];
  for (size_t i = 0; i < kAudioBlockSize; ++i) {
    if ((i % decimation_factor) == 0) {
      sample = render_buffer[i] & bit_mask;
      if (settings.signature()) {
        sample = ws.Transform(sample);
      }
    }
    audio_samples.Overwrite(sample + 32768);
  }
  debug_pin.Low();
}

int main(void) {
  Init();
  while (1) {
    while (audio_samples.writable() >= kAudioBlockSize) {
      RenderBlock();
    }
    ui.DoEvents();
  }
}
