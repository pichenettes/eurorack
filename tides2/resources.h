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
// Resources definitions.
//
// Automatically generated with:
// make resources


#ifndef TIDES2_RESOURCES_H_
#define TIDES2_RESOURCES_H_


#include "stmlib/stmlib.h"



namespace tides {

typedef uint8_t ResourceId;

extern const float* lookup_table_table[];

extern const int16_t* lookup_table_i16_table[];

extern const float lut_sine[];
extern const float lut_bipolar_fold[];
extern const float lut_unipolar_fold[];
extern const int16_t lut_wavetable[];
#define LUT_SINE 0
#define LUT_SINE_SIZE 1281
#define LUT_BIPOLAR_FOLD 1
#define LUT_BIPOLAR_FOLD_SIZE 1028
#define LUT_UNIPOLAR_FOLD 2
#define LUT_UNIPOLAR_FOLD_SIZE 1028
#define LUT_WAVETABLE 0
#define LUT_WAVETABLE_SIZE 12300

}  // namespace tides

#endif  // TIDES2_RESOURCES_H_
