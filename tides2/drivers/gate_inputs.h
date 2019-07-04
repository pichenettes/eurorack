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
// Driver for the two gate/trigger inputs.

#ifndef TIDES_DRIVERS_GATE_INPUTS_H_
#define TIDES_DRIVERS_GATE_INPUTS_H_

#include "stmlib/stmlib.h"

#include "tides2/io_buffer.h"

namespace tides {
  
class GateInputs {
 public:
  GateInputs() { }
  ~GateInputs() { }
  
  void Init();
  void Read(const IOBuffer::Slice& slice);
  void ReadNormalization(IOBuffer::Block* block, bool fm_normalization_bit);
  void DisableNormalizationProbe();
  
  inline bool is_normalized(int i) const {
    return normalized_[i];
  }
  
  inline bool value(int i) const {
    return previous_flags_[i] & stmlib::GATE_FLAG_HIGH;
  }
  uint32_t normalization_probe_state_;
    
 private:
  static const int kProbeSequenceDuration = 64;

  stmlib::GateFlags previous_flags_[kNumInputs + 1];

  bool normalized_[kNumInputs + 1];
  int normalization_mismatch_count_[kNumInputs + 1];
  int normalization_decision_count_;
  
  DISALLOW_COPY_AND_ASSIGN(GateInputs);
};

}  // namespace tides

#endif  // TIDES_DRIVERS_GATE_INPUTS_H_
