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
// FM Algorithms and how to render them.

#include "plaits/dsp/fm/algorithms.h"

namespace plaits {

namespace fm {

#define MOD(n) (n << 4)
#define ADD(n) (n | Algorithms<4>::ADDITIVE_FLAG)
#define OUT(n) n
#define FB_SRC Algorithms<4>::FEEDBACK_SOURCE_FLAG
#define FB_DST MOD(3)
#define FB FB_SRC | FB_DST

// Syntactic sugar
#define NO_MOD MOD(0)
  
// This can be replaced by OUT(0) for code that does not mix into the output buffer.
#define OUTPUT ADD(0)
  
/* static */
template<>
const uint8_t Algorithms<4>::opcodes_[8][4] = {
  {
    // Algorithm 1: 4 -> 3 -> 2 -> 1
    FB | OUT(1),
    MOD(1) | OUT(1),
    MOD(1) | OUT(1),
    MOD(1) | OUTPUT
  },
  {
    // Algorithm 2: 4 + 3 -> 2 -> 1
    FB | OUT(1),
    ADD(1),
    MOD(1) | OUT(1),
    MOD(1) | OUTPUT
  },
  {
    // Algorithm 3: 4 + (3 -> 2) -> 1
    FB | OUT(1),
    OUT(2),
    MOD(2) | ADD(1),
    MOD(1) | OUTPUT
  },
  {
    // Algorithm 4: (4 -> 3) + 2 -> 1
    FB | OUT(1),
    MOD(1) | OUT(1),
    ADD(1),
    MOD(1) | OUTPUT
  },
  {
    // Algorithm 5: (4 -> 3) + (2 -> 1)
    FB | OUT(1),
    MOD(1) | OUTPUT,
    OUT(1),
    MOD(1) | ADD(0)
  },
  {
    // Algorithm 6: (4 -> 3) + (4 -> 2) + (4 -> 1)
    FB | OUT(1),
    MOD(1) | OUTPUT,
    MOD(1) | ADD(0),
    MOD(1) | ADD(0)
  },
  {
    // Algorithm 7: (4 -> 3) + 2 + 1
    FB | OUT(1),
    MOD(1) | OUTPUT,
    ADD(0),
    ADD(0)
  },
  {
    // Algorithm 8: 4 + 3 + 2 + 1
    FB | OUTPUT,
    ADD(0),
    ADD(0),
    ADD(0)
  }
};

/* static */
template<>
const uint8_t Algorithms<6>::opcodes_[32][6] = {
  {
     // Algorithm 1
     FB | OUT(1),               // Op 6
     MOD(1) | OUT(1),           // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 2
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUT(1),           // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     FB | OUT(1),               // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 3
     FB | OUT(1),               // Op 6
     MOD(1) | OUT(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 4
     FB_DST | NO_MOD | OUT(1),  // Op 6
     MOD(1) | OUT(1),           // Op 5
     FB_SRC | MOD(1) | OUTPUT,  // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 5
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     NO_MOD | OUT(1),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 6
     FB_DST | NO_MOD | OUT(1),  // Op 6
     FB_SRC | MOD(1) | OUTPUT,  // Op 5
     NO_MOD | OUT(1),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 7
     FB | OUT(1),               // Op 6
     MOD(1) | OUT(1),           // Op 5
     NO_MOD | ADD(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 8
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUT(1),           // Op 5
     FB | ADD(1),               // Op 4
     MOD(1) | OUTPUT,           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 9
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUT(1),           // Op 5
     NO_MOD | ADD(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     FB | OUT(1),               // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 10
     NO_MOD | OUT(1),           // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     FB | OUT(1),               // Op 3
     MOD(1) | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 11
     FB | OUT(1),               // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 12
     NO_MOD | OUT(1),           // Op 6
     NO_MOD | ADD(1),           // Op 5
     NO_MOD | ADD(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     FB | OUT(1),               // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 13
     FB | OUT(1),               // Op 6
     NO_MOD | ADD(1),           // Op 5
     NO_MOD | ADD(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 14
     FB | OUT(1),               // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 15
     NO_MOD | OUT(1),           // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | OUTPUT,           // Op 3
     FB | OUT(1),               // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 16
     FB | OUT(1),               // Op 6
     MOD(1) | OUT(1),           // Op 5
     NO_MOD | OUT(2),           // Op 4
     MOD(2) | ADD(1),           // Op 3
     NO_MOD | ADD(1),           // Op 2
     MOD(1) | OUTPUT            // Op 1
  },
  {
     // Algorithm 17
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUT(1),           // Op 5
     NO_MOD | OUT(2),           // Op 4
     MOD(2) | ADD(1),           // Op 3
     FB | ADD(1),               // Op 2
     MOD(1) | OUTPUT            // Op 1
  },
  {
     // Algorithm 18
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUT(1),           // Op 5
     MOD(1) | OUT(1),           // Op 4
     FB | ADD(1),               // Op 3
     NO_MOD | ADD(1),           // Op 2
     MOD(1) | OUTPUT            // Op 1
  },
  {
     // Algorithm 19
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 20
     NO_MOD | OUT(1),           // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     FB | OUT(1),               // Op 3
     MOD(1) | ADD(0),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 21
     NO_MOD | OUT(1),           // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     FB | OUT(1),               // Op 3
     MOD(1) | ADD(0),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 22
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 23
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 24
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 25
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     MOD(1) | ADD(0),           // Op 4
     NO_MOD | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 26
     FB | OUT(1),               // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     NO_MOD | OUT(1),           // Op 3
     MOD(1) | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 27
     NO_MOD | OUT(1),           // Op 6
     NO_MOD | ADD(1),           // Op 5
     MOD(1) | OUTPUT,           // Op 4
     FB | OUT(1),               // Op 3
     MOD(1) | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 28
     NO_MOD | OUTPUT,           // Op 6
     FB | OUT(1),               // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | OUT(1),           // Op 2
     MOD(1) | ADD(0)            // Op 1
  },
  {
     // Algorithm 29
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     NO_MOD | OUT(1),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 30
     NO_MOD | OUTPUT,           // Op 6
     FB | OUT(1),               // Op 5
     MOD(1) | OUT(1),           // Op 4
     MOD(1) | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 31
     FB | OUT(1),               // Op 6
     MOD(1) | OUTPUT,           // Op 5
     NO_MOD | ADD(0),           // Op 4
     NO_MOD | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  },
  {
     // Algorithm 32
     FB | OUTPUT,               // Op 6
     NO_MOD | ADD(0),           // Op 5
     NO_MOD | ADD(0),           // Op 4
     NO_MOD | ADD(0),           // Op 3
     NO_MOD | ADD(0),           // Op 2
     NO_MOD | ADD(0)            // Op 1
  }
};

#define INSTANTIATE_RENDERER(n, m, a) { n, m, a, &RenderOperators<n, m, a> }

/* static */
template<>
const Algorithms<4>::RendererSpecs Algorithms<4>::renderers_[] = {
  // Core
  INSTANTIATE_RENDERER(1, -2, false),
  INSTANTIATE_RENDERER(1, -2, true),
  INSTANTIATE_RENDERER(1, -1, false),
  INSTANTIATE_RENDERER(1, -1, true),
  INSTANTIATE_RENDERER(1,  0, false),
  INSTANTIATE_RENDERER(1,  0, true),
  
  // Optimized
  /*INSTANTIATE_RENDERER(4, -1, true),
  INSTANTIATE_RENDERER(4,  0, true),
  INSTANTIATE_RENDERER(2, -1, true),
  INSTANTIATE_RENDERER(2,  0, false),
  INSTANTIATE_RENDERER(2,  0, true),*/

  { 0, 0, 0, NULL}
};

/* static */
template<>
const Algorithms<6>::RendererSpecs Algorithms<6>::renderers_[] = {
  // Core
  INSTANTIATE_RENDERER(1, -2, false),
  INSTANTIATE_RENDERER(1, -2, true),
  INSTANTIATE_RENDERER(1, -1, false),
  INSTANTIATE_RENDERER(1, -1, true),
  INSTANTIATE_RENDERER(1,  0, false),
  INSTANTIATE_RENDERER(1,  0, true),
  
  // Pesky feedback loops spanning several operators
  INSTANTIATE_RENDERER(3,  2, true),
  INSTANTIATE_RENDERER(2,  1, true),

  // Optimized
  /*INSTANTIATE_RENDERER(4, -1, true),
  INSTANTIATE_RENDERER(4,  0, true),
  INSTANTIATE_RENDERER(3, -1, false),
  INSTANTIATE_RENDERER(3, -1, true),
  INSTANTIATE_RENDERER(3,  0, true),
  INSTANTIATE_RENDERER(2, -2, true),
  INSTANTIATE_RENDERER(2, -1, false),
  INSTANTIATE_RENDERER(2, -1, true),
  INSTANTIATE_RENDERER(2,  0, false),
  INSTANTIATE_RENDERER(2,  0, true),*/

  { 0, 0, 0, NULL}
};

}  // namespace fm
  
}  // namespace plaits
