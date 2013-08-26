// Copyright 2012 Olivier Gillet.
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

#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "stmlib/utils/dsp.h"

using namespace braids;
using namespace stmlib;

const uint32_t kSampleRate = 96000;
const uint16_t kAudioBlockSize = 24;

void write_wav_header(FILE* fp, int num_samples) {
  uint32_t l;
  uint16_t s;
  
  fwrite("RIFF", 4, 1, fp);
  l = 36 + num_samples * 2;
  fwrite(&l, 4, 1, fp);
  fwrite("WAVE", 4, 1, fp);
  
  fwrite("fmt ", 4, 1, fp);
  l = 16;
  fwrite(&l, 4, 1, fp);
  s = 1;
  fwrite(&s, 2, 1, fp);
  fwrite(&s, 2, 1, fp);
  l = kSampleRate;
  fwrite(&l, 4, 1, fp);
  l = static_cast<uint32_t>(kSampleRate) * 2;
  fwrite(&l, 4, 1, fp);
  s = 2;
  fwrite(&s, 2, 1, fp);
  s = 16;
  fwrite(&s, 2, 1, fp);
  
  fwrite("data", 4, 1, fp);
  l = num_samples * 2;
  fwrite(&l, 4, 1, fp);
}

int main(void) {
  MacroOscillator osc;
  FILE* fp = fopen("sound.wav", "wb");
  write_wav_header(fp, kSampleRate * 10);
  
  osc.Init();
  osc.set_shape(MACRO_OSC_SHAPE_STRUCK_DRUM);
  osc.set_parameters(16000, 24000);
  
  for (uint32_t i = 0; i < kSampleRate * 10 / kAudioBlockSize; ++i) {
    int16_t buffer[kAudioBlockSize];
    uint8_t sync_buffer[kAudioBlockSize];
    uint16_t tri = i * 2;
    uint16_t ramp = i * 150;
    tri = tri > 32767 ? 65535 - tri : tri;
    if ((i % ((kSampleRate / 4) / kAudioBlockSize)) == 0) {
      osc.Strike();
    }
    osc.set_parameters(16384, 0);
    //memset(sync_buffer, 0, sizeof(sync_buffer));
    osc.set_pitch((0 << 7));
    osc.Render(sync_buffer, buffer, kAudioBlockSize);
    fwrite(buffer, sizeof(int16_t), kAudioBlockSize, fp);
  }
  fclose(fp);
}
