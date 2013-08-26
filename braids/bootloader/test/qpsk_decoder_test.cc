// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "stm_audio_bootloader/qpsk/demodulator.h"
#include "stm_audio_bootloader/qpsk/packet_decoder.h"

#include "stmlib/utils/dsp.h"

using namespace stm_audio_bootloader;
using namespace stmlib;
using namespace std;

const double kModulationRate = 6000.0;
const double kSampleRate = 48000.0;
const double kBitRate = 12000.0;

int main(int argc, char* argv[]) {
  PacketDecoder decoder;
  Demodulator demodulator;
  
  decoder.Init();
  demodulator.Init(
      kModulationRate / kSampleRate * 4294967296.0,
      kSampleRate / kModulationRate,
      2.0 * kSampleRate / kBitRate);

  decoder.Reset();
  demodulator.SyncCarrier(true);
  
  FILE* fp = fopen(argv[1], "rb");
  fseek(fp, 44, SEEK_SET);
  
  bool done = false;
  uint16_t packet_index = 0;  
  
  int32_t previous = 2163;
  
  while (!feof(fp) && !done) {
    short buffer[32];
    size_t num_samples = fread(buffer, sizeof(short), 32, fp);
    
    for (size_t i = 0; i < num_samples; ++i) {
      int32_t sample = (buffer[i] / 40) + 2163;
      // Simulate LP filter.
      //sample = (43198 * previous + sample * 22337) >> 16;
      // Add 4 bits of noise.
      //sample += rand() % 32;
      demodulator.PushSample(sample);
      previous = sample;
    }
    
    demodulator.ProcessAtLeast(32);
    while (demodulator.available() && !done) {
      PacketDecoderState state = decoder.ProcessSymbol(demodulator.NextSymbol());
      switch (state) {
        case PACKET_DECODER_STATE_OK:
          {
            printf("LOG: Successfully decoded packet!\n");
            ++packet_index;
            if (packet_index == 4) {
              packet_index = 0;
              printf("LOG: 4 packets decoded.\n");
              size_t skipped_samples = 500 + (rand() % 1000);
              fseek(fp, ftell(fp) + sizeof(short) * skipped_samples, 0);
              decoder.Reset();
              demodulator.SyncCarrier(false);
            } else {
              decoder.Reset();
              demodulator.SyncDecision();
            }
          }
          break;
          
        case PACKET_DECODER_STATE_END_OF_TRANSMISSION:
          printf("LOG: End of transmission detected!\n");
          done = true;
          break;

        case PACKET_DECODER_STATE_ERROR_SYNC:
          printf("ERR: Sync error!\n");
          done = true;
          break;
          
        case PACKET_DECODER_STATE_ERROR_CRC:
          printf("ERR: CRC error!\n");
          decoder.Reset();
          demodulator.SyncCarrier(false);
          return 0;
          break;
          
        default:
          break;
      }
    }
  }
}
