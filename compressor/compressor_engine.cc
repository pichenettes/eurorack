// =============================================================================
// compressor_engine.cc
// =============================================================================

#include "compressor_engine.h"
#include <math.h>
#include <string.h>

namespace compressor {

void CompressorEngine::Init() {
  ch_[0].Init();
  ch_[1].Init();
  hpf_[0].Init();
  hpf_[1].Init();
}

void CompressorEngine::ResetModel() {
  ch_[0].Init();
  ch_[1].Init();
}

float CompressorEngine::ScaleThreshold(float n) {
  return -60.0f; // DEBUG // 0→-40dBFS  1→0dBFS
}

float CompressorEngine::ScaleRatio(float n) {
  if (n >= 0.99f) return 1000.0f;
  return 1.0f + n * n * 49.0f;
}

float CompressorEngine::ScaleAttack(float n) {
  float ms = 0.1f * __builtin_powf(3000.0f, n);
  float tc = ms * 0.001f * kSampleRate;
  return __builtin_expf(-1.0f / tc);
}

float CompressorEngine::ScaleRelease(float n) {
  float ms = 10.0f * __builtin_powf(300.0f, n);
  float tc = ms * 0.001f * kSampleRate;
  return __builtin_expf(-1.0f / tc);
}

float CompressorEngine::ScaleInputGain(float n) {
  float db = (n - 0.5f) * 24.0f;
  return DbToLin(db);
}

float CompressorEngine::ScaleOutputGain(float n) {
  float db = (n - 0.5f) * 40.0f;
  return DbToLin(db);
}

float CompressorEngine::DetectRMS(const float* buf, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += buf[i] * buf[i];
  float rms = __builtin_sqrtf(sum / n);
  return LinToDb(rms + 1e-10f);
}

float CompressorEngine::DetectPeak(
    const float* buf, int n, float& env, float att, float rel) {
  for (int i = 0; i < n; i++) {
    float x = __builtin_fabsf(buf[i]);
    env = (x > env) ? att * env + (1.0f - att) * x : rel * env;
  }
  return LinToDb(env + 1e-10f);
}

void CompressorEngine::TickPump(
    int ch, bool trig, float period, float att_coef, float rel_coef) {
  if (trig) {
    ch_[ch].pump_env = 1.0f;
  } else {
    ch_[ch].pump_env *= rel_coef;
  }
}

float CompressorEngine::GainSSL(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);
  float ratio  = ScaleRatio(p.ratio);
  float att    = ScaleAttack(p.attack);
  float rel    = ScaleRelease(p.release);

  float pgm_factor = (ch_[ch].gain_smooth < -3.0f) ? 0.5f : 1.0f;
  float rel_pgm = rel + (1.0f - rel) * (1.0f - pgm_factor);

  float over = det_db - thresh;
  float gr = 0.0f;

  if (p.soft_knee) {
    gr = SoftKnee(over, 3.0f) * (1.0f - 1.0f / ratio);
  } else {
    if (over > 0.0f) gr = over * (1.0f - 1.0f / ratio);
  }

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel_pgm * env + (1.0f - rel_pgm) * target;

  ch_[ch].gain_smooth = env;
  return env;
}

float CompressorEngine::Gain1176(float det_db, const Parameters& p, int ch) {
  float ratio;
  if (p.alt_mode) {
    ratio = 1000.0f;
  } else if (p.ratio < 0.25f) {
    ratio = 4.0f;
  } else if (p.ratio < 0.5f) {
    ratio = 8.0f;
  } else if (p.ratio < 0.75f) {
    ratio = 12.0f;
  } else {
    ratio = 20.0f;
  }

  float att_ms = 0.02f * __builtin_powf(40.0f, p.attack);
  float att = __builtin_expf(-1.0f / (att_ms * 0.001f * kSampleRate));
  float rel_ms = 50.0f * __builtin_powf(22.0f, p.release);
  float rel = __builtin_expf(-1.0f / (rel_ms * 0.001f * kSampleRate));

  float thresh = ScaleThreshold(p.threshold);
  float over = det_db - thresh;
  float gr = (over > 0.0f) ? over * (1.0f - 1.0f / ratio) : 0.0f;

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  return env;
}

float CompressorEngine::GainOpto(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);
  float att_ms = 10.0f * __builtin_powf(5.0f, p.attack);
  float att = __builtin_expf(-1.0f / (att_ms * 0.001f * kSampleRate));

  float rel_fast_ms = 40.0f  * __builtin_powf(10.0f, p.release);
  float rel_slow_ms = 500.0f * __builtin_powf(10.0f, p.release);
  float rf = __builtin_expf(-1.0f / (rel_fast_ms * 0.001f * kSampleRate));
  float rs = __builtin_expf(-1.0f / (rel_slow_ms * 0.001f * kSampleRate));
  float rel = rf + ch_[ch].opto_release * (rs - rf);

  float over = det_db - thresh;
  float gr = 0.0f;

  if (over > 0.0f) {
    gr = SoftKnee(over, 5.0f) * (1.0f - 1.0f / ScaleRatio(p.ratio));
    ch_[ch].opto_release += (1.0f - ch_[ch].opto_release) * 0.0001f;
  } else {
    ch_[ch].opto_release *= 0.9999f;
  }

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  return env;
}

float CompressorEngine::GainDistressor(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);
  float ratio  = ScaleRatio(p.ratio);
  float att    = ScaleAttack(p.attack);
  float rel    = ScaleRelease(p.release);

  float over = det_db - thresh;
  float gr = 0.0f;
  if (over > 0.0f) {
    gr = SoftKnee(over, 1.0f) * (1.0f - 1.0f / ratio);
  }

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  return env;
}

void CompressorEngine::Process(
    const float* in_l, const float* in_r,
    const float* sc_l, const float* sc_r,
    bool         clock_trig,
    float*       out_l, float*       out_r,
    MeterValues* meter,
    const Parameters& params) {

  float hpf_freq = 20.0f * __builtin_powf(25.0f, params.sc_hpf_freq);
  hpf_[0].SetFrequency(hpf_freq);
  hpf_[1].SetFrequency(hpf_freq);

  float in_gain  = ScaleInputGain(params.input_gain);
  float out_gain = ScaleOutputGain(params.output_gain);

  float buf_l[kBlockSize], buf_r[kBlockSize];
  float sc_buf_l[kBlockSize], sc_buf_r[kBlockSize];

  for (int i = 0; i < kBlockSize; i++) {
    buf_l[i] = in_l[i] * in_gain;
    buf_r[i] = in_r[i] * in_gain;
  }

  switch (params.sc_mode) {
    case SC_INTERNAL:
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = hpf_[0].Process(buf_l[i]);
        sc_buf_r[i] = hpf_[1].Process(buf_r[i]);
      }
      break;
    case SC_EXTERNAL_MONO:
      for (int i = 0; i < kBlockSize; i++) {
        float s = hpf_[0].Process(sc_l[i]);
        sc_buf_l[i] = sc_buf_r[i] = s;
      }
      break;
    case SC_DUAL_MONO:
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = hpf_[0].Process(sc_l[i]);
        sc_buf_r[i] = hpf_[1].Process(sc_r[i]);
      }
      break;
    case SC_CLOCK:
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = ch_[0].pump_env;
        sc_buf_r[i] = ch_[1].pump_env;
      }
      {
        float att_tc = 0.0005f * kSampleRate;
        float att_c  = __builtin_expf(-1.0f / att_tc);
        float rel_c  = ScaleRelease(params.release);
        TickPump(0, clock_trig, params.clock_period, att_c, rel_c);
        TickPump(1, clock_trig, params.clock_period, att_c, rel_c);
      }
      break;
  }

  float det_db_l, det_db_r;

  if (params.model == MODEL_1176) {
    det_db_l = DetectPeak(sc_buf_l, kBlockSize, ch_[0].peak_env,
                          ScaleAttack(params.attack), ScaleRelease(params.release));
    det_db_r = DetectPeak(sc_buf_r, kBlockSize, ch_[1].peak_env,
                          ScaleAttack(params.attack), ScaleRelease(params.release));
  } else {
    det_db_l = DetectRMS(sc_buf_l, kBlockSize);
    det_db_r = DetectRMS(sc_buf_r, kBlockSize);
  }

  if (params.stereo_link) {
    float det_max = (det_db_l > det_db_r) ? det_db_l : det_db_r;
    det_db_l = det_db_r = det_max;
  }

  if (params.sc_mode == SC_CLOCK) {
    float pump_db_l = LinToDb(ch_[0].pump_env + 1e-6f);
    float pump_db_r = LinToDb(ch_[1].pump_env + 1e-6f);
    det_db_l = ScaleThreshold(params.threshold) + pump_db_l * 0.5f;
    det_db_r = ScaleThreshold(params.threshold) + pump_db_r * 0.5f;
  }

  float gr_db_l = 0.0f, gr_db_r = 0.0f;

  if (params.engaged) {
    switch (params.model) {
      case MODEL_SSL_BUS:
        gr_db_l = GainSSL(det_db_l, params, 0);
        gr_db_r = GainSSL(det_db_r, params, 1);
        break;
      case MODEL_1176:
        gr_db_l = Gain1176(det_db_l, params, 0);
        gr_db_r = Gain1176(det_db_r, params, 1);
        break;
      case MODEL_OPTO:
        gr_db_l = GainOpto(det_db_l, params, 0);
        gr_db_r = GainOpto(det_db_r, params, 1);
        break;
      case MODEL_DISTRESSOR:
        gr_db_l = GainDistressor(det_db_l, params, 0);
        gr_db_r = GainDistressor(det_db_r, params, 1);
        break;
      default:
        break;
    }
  }

  float vca_l = params.vca_cv_l;
  float vca_r = params.vca_cv_r;

  for (int i = 0; i < kBlockSize; i++) {
    float gl = DbToLin(gr_db_l) * out_gain * vca_l;
    float gr = DbToLin(gr_db_r) * out_gain * vca_r;
    out_l[i] = buf_l[i] * gl;
    out_r[i] = buf_r[i] * gr;
  }

  float peak_in_l = 0.0f, peak_in_r = 0.0f;
  for (int i = 0; i < kBlockSize; i++) {
    float al = __builtin_fabsf(buf_l[i]);
    float ar = __builtin_fabsf(buf_r[i]);
    if (al > peak_in_l) peak_in_l = al;
    if (ar > peak_in_r) peak_in_r = ar;
  }
  meter->input_l = (peak_in_l > 1.0f) ? 1.0f : peak_in_l;
  meter->input_r = (peak_in_r > 1.0f) ? 1.0f : peak_in_r;
  meter->gr_l = (-gr_db_l > 20.0f) ? 1.0f : -gr_db_l / 20.0f;
  meter->gr_r = (-gr_db_r > 20.0f) ? 1.0f : -gr_db_r / 20.0f;
}

}  // namespace compressor
