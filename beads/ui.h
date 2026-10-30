// Copyright 2018 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Author: Frédéric Meslin (fred@fredslab.net)
// Author: Oliver Rockstedt (info@sourcebox.de)
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
// User interface

#ifndef BEADS_UI_H_
#define BEADS_UI_H_

#include "stmlib/ui/event_queue.h"

#include "beads/pot_controller.h"
#include "beads/drivers/leds.h"
#include "beads/drivers/switches.h"

// Enable two advanced features:
// - MEDIA + SEED enables or disables monophonic grain mode.
// - MEDIA + FREEZE trims the duration of the buffer to 0.
// #define ENABLE_ADVANCED_FEATURES

namespace beads {

class BackgroundBufferBackup;
class Calibrator;
class GainControl;
class Settings;

class Ui {
 public:
  Ui() { }
  ~Ui() { }
  
  enum Mode {
    MODE_NORMAL,
    MODE_CALIBRATION_1,
    MODE_CALIBRATION_2,
    MODE_CALIBRATION_3,
    MODE_CALIBRATION_4,
    MODE_DISPLAY_AUTOSAVE,
    MODE_FACTORY_TEST,
    MODE_PANIC
  };
  
  enum MixParameter {
    MIX_PARAMETER_FEEDBACK,
    MIX_PARAMETER_DRY_WET,
    MIX_PARAMETER_REVERB,
    MIX_PARAMETER_LAST,
  };
  
  void Init(
      Settings* settings,
      GainControl* gain_control,
      Calibrator* calibrator,
      BackgroundBufferBackup* background_buffer_backup);
  void Poll();
  void DoEvents();
  void FlushEvents();
  void Panic() {
    mode_ = MODE_PANIC;
  }
  
  inline bool should_ignore_state_change() const {
    bool frozen = freeze_led_state_;
    bool calibrating = mode_ >= MODE_CALIBRATION_1 && \
        mode_ <= MODE_CALIBRATION_4;
    bool testing = mode_ == MODE_FACTORY_TEST;
    
    return frozen || calibrating || testing;
  }
  
  inline bool pressed_immediate(Switch s) const {
    return switches_.pressed_immediate(s);
  }
  
  inline bool freeze_led_state() const {
    return freeze_led_state_;
  }
  
  inline bool freeze_button_state() const {
    return freeze_button_state_;
  }
  
  inline bool seed_button_state() const {
    return seed_button_state_;
  }

  inline bool seed_button_locked() const {
    return seed_button_locked_;
  }
  
  inline void set_freeze_button_state(bool freeze_button_state) {
    freeze_button_state_ = freeze_button_state;
    freeze_led_state_ = freeze_button_state;
  }
  
  inline float mix_parameter(MixParameter parameter) {
    return assignable_cv_pot_[parameter].main_parameter();
  }
  
  inline void set_freeze_led_state(bool freeze_led_state) {
    freeze_led_state_ = freeze_led_state;
  }

  inline void set_seed_led_state(bool seed_led_state) {
    seed_led_state_ = seed_led_state;
  }
  
  inline void set_overwrite_reserve(float overwrite_reserve) {
    overwrite_reserve_ = overwrite_reserve;
  }
  
  void ProcessPot(MixParameter parameter, float value) {
    assignable_cv_pot_[parameter].ProcessControlRate(value);
  }
  
  inline const Switches& switches() const {
    return switches_;
  }
  
  inline void set_factory_test(bool factory_test) {
    mode_ = factory_test ? MODE_FACTORY_TEST : MODE_NORMAL;
  }
  
  inline bool trim_flag() const {
    return trim_flag_;
  }
  
  inline void ClearTrimFlag() {
    trim_flag_ = false;
  }
  
 private:
  void OnSwitchPressed(const stmlib::Event& e);
  void OnSwitchReleased(const stmlib::Event& e);

  void UpdateLeds();
  void NextCalibrationStep();

  stmlib::EventQueue<16> queue_;

  Leds leds_;
  Switches switches_;
  uint32_t press_time_[SWITCH_LAST];
  static const uint32_t long_press_duration[SWITCH_LAST];
  bool ignore_release_[SWITCH_LAST];
  Mode mode_;
  
  bool trim_flag_;
  bool freeze_button_state_;
  bool freeze_led_state_;
  
  bool seed_button_state_;
  bool seed_button_locked_;
  bool seed_led_state_;
  
  float overwrite_reserve_;
  
  GainControl* gain_control_;
  Settings* settings_;
  Calibrator* calibrator_;
  BackgroundBufferBackup* background_buffer_backup_;
  
  PotController assignable_cv_pot_[MIX_PARAMETER_LAST];

  static const Leds::Color media_palette_[4];
  
  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace beads

#endif  // BEADS_UI_H_