// Copyright 2017-2018 Emilie Gillet, Fred's Lab
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Author: Frédéric Meslin (fred@fredslab.net)
// Author: Oliver Rockstedt (info@sourcebox.de)
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

// #define ENABLE_ERROR_DETECTION
#define ENABLE_ERASE_BUTTON_COMBO

#include "stmlib/system/bootloader_utils.h"
#include "stmlib/system/system_clock.h"

#include "beads/drivers/codec.h"
#include "beads/drivers/leds.h"
#include "beads/drivers/pots_adc.h"
#include "beads/drivers/switches.h"
#include "beads/drivers/system.h"

#include "stm_audio_bootloader/qpsk/demodulator.h"
#include "stm_audio_bootloader/qpsk/packet_decoder.h"

#include <algorithm>

using namespace beads;
using namespace std;
using namespace stmlib;
using namespace stm_audio_bootloader;

const double kSampleRate = 48000.0;
const double kModulationRate = 6000.0;
const double kBitRate = 12000.0;

const uint32_t kStartAddress = 0x08020000;

Codec codec;
PotsAdc pots_adc;
Leds leds;
Switches switches;
PacketDecoder decoder;
Demodulator demodulator;

void UpdateLeds();
volatile bool switch_released = false;
volatile bool ready = false;

// Default interrupt handlers.
extern "C" {

void SysTick_Handler() {
  HAL_IncTick();
  if (!ready) {
    return;
  }
  system_clock.Tick();
  switches.Debounce();
  if (switches.released(SWITCH_FREEZE)) {
    switch_released = true;
  }
  UpdateLeds();
}

}

enum UiState {
  UI_STATE_WAITING,
  UI_STATE_SYNCING,
  UI_STATE_RECEIVING,
  UI_STATE_ERROR,
  UI_STATE_WRITING
};

volatile UiState ui_state;
volatile int32_t peak;

bool Blink(int mask) {
  return system_clock.milliseconds() & mask;
}

void UpdateLeds() {
  leds.Clear();
  
  switch (ui_state) {
    case UI_STATE_WAITING:
      leds.set(LED_FREEZE, Blink(128) ? 255 : 0);
      leds.set(LED_SEED, Blink(128) ? 0 : 255);
      break;

    case UI_STATE_SYNCING:
      leds.set(LED_FREEZE, Blink(64) ? 255 : 0);
      leds.set(LED_SEED, Blink(64) ? 0 : 255);
      break;

    case UI_STATE_RECEIVING:
      leds.set(LED_FREEZE, Blink(32) ? 255 : 0);
      leds.set(LED_SEED, Blink(32) ? 0 : 255);
      break;

    case UI_STATE_ERROR:
      for (int i = 0; i < LED_LAST; ++i) {
        leds.set(Led(i), Blink(256) ? 255 : 0);
      }
      break;

    case UI_STATE_WRITING:
      for (int i = 0; i < LED_LAST; ++i) {
        leds.set(Led(i), 255);
      }
      break;
  }
  
  if (ui_state != UI_STATE_WRITING) {
    leds.set(LED_VU_METER, peak >> 7);
    if (peak < 8192) {
      leds.set(LED_MEDIA_G, peak >> 5);
      leds.set(LED_MEDIA_R, 0);
      leds.set(LED_MEDIA_B, 0);
    } else if (peak < 16384) {
      leds.set(LED_MEDIA_G, 255);
      leds.set(LED_MEDIA_R, (peak - 8192) >> 5);
      leds.set(LED_MEDIA_B, 0);
    } else {
      leds.set(LED_MEDIA_G, 255 - ((peak - 16384) >> 6));
      leds.set(LED_MEDIA_R, 255);
      leds.set(LED_MEDIA_B, 0);
    }
  }

  leds.Write();
}

int32_t gain_pot = 16;
size_t discard_samples = 8000;

void Process(
    const Codec::Event* events,
    const Codec::Frame* in,
    Codec::Frame* out,
    size_t size) {

  // Read and smooth gain pot (TIME).
  gain_pot = (pots_adc.value(POTS_ADC_CHANNEL_TIME) + 1023 * gain_pot) >> 10;
  int32_t gain = ((gain_pot >> 1) * gain_pot >> 21) + 128;  // 128 to 1152

  if (!discard_samples) {
    for (size_t i = 0; i < size; ++i) {
      int32_t sample = (in[i].l >> 16) + (in[i].r >> 16);
      sample = sample * gain >> 7;
      CONSTRAIN(sample, -32768, 32767);
      
      int32_t rect = sample > 0 ? sample : -sample;
      peak = rect > peak ? rect : (rect + 32767 * peak) >> 15;
      
      demodulator.PushSample(2048 + (sample >> 4));
    }
  } else {
    --discard_samples;
  }
  
  copy(&in[0], &in[size], &out[0]);
  
  pots_adc.Convert();
}

static size_t current_address;
static uint16_t packet_index;
const uint32_t kBlockSize = 0x10000;
const uint16_t kPacketsPerBlock = ::kBlockSize / kPacketSize;
uint8_t rx_buffer[::kBlockSize];

#ifdef ENABLE_ERROR_DETECTION

bool ProgramPage(const uint8_t* data, size_t size) {
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }
  
  // Erase a sector if the current address is the beginning of a sector.
  if ((current_address % 0x00020000) == 0) {
    const size_t sector_index = (current_address - 0x08000000) / 0x00020000;

    FLASH_EraseInitTypeDef erase_params;
    erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_params.Banks = (sector_index < 8) ? FLASH_BANK_1 : FLASH_BANK_2;
    erase_params.Sector = sector_index;
    erase_params.NbSectors = 1;
    erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    uint32_t sector_error;
    if (HAL_FLASHEx_Erase(&erase_params, &sector_error) != HAL_OK) {
      return false;
    }
  }
  
  const size_t flash_block_size = 32;

  for (size_t written = 0; written < size; written += flash_block_size) {
    if (HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        current_address,
        (uint64_t)(&data[0])) != HAL_OK) {
      return false;
    }
    current_address += flash_block_size;
    data += flash_block_size;
  }
  if (HAL_FLASH_Lock() != HAL_OK) {
    return false;
  }
  
  return true;
}

#else

void ProgramPage(const uint8_t* data, size_t size) {
  HAL_FLASH_Unlock();
  
  // Erase a sector if the current address is the beginning of a sector.
  if ((current_address % 0x00020000) == 0) {
    const size_t sector_index = (current_address - 0x08000000) / 0x00020000;

    FLASH_EraseInitTypeDef erase_params;
    erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_params.Banks = (sector_index < 8) ? FLASH_BANK_1 : FLASH_BANK_2;
    erase_params.Sector = sector_index;
    erase_params.NbSectors = 1;
    erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    uint32_t sector_error;
    HAL_FLASHEx_Erase(&erase_params, &sector_error);
  }
  
  const size_t flash_block_size = 32;

  for (size_t written = 0; written < size; written += flash_block_size) {
    HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        current_address,
        (uint64_t)(&data[0]));
    current_address += flash_block_size;
    data += flash_block_size;
  }
  HAL_FLASH_Lock();
}

#endif  // ENABLE_ERROR_DETECTION

void EraseAllFirmwareSectors() {
  HAL_FLASH_Unlock();
  FLASH_EraseInitTypeDef erase_params;
  erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_params.Banks = FLASH_BANK_1;
  erase_params.Sector = 1;
  erase_params.NbSectors = 7;
  erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
  uint32_t sector_error;
  for (int i = 0; i < 2; ++i) {
    if (HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK) {
      break;
    }
  }
  HAL_FLASH_Lock();
}

void InitializeReception() {
  decoder.Init(20000, true);
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

  switches.Init();
  leds.Init();
  pots_adc.Init();
  pots_adc.StartMuxedPotsConversion();
  pots_adc.Convert();
  ready = true;
  
  codec.Init();
  codec.Start(48000, 12, &Process);
}

int main(void) {
  Init();
  InitializeReception();

  bool exit_updater = !switches.pressed_immediate(SWITCH_FREEZE);

#ifdef ENABLE_ERASE_BUTTON_COMBO
  if (switches.pressed_immediate(SWITCH_FREEZE) && switches.pressed_immediate(SWITCH_ASSIGN)) {
    EraseAllFirmwareSectors();
    InitializeReception();
  }
#endif  // ENABLE_ERASE_BUTTON_COMBO

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
            const uint8_t* source = decoder.packet_data();
            uint8_t* destination = rx_buffer + \
                (packet_index % kPacketsPerBlock) * kPacketSize;
            copy(source, source + kPacketSize, destination);
            ++packet_index;
            if ((packet_index % kPacketsPerBlock) == 0) {
              ui_state = UI_STATE_WRITING;
              UpdateLeds();
#ifdef ENABLE_ERROR_DETECTION
              if (!ProgramPage(rx_buffer, ::kBlockSize)) {
                EraseAllFirmwareSectors();
                error = true;
              }
#else
              ProgramPage(rx_buffer, ::kBlockSize);
#endif  // ENABLE_ERROR_DETECTION
              ui_state = UI_STATE_SYNCING;
              decoder.Reset();
              demodulator.SyncCarrier(false);
            } else {
              decoder.Reset();
              demodulator.SyncDecision();
            }
          }
          break;
        case PACKET_DECODER_STATE_ERROR_SYNC:
        case PACKET_DECODER_STATE_ERROR_CRC:
          error = true;
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
  codec.Stop();
  pots_adc.DeInit();
  Uninitialize();
  JumpTo(kStartAddress);
  while (1) { }
}
