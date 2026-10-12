#pragma once
// =============================================================================
// compressor_engine.h
// Stereo Compressor/Limiter for Mutable Instruments Clouds (STM32F405)
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

namespace compressor {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const float kSampleRate      = 32000.0f;
static const float kInvSampleRate   = 1.0f / kSampleRate;
static const float kLog2            = 0.6931471805599453f;
static const float kNegInf          = -120.0f; // dBFS floor
static const int   kBlockSize       = 32;       // matches Clouds block size

// ---------------------------------------------------------------------------
// Compressor model identifiers
// ---------------------------------------------------------------------------
enum CompressorModel {
  MODEL_SSL_BUS   = 0,
  MODEL_1176      = 1,
  MODEL_OPTO      = 2,
  MODEL_DISTRESSOR = 3,
  MODEL_COUNT     = 4
};

// ---------------------------------------------------------------------------
// Sidechain mode — determined by jack presence at runtime
// ---------------------------------------------------------------------------
enum SidechainMode {
  SC_INTERNAL    = 0,  // keys from program audio
  SC_EXTERNAL_MONO = 1,  // SC1 only — feeds both channels
  SC_CLOCK       = 2,  // SC2 only — autopump
  SC_DUAL_MONO   = 3   // SC1→L, SC2→R independent
};

// ---------------------------------------------------------------------------
// Parameters — all normalised 0..1 from ADC, scaled internally
// ---------------------------------------------------------------------------
struct Parameters {
  float input_gain;    // -12..+12 dB
  float threshold;     // -40..0 dBFS
  float ratio;         // 1..inf (∞ = limiter)
  float attack;        // 0.1..300 ms
  float release;       // 10..3000 ms
  float output_gain;   // 0..+20 dB (make-up)
  float sc_hpf_freq;   // 20..500 Hz
  float vca_cv_l;      // 0..1 post-compressor VCA L
  float vca_cv_r;      // 0..1 post-compressor VCA R

  // CV modulation (0..1, unipolar)
  float cv_ratio;
  float cv_attack;
  float cv_release;
  float cv_bypass;     // gate

  // State
  CompressorModel model;
  SidechainMode   sc_mode;
  bool            stereo_link;
  bool            engaged;        // IN button
  bool            soft_knee;      // SSL/generic only
  bool            alt_mode;       // model-specific (1176 all-buttons, Dist mode, etc.)

  // Tap/clock tempo (seconds per beat)
  float           clock_period;
};

// ---------------------------------------------------------------------------
// Per-channel compressor state (ballistics, detector memory)
// ---------------------------------------------------------------------------
struct ChannelState {
  float env_db;          // detector envelope in dB
  float gain_db;         // current gain reduction in dB (negative)
  float gain_smooth;     // smoothed GR for opto program-dependent release
  float opto_release;    // dynamic release coefficient (opto model)

  // 1176 peak detector
  float peak_env;

  // Distressor harmonic state
  float dist_sat;

  // Autopump
  float pump_env;        // synthetic kick envelope

  void Init() {
    env_db      = kNegInf;
    gain_db     = 0.0f;
    gain_smooth = 0.0f;
    opto_release = 0.0f;
    peak_env    = 0.0f;
    dist_sat    = 0.0f;
    pump_env    = 0.0f;
  }
};

// ---------------------------------------------------------------------------
// Metering output — what the LED driver reads
// ---------------------------------------------------------------------------
struct MeterValues {
  float input_l;   // 0..1 (pre-compressor, post input gain)
  float input_r;
  float gr_l;      // 0..1 (gain reduction, 0=none, 1=max)
  float gr_r;
};

// ---------------------------------------------------------------------------
// Sidechain HPF (one-pole)
// ---------------------------------------------------------------------------
struct OnePoleHPF {
  float x1, y1, a;

  void SetFrequency(float freq) {
    // Bilinear approximation — accurate enough for sidechain
    float w = freq * kInvSampleRate;
    a = 1.0f - (2.0f * 3.14159265f * w);
    if (a < 0.0f) a = 0.0f;
    if (a > 0.9999f) a = 0.9999f;
  }

  void Init() { x1 = y1 = 0.0f; a = 0.99f; }

  inline float Process(float x) {
    y1 = a * (y1 + x - x1);
    x1 = x;
    return y1;
  }
};

// ---------------------------------------------------------------------------
// CompressorEngine — main DSP class
// ---------------------------------------------------------------------------
class CompressorEngine {
 public:
  CompressorEngine() {}
  ~CompressorEngine() {}

  void Init();

  // Process one stereo block.
  // in_l/in_r       : interleaved input buffers [kBlockSize]
  // sc_l/sc_r       : sidechain input buffers (may == in_l/in_r for internal)
  // clock_trig       : single sample clock pulse this block (for autopump)
  // out_l/out_r      : output buffers [kBlockSize]
  // meter            : updated each block
  void Process(
    const float* in_l, const float* in_r,
    const float* sc_l, const float* sc_r,
    bool         clock_trig,
    float*       out_l, float*       out_r,
    MeterValues* meter,
    const Parameters& params);

  // Call when model changes to reset state
  void ResetModel();

 private:
  ChannelState ch_[2];       // 0=L, 1=R
  OnePoleHPF   hpf_[2];     // sidechain HPF per channel

  // ---- Gain computer (all models share this entry point) ----
  float ComputeGain(float det_db, const Parameters& p, int ch);

  // ---- Detector ----
  float DetectRMS(const float* buf, int n);
  float DetectPeak(const float* buf, int n, float& env, float att, float rel);

  // ---- Per-model compute ----
  float GainSSL(float det_db, const Parameters& p, int ch);
  float Gain1176(float det_db, const Parameters& p, int ch);
  float GainOpto(float det_db, const Parameters& p, int ch);
  float GainDistressor(float det_db, const Parameters& p, int ch);

  // ---- Autopump ----
  void  TickPump(int ch, bool trig, float period, float att_coef, float rel_coef);

  // ---- Parameter scaling ----
  static float ScaleThreshold(float n);   // 0..1 → -40..0 dBFS
  static float ScaleRatio(float n);       // 0..1 → 1..inf
  static float ScaleAttack(float n);      // 0..1 → 0.1..300ms → coefficient
  static float ScaleRelease(float n);     // 0..1 → 10..3000ms → coefficient
  static float ScaleInputGain(float n);   // 0..1 → -12..+12 dB → linear
  static float ScaleOutputGain(float n);  // 0..1 → 0..+20 dB → linear

  // ---- Utility ----
  static inline float LinToDb(float lin) {
    return (lin > 1e-6f) ? 8.685889638f * __builtin_logf(lin) : kNegInf;
  }
  static inline float DbToLin(float db) {
    return __builtin_expf(db * 0.11512925465f);
  }
  static inline float SoftKnee(float over, float half_knee) {
    // Smooth transition region around threshold
    if (over < -half_knee) return 0.0f;
    if (over >  half_knee) return over;
    float x = over + half_knee;
    return (x * x) / (4.0f * half_knee);
  }

  DISALLOW_COPY_AND_ASSIGN(CompressorEngine);
};

}  // namespace compressor
