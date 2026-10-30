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
// Delay line.

#ifndef BEADS_DSP_DELAY_LINE_H_
#define BEADS_DSP_DELAY_LINE_H_

#include <algorithm>
#include <cassert>

#include "stmlib/stmlib.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/dsp.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/mu_law.h"

namespace beads {

enum Resolution {
  RESOLUTION_8_BIT,
  RESOLUTION_12_BIT,
  RESOLUTION_16_BIT,
  RESOLUTION_LAST
};

template<Resolution resolution, int num_channels>
struct Item { };

template<>
struct Item<RESOLUTION_16_BIT, 1> {
  void Store(int i, FloatFrame s) {
    lr = stmlib::Clip16(static_cast<int32_t>((s.l + s.r) * 16384.0f));
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = result.r = static_cast<float>(lr) / 32768.0f;
    return result;
  }
  enum { bundle_size = 1 };
  int16_t lr;
};

template<>
struct Item<RESOLUTION_16_BIT, 2> {
  void Store(int i, FloatFrame s) {
    l = stmlib::Clip16(static_cast<int32_t>(s.l * 32768.0f));
    r = stmlib::Clip16(static_cast<int32_t>(s.r * 32768.0f));
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = static_cast<float>(l) / 32768.0f;
    result.r = static_cast<float>(r) / 32768.0f;
    return result;
  }
  enum { bundle_size = 1 };
  int16_t l;
  int16_t r;
};

template<>
struct Item<RESOLUTION_12_BIT, 2> {
  void Store(int i, FloatFrame s) {
    l = stmlib::Clip16(static_cast<int32_t>(s.l * 32768.0f)) >> 4;
    r = stmlib::Clip16(static_cast<int32_t>(s.r * 32768.0f)) >> 4;
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = static_cast<float>(l) / 2048.0f;
    result.r = static_cast<float>(r) / 2048.0f;
    return result;
  }
  enum { bundle_size = 1 };
  int16_t l : 12;
  int16_t r : 12;
} __attribute__((packed));

template<>
struct Item<RESOLUTION_12_BIT, 1> {
  void Store(int i, FloatFrame s) {
    int16_t lr = \
        stmlib::Clip16(static_cast<int32_t>((s.l + s.r) * 16384.0f)) >> 4;
    if (i) {
      odd = lr;
    } else {
      even = lr;
    }
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = result.r = static_cast<float>(i ? odd : even) / 2048.0f;
    return result;
  }
  enum { bundle_size = 2 };
  int16_t odd : 12;
  int16_t even : 12;
} __attribute__((packed));

template<>
struct Item<RESOLUTION_8_BIT, 1> {
  void Store(int i, FloatFrame s) {
    lr = Lin2MuLaw(
        stmlib::Clip16(static_cast<int32_t>((s.l + s.r) * 16384.0f)));
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = result.r = MuLaw2Lin(lr);
    return result;
  }
  enum { bundle_size = 1 };
  uint8_t lr;
};

template<>
struct Item<RESOLUTION_8_BIT, 2> {
  void Store(int i, FloatFrame s) {
    l = Lin2MuLaw(stmlib::Clip16(static_cast<int32_t>(s.l * 32768.0f)));
    r = Lin2MuLaw(stmlib::Clip16(static_cast<int32_t>(s.r * 32768.0f)));
  }
  inline FloatFrame value(int i) const {
    FloatFrame result;
    result.l = MuLaw2Lin(l);
    result.r = MuLaw2Lin(r);
    return result;
  }
  enum { bundle_size = 1 };
  uint8_t l;
  uint8_t r;
};

enum Interpolation {
  INTERPOLATION_ZOH,
  INTERPOLATION_LINEAR,
  INTERPOLATION_HERMITE
};

template<
    Resolution resolution,
    int max_delay,
    int num_channels,
    size_t bank_size=262144,
    size_t num_banks=3>
class DelayLine {
 public:
  DelayLine() { }
  ~DelayLine() { }
  
  typedef Item<resolution, num_channels> ItemType;

  enum {
    NUM_CHANNELS = num_channels,
    MAX_DELAY = max_delay,
    
    interpolation_tail = 4,
    freeze_tail_size = 256,
  };
  
  void Init(uint8_t* buffer, size_t allocated_size) {
    assert(
        allocated_size >= bank_size * num_banks + freeze_tail_size_in_bytes());
    uint8_t* buffers[num_banks];
    for (size_t i = 0; i < num_banks; ++i) {
      buffers[i] = buffer;
      buffer += bank_size;
    }
    Init(buffers);
  }

  void Init(uint8_t* buffers[]) {
    /*assert((delay_line_size_in_bytes() + freeze_tail_size_in_bytes()) <= \
           bank_size * num_banks);
    if (resolution == RESOLUTION_16_BIT && num_channels == 2) {
      assert(sizeof(ItemType) == 4);
    }
    if (resolution == RESOLUTION_16_BIT && num_channels == 1) {
      assert(sizeof(ItemType) == 2);
    }
    if (resolution == RESOLUTION_12_BIT && num_channels == 2) {
      assert(sizeof(ItemType) == 3);
    }
    if (resolution == RESOLUTION_12_BIT && num_channels == 1) {
      assert(sizeof(ItemType) == 3);
    }
    if (resolution == RESOLUTION_8_BIT && num_channels == 2) {
      assert(sizeof(ItemType) == 2);
    }
    if (resolution == RESOLUTION_8_BIT && num_channels == 1) {
      assert(sizeof(ItemType) == 1);
    }*/

    freeze_tail_ = static_cast<FloatFrame*>(static_cast<void*>(
        &buffers[num_banks - 1][bank_size - freeze_tail_size_in_bytes()]));
    for (size_t i = 0; i < num_banks; ++i) {
      items_[i] = static_cast<ItemType*>(static_cast<void*>(buffers[i]));
    }
    
    Reset();
  }
  
  inline size_t freeze_tail_size_in_bytes() const {
    return freeze_tail_size * sizeof(FloatFrame);
  }
  
  inline size_t delay_line_size_in_bytes() {
    return (max_delay + interpolation_tail) * sizeof(ItemType) / ItemType::bundle_size;
  }
  
  inline size_t items_per_bank() const {
    return bank_size / sizeof(ItemType);
  }
  
  inline uint8_t* raw_bytes(size_t offset) {
    return static_cast<uint8_t*>(
        static_cast<void*>(
            items_[offset / bank_size])) + (offset % bank_size);
  }
  
  inline int* mutable_write_ptr() { return &write_ptr_; }
  
  inline void Reset(bool clear) {
    write_ptr_ = 0;
    tail_write_ptr_ = 0;
    tail_xfade_duration_ = 0;
    frozen_ = false;
    written_since_last_frozen_ = 0;
    duration_ = max_delay;
    
    if (clear) {
      FloatFrame zero;
      zero.l = zero.r = 0.0f;

      ItemType zero_item;
      zero_item.Store(0, zero);
      zero_item.Store(1, zero);
      for (size_t bank = 0; bank < num_banks; ++bank) {
        std::fill(&items_[bank][0], &items_[bank][items_per_bank()], zero_item);
      }
      std::fill(&freeze_tail_[0], &freeze_tail_[freeze_tail_size], zero);
    }
  }
  
  inline void Reset() {
    Reset(true);
  }
  
  inline void Write(FloatFrame in) {
    Store(write_ptr_, in);
    if (write_ptr_ < interpolation_tail) {
      Store(write_ptr_ + max_delay, in);
    }
    if (write_ptr_ == 0) {
      write_ptr_ = max_delay - 1;
    } else {
      --write_ptr_;
    }
    ++written_since_last_frozen_;
    if (written_since_last_frozen_ >= max_delay) {
      written_since_last_frozen_ = 0;
    }
    ++duration_;
    if (duration_ >= max_delay) {
      duration_ = max_delay;
    }
  }
  
  inline void Trim() {
    tail_write_ptr_ = 0;
    duration_ = 0;
  }
  
  inline void Write(FloatFrame in, bool frozen) {
    if (frozen) {
      if (!frozen_) {
        tail_write_ptr_ = 0;
      }
      if (tail_write_ptr_ < freeze_tail_size) {
        freeze_tail_[tail_write_ptr_].l = in.l;
        freeze_tail_[tail_write_ptr_].r = in.r;
        ++tail_write_ptr_;
      }
      written_since_last_frozen_ = 0;
    } else {
      if (frozen_ && tail_write_ptr_) {
        tail_xfade_duration_ = tail_write_ptr_;
      }
      if (tail_write_ptr_) {
        const int tail_position = tail_xfade_duration_ - tail_write_ptr_;
        const float t = float(tail_write_ptr_) / float(tail_xfade_duration_);
        const float gain = stmlib::SmoothStep(t);
        in.l += (freeze_tail_[tail_position].l - in.l) * gain;
        in.r += (freeze_tail_[tail_position].r - in.r) * gain;

        --tail_write_ptr_;
      }
      Write(in);
    }
    frozen_ = frozen;
  }
  
  template<Interpolation interpolation>
  inline FloatFrame Read(int32_t delay) const {
    int32_t delay_integral = delay >> 10;
    float delay_fractional = float(delay & 1023) / 1024.0f;
    return Read<interpolation>(delay_integral, delay_fractional);
  }
  
  template<Interpolation interpolation>
  inline FloatFrame Read(int delay_integral, float delay_fractional) const {
    if (interpolation == INTERPOLATION_HERMITE) {
      delay_integral += max_delay - 1; // Fix latency.
    }
    int i = (write_ptr_ + delay_integral) % max_delay;
    
    FloatFrame value;
    
    if (interpolation == INTERPOLATION_ZOH) {
      value = Load(i);
    } else if (interpolation == INTERPOLATION_LINEAR) {
      const FloatFrame x0 = Load(i);
      const FloatFrame x1 = Load(i + 1);
      value = x0 + (x1 - x0) * delay_fractional;
    } else {
      FloatFrame x[4];
      LoadQuad(i, x);
      
      const FloatFrame c = (x[2] - x[0]) * 0.5f;
      const FloatFrame v = x[1] - x[2];
      const FloatFrame w = c + v;
      const FloatFrame a = w + v + (x[3] - x[1]) * 0.5f;
      const FloatFrame b_neg = w + a;
      const float t = delay_fractional;
      value = (((a * t) - b_neg) * t + c) * delay_fractional + x[1];
    }
    return value;
  }
  
  template<Interpolation interpolation>
  inline FloatFrame Read(float delay) const {
    MAKE_INTEGRAL_FRACTIONAL(delay)
    return Read<interpolation>(delay_integral, delay_fractional);
  }
  
  inline bool frozen() const { return frozen_; }
  
  inline int duration() const { return duration_; }

  inline int remaining() const {
    return max_delay - written_since_last_frozen_;
  }
  
 private:
  inline void Store(int i, FloatFrame value) {
    mutable_item(i / ItemType::bundle_size)->Store(
        i % ItemType::bundle_size, value);
  }
  
  inline FloatFrame Load(int i) const {
    return item(i / ItemType::bundle_size).value(i % ItemType::bundle_size);
  }
  
  inline void LoadQuad(int i, FloatFrame* x) const {
    // Try to find optimizations if there are several banks and if the
    // memory layout is not too gnarly.
    if (num_banks != 1) {
      if (ItemType::bundle_size == 1) {
        // Load the first item.
        size_t offset = size_t(i) % items_per_bank();
        size_t bank = size_t(i) / items_per_bank();
        if ((offset + 3) < items_per_bank()) {
          // Optimization found! All items belong in the same bank!
          x[0] = items_[bank][offset].value(0);
          x[1] = items_[bank][offset + 1].value(0);
          x[2] = items_[bank][offset + 2].value(0);
          x[3] = items_[bank][offset + 3].value(0);
          return;
        }
      } else if (ItemType::bundle_size == 2) {
        // Load the first item.
        size_t offset = size_t(i >> 1) % items_per_bank();
        size_t bank = size_t(i >> 1) / items_per_bank();

        if ((offset + 2) < items_per_bank()) {
          // Optimization found! All items belong in the same bank!
          if ((i % 2) == 0) {
            x[0] = items_[bank][offset].value(0);
            x[1] = items_[bank][offset].value(1);
            x[2] = items_[bank][offset + 1].value(0);
            x[3] = items_[bank][offset + 1].value(1);
          } else {
            x[0] = items_[bank][offset].value(1);
            x[1] = items_[bank][offset + 1].value(0);
            x[2] = items_[bank][offset + 1].value(1);
            x[3] = items_[bank][offset + 2].value(0);
          }
          return;
        }
      }
    }
    
    // No optmization was found, use the slow code.
    x[0] = Load(i);
    x[1] = Load(i + 1);
    x[2] = Load(i + 2);
    x[3] = Load(i + 3);
  }
   
  // Non-const flavor write access.
  inline ItemType* mutable_item(int i) {
    return const_cast<ItemType*>(&item(i));
  }
  
  inline const ItemType& item(int i) const {
    if (num_banks == 1) {
      return items_[0][i];
    } else {
      size_t offset = size_t(i) % items_per_bank();
      size_t bank = size_t(i) / items_per_bank();
      return items_[bank][offset];
    }
  }
  
  ItemType* items_[num_banks];
  FloatFrame* freeze_tail_;
  
  int write_ptr_;
  int tail_write_ptr_;
  int tail_xfade_duration_;
  int written_since_last_frozen_;
  int duration_;
  
  bool frozen_;

  DISALLOW_COPY_AND_ASSIGN(DelayLine);
};

}  // namespace beads

#endif  // BEADS_DSP_DELAY_LINE_H_
