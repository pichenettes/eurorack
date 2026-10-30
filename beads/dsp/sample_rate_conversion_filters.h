// Copyright 2018 Emilie Gillet.
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
// Sample rate conversion filters.

#ifndef BEADS_DSP_SAMPLE_RATE_CONVERSION_FILTERS_H_
#define BEADS_DSP_SAMPLE_RATE_CONVERSION_FILTERS_H_

namespace stmlib {

// Generated with:
// 3 * scipy.signal.remez(48, [0, 0.200000 / 3, 0.5 / 3, 0.5], [1, 0])
// Cutoff: 5156.2 Hz, Stopband attenuation: -80.6 dB
template<>
struct SRC_FIR<SRC_UP, 3, 48> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -4.356038322e-04, -6.422873762e-04, -2.593371210e-04,  1.171956473e-03,
       3.232472988e-03,  4.319269155e-03,  2.297977786e-03, -3.772076453e-03,
      -1.179032010e-02, -1.632052254e-02, -1.107903116e-02,  6.315252159e-03,
       3.005545167e-02,  4.601037586e-02,  3.788073529e-02, -2.008999173e-03,
      -6.293876553e-02, -1.140020868e-01, -1.141894319e-01, -3.173716631e-02,
       1.362531525e-01,  3.550083711e-01,  5.608964543e-01,  6.857341590e-01,
    };
    return h[i];
  }
};

// Generated with:
// 1 * scipy.signal.remez(48, [0, 0.200000 / 3, 0.5 / 3, 0.5], [1, 0])
// Cutoff: 5156.2 Hz, Stopband attenuation: -80.6 dB
template<>
struct SRC_FIR<SRC_DOWN, 3, 48> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -1.452012774e-04, -2.140957921e-04, -8.644570700e-05,  3.906521578e-04,
       1.077490996e-03,  1.439756385e-03,  7.659925953e-04, -1.257358818e-03,
      -3.930106701e-03, -5.440174180e-03, -3.693010386e-03,  2.105084053e-03,
       1.001848389e-02,  1.533679195e-02,  1.262691176e-02, -6.696663910e-04,
      -2.097958851e-02, -3.800069559e-02, -3.806314397e-02, -1.057905544e-02,
       4.541771750e-02,  1.183361237e-01,  1.869654848e-01,  2.285780530e-01,
    };
    return h[i];
  }
};

// Generated with:
// 2 * scipy.signal.remez(48, [0, 0.325000 / 2, 0.5 / 2, 0.5], [1, 0])
// Cutoff: 6312.5 Hz, Stopband attenuation: -72.6 dB
template<>
struct SRC_FIR<SRC_UP, 2, 48> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -4.287457573e-04, -8.226487157e-04,  7.100052364e-04,  2.247177610e-03,
       4.027580292e-04, -4.271779932e-03, -3.893072135e-03,  4.843915649e-03,
       1.019934854e-02, -9.694387912e-04, -1.727659358e-02, -1.027548927e-02,
       2.001194882e-02,  2.929003471e-02, -1.083867877e-02, -5.164495731e-02,
      -1.821391107e-02,  6.671043028e-02,  7.404344683e-02, -5.604828388e-02,
      -1.683376729e-01, -2.385313356e-02,  3.910061488e-01,  7.674091912e-01,
    };
    return h[i];
  }
};

// Generated with:
// 1 * scipy.signal.remez(48, [0, 0.325000 / 2, 0.5 / 2, 0.5], [1, 0])
// Cutoff: 6312.5 Hz, Stopband attenuation: -72.6 dB
template<>
struct SRC_FIR<SRC_DOWN, 2, 48> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -2.143728787e-04, -4.113243578e-04,  3.550026182e-04,  1.123588805e-03,
       2.013790146e-04, -2.135889966e-03, -1.946536068e-03,  2.421957824e-03,
       5.099674271e-03, -4.847193956e-04, -8.638296788e-03, -5.137744633e-03,
       1.000597441e-02,  1.464501735e-02, -5.419339385e-03, -2.582247866e-02,
      -9.106955536e-03,  3.335521514e-02,  3.702172341e-02, -2.802414194e-02,
      -8.416883646e-02, -1.192656678e-02,  1.955030744e-01,  3.837045956e-01,
    };
    return h[i];
  }
};

// Generated with:
// 2 * scipy.signal.remez(32, [0, 0.250000 / 2, 0.5 / 2, 0.5], [1, 0])
// Cutoff: 5562.5 Hz, Stopband attenuation: -69.1 dB
template<>
struct SRC_FIR<SRC_UP, 2, 32> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -6.323190584e-04, -2.386540179e-03, -5.385471400e-04,  6.009796963e-03,
       8.838821417e-03, -3.684327982e-03, -2.300786763e-02, -1.754875738e-02,
       2.569993004e-02,  5.980935261e-02,  1.513679092e-02, -9.494675352e-02,
      -1.317619816e-01,  4.772618956e-02,  4.066987177e-01,  7.045874953e-01,
    };
    return h[i];
  }
};

// Generated with:
// 1 * scipy.signal.remez(32, [0, 0.250000 / 2, 0.5 / 2, 0.5], [1, 0])
// Cutoff: 5562.5 Hz, Stopband attenuation: -69.1 dB
template<>
struct SRC_FIR<SRC_DOWN, 2, 32> {
  template<int32_t i> inline float Read() const {
    const float h[] = {
      -3.161595292e-04, -1.193270089e-03, -2.692735700e-04,  3.004898481e-03,
       4.419410708e-03, -1.842163991e-03, -1.150393381e-02, -8.774378689e-03,
       1.284996502e-02,  2.990467630e-02,  7.568395461e-03, -4.747337676e-02,
      -6.588099082e-02,  2.386309478e-02,  2.033493589e-01,  3.522937476e-01,
    };
    return h[i];
  }
};

}  // namespace stmlib

#endif  // BEADS_DSP_SAMPLE_RATE_CONVERSION_FILTERS_H_
