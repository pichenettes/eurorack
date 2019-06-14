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
#include <cstring>

#include "stmlib/system/bootloader_utils.h"
#include "stmlib/system/flash_programming.h"
#include "stmlib/system/system_clock.h"

#include "stm_audio_bootloader/qpsk/packet_decoder.h"
#include "stm_audio_bootloader/qpsk/demodulator.h"

#include "tides2/drivers/firmware_update_adc.h"
#include "tides2/drivers/firmware_update_dac.h"
#include "tides2/drivers/leds.h"
#include "tides2/drivers/switches.h"
#include "tides2/drivers/system.h"

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

using namespace tides;
using namespace stmlib;
using namespace std;
using namespace stm_audio_bootloader;

const double kSampleRate = 48000.0;
const double kModulationRate = 6000.0;
const double kBitRate = 12000.0;
const uint32_t kStartAddress = 0x08008000;
const uint16_t kPacketsPerPage = PAGE_SIZE / kPacketSize;

enum UiState {
  UI_STATE_WAITING,
  UI_STATE_RECEIVING,
  UI_STATE_ERROR,
  UI_STATE_WRITING
};

FirmwareUpdateDac dac;
FirmwareUpdateAdc adc;
Leds leds;
Switches switches;
PacketDecoder decoder;
Demodulator demodulator;

int discard_samples = 8000;
int32_t peak = 0;
int32_t gain_pot = 0;
uint32_t current_address;
uint16_t packet_index;
uint8_t rx_buffer[PAGE_SIZE];

volatile bool switch_released = false;
volatile UiState ui_state;

inline void UpdateLeds() {
  leds.Clear();
  
  if (ui_state != UI_STATE_WRITING) {
    uint8_t pwm = system_clock.milliseconds() & 15;
    if (peak < 8192) {
      leds.set(
          LED_RANGE,
          (peak >> 9) > pwm ? LED_COLOR_GREEN : 0);
    } else if (peak < 16384) {
      leds.set(
          LED_RANGE,
          ((peak - 8192) >> 9) >= pwm ? LED_COLOR_YELLOW : LED_COLOR_GREEN);
    } else if (peak < 16384 + 8192) {
      leds.set(
          LED_RANGE,
          ((peak - 16384 - 8192) >> 9) >= pwm ?
              LED_COLOR_RED : LED_COLOR_YELLOW);
    } else {
      leds.set(LED_RANGE, LED_COLOR_RED);
    }
  }
  
  switch (ui_state) {
    case UI_STATE_WAITING:
      leds.set(
          LED_MODE,
          system_clock.milliseconds() & 128 ? LED_COLOR_YELLOW : 0);
      leds.set(
          LED_SHIFT,
          system_clock.milliseconds() & 128 ? 0 : LED_COLOR_YELLOW);
      break;

    case UI_STATE_RECEIVING:
      leds.set(
          LED_MODE,
          system_clock.milliseconds() & 32 ? LED_COLOR_GREEN : 0);
      leds.set(
          LED_SHIFT,
          system_clock.milliseconds() & 32 ? 0 : LED_COLOR_GREEN);
      break;

    case UI_STATE_ERROR:
      {
        bool on = system_clock.milliseconds() & 256;
        for (int i = 0; i < LED_LAST; ++i) {
          leds.set(Led(i), on ? LED_COLOR_RED : 0);
        }
      }
      break;

    case UI_STATE_WRITING:
      {
        for (int i = 0; i < LED_LAST; ++i) {
          leds.set(Led(i), LED_COLOR_GREEN);
        }
      }
      break;
  }
  
  leds.Write();
}

extern "C" {
  
void SysTick_Handler() {
  IWDG_ReloadCounter();
  system_clock.Tick();
  switches.Debounce();
  if (switches.released(SWITCH_MODE)) {
    switch_released = true;
  }
  UpdateLeds();
}

}

uint16_t NextSample() {
  static int subsample = 0;
  
  int32_t pot = adc.pot();
  int32_t sample = 32768 - adc.sample();
  adc.Convert();

  ++subsample;
  gain_pot = (pot + 4095 * gain_pot) >> 12;

  int32_t gain = ((gain_pot >> 1) * gain_pot >> 21) + 128;
  sample = sample * gain >> 8;
  CONSTRAIN(sample, -32768, 32767)

  int32_t rect = abs(sample);
  peak = rect > peak ? rect : (rect + 32767 * peak) >> 15;

  uint16_t sample_12bit = 2048 + (sample >> 6);
  uint16_t sample_16bit = 32767 - sample;

  if (!discard_samples) {
    demodulator.PushSample(sample_12bit);
  } else {
    --discard_samples;
  }
  
  return sample_16bit;
}

void ProgramPage(const uint8_t* data, size_t size) {
  FLASH_Unlock();
  FLASH_ErasePage(current_address);
  const uint32_t* words = static_cast<const uint32_t*>(
      static_cast<const void*>(data));
  for (size_t written = 0; written < size; written += 4) {
    FLASH_ProgramWord(current_address, *words++);
    current_address += 4;
  }
}

void InitializeReception() {
  decoder.Init(1000, true);
  demodulator.Init(
      kModulationRate / kSampleRate * 4294967296.0,
      kSampleRate / kModulationRate,
      2.0 * kSampleRate / kBitRate);
  demodulator.SyncCarrier(true);
  decoder.Reset();
  current_address = kStartAddress;
  packet_index = 0;
  ui_state = UI_STATE_WAITING;
}

void Init() {
  System sys;
  sys.Init(false);
  leds.Init();
  switches.Init();
  dac.Init(kSampleRate);
  adc.Init();
  sys.StartTimers();
  dac.Start(&NextSample);
}

int main(void) {
  Init();
  InitializeReception();

  bool exit_updater = !switches.pressed_immediate(SWITCH_RANGE);
  while (!exit_updater) {
    bool error = false;

    if (demodulator.state() == DEMODULATOR_STATE_OVERFLOW) {
      error = true;
    } else {
      demodulator.ProcessAtLeast(32);
    }

    while (demodulator.available() && !error && !exit_updater) {
      uint8_t symbol = demodulator.NextSymbol();
      PacketDecoderState state = decoder.ProcessSymbol(symbol);
      switch (state) {
        case PACKET_DECODER_STATE_OK:
          {
            ui_state = UI_STATE_RECEIVING;
            memcpy(
                rx_buffer + (packet_index % kPacketsPerPage) * kPacketSize,
                decoder.packet_data(),
                kPacketSize);
            ++packet_index;
            if ((packet_index % kPacketsPerPage) == 0) {
              ui_state = UI_STATE_WRITING;
              ProgramPage(rx_buffer, PAGE_SIZE);
              decoder.Reset();
              demodulator.SyncCarrier(false);
              ui_state = UI_STATE_RECEIVING;
            } else {
              decoder.Reset();
              demodulator.SyncDecision();
            }
          }
          break;
        case PACKET_DECODER_STATE_ERROR_CRC:
        case PACKET_DECODER_STATE_ERROR_SYNC:
          error = true;
          break;
          break;
        case PACKET_DECODER_STATE_END_OF_TRANSMISSION:
          exit_updater = true;
          break;
        default:
          break;
      }
    }

    if (error) {
      ui_state = UI_STATE_ERROR;
      switch_released = false;
      while (!switch_released);  // Polled in ISR
      InitializeReception();
    }
  }

  adc.DeInit();
  dac.Stop();

  Uninitialize();
  JumpTo(kStartAddress);
  while (1) { }
}
