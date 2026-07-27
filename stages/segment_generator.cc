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
// Multi-stage envelope

#include "stages/segment_generator.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include <cassert>
#include <cmath>
#include <algorithm>

#include "stages/resources.h"

namespace stages {

using namespace stmlib;
using namespace std;
using namespace segment;

// Duration of the "tooth" in the output when a trigger is received while the
// output is high.
const int kRetrigDelaySamples = 32;

// S&H delay (for all those sequencers whose CV and GATE outputs are out of
// sync).
const size_t kSampleAndHoldDelay = kSampleRate * 2 / 1000;

// Clock inhibition following a rising edge on the RESET input
const size_t kClockInhibitDelay = kSampleRate * 5 / 1000;

void SegmentGenerator::Init(stmlib::HysteresisQuantizer2* step_quantizer) {
  process_fn_ = &SegmentGenerator::ProcessMultiSegment;
  
  phase_ = 0.0f;

  zero_ = 0.0f;
  half_ = 0.5f;
  one_ = 1.0f;

  start_ = 0.0f;
  value_ = 0.0f;
  lp_ = 0.0f;
  
  monitored_segment_ = 0;
  active_segment_ = 0;
  previous_segment_ = 0;
  retrig_delay_ = 0;
  primary_ = 0;

  Segment s;
  s.start = &zero_;
  s.end = &zero_;
  s.time = &zero_;
  s.curve = &half_;
  s.portamento = &zero_;
  s.phase = NULL;
  s.if_rising = 0;
  s.if_falling = 0;
  s.if_complete = 0;
  fill(&segments_[0], &segments_[kMaxNumSegments + 1], s);
  
  Parameters p;
  p.primary = 0.0f;
  p.secondary = 0.0f;
  fill(&parameters_[0], &parameters_[kMaxNumSegments], p);
  
  ramp_extractor_.Init(
      kSampleRate,
      1000.0f / kSampleRate);

  delay_line_.Init();
  gate_delay_.Init();
  
  function_quantizer_.Init(2, 0.025f, false);
  address_quantizer_.Init(2, 0.025f, false);
  
  num_segments_ = 0;
  
  first_step_ = 1;
  last_step_ = 1;
  quantized_output_ = false;
  up_down_counter_ = inhibit_clock_ = 0;
  reset_ = false;
  accepted_gate_ = true;
  step_quantizer_ = step_quantizer;
  
  audio_osc_.Init();
}

inline float SegmentGenerator::WarpPhase(float t, float curve) const {
  curve -= 0.5f;
  const bool flip = curve < 0.0f;
  if (flip) {
    t = 1.0f - t;
  }
  const float a = 128.0f * curve * curve;
  t = (1.0f + a) * t / (1.0f + a * t);
  if (flip) {
    t = 1.0f - t;
  }
  return t;
}

inline float SegmentGenerator::RateToFrequency(float rate) const {
  int32_t i = static_cast<int32_t>(rate * 2048.0f);
  CONSTRAIN(i, 0, LUT_ENV_FREQUENCY_SIZE);
  return lut_env_frequency[i];
}

inline float SegmentGenerator::PortamentoRateToLPCoefficient(float rate) const {
  int32_t i = static_cast<int32_t>(rate * 512.0f);
  return lut_portamento_coefficient[i];
}

// Seems popular enough :)
#define TRACK_PREVIOUS_SEGMENT

void SegmentGenerator::ProcessMultiSegment(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  float phase = phase_;
  float start = start_;
  float lp = lp_;
  float value = value_;
  
  while (size--) {
    const Segment& segment = segments_[active_segment_];
    
#ifdef TRACK_PREVIOUS_SEGMENT
    const Segment& previous = segments_[previous_segment_];
    if (!segment.start && previous.phase && segment.end != previous.end) {
      ONE_POLE(
          start,
          *previous.end,
          PortamentoRateToLPCoefficient(*previous.portamento));
    }
#endif  // TRACK_PREVIOUS_SEGMENT

    if (segment.time) {
      phase += RateToFrequency(*segment.time);
    }
    
    bool complete = phase >= 1.0f;
    if (complete) {
      phase = 1.0f;
    }
    value = Crossfade(
        start,
        *segment.end,
        WarpPhase(segment.phase ? *segment.phase : phase, *segment.curve));
  
    ONE_POLE(lp, value, PortamentoRateToLPCoefficient(*segment.portamento));
  
    // Decide what to do next.
    int go_to_segment = -1;
    if (*gate_flags & GATE_FLAG_RISING) {
      go_to_segment = segment.if_rising;
    } else if (*gate_flags & GATE_FLAG_FALLING) {
      go_to_segment = segment.if_falling;
    } else if (complete) {
      go_to_segment = segment.if_complete;
    }
  
    if (go_to_segment != -1) {
      phase = 0.0f;
      const Segment& destination = segments_[go_to_segment];
      start = destination.start
          ? *destination.start
          : (go_to_segment == active_segment_ ? start : value);
      if (go_to_segment != active_segment_) {
        previous_segment_ = active_segment_;
      }
      active_segment_ = go_to_segment;
    }
    
    out->value = lp;
    out->phase = phase;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
  phase_ = phase;
  start_ = start;
  lp_ = lp;
  value_ = value;
}

void SegmentGenerator::ProcessDecayEnvelope(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float frequency = RateToFrequency(parameters_[0].primary);
  while (size--) {
    if (*gate_flags & GATE_FLAG_RISING) {
      phase_ = 0.0f;
      active_segment_ = 0;
    }
  
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ = 1.0f;
      active_segment_ = 1;
    }
    lp_ = value_ = 1.0f - WarpPhase(phase_, parameters_[0].secondary);
    out->value = lp_;
    out->phase = phase_;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
}

void SegmentGenerator::ProcessTimedPulseGenerator(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float frequency = RateToFrequency(parameters_[0].secondary);
  
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  while (size--) {
    if (*gate_flags & GATE_FLAG_RISING) {
      retrig_delay_ = active_segment_ == 0 ? kRetrigDelaySamples : 0;
      phase_ = 0.0f;
      active_segment_ = 0;
    }
    if (retrig_delay_) {
      --retrig_delay_;
    }
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ = 1.0f;
      active_segment_ = 1;
    }
  
    const float p = primary.Next();
    lp_ = value_ = active_segment_ == 0 && !retrig_delay_ ? p : 0.0f;
    out->value = lp_;
    out->phase = phase_;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
}

void SegmentGenerator::ProcessGateGenerator(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  while (size--) {
    if (*gate_flags & GATE_FLAG_RISING) {
      accepted_gate_ = Random::GetFloat() < parameters_[0].secondary * 1.01f;
    }
    active_segment_ = (*gate_flags & GATE_FLAG_HIGH) && accepted_gate_ ? 0 : 1;

    const float p = primary.Next();
    lp_ = value_ = active_segment_ == 0 ? p : 0.0f;
    out->value = lp_;
    out->phase = 0.5f;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
}

void SegmentGenerator::ProcessSampleAndHold(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float coefficient = PortamentoRateToLPCoefficient(
      parameters_[0].secondary);
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  
  while (size--) {
    const float p = primary.Next();
    gate_delay_.Write(*gate_flags);
    if (gate_delay_.Read(kSampleAndHoldDelay) & GATE_FLAG_RISING) {
      value_ = p;
    }
    active_segment_ = *gate_flags & GATE_FLAG_HIGH ? 0 : 1;

    ONE_POLE(lp_, value_, coefficient);
    out->value = lp_;
    out->phase = 0.5f;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
}

void SegmentGenerator::ProcessClockedSampleAndHold(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float frequency = RateToFrequency(parameters_[0].secondary);
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  while (size--) {
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;

      const float reset_time = phase_ / frequency;
      value_ = primary.subsample(1.0f - reset_time);
    }
    primary.Next();
    active_segment_ = phase_ < 0.5f ? 0 : 1;
    out->value = value_;
    out->phase = phase_;
    out->segment = active_segment_;
    ++out;
  }
}

tides::Ratio divider_ratios[] = {
  { 0.249999f, 4 },
  { 0.333333f, 3 },
  { 0.499999f, 2 },
  { 0.999999f, 1 },
  { 1.999999f, 1 },
  { 2.999999f, 1 },
  { 3.999999f, 1 },
};

void SegmentGenerator::ProcessTapLFO(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  ProcessOscillator(false, gate_flags, out, size);
}

void SegmentGenerator::ProcessFreeRunningLFO(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  ProcessOscillator(false, NULL, out, size);
}

void SegmentGenerator::ProcessPLLOscillator(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  ProcessOscillator(true, gate_flags, out, size);
}

void SegmentGenerator::ProcessFreeRunningOscillator(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  ProcessOscillator(true, NULL, out, size);
}

inline float Log2Fast(float x) {
  union {
    float f;
    int32_t w;
  } r;
  r.f = x;
  float log2f = (float)(((r.w >> 23) & 255) - 128);
  r.w  &= ~(255 << 23);
  r.w  += 127 << 23;
  log2f += ((-0.34484843f) * r.f + 2.02466578f) * r.f - 0.67487759f;
  return log2f;
}

void SegmentGenerator::ProcessOscillator(
    bool audio_rate,
    const GateFlags* gate_flags,
    SegmentGenerator::Output* out,
    size_t size) {

  float frequency = 0.0f;
  const float root_note = audio_rate ? 261.6255616f : 2.0439497f;
  float ramp[size];
  
  tides::Ratio r = { 1.0f, 1 };
  if (gate_flags) {
    r = function_quantizer_.Lookup(
        divider_ratios,
        parameters_[0].primary * 1.03f);
    frequency = ramp_extractor_.Process(
        audio_rate, false, r, gate_flags, ramp, size);
  } else {
    float f = 96.0f * (parameters_[0].primary - 0.5f);
    CONSTRAIN(f, -128.0f, 127.0f);
    frequency = SemitonesToRatio(f) * root_note / kSampleRate;
  }
  
  if (audio_rate) {
    audio_osc_.Render(frequency, parameters_[0].secondary, ramp, size);

    // Blinking rate follows the distance to the nearest C.
    float distance_to_c = frequency <= 0.0f
        ? 0.5f
        : Log2Fast(frequency / r.ratio * kSampleRate / root_note);
    
    // Wrap to [-0.5, 0.5]
    MAKE_INTEGRAL_FRACTIONAL(distance_to_c);
    if (distance_to_c_fractional < -0.5f) {
      distance_to_c_fractional += 1.0f;
    } else if (distance_to_c_fractional > 0.5f) {
      distance_to_c_fractional -= 1.0f;
    }
    float d = std::min(2.0f * fabsf(distance_to_c_fractional), 1.0f);

    // Blink f between 0.125 Hz and 16 Hz depending on distance to C.
    const float blink_frequency = float(size) * \
        (16.0f * d * (2.0f - d) + 0.125f) / kSampleRate;
    phase_ += blink_frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    for (size_t i = 0; i < size; ++i) {
      out[i].phase = ramp[i] * 2.0f - 1.0f;
      out[i].value = ramp[i] * 5.0f / 8.0f;
      out[i].segment = phase_ < 0.5f ? 0 : 1;
    }
  } else {
    if (!gate_flags) {
      for (size_t i = 0; i < size; ++i) {
        phase_ += frequency;
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
        }
        ramp[i] = phase_;
      }
    }
    ShapeLFO(parameters_[0].secondary, ramp, out, size);
  }
  active_segment_ = out[size - 1].segment;
}
  
void SegmentGenerator::ProcessDelay(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float max_delay = static_cast<float>(kMaxDelay - 1);
  
  float delay_time = SemitonesToRatio(
      2.0f * (parameters_[0].secondary - 0.5f) * 36.0f) * 0.5f * kSampleRate;
  float clock_frequency = 1.0f;
  float delay_frequency = 1.0f / delay_time;
  
  if (delay_time >= max_delay) {
    clock_frequency = max_delay * delay_frequency;
    delay_time = max_delay;
  }
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  
  active_segment_ = 0;
  while (size--) {
    phase_ += clock_frequency;
    ONE_POLE(lp_, primary.Next(), clock_frequency);
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      delay_line_.Write(lp_);
    }
    
    aux_ += delay_frequency;
    if (aux_ >= 1.0f) {
      aux_ -= 1.0f;
    }
    active_segment_ = aux_ < 0.5f ? 0 : 1;
    
    ONE_POLE(
        value_,
        delay_line_.Read(delay_time - phase_),
        clock_frequency);
    out->value = value_;
    out->phase = aux_;
    out->segment = active_segment_;
    ++out;
  }
}

void SegmentGenerator::ProcessPortamento(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  const float coefficient = PortamentoRateToLPCoefficient(
      parameters_[0].secondary);
  ParameterInterpolator primary(&primary_, parameters_[0].primary, size);
  
  active_segment_ = 0;
  while (size--) {
    value_ = primary.Next();
    ONE_POLE(lp_, value_, coefficient);
    out->value = lp_;
    out->phase = 0.5f;
    out->segment = active_segment_;
    ++out;
  }
}

void SegmentGenerator::ProcessZero(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  value_ = 0.0f;
  active_segment_ = 1;
  while (size--) {
    out->value = 0.0f;
    out->phase = 0.5f;
    out->segment = 1;
    ++out;
  }
}

void SegmentGenerator::ProcessSlave(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  while (size--) {
    active_segment_ = out->segment == monitored_segment_ ? 0 : 1;
    out->value = active_segment_ ? 0.0f : 1.0f - out->phase;
    ++out;
  }
}

/* static */
void SegmentGenerator::ShapeLFO(
    float shape,
    const float* input_phase,
    SegmentGenerator::Output* out,
    size_t size) {
  shape -= 0.5f;
  shape = 2.0f + 9.999999f * shape / (1.0f + 3.0f * fabs(shape));
  
  const float slope = min(shape * 0.5f, 0.5f);
  const float plateau_width = max(shape - 3.0f, 0.0f);
  const float sine_amount = max(
      shape < 2.0f ? shape - 1.0f : 3.0f - shape, 0.0f);
  
  const float slope_up = 1.0f / slope;
  const float slope_down = 1.0f / (1.0f - slope);
  const float plateau = 0.5f * (1.0f - plateau_width);
  const float normalization = 1.0f / plateau;
  const float phase_shift = plateau_width * 0.25f;
  
  while (size--) {
    float phase = *input_phase + phase_shift;
    if (phase > 1.0f) {
      phase -= 1.0f;
    }
    float triangle = phase < slope
        ? slope_up * phase
        : 1.0f - (phase - slope) * slope_down;
    triangle -= 0.5f;
    CONSTRAIN(triangle, -plateau, plateau);
    triangle = triangle * normalization;
    float sine = InterpolateWrap(lut_sine, phase + 0.75f, 1024.0f);
    out->phase = *input_phase;
    out->value = 0.5f * Crossfade(triangle, sine, sine_amount) + 0.5f;
    out->segment = phase < 0.5f ? 0 : 1;
    ++out;
    ++input_phase;
  }
}

void SegmentGenerator::ProcessSequencer(
    const GateFlags* gate_flags, SegmentGenerator::Output* out, size_t size) {
  // Read the value of the small pot to determine the direction.
  Direction direction = Direction(function_quantizer_.Process(
      parameters_[0].secondary));
  
  if (direction == DIRECTION_ADDRESSABLE) {
    reset_ = false;
    active_segment_ = address_quantizer_.Process(
        parameters_[0].primary) + first_step_;
  } else {
    // Detect a rising edge on the slider/CV to reset to the first step.
    if (parameters_[0].primary > 0.125f && !reset_) {
      reset_ = true;
      active_segment_ = direction == DIRECTION_DOWN ? last_step_ : first_step_;
      up_down_counter_ = 0;
      inhibit_clock_ = kClockInhibitDelay;
    }
    if (reset_ && parameters_[0].primary < 0.0625f) {
      reset_ = false;
    }
  }
  while (size--) {
    if (inhibit_clock_) {
      --inhibit_clock_;
    }
    
    bool clockable = !inhibit_clock_ && !reset_ && \
        direction != DIRECTION_ADDRESSABLE;
    
    // If a rising edge is detected on the gate input, advance to the next step.
    if ((*gate_flags & GATE_FLAG_RISING) && clockable) {
      switch (direction) {
        case DIRECTION_UP:
          ++active_segment_;
          if (active_segment_ > last_step_) {
            active_segment_ = first_step_;
          }
          break;

        case DIRECTION_DOWN:
          --active_segment_;
          if (active_segment_ < first_step_) {
            active_segment_ = last_step_;
          }
          break;

        case DIRECTION_UP_DOWN:
          {
            int n = last_step_ - first_step_ + 1;
            if (n == 1) {
              active_segment_ = first_step_;
            } else {
              up_down_counter_ = (up_down_counter_ + 1) % (2 * (n - 1));
              active_segment_ = first_step_ + (up_down_counter_ < n
                  ? up_down_counter_
                  : 2 * (n - 1) - up_down_counter_);
            }
          }
          break;
          
        case DIRECTION_ALTERNATING:
          {
            int n = last_step_ - first_step_ + 1;
            if (n == 1) {
              active_segment_ = first_step_;
            } else if (n == 2) {
              up_down_counter_ = (up_down_counter_ + 1) % 2;
              active_segment_ = first_step_ + up_down_counter_;
            } else {
              up_down_counter_ = (up_down_counter_ + 1) % (4 * n - 8);
              int i = (up_down_counter_ - 1) / 2;
              active_segment_ = first_step_ + ((up_down_counter_ & 1)
                  ? 1 + ((i < (n - 1)) ? i : 2 * (n - 2) - i)
                  : 0);
            }
          }
          break;

        case DIRECTION_RANDOM:
          active_segment_ = first_step_ + static_cast<int>(
              Random::GetFloat() * static_cast<float>(
                  last_step_ - first_step_ + 1));
          break;
          
        case DIRECTION_RANDOM_WITHOUT_REPEAT:
          {
            int n = last_step_ - first_step_ + 1;
            int r = static_cast<int>(
                Random::GetFloat() * static_cast<float>(n - 1));
            active_segment_ = first_step_ + \
                ((active_segment_ - first_step_ + r + 1) % n);
          }
          break;
          
        case DIRECTION_ADDRESSABLE:
        case DIRECTION_LAST:
          break;
      }
    }
    
    value_ = parameters_[active_segment_].primary;
    if (quantized_output_) {
      int note = step_quantizer_[active_segment_].Process(value_);
      value_ = static_cast<float>(note) / 96.0f;
    }
    
    ONE_POLE(
        lp_,
        value_,
        PortamentoRateToLPCoefficient(parameters_[active_segment_].secondary));

    out->value = lp_;
    out->phase = 0.0f;
    out->segment = active_segment_;
    ++gate_flags;
    ++out;
  }
}

void SegmentGenerator::ConfigureSequencer(
    const Configuration* segment_configuration,
    int num_segments) {
  num_segments_ = num_segments;

  first_step_ = 0;
  for (int i = 1; i < num_segments; ++i) {
    if (segment_configuration[i].loop) {
      if (!first_step_) {
        first_step_ = last_step_ = i;
      } else {
        last_step_ = i;
      }
    }
  }
  if (!first_step_) {
    // No loop has been found, use the whole group.
    first_step_ = 1;
    last_step_ = num_segments - 1;
  }
  
  int num_steps = last_step_ - first_step_ + 1;
  address_quantizer_.Init(
      num_steps,
      0.02f / 8.0f * static_cast<float>(num_steps),
      false);
  
  inhibit_clock_ = up_down_counter_ = 0;
  quantized_output_ = (segment_configuration[0].type == TYPE_RAMP) && \
      step_quantizer_;
  reset_ = false;
  lp_ = value_ = 0.0f;
  active_segment_ = first_step_;
  process_fn_ = &SegmentGenerator::ProcessSequencer;
}

void SegmentGenerator::Configure(
    bool has_trigger,
    const Configuration* segment_configuration,
    int num_segments) {
  if (num_segments == 1) {
    function_quantizer_.Init(7, 0.025f, false);
    ConfigureSingleSegment(has_trigger, segment_configuration[0]);
    return;
  }
  
  bool sequencer_mode = segment_configuration[0].type != TYPE_STEP && \
      !segment_configuration[0].loop && num_segments >= 3;
  for (int i = 1; i < num_segments; ++i) {
    sequencer_mode = sequencer_mode && \
        segment_configuration[i].type == TYPE_STEP;
  }
  if (sequencer_mode) {
    function_quantizer_.Init(DIRECTION_LAST, 0.025f, false);
    ConfigureSequencer(segment_configuration, num_segments);
    return;
  }
  
  num_segments_ = num_segments;
  
  // assert(has_trigger);
  
  process_fn_ = &SegmentGenerator::ProcessMultiSegment;
  
  // A first pass to collect loop points, and check for STEP segments.
  int loop_start = -1;
  int loop_end = -1;
  bool has_step_segments = false;
  int last_segment = num_segments - 1;
  int first_ramp_segment = -1;
  
  for (int i = 0; i <= last_segment; ++i) {
    has_step_segments = has_step_segments || \
        segment_configuration[i].type == TYPE_STEP;
    if (segment_configuration[i].loop) {
      if (loop_start == -1) {
        loop_start = i;
      }
      loop_end = i;
    }
    if (segment_configuration[i].type == TYPE_RAMP) {
      if (first_ramp_segment == -1) {
        first_ramp_segment = i;
      }
    }
  }
  
  // Check if there are step segments inside the loop.
  bool has_step_segments_inside_loop = false;
  if (loop_start != -1) {
    for (int i = loop_start; i <= loop_end; ++i) {
      if (segment_configuration[i].type == TYPE_STEP) {
        has_step_segments_inside_loop = true;
        break;
      }
    }
  }
  
  for (int i = 0; i <= last_segment; ++i) {
    Segment* s = &segments_[i];
    if (segment_configuration[i].type == TYPE_RAMP) {
      s->start = (num_segments == 1) ? &one_ : NULL;
      s->time = &parameters_[i].primary;
      s->curve = &parameters_[i].secondary;
      s->portamento = &zero_;
      s->phase = NULL;
      
      if (i == last_segment) {
        s->end = &zero_;
      } else if (segment_configuration[i + 1].type != TYPE_RAMP) {
        s->end = &parameters_[i + 1].primary;
      } else if (i == first_ramp_segment) {
        s->end = &one_;
      } else {
        s->end = &parameters_[i].secondary;
        // The whole "reuse the curve from other segment" thing
        // is a bit too complicated...
        //
        // for (int j = i + 1; j <= last_segment; ++j) {
        //   if (segment_configuration[j].type == TYPE_RAMP) {
        //     if (j == last_segment ||
        //         segment_configuration[j + 1].type != TYPE_RAMP) {
        //       s->curve = &parameters_[j].secondary;
        //       break;
        //     }
        //   }
        // }
        s->curve = &half_;
      }
    } else {
      s->start = s->end = &parameters_[i].primary;
      s->curve = &half_;
      if (segment_configuration[i].type == TYPE_STEP) {
        s->portamento = &parameters_[i].secondary;
        s->time = NULL;
        // Sample if there is a loop of length 1 on this segment. Otherwise
        // track.
        s->phase = i == loop_start && i == loop_end ? &zero_ : &one_;
      } else {
        s->portamento = &zero_;
        // Hold if there's a loop of length 1 of this segment. Otherwise, use
        // the programmed time.
        s->time = i == loop_start && i == loop_end
            ? NULL : &parameters_[i].secondary;
        s->phase = &one_;  // Track the changes on the slider.
      }
    }

    s->if_complete = i == loop_end ? loop_start : i + 1;
    s->if_falling = loop_end == -1 || loop_end == last_segment || has_step_segments ? -1 : loop_end + 1;
    s->if_rising = 0;
    
    if (has_step_segments) {
      if (!has_step_segments_inside_loop && i >= loop_start && i <= loop_end) {
        s->if_rising = (loop_end + 1) % num_segments;
      } else {
        // Just go to the next stage.
        // s->if_rising = (i == loop_end) ? loop_start : (i + 1) % num_segments;
        
        // Find the next STEP segment.
        bool follow_loop = loop_end != -1;
        int next_step = i;
        while (segment_configuration[next_step].type != TYPE_STEP) {
          ++next_step;
          if (follow_loop && next_step == loop_end + 1) {
            next_step = loop_start;
            follow_loop = false;
          }
          if (next_step >= num_segments) {
            next_step = num_segments - 1;
            break;
          }
        }
        s->if_rising = next_step == loop_end
            ? loop_start
            : (next_step + 1) % num_segments;
      }
    }
  }
  
  Segment* sentinel = &segments_[num_segments];
  sentinel->end = sentinel->start = segments_[num_segments - 1].end;
  sentinel->time = &zero_;
  sentinel->curve = &half_;
  sentinel->portamento = &zero_;
  sentinel->if_rising = 0;
  sentinel->if_falling = -1;
  sentinel->if_complete = loop_end == last_segment ? 0 : -1;
  
  // After changing the state of the module, we go to the sentinel.
  previous_segment_ = active_segment_ = num_segments;
}

/* static */
SegmentGenerator::ProcessFn SegmentGenerator::process_fn_table_[16] = {
  // RAMP
  &SegmentGenerator::ProcessZero,
  &SegmentGenerator::ProcessFreeRunningLFO,
  &SegmentGenerator::ProcessDecayEnvelope,
  &SegmentGenerator::ProcessTapLFO,
  
  // STEP
  &SegmentGenerator::ProcessPortamento,
  &SegmentGenerator::ProcessPortamento,
  &SegmentGenerator::ProcessSampleAndHold,
  &SegmentGenerator::ProcessSampleAndHold,
  
  // HOLD
  &SegmentGenerator::ProcessDelay,
  &SegmentGenerator::ProcessDelay,
  // &SegmentGenerator::ProcessClockedSampleAndHold,
  &SegmentGenerator::ProcessTimedPulseGenerator,
  &SegmentGenerator::ProcessGateGenerator,

  // ALT
  &SegmentGenerator::ProcessZero,
  &SegmentGenerator::ProcessFreeRunningOscillator,
  &SegmentGenerator::ProcessDecayEnvelope,
  &SegmentGenerator::ProcessPLLOscillator
};

}  // namespace stages