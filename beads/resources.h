// Copyright 2016 Emilie Gillet.
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


#ifndef BEADS_RESOURCES_H_
#define BEADS_RESOURCES_H_


#include "stmlib/stmlib.h"



namespace beads {

typedef uint8_t ResourceId;

extern const float* lookup_table_table[];

extern const int16_t* lookup_table_i16_table[];

extern const int8_t* lookup_table_i8_table[];

extern const int16_t* wavetables_table[];

extern const float lut_sine[];
extern const int16_t wav_integrated_waves[];
#define LUT_SINE 0
#define LUT_SINE_SIZE 1281
#define WAV_INTEGRATED_WAVES 0
#define WAV_INTEGRATED_WAVES_SIZE 49920

}  // namespace beads

#endif  // BEADS_RESOURCES_H_
