// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to enable, copy, modify, merge, publish, distribute, sublicense, and/or sell
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

#ifndef TIDES_POLY_SLOPE_GENERATOR_H_
#define TIDES_POLY_SLOPE_GENERATOR_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/hysteresis_quantizer.h"

#include "tides2/ramp_generator.h"
#include "tides2/ramp_shaper.h"
#include "tides2/resources.h"

namespace tides2 {

#define INSTANTIATE(x, y, z) \
  render_fn_table_[x][y][z] = &PolySlopeGenerator::RenderInternal<x, y, z>;

#define INSTANTIATE_RAM(x, y, z) \
  render_fn_table_[x][y][z] = &PolySlopeGenerator::RenderInternal_RAM<x, y, z>;

template<size_t num_channels>
class Filter {
 public:
  Filter() { }
  ~Filter() { }

  void Init() {
    std::fill(&lp_1_[0], &lp_1_[num_channels], 0.0f);
    std::fill(&lp_2_[0], &lp_2_[num_channels], 0.0f);
  }

  template<size_t num_effective_channels>
  inline void Process(
      float* f,
      float* in_out,
      size_t size) {
    while (size--) {
      for (size_t i = 0; i < num_effective_channels; ++i) {
        ONE_POLE(lp_1_[i], *in_out, f[i]);
        ONE_POLE(lp_2_[i], lp_1_[i], f[i]);
        *in_out++ = lp_2_[i];
      }
      in_out += num_channels - num_effective_channels;
    }
  }
  
 private:
  float lp_1_[num_channels];
  float lp_2_[num_channels];

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

class PolySlopeGenerator {
 public:
  PolySlopeGenerator() { }
  ~PolySlopeGenerator() { }
  
  enum {
    num_channels = 4
  };
  
  struct OutputSample {
    float channel[num_channels];
  };
  
  void Reset() {
    filter_.Init();
  }
  
  void Init() {
    frequency_ = 0.01f;
    pw_ = 0.0f;
    shift_ = 0.0f;
    shape_ = 0.0f;
    fold_ = 0.0f;
    
    ramp_generator_.Init();
    for (size_t i = 0; i < num_channels; ++i) {
      ramp_shaper_[i].Init();
      ramp_waveshaper_[i].Init();
    }
    filter_.Init();
    
    ratio_index_quantizer_.Init(21, 0.05f, false);
    
    // Force template instantiation for all combinations of settings.
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);

    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);

    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);
  }
  
  typedef void (PolySlopeGenerator::*RenderFn)(
      float frequency, float pw, float shape, float smoothness, float shift,
      const stmlib::GateFlags* gate_flags, const float* ramp,
      OutputSample* output, size_t size);
  
  void Render(
      RampMode ramp_mode,
      OutputMode output_mode,
      Range range,
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    
    // const float max_ratio = output_mode == OUTPUT_MODE_FREQUENCY
    //     ? (range == RANGE_CONTROL ? 0.125f : 0.25f)
    //     : 1.0f;
    const float max_ratio = 1.0f;
    frequency = std::min(frequency, 0.25f * max_ratio);
    
    if (range == RANGE_CONTROL && pw < 0.5f) {
      // Skew the response of the pulse width parameter, so that a more
      // interesting range of attack times can be set.
      pw = 0.5f + 0.6f * (pw - 0.5f) / (fabsf(pw - 0.5f) + 0.1f);
    }

    if (ramp && ramp_mode == RAMP_MODE_AR) {
      // When locking unto an external ramp, adjust the frequency to get
      // interesting trapezoidal shapes.
      frequency *= 1.0f + 2.0f * fabsf(pw - 0.5f);
    }
    
    const float slope = 3.0f + fabsf(pw - 0.5f) * 5.0f;
    const float shape_amount = fabsf(shape - 0.5f) * 2.0f;
    const float shape_amount_attenuation = Tame(frequency, slope, 16.0f);
    shape = 0.5f + (shape - 0.5f) * shape_amount_attenuation;

    if (smoothness > 0.5f) {
      smoothness = 0.5f + (smoothness - 0.5f) * Tame(
          frequency,
          slope * (3.0f + shape_amount * shape_amount_attenuation * 5.0f),
          12.0f);
    }

    (this->*render_fn_table_[ramp_mode][output_mode][range])(
        frequency, pw, shape, smoothness, shift, gate_flags, ramp, out, size);
    
    if (smoothness < 0.5f) {
      float ratio = smoothness * 2.0f;
      ratio *= ratio;
      ratio *= ratio;
      
      float f[4];
      size_t last_channel = output_mode == OUTPUT_MODE_GATES ? 1 : num_channels;
      for (size_t i = 0; i < last_channel; ++i) {
        size_t source = output_mode == OUTPUT_MODE_FREQUENCY ? i : 0;
        f[i] = ramp_generator_.frequency(source) * 0.5f;
        f[i] += (1.0f - f[i]) * ratio;
      }
      if (output_mode == OUTPUT_MODE_GATES) {
        filter_.Process<1>(f, &out[0].channel[0], size);
      } else {
        filter_.Process<num_channels>(f, &out[0].channel[0], size);
      }
    }
  }
  
 private:
  template<RampMode ramp_mode, OutputMode output_mode, Range range>
  inline void RenderInternal(
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    const bool is_phasor = !(range == RANGE_AUDIO && \
        ramp_mode == RAMP_MODE_LOOPING);

    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    stmlib::ParameterInterpolator pwm(&pw_, pw, size);
    stmlib::ParameterInterpolator shift_modulation(
        &shift_, 2.0f * shift - 1.0f, size);
    stmlib::ParameterInterpolator shape_modulation(
        &shape_, is_phasor ? shape * 5.9999f + 5.0f : shape * 3.9999f, size);
    stmlib::ParameterInterpolator fold_modulation(
        &fold_, std::max(2.0f * (smoothness - 0.5f), 0.0f), size);
    
    if (output_mode == OUTPUT_MODE_FREQUENCY) {
      const int ratio_index = ratio_index_quantizer_.Process(shift);
      if (range == RANGE_CONTROL) {
        ramp_generator_.set_next_ratio(control_ratio_table_[ratio_index]);
      } else {
        ramp_generator_.set_next_ratio(audio_ratio_table_[ratio_index]);
      }
    }
    
    for (size_t i = 0; i < size; ++i) {
      const float f0 = fm.Next();
      const float pw = pwm.Next();
      const float shift = shift_modulation.Next();
      const float step = shift * (1.0f / (num_channels - 1));
      const float partial_step = shift * (1.0f / num_channels);
      const float fold = fold_modulation.Next();

      float per_channel_pw[num_channels];
      const float pw_increment = (shift > 0.0f ? (1.0f - pw) : pw) * step;
      for (size_t j = 0; j < num_channels; ++j) {
        per_channel_pw[j] = pw + pw_increment * float(j);
      }

      // Increment ramps.
      if (output_mode == OUTPUT_MODE_SLOPE_PHASE && ramp_mode == RAMP_MODE_AR) {
        if (ramp) {
          ramp_generator_.Step<ramp_mode, output_mode, range, true>(
              f0, per_channel_pw, stmlib::GATE_FLAG_LOW, ramp[i]);
        } else {
          ramp_generator_.Step<ramp_mode, output_mode, range, false>(
              f0, per_channel_pw, gate_flags[i], 0.0f);
        }
      } else {
        if (ramp) {
          ramp_generator_.Step<ramp_mode, output_mode, range, true>(
              f0, &pw, stmlib::GATE_FLAG_LOW, ramp[i]);
        } else {
          ramp_generator_.Step<ramp_mode, output_mode, range, false>(
              f0, &pw, gate_flags[i], 0.0f);
        }
      }
      
      // Compute shape.
      const float shape = shape_modulation.Next();
      MAKE_INTEGRAL_FRACTIONAL(shape);
      const int16_t* shape_table = &lut_wavetable[shape_integral * 1025];
      
      if (output_mode == OUTPUT_MODE_GATES) {
        const float phase = ramp_generator_.phase(0);
        const float frequency = ramp_generator_.frequency(0);
        const float raw = ramp_shaper_[0].Slope<
              ramp_mode, range>(phase, 0.0f, frequency, pw);
        const float slope = ramp_waveshaper_[0].Shape<
              ramp_mode>(raw, shape_table, shape_fractional);

        out[i].channel[0] = Fold<ramp_mode>(slope, fold) * shift;
        out[i].channel[1] = Scale<ramp_mode>(is_phasor
            ? ramp_waveshaper_[1].Shape<ramp_mode>(
                raw, &lut_wavetable[8200], 0.0f)
            : raw);
        out[i].channel[2] = ramp_shaper_[2].EOA<ramp_mode, range>(
            phase, frequency, pw) * 8.0f;
        out[i].channel[3] = ramp_shaper_[3].EOR<ramp_mode, range>(
            phase, frequency, pw) * 8.0f;
      } else if (output_mode == OUTPUT_MODE_AMPLITUDE) {
        const float phase = ramp_generator_.phase(0);
        const float frequency = ramp_generator_.frequency(0);
        const float raw = ramp_shaper_[0].Slope<
              ramp_mode, range>(phase, 0.0f, frequency, pw);
        const float shaped = ramp_waveshaper_[0].Shape<
              ramp_mode>(raw, shape_table, shape_fractional);
        const float slope = Fold<ramp_mode>(shaped, fold) * \
              (shift < 0.0f ? -1.0f : + 1.0f);
        const float channel_index = fabsf(shift * 5.1f);
        for (size_t j = 0; j < num_channels; ++j) {
          const float channel = static_cast<float>(j + 1);
          const float gain = std::max(
              1.0f - fabsf(channel - channel_index), 0.0f);
          const bool equal_pow = range == RANGE_AUDIO;
          out[i].channel[j] = slope * gain * (equal_pow ? (2.0f - gain) : 1.0f);
        }
      } else if (output_mode == OUTPUT_MODE_SLOPE_PHASE) {
        float phase_shift = 0.0f;
        for (size_t j = 0; j < num_channels; ++j) {
          size_t source = ramp_mode == RAMP_MODE_AR ? j : 0;
          out[i].channel[j] = Fold<ramp_mode>(
              ramp_waveshaper_[j].Shape<ramp_mode>(
                  ramp_shaper_[j].Slope<ramp_mode, range>(
                      ramp_generator_.phase(source),
                      phase_shift, 
                      ramp_generator_.frequency(source),
                      ramp_mode == RAMP_MODE_AD ? per_channel_pw[j] : pw),
                  shape_table,
                  shape_fractional),
              fold);
          phase_shift -= range == RANGE_AUDIO ? step : partial_step;
        }
      } else if (output_mode == OUTPUT_MODE_FREQUENCY) {
        for (size_t j = 0; j < num_channels; ++j) {
          out[i].channel[j] = Fold<ramp_mode>(
              ramp_waveshaper_[j].Shape<ramp_mode>(
                  ramp_shaper_[j].Slope<ramp_mode, range>(
                      ramp_generator_.phase(j),
                      0.0f, 
                      ramp_generator_.frequency(j),
                      pw),
                  shape_table,
                  shape_fractional),
              fold);
        }
      }
    }
  }
  
  template<RampMode ramp_mode, OutputMode output_mode, Range range>
  void IN_RAM RenderInternal_RAM(
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    RenderInternal<ramp_mode, output_mode, range>(
        frequency, pw, shape, smoothness, shift, gate_flags, ramp, out, size);
  }
  
  template<RampMode ramp_mode>
  inline float Fold(float unipolar, float fold_amount) {
    if (ramp_mode == RAMP_MODE_LOOPING) {
      float bipolar = 2.0f * unipolar - 1.0f;
      float folded = fold_amount > 0.0f ? stmlib::Interpolate(
          lut_bipolar_fold,
          0.5f + bipolar * (0.03f + 0.46f * fold_amount),
          1024.0f) : 0.0f;
      return 5.0f * (bipolar + (folded - bipolar) * fold_amount);
    } else {
      float folded = fold_amount > 0.0f ? stmlib::Interpolate(
          lut_unipolar_fold,
          unipolar * fold_amount,
          1024.0f) : 0.0f;
      return 8.0f * (unipolar + (folded - unipolar) * fold_amount);
    }
  }
  
  template<RampMode ramp_mode>
  inline float Scale(float unipolar) {
    if (ramp_mode == RAMP_MODE_LOOPING) {
      return 10.0f * unipolar - 5.0f;
    } else {
      return 8.0f * unipolar;
    }
  }
  
  inline float Tame(float f0, float harmonics, float order) {
    f0 *= harmonics;
    float max_f = 0.5f * (1.0f / order);
    float max_amount = 1.0f - (f0 - max_f) / (0.5f - max_f);
    CONSTRAIN(max_amount, 0.0f, 1.0f);
    return max_amount * max_amount * max_amount;
  }
  
  float frequency_;
  float pw_;
  float shift_;
  float shape_;
  float fold_;
  
  stmlib::HysteresisQuantizer2 ratio_index_quantizer_;

  RampGenerator<num_channels> ramp_generator_;

  RampShaper ramp_shaper_[num_channels];
  RampWaveshaper ramp_waveshaper_[num_channels];
  Filter<num_channels> filter_;
  
  static Ratio audio_ratio_table_[21][num_channels];
  static Ratio control_ratio_table_[21][num_channels];
  static RenderFn render_fn_table_[RAMP_MODE_LAST][OUTPUT_MODE_LAST][
      RANGE_LAST];

  DISALLOW_COPY_AND_ASSIGN(PolySlopeGenerator);
};

}  // namespace tides2

#endif  // TIDES_POLY_SLOPE_GENERATOR_H_
