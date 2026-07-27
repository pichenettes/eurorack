// Copyright 2021 Emilie Gillet.
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
// User data receiver.

#ifndef PLAITS_USER_DATA_RECEIVER_H_
#define PLAITS_USER_DATA_RECEIVER_H_

#include "stm_audio_bootloader/fsk/packet_decoder.h"
#include "stmlib/dsp/dsp.h"

namespace plaits {

class AdaptiveThreshold {
 public:
  AdaptiveThreshold() { }
  ~AdaptiveThreshold() { }
  
  void Init(float lp_coefficient, float threshold) {
    dc_offset_ = 0.0f;
    mean_ = 0.0f;
    power_ = 0.0f;
    previous_ = 0.0f;
    sign_ = false;
    
    lp_coefficient_ = lp_coefficient;
    threshold_ = threshold;
    power_threshold_ = (5.0f * threshold) * (5.0f * threshold);
  }
  
  inline bool ProcessSine(float sample) {
    return Threshold(Center(sample));
  }

  inline bool ProcessCosine(float sample) {
    return Threshold(Differentiate(sample));
  }
  
  inline float Center(float sample) {
    if (power_ < power_threshold_) {
      // The DC offset can be reliably estimated only when there is no signal!
      // This is because the full range signal is not symmetrical (the TRIG
      // input clips below -0.8V).
      ONE_POLE(dc_offset_, sample, lp_coefficient_);
    }
    ONE_POLE(mean_, sample, lp_coefficient_);
    ONE_POLE(power_,
        (sample - mean_) * (sample - mean_), lp_coefficient_ * 10.0f);
    return sample - dc_offset_;
  }
  
  inline float Threshold(float sample) {
    sign_ = sample > threshold_ * (sign_ ? -1.0f : 1.0f);
    return sign_;
  }
  
  inline float Differentiate(float sample) {
    float d = sample - previous_;
    previous_ = sample;
    return d;
  }
  
  inline float dc_offset() const { return dc_offset_; }
  inline float power() const { return power_; }
  
 private:
  float dc_offset_;
  float mean_;
  float power_;
  float previous_;
  bool sign_;
  
  float lp_coefficient_;
  float threshold_;
  float power_threshold_;
  
  DISALLOW_COPY_AND_ASSIGN(AdaptiveThreshold);
};


// Do not reuse the decoder from stm_audio_bootloader: the symbol rate is
// glacially slow, we do not need to buffer incming symbols while packets are
// being decoded. A few symbols might be dropped when we have received a packet
// and are checking its CRC, but we are resync'ing after that anyway!
template<uint32_t pause, uint32_t one, uint32_t zero>
class Demodulator {
 public:
  Demodulator() { }
  ~Demodulator() { }

  void Sync() {
    previous_sample_ = false;
    duration_ = 0;
    skip_ = 4;
  }
  
  uint8_t Process(bool sample) {
    uint8_t symbol = 0xff;
    if (previous_sample_ == sample) {
      ++duration_;
    } else {
      const uint32_t pause_one_threshold = (pause + one) >> 1;
      const uint32_t one_zero_threshold = (one + zero) >> 1;

      previous_sample_ = sample;
      if (duration_ >= pause_one_threshold) {
        symbol = 2;
      } else if (duration_ >= one_zero_threshold) {
        symbol = 1;
      } else {
        symbol = 0;
      }
    
      if (skip_) {
        symbol = 2;
        --skip_;
      }
      duration_ = 0;
    }
    return symbol;
  }

 private:
  uint32_t duration_;

  bool previous_sample_;
  int skip_;

  DISALLOW_COPY_AND_ASSIGN(Demodulator);
};

class UserDataReceiver {
 public:
  UserDataReceiver() { }
  ~UserDataReceiver() { }
  
  void Init(uint8_t* buffer, size_t size);
  stm_audio_bootloader::PacketDecoderState Process(float sample);
  
  inline float progress() const {
    return static_cast<float>(received_) / static_cast<float>(size_);
  }

  inline uint8_t* rx_buffer() {
    return rx_buffer_;
  }
  void Reset();
  
 private:
  void Save();
  
  stm_audio_bootloader::PacketDecoder decoder_;
  AdaptiveThreshold threshold_;
  Demodulator<9, 5, 2> demodulator_;
  stm_audio_bootloader::PacketDecoderState state_;
  
  uint8_t* rx_buffer_;
  size_t size_;
  size_t received_;
  
  DISALLOW_COPY_AND_ASSIGN(UserDataReceiver);
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_RECEIVER_H_
