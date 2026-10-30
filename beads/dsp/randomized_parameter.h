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
// Represents a parameter subject to internal random modulation, or external CV.

#ifndef BEADS_DSP_RANDOMIZED_PARAMETER_H_
#define BEADS_DSP_RANDOMIZED_PARAMETER_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "beads/resources.h"

namespace beads {

inline float CenteredUniform() {
  return 2.0f * stmlib::Random::GetFloat() - 1.0f;
}

inline float PseudoGaussian() {
  const float u = stmlib::Random::GetFloat();
  return u * (u * (4.0f * u - 6.0f) + 4.0f) - 1.0f;
}

inline float BoundedRandomFloat(float spread) {
  const float u = stmlib::Random::GetFloat();
  const float v = stmlib::Random::GetFloat();
  return 0.5f + (u - 0.5f) * std::min(spread * v, 1.0f);
}


class RandomWalkModulation {
 public:
  RandomWalkModulation() { }
  ~RandomWalkModulation() { }
  
  void Init(float frequency) {
    frequency_ = frequency;
    phase_ = 0.0f;
    from_ = 0.0f;
    to_ = 1.0f;
  }
  
  float value(float attenuverter, size_t step_size) {
    float frequency = (attenuverter <= 0.0f ? 0.25f : 4.0f) * frequency_ * \
        float(step_size);
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      from_ = to_;
      to_ = CenteredUniform();
    }
    return from_ + (to_ - from_) * stmlib::SmoothStep(phase_);
  }
  
 private:
  float phase_;
  float frequency_;
  float from_;
  float to_;
  
  DISALLOW_COPY_AND_ASSIGN(RandomWalkModulation);
};

struct RandomizedParameter {
  float base;
  float attenuverter;
  float cv;
  bool has_cv;
  
  inline float randomness_amount() const {
    if (has_cv) {
      return std::max(-attenuverter, 0.0f);
    } else {
      return fabsf(attenuverter);
    }
  }
  
  inline float value(
      RandomWalkModulation* modulation,
      size_t step_size,
      float range,
      bool clamp) const {
    float result = base;

    if (!has_cv) {
      // The attenuverter controls the type of internal modulation.
      if (modulation) {
        result += attenuverter * range * modulation->value(
            attenuverter, step_size);
      } else {
        const float blend = attenuverter > 0.0f
            ? attenuverter
            : std::min(1.0f,
              attenuverter * stmlib::Random::GetFloat() * -3.0f);
        
        const float random_value = !clamp
            ? CenteredUniform() * range
            : stmlib::Random::GetFloat() * range - result;
        result += random_value * blend;
      }
    } else {
      if (attenuverter < 0.0f) {
        const float extra_modulation = (modulation ? modulation->value(
              1.0f, step_size) : PseudoGaussian());
        result += std::max(cv, 0.0f) * -attenuverter * extra_modulation;
      } else {
        result += attenuverter * cv;
      }
    }
    
    if (clamp) {
      CONSTRAIN(result, 0.0f, 1.0f);
    } 
    
    return result;
  }
  
  inline float value() const {
    return value(NULL, 0, 1.0f, true);
  }
  
  inline float pitch_value() const {
    return stmlib::SemitonesToRatio(value(NULL, 0, 24.0f, false));
  }
};

}  // namespace beads

#endif  // BEADS_DSP_RANDOMIZED_PARAMETER_H_
