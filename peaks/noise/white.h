//

#ifndef PEAKS_WHITE_NOISE_GENERATOR_H_
#define PEAKS_WHITE_NOISE_GENERATOR_H_

#include "stmlib/stmlib.h"

#include "peaks/drums/svf.h"

#include "peaks/gate_processor.h"

namespace peaks {

class WhiteNoiseGenerator {
 public:
  WhiteNoiseGenerator() { }
  ~WhiteNoiseGenerator() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:
  Svf noise_;

  uint32_t phase_[6];

  DISALLOW_COPY_AND_ASSIGN(WhiteNoiseGenerator);
};

}  // namespace peaks

#endif  // PEAKS_WHITE_WHITE_NOISE_GENERATOR_H_
