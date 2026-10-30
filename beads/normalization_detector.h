// Copyright 2019 Emilie Gillet.
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
// Detection of normalization for the SEED, CV, and audio inputs.

#ifndef BEADS_NORMALIZATION_DETECTOR_H_
#define BEADS_NORMALIZATION_DETECTOR_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/codec.h"
#include "beads/drivers/cv_adc.h"
#include "beads/drivers/normalization_probe.h"

namespace beads {
  
enum NormalizationDetectorChannel {
  NORMALIZATION_DETECTOR_CHANNEL_SEED,
  NORMALIZATION_DETECTOR_CHANNEL_TIME_CV,
  NORMALIZATION_DETECTOR_CHANNEL_SIZE_CV,
  NORMALIZATION_DETECTOR_CHANNEL_SHAPE_CV,
  NORMALIZATION_DETECTOR_CHANNEL_PITCH_CV,
  NORMALIZATION_DETECTOR_CHANNEL_DENSITY_CV,
  NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT,
  NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT,
  NORMALIZATION_DETECTOR_CHANNEL_LAST
};

class NormalizationDetector {
 public:
  NormalizationDetector() { }
  ~NormalizationDetector() { }

  void Init(int prescaler);
  void Process(
      const Codec::Event& codec_event,
      const Codec::Frame& codec_in,
      const uint16_t* cv_values);

  inline bool normalized(NormalizationDetectorChannel channel) const {
    return normalized_[channel];
  }
  
  inline bool mono() {
    return normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT) || \
        normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT);
  }
  
  inline bool no_input() {
    return normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT) && \
        normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT);
  }
  
  inline void Stop() {
    // TODO(pichenettes): it's a dirty way of halting the detector,
    // since it'll utimately kick back into action after 140 hours.
    prescaler_count_ = 1 << 31;
    normalization_probe_.Low();
  }

  inline void Start() {
    prescaler_count_ = prescaler_;
  }

 private:
  static const int kProbeSequenceDuration = 64;
  static const int kMismatchStreakCount = 8;

  bool normalized_[NORMALIZATION_DETECTOR_CHANNEL_LAST];
  int mismatch_count_[NORMALIZATION_DETECTOR_CHANNEL_LAST];
  int mismatch_streak_count_[NORMALIZATION_DETECTOR_CHANNEL_LAST];

  int decision_count_;
  uint32_t rng_state_;
  
  int prescaler_count_;
  int prescaler_;
  
  NormalizationProbe normalization_probe_;

  DISALLOW_COPY_AND_ASSIGN(NormalizationDetector);
};

}  // namespace beads

#endif  // BEADS_NORMALIZATION_DETECTOR_H_