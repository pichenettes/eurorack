//

#include "peaks/noise/white.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void WhiteNoiseGenerator::Init() {
  noise_.Init();
  noise_.set_frequency(105 << 7);  // 8kHz
  noise_.set_resonance(24000);
  noise_.set_mode(SVF_MODE_HP);
}

int16_t WhiteNoiseGenerator::ProcessSingleSample(uint8_t control) {

  int16_t noise = Random::GetSample();

  CLIP(noise);
  return noise;
}

}  // namespace peaks
