// =============================================================================
// ui.h / ui.cc
// UI handler — buttons, LEDs, knobs, CV, tap tempo
// Mutable Instruments Clouds hardware mapping
// =============================================================================
#pragma once

#include "compressor_engine.h"
#include "stmlib/stmlib.h"

namespace compressor {

// ---------------------------------------------------------------------------
// LED driver — maps MeterValues to 4x red/green LEDs
//
// LED assignment (fixed, never changes):
//   LED 0 — Left channel INPUT level   (green→red)
//   LED 1 — Right channel INPUT level  (green→red)
//   LED 2 — Left channel GR            (green→red, green=no GR)
//   LED 3 — Right channel GR           (green→red, green=no GR)
// ---------------------------------------------------------------------------

// Thresholds for LED colour switching (0..1 normalised)
static const float kLedYellowThresh = 0.5f;   // input: above = yellow (both on)
static const float kLedRedThresh    = 0.85f;  // input: above = red only
static const float kGrGreenMax      = 0.15f;  // GR: below = green (healthy)
static const float kGrYellowMax     = 0.5f;   // GR: below = yellow
                                               // GR: above = red (heavy compression)

struct LedState {
  bool red;
  bool green;
};

inline LedState InputLevelToLed(float level) {
  LedState s;
  if (level < kLedYellowThresh) {
    s.green = true;  s.red = false; // Green: nominal
  } else if (level < kLedRedThresh) {
    s.green = true;  s.red = true;  // Yellow: approaching clip
  } else {
    s.green = false; s.red = true;  // Red: hot/clipping
  }
  return s;
}

inline LedState GrToLed(float gr) {
  LedState s;
  if (gr < kGrGreenMax) {
    s.green = true;  s.red = false; // Green: little/no GR
  } else if (gr < kGrYellowMax) {
    s.green = true;  s.red = true;  // Yellow: moderate GR
  } else {
    s.green = false; s.red = true;  // Red: heavy GR
  }
  return s;
}

// ---------------------------------------------------------------------------
// Tap tempo
// ---------------------------------------------------------------------------
static const int   kTapMaxCount   = 4;    // average over 4 taps
static const float kTapTimeout    = 2.0f; // seconds before reset
static const float kTapMinPeriod  = 0.1f; // 600 BPM max
static const float kTapMaxPeriod  = 4.0f; // 15 BPM min

class TapTempo {
 public:
  TapTempo() : count_(0), period_(0.5f), accum_(0.0f) {}

  void Init() { count_ = 0; period_ = 0.5f; accum_ = 0.0f; }

  // Call every audio block — dt = block duration in seconds
  void Tick(float dt) {
    accum_ += dt;
    if (accum_ > kTapTimeout) {
      count_ = 0; // timeout — reset
    }
  }

  // Call on tap button press
  void Tap() {
    if (count_ == 0) {
      accum_ = 0.0f;
      count_++;
      return;
    }

    float tap_period = accum_;
    accum_ = 0.0f;

    if (tap_period < kTapMinPeriod || tap_period > kTapMaxPeriod) {
      count_ = 1;
      return;
    }

    // Running average
    if (count_ < kTapMaxCount) {
      period_ = (period_ * (count_ - 1) + tap_period) / count_;
      count_++;
    } else {
      period_ = period_ * 0.75f + tap_period * 0.25f;
    }
  }

  float period() const { return period_; } // seconds

 private:
  int   count_;
  float period_;
  float accum_;
};

// ---------------------------------------------------------------------------
// Button debouncer — simple counter based
// ---------------------------------------------------------------------------
class Button {
 public:
  Button() : state_(false), last_(false), press_(false), held_(false),
             hold_counter_(0) {}

  // raw: true if physically pressed this sample
  void Update(bool raw) {
    last_  = state_;
    state_ = raw;
    press_ = (state_ && !last_);
    held_  = state_;
    if (state_) hold_counter_++; else hold_counter_ = 0;
  }

  bool pressed()    const { return press_; }           // rising edge
  bool held()       const { return held_; }            // currently down
  bool long_press() const { return hold_counter_ > kLongPressThresh; }
  bool released()   const { return (!state_ && last_); } // falling edge

 private:
  static const int kLongPressThresh = 24000; // ~0.5s at 48kHz
  bool state_, last_, press_, held_;
  int  hold_counter_;
};

// ---------------------------------------------------------------------------
// UI — top level, owns Parameters and LED state
// ---------------------------------------------------------------------------
class UI {
 public:
  UI() {}

  void Init(Parameters* params);

  // Call once per audio block with:
  //   adc[0..6]    : 7 knob ADC values 0..1
  //   cv[0..5]     : 6 CV jack values 0..1
  //   btn_in       : IN (freeze) button state
  //   btn_sc       : sidechain button state
  //   btn_shift    : shift button state
  //   jack_sc1     : SC1 jack present
  //   jack_sc2     : SC2 jack present
  //   jack_vca_l   : VCA L CV jack present
  //   jack_vca_r   : VCA R CV jack present
  //   meter        : current meter values
  //   dt           : block duration (seconds)
  void Process(
    const float* adc,
    const float* cv,
    bool btn_in, bool btn_sc, bool btn_shift,
    bool jack_sc1, bool jack_sc2,
    bool jack_vca_l, bool jack_vca_r,
    const MeterValues& meter,
    float dt);

  // After Process(), read these to drive hardware LEDs
  LedState led[4];        // LED 0..3
  bool     freeze_led;    // IN button LED

  // Clock trigger output for engine
  bool     clock_trig;

 private:
  Parameters* params_;

  Button btn_in_, btn_sc_, btn_shift_;
  TapTempo tap_;

  CompressorModel pending_model_; // for shift+sc cycling
  bool            shift_was_held_;

  // Model name flash on change
  int  flash_counter_;
  int  flash_pattern_;

  void UpdateSidechainMode(bool jack_sc1, bool jack_sc2);
  void UpdateLeds(const MeterValues& meter);
  void CycleModel();
  void FlashModelIndicator();
};

// ---------------------------------------------------------------------------
// UI implementation
// ---------------------------------------------------------------------------
inline void UI::Init(Parameters* params) {
  params_         = params;
  shift_was_held_ = false;
  flash_counter_  = 0;
  flash_pattern_  = 0;
  clock_trig      = false;
  freeze_led      = false;
  for (int i = 0; i < 4; i++) led[i] = {false, false};
}

inline void UI::CycleModel() {
  int m = (int)params_->model + 1;
  if (m >= MODEL_COUNT) m = 0;
  params_->model = (CompressorModel)m;

  // Brief flash to indicate model — number of flashes = model index + 1
  flash_pattern_  = (int)params_->model + 1;
  flash_counter_  = flash_pattern_ * 6000; // ~0.125s per flash at 48kHz
}

inline void UI::UpdateSidechainMode(bool jack_sc1, bool jack_sc2) {
  if (!jack_sc1 && !jack_sc2) {
    params_->sc_mode = SC_INTERNAL;
  } else if (jack_sc1 && !jack_sc2) {
    params_->sc_mode = SC_EXTERNAL_MONO;
  } else if (!jack_sc1 && jack_sc2) {
    params_->sc_mode = SC_CLOCK;
  } else {
    params_->sc_mode = SC_DUAL_MONO;
  }
}

inline void UI::UpdateLeds(const MeterValues& meter) {
  if (flash_counter_ > 0) {
    // Flash all LEDs to indicate model change
    // Pattern: N flashes where N = model index + 1
    int phase = flash_counter_ % 6000;
    bool lit  = (phase > 3000);
    for (int i = 0; i < 4; i++) {
      led[i].red   = lit;
      led[i].green = false;
    }
    flash_counter_--;
    return;
  }

  // Normal metering
  led[0] = InputLevelToLed(meter.input_l);
  led[1] = InputLevelToLed(meter.input_r);
  led[2] = GrToLed(meter.gr_l);
  led[3] = GrToLed(meter.gr_r);

  // If not engaged, dim everything
  if (!params_->engaged) {
    for (int i = 0; i < 4; i++) led[i] = {false, false};
  }
}

inline void UI::Process(
    const float* adc,
    const float* cv,
    bool btn_in_raw, bool btn_sc_raw, bool btn_shift_raw,
    bool jack_sc1, bool jack_sc2,
    bool jack_vca_l, bool jack_vca_r,
    const MeterValues& meter,
    float dt) {

  // --- Update buttons ---
  btn_in_.Update(btn_in_raw);
  btn_sc_.Update(btn_sc_raw);
  btn_shift_.Update(btn_shift_raw);

  // --- IN button: toggle engage ---
  if (btn_in_.pressed()) {
    params_->engaged = !params_->engaged;
  }
  freeze_led = params_->engaged;

  // --- Shift + SC button: cycle model ---
  if (btn_sc_.pressed()) {
    if (btn_shift_.held()) {
      CycleModel();
    } else {
      // SC button alone: toggle stereo link
      params_->stereo_link = !params_->stereo_link;
    }
  }

  // --- Shift button alone (tap tempo when SC2 not patched) ---
  if (btn_shift_.pressed()) {
    if (params_->sc_mode != SC_CLOCK) {
      tap_.Tap();
      clock_trig = true; // one-shot for this block
    } else {
      clock_trig = false;
    }
  } else {
    clock_trig = false;
  }

  // --- Update tap ---
  tap_.Tick(dt);
  params_->clock_period = tap_.period();

  // --- Knob mapping ---
  // adc[0] = Input gain
  // adc[1] = Threshold
  // adc[2] = Ratio
  // adc[3] = Attack
  // adc[4] = Release
  // adc[5] = Filter (sidechain HPF)
  // adc[6] = Output gain (make-up)
  params_->input_gain   = adc[0];
  params_->threshold    = adc[1];
  params_->ratio        = adc[2];
  params_->attack       = adc[3];
  params_->release      = adc[4];
  params_->sc_hpf_freq  = adc[5];
  params_->output_gain  = adc[6];

  // --- CV modulation (additive, clamped) ---
  // CV over ratio, attack, release
  auto clamp01 = [](float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); };

  params_->cv_ratio   = clamp01(cv[1]);
  params_->cv_attack  = clamp01(cv[2]);
  params_->cv_release = clamp01(cv[3]);
  params_->cv_bypass  = cv[0] > 0.5f; // gate

  // CV bypass gate
  if (params_->cv_bypass) {
    params_->engaged = false;
  }

  // --- VCA CV ---
  // If jack present: use CV value.  If not: unity (1.0)
  params_->vca_cv_l = jack_vca_l ? clamp01(cv[4]) : 1.0f;
  params_->vca_cv_r = jack_vca_r ? clamp01(cv[5]) : 1.0f;

  // Apply CV modulation to knob values
  params_->ratio   = clamp01(params_->ratio   + params_->cv_ratio   * 0.5f);
  params_->attack  = clamp01(params_->attack  + params_->cv_attack  * 0.5f);
  params_->release = clamp01(params_->release + params_->cv_release * 0.5f);

  // --- Sidechain mode from jack presence ---
  UpdateSidechainMode(jack_sc1, jack_sc2);

  // --- Model-specific alt mode ---
  // Shift held = enable alt mode (1176 all-buttons, Distressor Dist3, Opto tube)
  params_->alt_mode = btn_shift_.held();

  // --- Update LEDs ---
  UpdateLeds(meter);
}

}  // namespace compressor
