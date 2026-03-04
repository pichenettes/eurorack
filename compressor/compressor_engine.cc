// =============================================================================
// compressor_engine.cc
// Stereo Compressor/Limiter — DSP implementation
// =============================================================================

#include "compressor_engine.h"
#include <math.h>
#include <string.h>

namespace compressor {

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Parameter scaling
// ---------------------------------------------------------------------------
float CompressorEngine::ScaleThreshold(float n) {
  // 0→-40dBFS  1→0dBFS
  return -40.0f + n * 40.0f;
}

float CompressorEngine::ScaleRatio(float n) {
  // 0→1:1  0.9→10:1  1.0→inf (limiter)
  if (n >= 0.99f) return 1000.0f; // effectively infinite
  return 1.0f + n * n * 49.0f;   // exponential taper 1..50
}

float CompressorEngine::ScaleAttack(float n) {
  // 0→0.1ms  1→300ms — exponential, return one-pole coefficient
  float ms = 0.1f * __builtin_powf(3000.0f, n);
  float tc = ms * 0.001f * kSampleRate;
  return __builtin_expf(-1.0f / tc);
}

float CompressorEngine::ScaleRelease(float n) {
  // 0→10ms  1→3000ms
  float ms = 10.0f * __builtin_powf(300.0f, n);
  float tc = ms * 0.001f * kSampleRate;
  return __builtin_expf(-1.0f / tc);
}

float CompressorEngine::ScaleInputGain(float n) {
  // 0→-12dB  0.5→0dB  1→+12dB  returns linear multiplier
  float db = (n - 0.5f) * 24.0f;
  return DbToLin(db);
}

float CompressorEngine::ScaleOutputGain(float n) {
  // 0→0dB  1→+20dB
  return DbToLin(n * 20.0f);
}

// ---------------------------------------------------------------------------
// Detectors
// ---------------------------------------------------------------------------
float CompressorEngine::DetectRMS(const float* buf, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += buf[i] * buf[i];
  float rms = __builtin_sqrtf(sum / n);
  return LinToDb(rms);
}

float CompressorEngine::DetectPeak(
    const float* buf, int n, float& env, float att, float rel) {
  for (int i = 0; i < n; i++) {
    float x = __builtin_fabsf(buf[i]);
    env = (x > env) ? att * env + (1.0f - att) * x
                    : rel * env;
  }
  return LinToDb(env);
}

// ---------------------------------------------------------------------------
// Autopump — synthetic kick envelope
// ---------------------------------------------------------------------------
void CompressorEngine::TickPump(
    int ch, bool trig, float period, float att_coef, float rel_coef) {
  if (trig) {
    ch_[ch].pump_env = 1.0f;
  } else {
    ch_[ch].pump_env *= rel_coef;
  }
}

// ---------------------------------------------------------------------------
// SSL Bus model
// Characteristics:
//   RMS detector, program-dependent release, soft knee option
//   Classic 4000G bus compressor feel
// ---------------------------------------------------------------------------
float CompressorEngine::GainSSL(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);
  float ratio  = ScaleRatio(p.ratio);
  float att    = ScaleAttack(p.attack);
  float rel    = ScaleRelease(p.release);

  // Program-dependent release: faster release on sustained signals
  // SSL compressor speeds up release when signal stays above threshold
  float pgm_factor = 1.0f;
  if (ch_[ch].gain_smooth < -3.0f) {
    pgm_factor = 0.5f; // double speed when heavily compressed
  }
  float rel_pgm = rel + (1.0f - rel) * (1.0f - pgm_factor);

  // Gain computer
  float over = det_db - thresh;
  float gr = 0.0f;

  if (p.soft_knee) {
    float knee = 6.0f; // 6dB knee — classic SSL
    float knee_over = SoftKnee(over, knee * 0.5f);
    gr = knee_over * (1.0f - 1.0f / ratio);
  } else {
    if (over > 0.0f) gr = over * (1.0f - 1.0f / ratio);
  }

  // Ballistics
  float target = -gr;
  float& env = ch_[ch].gain_db;
  if (target < env) {
    env = att * env + (1.0f - att) * target; // attack
  } else {
    env = rel_pgm * env + (1.0f - rel_pgm) * target; // release
  }

  ch_[ch].gain_smooth = env;
  return env; // dB of gain (negative = reduction)
}

// ---------------------------------------------------------------------------
// 1176 model
// Characteristics:
//   Peak detector, very fast FET ballistics, ratio switches (4:1 8:1 12:1 20:1)
//   All-buttons mode: all ratios simultaneously = aggressive limiting
//   Slight harmonic character from FET saturation
// ---------------------------------------------------------------------------
float CompressorEngine::Gain1176(float det_db, const Parameters& p, int ch) {
  // 1176 has discrete ratio steps — map ratio knob to steps
  float ratio;
  float r = p.ratio;
  if (p.alt_mode) {
    ratio = 1000.0f; // all-buttons = infinite ratio
  } else if (r < 0.25f) {
    ratio = 4.0f;
  } else if (r < 0.5f) {
    ratio = 8.0f;
  } else if (r < 0.75f) {
    ratio = 12.0f;
  } else {
    ratio = 20.0f;
  }

  // 1176 attack is very fast — scale tighter
  // Attack range: 20us – 800us (much faster than other compressors)
  float att_ms = 0.02f * __builtin_powf(40.0f, p.attack);
  float att_tc = att_ms * 0.001f * kSampleRate;
  float att    = __builtin_expf(-1.0f / att_tc);

  // Release: 50ms – 1100ms
  float rel_ms = 50.0f * __builtin_powf(22.0f, p.release);
  float rel_tc = rel_ms * 0.001f * kSampleRate;
  float rel    = __builtin_expf(-1.0f / rel_tc);

  // Peak detection
  float thresh = ScaleThreshold(p.threshold);
  float det = DetectPeak(nullptr, 0, ch_[ch].peak_env, att, rel);
  // We already ran detection before this call — use det_db directly
  float over = det_db - thresh;
  float gr = (over > 0.0f) ? over * (1.0f - 1.0f / ratio) : 0.0f;

  // Ballistics on gain
  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  // FET saturation colour in alt mode
  if (p.alt_mode) {
    // Mild soft clip on the gain reduction signal to emulate transformer sat
    float x = env * 0.1f;
    env = env * (1.0f - 0.05f * x * x);
  }

  return env;
}

// ---------------------------------------------------------------------------
// Opto model (LA-2A style)
// Characteristics:
//   RMS detector, program-dependent release (frequency weighted)
//   Very slow, smooth, musical — release time is signal-history dependent
//   alt_mode: LA-2A (tube) vs LA-3A (solid state) character
// ---------------------------------------------------------------------------
float CompressorEngine::GainOpto(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);

  // Opto: attack is fixed and slow (characteristic of photocell)
  // User attack knob modulates the "speed" of the opto cell
  float att_ms = 10.0f * __builtin_powf(5.0f, p.attack); // 10ms..50ms
  float att_tc = att_ms * 0.001f * kSampleRate;
  float att    = __builtin_expf(-1.0f / att_tc);

  // Program-dependent release — opto cell retains "charge"
  // Short transients → fast recovery, sustained signals → slow recovery
  // This is modelled with a dual-time-constant release
  float rel_fast_ms = 40.0f  * __builtin_powf(10.0f,  p.release);
  float rel_slow_ms = 500.0f * __builtin_powf(10.0f,  p.release);
  float rf = __builtin_expf(-1.0f / (rel_fast_ms * 0.001f * kSampleRate));
  float rs = __builtin_expf(-1.0f / (rel_slow_ms * 0.001f * kSampleRate));

  // Blend fast/slow based on how long we've been in compression
  float blend = ch_[ch].opto_release;
  float rel = rf + blend * (rs - rf);

  float over = det_db - thresh;
  float gr = 0.0f;

  if (over > 0.0f) {
    // Opto has a very gentle knee — essentially always soft
    float knee = 10.0f;
    gr = SoftKnee(over, knee * 0.5f) * (1.0f - 1.0f / ScaleRatio(p.ratio));
    // Accumulate "charge" in opto cell
    ch_[ch].opto_release += (1.0f - ch_[ch].opto_release) * 0.0001f;
  } else {
    ch_[ch].opto_release *= 0.9999f; // slowly discharge
  }

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  // LA-2A vs LA-3A: tube model adds 2nd harmonic warmth
  if (p.alt_mode) {
    // Very mild even-order saturation
    float e = env * 0.05f;
    env = env * (1.0f + e * e * 0.1f);
  }

  return env;
}

// ---------------------------------------------------------------------------
// Distressor model
// Characteristics:
//   Hybrid RMS+Peak detector, multiple ratio/distortion modes
//   Dist 2: adds 2nd harmonic saturation
//   Dist 3: adds 3rd harmonic saturation (alt_mode)
//   Aggressive program-dependent response
// ---------------------------------------------------------------------------
float CompressorEngine::GainDistressor(float det_db, const Parameters& p, int ch) {
  float thresh = ScaleThreshold(p.threshold);
  float ratio  = ScaleRatio(p.ratio);
  float att    = ScaleAttack(p.attack);
  float rel    = ScaleRelease(p.release);

  float over = det_db - thresh;
  float gr = 0.0f;

  // Distressor knee: tighter than SSL, more aggressive
  if (over > 0.0f) {
    float knee = 2.0f;
    gr = SoftKnee(over, knee * 0.5f) * (1.0f - 1.0f / ratio);
  }

  float target = -gr;
  float& env = ch_[ch].gain_db;
  env = (target < env)
      ? att * env + (1.0f - att) * target
      : rel * env + (1.0f - rel) * target;

  // Harmonic saturation — applied to gain signal to model VCA colour
  if (!p.alt_mode) {
    // Dist 2: 2nd harmonic (even order) — adds warmth/fatness
    float x = env * 0.15f;
    ch_[ch].dist_sat = env + 0.1f * x * x;
  } else {
    // Dist 3: 3rd harmonic (odd order) — adds grit/aggression
    float x = env * 0.15f;
    ch_[ch].dist_sat = env + 0.07f * x * x * x;
  }

  return ch_[ch].dist_sat;
}

// ---------------------------------------------------------------------------
// Main process block
// ---------------------------------------------------------------------------
void CompressorEngine::Process(
    const float* in_l, const float* in_r,
    const float* sc_l, const float* sc_r,
    bool         clock_trig,
    float*       out_l, float*       out_r,
    MeterValues* meter,
    const Parameters& params) {

  // --- Update HPF frequency ---
  float hpf_freq = 20.0f * __builtin_powf(25.0f, params.sc_hpf_freq); // 20..500Hz
  hpf_[0].SetFrequency(hpf_freq);
  hpf_[1].SetFrequency(hpf_freq);

  // --- Apply input gain ---
  float in_gain  = ScaleInputGain(params.input_gain);
  float out_gain = ScaleOutputGain(params.output_gain);

  // Temporary buffers
  float buf_l[kBlockSize], buf_r[kBlockSize];
  float sc_buf_l[kBlockSize], sc_buf_r[kBlockSize];

  for (int i = 0; i < kBlockSize; i++) {
    buf_l[i] = in_l[i] * in_gain;
    buf_r[i] = in_r[i] * in_gain;
  }

  // --- Build sidechain buffers + HPF ---
  switch (params.sc_mode) {
    case SC_INTERNAL:
      // Key from program audio
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = hpf_[0].Process(buf_l[i]);
        sc_buf_r[i] = hpf_[1].Process(buf_r[i]);
      }
      break;

    case SC_EXTERNAL_MONO:
      // SC1 feeds both channels
      for (int i = 0; i < kBlockSize; i++) {
        float s = hpf_[0].Process(sc_l[i]);
        sc_buf_l[i] = s;
        sc_buf_r[i] = s;
      }
      break;

    case SC_DUAL_MONO:
      // Independent sidechain per channel
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = hpf_[0].Process(sc_l[i]);
        sc_buf_r[i] = hpf_[1].Process(sc_r[i]);
      }
      break;

    case SC_CLOCK:
      // Autopump — synthetic transient replaces sidechain
      // pump_env drives gain reduction directly, ignore sc audio
      for (int i = 0; i < kBlockSize; i++) {
        sc_buf_l[i] = ch_[0].pump_env;
        sc_buf_r[i] = ch_[1].pump_env;
      }
      // Tick pump (trig only fires once per block)
      {
        float att_ms = 0.5f; // fixed fast attack for pump
        float att_tc = att_ms * 0.001f * kSampleRate;
        float att_c  = __builtin_expf(-1.0f / att_tc);
        float rel_c  = ScaleRelease(params.release);
        TickPump(0, clock_trig, params.clock_period, att_c, rel_c);
        TickPump(1, clock_trig, params.clock_period, att_c, rel_c);
      }
      break;
  }

  // --- Detect levels ---
  float det_db_l, det_db_r;

  if (params.model == MODEL_1176) {
    // Peak detection for 1176
    det_db_l = DetectPeak(sc_buf_l, kBlockSize, ch_[0].peak_env,
                          ScaleAttack(params.attack), ScaleRelease(params.release));
    det_db_r = DetectPeak(sc_buf_r, kBlockSize, ch_[1].peak_env,
                          ScaleAttack(params.attack), ScaleRelease(params.release));
  } else {
    // RMS for all others
    det_db_l = DetectRMS(sc_buf_l, kBlockSize);
    det_db_r = DetectRMS(sc_buf_r, kBlockSize);
  }

  // --- Stereo link: use max of L+R for linked mode ---
  if (params.stereo_link) {
    float det_max = (det_db_l > det_db_r) ? det_db_l : det_db_r;
    det_db_l = det_db_r = det_max;
  }

  // --- Autopump: override detection with pump envelope ---
  if (params.sc_mode == SC_CLOCK) {
    float pump_db_l = LinToDb(ch_[0].pump_env + 1e-6f);
    float pump_db_r = LinToDb(ch_[1].pump_env + 1e-6f);
    // Mix pump with threshold so it triggers compressor
    det_db_l = ScaleThreshold(params.threshold) + pump_db_l * 0.5f;
    det_db_r = ScaleThreshold(params.threshold) + pump_db_r * 0.5f;
  }

  // --- Compute gain reduction per model ---
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

  // --- Apply gain + make-up + VCA ---
  float vca_l = params.vca_cv_l; // 0..1 from CV jack (unity = 1.0)
  float vca_r = params.vca_cv_r;

  for (int i = 0; i < kBlockSize; i++) {
    float gl = DbToLin(gr_db_l) * out_gain * vca_l;
    float gr = DbToLin(gr_db_r) * out_gain * vca_r;
    out_l[i] = buf_l[i] * gl;
    out_r[i] = buf_r[i] * gr;
  }

  // --- Metering ---
  // Input level (post input gain, pre compressor)
  float peak_in_l = 0.0f, peak_in_r = 0.0f;
  for (int i = 0; i < kBlockSize; i++) {
    float al = __builtin_fabsf(buf_l[i]);
    float ar = __builtin_fabsf(buf_r[i]);
    if (al > peak_in_l) peak_in_l = al;
    if (ar > peak_in_r) peak_in_r = ar;
  }
  // Clamp to 0..1 (0dBFS = 1.0)
  meter->input_l = (peak_in_l > 1.0f) ? 1.0f : peak_in_l;
  meter->input_r = (peak_in_r > 1.0f) ? 1.0f : peak_in_r;

  // GR — normalise -20dB max GR to 0..1 range for LED display
  meter->gr_l = (-gr_db_l > 20.0f) ? 1.0f : -gr_db_l / 20.0f;
  meter->gr_r = (-gr_db_r > 20.0f) ? 1.0f : -gr_db_r / 20.0f;
}

}  // namespace compressor
