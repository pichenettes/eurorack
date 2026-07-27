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
// Various conversion routines for DX7 patch data.

#include "plaits/dsp/fm/dx_units.h"

namespace plaits {

namespace fm {

/* extern */
const float lut_coarse[32] = {
 -12.000000f,
   0.000000f,
  12.000000f,
  19.019550f,
  24.000000f,
  27.863137f,
  31.019550f,
  33.688259f,
  36.000000f,
  38.039100f,
  39.863137f,
  41.513180f,
  43.019550f,
  44.405276f,
  45.688259f,
  46.882687f,
  48.000000f,
  49.049554f,
  50.039100f,
  50.975130f,
  51.863137f,
  52.707809f,
  53.513180f,
  54.282743f,
  55.019550f,
  55.726274f,
  56.405276f,
  57.058650f,
  57.688259f,
  58.295772f,
  58.882687f,
  59.450356f
};

/* extern */
const float lut_amp_mod_sensitivity[4] = {
  0.0f,
  0.2588f,
  0.4274f,
  1.0f
};
  
/* extern */
const float lut_pitch_mod_sensitivity[8] = {
  0.0f,
  0.0781250f,
  0.1562500f,
  0.2578125f,
  0.4296875f,
  0.7187500f,
  1.1953125f,
  2.0f
};

/* extern */
const float lut_cube_root[17] = {
  0.0f,
  0.39685062976f,
  0.50000000000f,
  0.57235744065f,
  0.62996081605f,
  0.67860466725f,
  0.72112502092f,
  0.75914745216f,
  0.79370070937f,
  0.82548197054f,
  0.85498810729f,
  0.88258719406f,
  0.90856038354f,
  0.93312785379f,
  0.95646563396f,
  0.97871693135f,
  1.0f
};


}  // namespace fm
  
}  // namespace plaits
