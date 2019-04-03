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
// 4 related slope generators.

#include "tides2/poly_slope_generator.h"

namespace tides {
  
/* static */
Ratio PolySlopeGenerator::audio_ratio_table_[21][PolySlopeGenerator::num_channels] = {
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.25f, 4 }, { 0.125f, 8 } },
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.33333333f, 3 }, { 0.2f, 5 } },
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.33333333f, 3 }, { 0.25f, 4 } },
  { { 1.0f, 1 }, { 0.66666666f, 3 }, { 0.44444444f, 9 }, { 0.296296297f, 27 } },
  { { 1.0f, 1 }, { 0.66666666f, 3 }, { 0.5f, 2 }, { 0.33333333f, 3 } },
  { { 1.0f, 1 }, { 0.75f, 4 }, { 0.66666666f, 3 }, { 0.5f, 2 } },
  { { 1.0f, 1 }, { 0.790123456f, 81 }, { 0.66666666f, 3 }, { 0.5f, 2 } },
  { { 1.0f, 1 }, { 0.790123456f, 81 }, { 0.75f, 4 }, { 0.66666666f, 3 } },
  { { 1.0f, 1 }, { 0.88888888f, 9 }, { 0.790123456f, 81 }, { 0.66666666f, 3 } },
  
  { { 1.0f, 1 }, { 0.99090909091f, 109 }, { 0.987341772f, 79 }, { 0.9811320755f, 53 } },
  { { 1.0f, 1 }, { 1.0f, 1 }, { 1.0f, 1 }, { 1.0f, 1 } },
  { { 1.0f, 1 }, { 1.009174312f, 109 }, { 1.01265823f, 79 }, { 1.0188679245f, 53 } },

  { { 1.0f, 1 }, { 1.125f, 8 }, { 1.265625f, 64 }, { 1.5f, 2 } },  // CDEG
  { { 1.0f, 1 }, { 1.265625f, 64 }, { 1.3333333f, 3 }, { 1.5f, 2 } },  // CEFG
  { { 1.0f, 1 }, { 1.265625f, 64 }, { 1.5f, 2 }, { 2.0f, 1 } },  // CEGC
  { { 1.0f, 1 }, { 1.33333333f, 3 }, { 1.5f, 2 }, { 2.0f, 1 } },     // CFGC
  { { 1.0f, 1 }, { 1.5f, 2 }, { 2.0f, 1 }, { 3.0f, 1 } },  // CGCG
  { { 1.0f, 1 }, { 1.5f, 2 }, { 2.25f, 4 }, { 3.375f, 8 } },  // CGDA
  { { 1.0f, 1 }, { 2.0f, 1 }, { 3.0f, 1 }, { 4.0f, 1 } },  // CCGC
  { { 1.0f, 1 }, { 2.0f, 1 }, { 3.0f, 1 }, { 5.0f, 1 } },  // CCGD
  { { 1.0f, 1 }, { 2.0f, 1 }, { 4.0f, 1 }, { 8.0f, 1 } },  // CCCC
};

/* static */
Ratio PolySlopeGenerator::control_ratio_table_[21][PolySlopeGenerator::num_channels] = {
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.25f, 4 }, { 0.125f, 8 } },
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.33333333f, 3 }, { 0.2f, 5 } },
  { { 1.0f, 1 }, { 0.5f, 2 }, { 0.33333333f, 3 }, { 0.25f, 4 } },
  { { 1.0f, 1 }, { 0.66666666f, 3 }, { 0.5f, 2 }, { 0.25f, 4 } },
  { { 1.0f, 1 }, { 0.66666666f, 3 }, { 0.5f, 2 }, { 0.33333333f, 3 } },
  { { 1.0f, 1 }, { 0.75f, 4 }, { 0.66666666f, 3 }, { 0.5f, 2 } },
  { { 1.0f, 1 }, { 0.8f, 5 }, { 0.66666666f, 3 }, { 0.5f, 2 } },
  { { 1.0f, 1 }, { 0.8f, 5 }, { 0.75f, 3 }, { 0.5f, 2 } },
  { { 1.0f, 1 }, { 0.8f, 5 }, { 0.75f, 4 }, { 0.66666666f, 3 } },
  
  { { 1.0f, 1 }, { 0.909090909091f, 11 }, { 0.857142857143f, 7 }, { 0.8f, 5 } },
  { { 1.0f, 1 }, { 1.0f, 1 }, { 1.0f, 1 }, { 1.0f, 1 } },
  { { 1.0f, 1 }, { 1.09090909091f, 11 }, { 1.142857143f, 7 }, { 1.2f, 5 } },
  
  { { 1.0f, 1 }, { 1.25f, 4 }, { 1.33333333f, 3 }, { 1.5f, 2 } },
  { { 1.0f, 1 }, { 1.25f, 4 }, { 1.33333333f, 3 }, { 2.0f, 2 } },
  { { 1.0f, 1 }, { 1.25f, 4 }, { 1.5f, 3 }, { 2.0f, 2 } },
  { { 1.0f, 1 }, { 1.33333333f, 3 }, { 1.5f, 2 }, { 2.0f, 1 } },
  { { 1.0f, 1 }, { 1.5f, 2 }, { 2.0f, 1 }, { 3.0f, 1 } },
  { { 1.0f, 1 }, { 1.5f, 2 }, { 2.0f, 1 }, { 4.0f, 1 } },
  { { 1.0f, 1 }, { 2.0f, 1 }, { 3.0f, 1 }, { 4.0f, 1 } },
  { { 1.0f, 1 }, { 2.0f, 1 }, { 3.0f, 1 }, { 5.0f, 1 } },
  { { 1.0f, 1 }, { 2.0f, 1 }, { 4.0f, 1 }, { 8.0f, 1 } },
};

/* static */
PolySlopeGenerator::RenderFn PolySlopeGenerator::render_fn_table_[RAMP_MODE_LAST][
    OUTPUT_MODE_LAST][RANGE_LAST];


}  // namespace tides
