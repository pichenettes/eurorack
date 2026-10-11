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
// Audio processing frames.

#ifndef BEADS_DSP_FRAME_H_
#define BEADS_DSP_FRAME_H_

#include "stmlib/stmlib.h"

namespace beads {

typedef struct { short l; short r; } ShortFrame;

template<typename T>
struct Frame { 
  T l;
  T r;
  
  Frame& operator+= (const Frame& rhs) {
    l += rhs.l;
    r += rhs.r;
    return *this;
  }
  
  Frame& operator-= (const Frame& rhs) {
    l -= rhs.l;
    r -= rhs.r;
    return *this;
  }

  Frame& operator= (const T& rhs) {
    l = rhs;
    r = rhs;
    return *this;
  }

  Frame& operator*= (const T& rhs) {
    l *= rhs;
    r *= rhs;
    return *this;
  }
  
  friend Frame operator+(Frame lhs, const Frame& rhs) {
    lhs += rhs;
    return lhs;
  }

  friend Frame operator-(Frame lhs, const Frame& rhs) {
    lhs -= rhs;
    return lhs;
  }
  
  friend Frame operator*(Frame lhs, const T& rhs) {
    lhs *= rhs;
    return lhs;
  }

  friend Frame operator*(const T& lhs, Frame rhs) {
    rhs *= lhs;
    return rhs;
  }
};

typedef Frame<float> FloatFrame;

}  // namespace beads

#endif  // BEADS_DSP_FRAME_H_
