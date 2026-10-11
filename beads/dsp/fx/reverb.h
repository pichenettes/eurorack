// Copyright 2014 Emilie Gillet.
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
// Reverb.

#ifndef BEADS_DSP_FX_REVERB_H_
#define BEADS_DSP_FX_REVERB_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/dsp/fx/fx_engine.h"

namespace beads {

class Reverb {
 public:
  Reverb() { }
  ~Reverb() { }
  
  void Init(uint16_t* buffer, float sample_rate) {
    engine_.Init(buffer);
    Reset(sample_rate);
  }
  
  void Reset(float sample_rate) {
    engine_.SetLFOFrequency(LFO_1, 0.5f / sample_rate);
    engine_.SetLFOFrequency(LFO_2, 0.3f / sample_rate);
    lp_ = 0.7f;
    lp_1_ = 0.0f;
    lp_2_ = 0.0f;
    mix_ = 0.0f;
    dry_ = 0.0f;
    wet_ = 0.0f;
    send_ = 0.0f;
  }
  
  void Process(FloatFrame* in_out, size_t size) {
    // This is the Griesinger topology described in the Dattorro paper
    // (4 AP diffusers on the input, then a loop of 2x 2AP+1Delay).
    // Modulation is applied in the loop of the first diffuser AP for additional
    // smearing; and to the two long delays for a slow shimmer/chorus effect.
    typedef E::Reserve<150,
      E::Reserve<214,
      E::Reserve<319,
      E::Reserve<527,
      E::Reserve<2182,
      E::Reserve<2690,
      E::Reserve<4501,
      E::Reserve<2525,
      E::Reserve<2197,
      E::Reserve<6312> > > > > > > > > > Memory;
    E::DelayLine<Memory, 0> ap1;
    E::DelayLine<Memory, 1> ap2;
    E::DelayLine<Memory, 2> ap3;
    E::DelayLine<Memory, 3> ap4;
    E::DelayLine<Memory, 4> dap1a;
    E::DelayLine<Memory, 5> dap1b;
    E::DelayLine<Memory, 6> del1;
    E::DelayLine<Memory, 7> dap2a;
    E::DelayLine<Memory, 8> dap2b;
    E::DelayLine<Memory, 9> del2;
    E::Context c;

    const float kap = 0.625f;
    const float klp = lp_;
    const float krt = reverb_time_;

    const float input_gain = 0.2f * (1.0f - 0.333f * reverb_time_);
    const float output_gain = 0.4f / input_gain;
    
    // const float send_amount = 1.0f;
    // const float wet = mix_ * (2.0f - mix_) * output_gain;
    
    const float send_amount_sqrt = mix_ * (2.0f - mix_);
    const float send_amount = send_amount_sqrt * (2.0f - send_amount_sqrt);
    const float wet = output_gain;
    const float dry = (1.0f - mix_) * (1.0f + mix_);
    
    stmlib::ParameterInterpolator wet_modulation(&wet_, wet, size);
    stmlib::ParameterInterpolator dry_modulation(&dry_, dry, size);
    stmlib::ParameterInterpolator send_modulation(
        &send_,
        send_amount * input_gain,
        size);

    while (size--) {
      const float dry_amount = dry_modulation.Next();
      const float wet_amount = wet_modulation.Next();
      const float send_amount = send_modulation.Next();
      
      float wet;
      float apout = 0.0f;
      engine_.Start(&c);
      
      // Smear AP1 inside the loop.
      c.Interpolate(ap1, 10.0f, LFO_1, 80.0f, 1.0f);
      c.Write(ap1, 100, 0.0f);
      
      c.Read(in_out->l + in_out->r, send_amount);
      
      // Diffuse through 4 allpasses.
      c.Read(ap1 TAIL, kap);
      c.WriteAllPass(ap1, -kap);
      c.Read(ap2 TAIL, kap);
      c.WriteAllPass(ap2, -kap);
      c.Read(ap3 TAIL, kap);
      c.WriteAllPass(ap3, -kap);
      c.Read(ap4 TAIL, kap);
      c.WriteAllPass(ap4, -kap);
      c.Write(apout);
      
      // Main reverb loop.
      c.Load(apout);
      c.Interpolate(del2, 6261.0f, LFO_2, 50.0f, krt);
      c.Lp(lp_1_, klp);
      c.Read(dap1a TAIL, -kap);
      c.WriteAllPass(dap1a, kap);
      c.Read(dap1b TAIL, kap);
      c.WriteAllPass(dap1b, -kap);
      c.Write(del1, 1.0f);
      c.Write(wet);

      in_out->l = in_out->l * dry_amount + wet * wet_amount;

      c.Load(apout);
      c.Interpolate(del1, 4460.0f, LFO_1, 40.0f, krt);
      c.Lp(lp_2_, klp);
      c.Read(dap2a TAIL, kap);
      c.WriteAllPass(dap2a, -kap);
      c.Read(dap2b TAIL, -kap);
      c.WriteAllPass(dap2b, kap);
      c.Write(del2, 1.0f);
      c.Write(wet);
      
      in_out->r = in_out->r * dry_amount + wet * wet_amount;
      ++in_out;
    }
  }
  
  inline void set_mix(float mix) {
    mix_ = mix;
  }
  
  inline void set_time(float reverb_time) {
    reverb_time_ = reverb_time;
  }
  
  inline void set_lp(float lp) {
    lp_ = lp;
  }
  
  inline void Clear() {
    engine_.Clear();
  }
  
 private:
  typedef FxEngine<32768, FORMAT_16_BIT> E;
  E engine_;
  
  float mix_;
  float wet_;
  float dry_;
  float send_;
  float reverb_time_;
  float lp_;
  
  float lp_1_;
  float lp_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Reverb);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_REVERB_H_
