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

#ifndef PLAITS_DSP_FM_ALGORITHMS_H_
#define PLAITS_DSP_FM_ALGORITHMS_H_

#include "stmlib/dsp/dsp.h"

#include "plaits/dsp/fm/operator.h"

#include <algorithm>

namespace plaits {

namespace fm {

// Number of algorithms as a function of the number of operators.
template<int num_operators> struct NumAlgorithms { enum { N = 1 }; };
template<> struct NumAlgorithms<4> { enum { N = 8 }; };
template<> struct NumAlgorithms<6> { enum { N = 32 }; };

// Store information about all FM algorithms, and which functions to call
// to render them.
// 
// The raw structure of each algorithm is stored as a sequence of "opcodes",
// where each opcode indicates, for each operator, from where does it get
// its phase modulation signal and to which buffer it writes the result.
// This data is compact - 1 byte / algorithm / operator.
//
// At run time, this data is "compiled" into a sequence of function calls
// to pre-compiled renderers. A renderer is specialized to efficiently render
// (without any branching, and as much loop unrolling as possible) one
// operator or a group of operators.
//
// Different code space and speed trade-off can be obtained by increasing the
// palette of available renderers (for example by specializing the code for
// a renderer rendering in a single pass a "tower" of 4 operators).
template<int num_operators>
class Algorithms {
 public:
  Algorithms() { }
  ~Algorithms() { }
  
  enum {
    NUM_ALGORITHMS = NumAlgorithms<num_operators>::N
  };
  
  enum OpcodeFlags {
    DESTINATION_MASK = 0x03,
    SOURCE_MASK = 0x30,
    SOURCE_FEEDBACK = 0x30,
    ADDITIVE_FLAG = 0x04,
    FEEDBACK_SOURCE_FLAG = 0x40,
  };
  
  struct RenderCall {
    RenderFn render_fn;
    int n;
    int input_index;
    int output_index;
  };
  
  inline void Init() {
    for (int i = 0; i < NUM_ALGORITHMS; ++i) {
      Compile(i);
    }
  }
  
  inline const RenderCall& render_call(int algorithm, int op) const {
    return render_call_[algorithm][op];
  }
  
  inline bool is_modulator(int algorithm, int op) const {
    return opcodes_[algorithm][op] & DESTINATION_MASK;
  }
  
 private:
  struct RendererSpecs {
    int n;
    int modulation_source;
    bool additive;
    RenderFn render_fn;
  };
     
  inline RenderFn GetRenderer(int n, int modulation_source, bool additive) {
    for (const RendererSpecs* r = renderers_; r->n; ++r) {
      if (r->n == n && \
          r->modulation_source == modulation_source && \
          r->additive == additive) {
        return r->render_fn;
      }
    }
    return NULL;
  }
  
  inline void Compile(int algorithm) {
    const uint8_t* opcodes = opcodes_[algorithm];
    for (int i = 0; i < num_operators; ) {
      uint8_t opcode = opcodes[i];

      int n = 1;
    
      while (i + n < num_operators) {
        uint8_t from = opcodes[i + n - 1];
        uint8_t to = (opcodes[i + n] & SOURCE_MASK) >> 4;

        bool has_additive = from & ADDITIVE_FLAG;
        bool broken = (from & DESTINATION_MASK) != to;

        if (has_additive || broken) {
          if (to == (opcode & DESTINATION_MASK)) {
            // If the same modulation happens to be reused by subsequent
            // operators (algorithms 19 to 25), discard the chain.
            n = 1;
          }
          break;
        }
        ++n;
      }
    
      // Try to find if a pre-compiled renderer is available for this chain.
      for (int attempt = 0; attempt < 2; ++attempt) {
        uint8_t out_opcode = opcodes[i + n - 1];
        bool additive = out_opcode & ADDITIVE_FLAG;
      
        int modulation_source = -3;
        if (!(opcode & SOURCE_MASK)) {
          modulation_source = -1;
        } else if ((opcode & SOURCE_MASK) != SOURCE_FEEDBACK) {
          modulation_source = -2;
        } else {
          for (int j = 0; j < n; ++j) {
            if (opcodes[i + j] & FEEDBACK_SOURCE_FLAG) {
              modulation_source = j;
            }
          }
        }
        RenderFn fn = GetRenderer(n, modulation_source, additive);
        if (fn) {
          RenderCall* call = &render_call_[algorithm][i];
          call->render_fn = fn;
          call->n = n;
          call->input_index = (opcode & SOURCE_MASK) >> 4;
          call->output_index = out_opcode & DESTINATION_MASK;
          // printf("  Algo %02d. Op %d uses renderer (%d, %d, %d)\n",
          //        algorithm + 1,
          //        num_operators - i,
          //        n, modulation_source, additive);
          break;
        } else {
          // printf("! Algo %02d. Op %d: no renderer for (%d, %d, %d)\n",
          //        algorithm + 1,
          //        num_operators - i,
          //        n, modulation_source, additive);
          if (n == 1) {
            // assert(false);
          } else {
            n = 1;
          }
        }
      }
      i += n;
    }
  }
  
  RenderCall render_call_[NUM_ALGORITHMS][num_operators];
  static const uint8_t opcodes_[NUM_ALGORITHMS][num_operators];
  static const RendererSpecs renderers_[];
  
  DISALLOW_COPY_AND_ASSIGN(Algorithms);
};

/* static */
template<> const uint8_t Algorithms<4>::opcodes_[][4];  // From DX100

/* static */
template<> const Algorithms<4>::RendererSpecs Algorithms<4>::renderers_[];

/* static */
template<> const uint8_t Algorithms<6>::opcodes_[][6];  // From DX7

/* static */
template<> const Algorithms<6>::RendererSpecs Algorithms<6>::renderers_[];

}  // namespace fm

}  // namespace plaits

#endif  // PLAITS_DSP_FM_ALGORITHMS_H_
