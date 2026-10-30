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

#include "beads/normalization_detector.h"

namespace beads {

using namespace std;

const uint16_t cv_threshold = uint16_t(1.65f / -16.0f * 65536.0f + 32768);
const uint16_t pitch_cv_threshold = uint16_t(1.65f / -10.0f * 65536.0f + 32768);

void NormalizationDetector::Init(int prescaler) {
  rng_state_ = 0;
  decision_count_ = 0;
  prescaler_count_ = prescaler_ = prescaler;
  
  for (size_t i = 0; i < NORMALIZATION_DETECTOR_CHANNEL_LAST; ++i) {
    mismatch_count_[i] = kProbeSequenceDuration;
    mismatch_streak_count_[i] = kMismatchStreakCount;
    normalized_[i] = false;
  }
  normalization_probe_.Init();
}

void NormalizationDetector::Process(
    const Codec::Event& codec_event,
    const Codec::Frame& codec_in,
    const uint16_t* cv_values) {
  --prescaler_count_;
  if (prescaler_count_) {
    return;
  }
  prescaler_count_ = prescaler_;
  
  // A decision is made every kProbeSequenceDuration calls. A channel is
  // considered normalized if the number of mismatches with the random sequence
  // is at most 2. To prevent jack insertion glitches, a long enough
  // sequence of mismatches has to be observed before the status of a channel
  // is changed to unnormalized. Until then, the internal normalization is
  // maintained.
  ++decision_count_;
  if (decision_count_ >= kProbeSequenceDuration) {
    decision_count_ = 0;
    for (size_t i = 0; i < NORMALIZATION_DETECTOR_CHANNEL_LAST; ++i) {
      bool normalized = mismatch_count_[i] <= 2;
      mismatch_count_[i] = 0;
      if (normalized) {
        mismatch_streak_count_[i] = 0;
      } else {
        ++mismatch_streak_count_[i];
        if (mismatch_streak_count_[i] > kMismatchStreakCount) {
          mismatch_streak_count_[i] = kMismatchStreakCount;
        }
      }
      normalized_[i] = mismatch_streak_count_[i] < kMismatchStreakCount;
    }
  }
  
  bool expected_value = (rng_state_ >> 31) != 0;
  bool read_value[NORMALIZATION_DETECTOR_CHANNEL_LAST];
  
  read_value[NORMALIZATION_DETECTOR_CHANNEL_SEED] = codec_event & \
      Codec::EVENT_SEED;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_TIME_CV] = \
      cv_values[CV_ADC_CHANNEL_TIME] < cv_threshold;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_SIZE_CV] = \
      cv_values[CV_ADC_CHANNEL_SIZE] < cv_threshold;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_SHAPE_CV] = \
      cv_values[CV_ADC_CHANNEL_SHAPE] < cv_threshold;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_PITCH_CV] = \
      cv_values[CV_ADC_CHANNEL_PITCH] < pitch_cv_threshold;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_DENSITY_CV] = \
      cv_values[CV_ADC_CHANNEL_DENSITY] < pitch_cv_threshold;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT] = codec_in.l > 0;
  read_value[NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT] = codec_in.r > 0;
  
  for (size_t i = 0; i < NORMALIZATION_DETECTOR_CHANNEL_LAST; ++i) {
    if (read_value[i] != expected_value) {
      ++mismatch_count_[i];
    }
  }
  
  rng_state_ = 1103515245 * rng_state_ + 12345;
  normalization_probe_.Set(rng_state_ >> 31);
}

}  // namespace beads