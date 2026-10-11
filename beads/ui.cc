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

#include "beads/ui.h"

#include <algorithm>

#include "stmlib/system/system_clock.h"

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/gain_control.h"
#include "beads/settings.h"

namespace beads {

using namespace std;
using namespace stmlib;

/* static */
const uint32_t Ui::long_press_duration[SWITCH_LAST] = {
  4000, 4000, 1500, 0x7fffffff
};

void Ui::Init(
    Settings* settings,
    GainControl* gain_control,
    Calibrator* calibrator,
    BackgroundBufferBackup* background_buffer_backup) {
  leds_.Init();
  switches_.Init();
  
  system_clock.Init();
  
  settings_ = settings;
  gain_control_ = gain_control;
  calibrator_ = calibrator;
  background_buffer_backup_ = background_buffer_backup;
  
  mode_ = MODE_NORMAL;
  
  queue_.Init();
  
  fill(&press_time_[0], &press_time_[SWITCH_LAST], 0);
  fill(&ignore_release_[0], &ignore_release_[SWITCH_LAST], false);
  
  freeze_button_state_ = false;
  freeze_led_state_ = false;
  
  seed_button_state_ = false;
  seed_button_locked_ = true;
  seed_led_state_ = false;
  trim_flag_ = false;
  
  overwrite_reserve_ = 1.0f;
  
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    assignable_cv_pot_[i].Init(1.0f, 0.0f);
  }
}

void Ui::Poll() {
  system_clock.Tick();
  switches_.Debounce();
  
  for (int i = 0; i < SWITCH_LAST; ++i) {
    Switch s = Switch(i);
    
    if (switches_.just_pressed(s)) {
      queue_.AddEvent(CONTROL_SWITCH, i, 0);
      press_time_[i] = system_clock.milliseconds();
      ignore_release_[i] = false;
    }
    
    if (switches_.pressed(s) && !ignore_release_[i]) {
      uint32_t pressed_time = system_clock.milliseconds() - press_time_[i];
      if (pressed_time >= long_press_duration[i]) {
        queue_.AddEvent(CONTROL_SWITCH, i, pressed_time);
        ignore_release_[i] = true;
      }
    }
    
    if (switches_.released(s) && !ignore_release_[i]) {
      queue_.AddEvent(
          CONTROL_SWITCH,
          i,
          system_clock.milliseconds() - press_time_[i] + 1);
      ignore_release_[i] = true;
    }
  }
  
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    PotController* p = &assignable_cv_pot_[i];
    p->ProcessUIRate();
    if (p->editing_hidden_parameter()) {
      State* state = settings_->mutable_state();
      if (switches_.pressed(SWITCH_MEDIA)) {
        // Big hack to prevent the long press detection to fire!
        press_time_[SWITCH_MEDIA] = system_clock.milliseconds();
        state->gain = 1.0f + p->hidden_parameter() * 255.0f;
      } else if (switches_.pressed(SWITCH_ASSIGN)) {
        state->assignable_cv_destination = 3;
        state->assignable_cv_amount[i] = p->hidden_parameter() * 256.0f;
      }
    }
  }
  
  UpdateLeds();
}

/* static */
const Leds::Color Ui::media_palette_[4] = {
  { 128, 0, 48 },
  { 255, 144, 0 },
  { 0, 255, 192 },
  { 192, 192, 208 },
};

void Ui::UpdateLeds() {
  leds_.Clear();
  
  int triangle = (system_clock.milliseconds() & 2047) >> 2;
  triangle = triangle >= 256 ? 511 - triangle : triangle;

  bool adjusting_manual_gain = switches_.pressed(SWITCH_MEDIA) && \
      assignable_cv_pot_[MIX_PARAMETER_FEEDBACK].editing_hidden_parameter();
  
  switch (mode_) {
    case MODE_PANIC:
      {
        bool blink = system_clock.milliseconds() & 64;
        for (int i = 0; i < LED_LAST; ++i) {
          leds_.set(Led(i), blink ? 255 : 0);
        }
      }
      break;
      
    case MODE_CALIBRATION_1:
    case MODE_CALIBRATION_2:
    case MODE_CALIBRATION_3:
    case MODE_CALIBRATION_4:
      {
        int channel = (int(mode_) - MODE_CALIBRATION_1) >> 1;
        int step = (int(mode_) - MODE_CALIBRATION_1) & 1;
        leds_.set(channel == 0 ? LED_FREEZE : LED_SEED, 255);
        leds_.set(LED_MEDIA_R, step == 1 ? 255 : 0);
        leds_.set(LED_MEDIA_G, step == 1 ? 255 : 255);
        leds_.set(LED_MEDIA_B, 0);
        leds_.set(LED_ASSIGN_1, triangle >> 2);
        leds_.set(LED_ASSIGN_2, triangle >> 2);
        leds_.set(LED_ASSIGN_3, triangle >> 2);
      }
      break;
    case MODE_DISPLAY_AUTOSAVE:
    case MODE_NORMAL:
      {
        const State& state = settings_->state();
        
        if (adjusting_manual_gain) {
          leds_.set_meter(0.05f + 0.95f * float(state.gain) / 256.0f);
        } else {
          Leds::Color media_color = media_palette_[state.media];
          const float brightness = overwrite_reserve_ * \
              (state.monophonic_mode ? 0.5f : 1.0f);
          leds_.set_media(media_color, brightness);
        }
        
        if (mode_ == MODE_DISPLAY_AUTOSAVE) {
          leds_.set(LED_FREEZE, state.disable_autosave ? 16 : 64);
        } else {
          leds_.set(LED_FREEZE,
              freeze_led_state_
                  ? (background_buffer_backup_->busy() ? 128 : 255)
                  : 0);
        }
        
        if (seed_button_locked_) {
          leds_.set(LED_SEED, (triangle * 3 >> 2) + 64);
        } else {
          leds_.set(
              LED_SEED,
              (seed_button_state_ || seed_led_state_) ? 255 : 0);
        }
        
        if (state.assignable_cv_destination == 3) {
          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
            leds_.set(
                Led(LED_ASSIGN_1 + i),
                state.assignable_cv_amount[i] >> 1);
          }
        } else {
          leds_.set(Led(LED_ASSIGN_1 + state.assignable_cv_destination), 128);
        }
      }
      break;
      
    case MODE_FACTORY_TEST:
      for (int i = 0; i < LED_LAST; ++i) {
        leds_.set(Led(i), triangle);
      }
      break;
  }
  
  if (mode_ != MODE_FACTORY_TEST) {
    if (gain_control_->adjusting()) {
      leds_.set(LED_VU_METER, (system_clock.milliseconds() & 128) ? 128 : 0);
    } else {
      leds_.set(LED_VU_METER, gain_control_->vu_meter());
    }
  }
  leds_.Write();
}

void Ui::FlushEvents() {
  queue_.Flush();
}

void Ui::OnSwitchPressed(const Event& e) {
  // Combo
  //
  // SEED + FREEZE = seed lock.
  //
  if (switches_.pressed(SWITCH_SEED) && e.control_id == SWITCH_FREEZE) {
    seed_button_locked_ = true;
    seed_button_state_ = false;
    ignore_release_[SWITCH_SEED] = ignore_release_[SWITCH_FREEZE] = true;
    return;
  }
  
  // Combo
  //
  // Assign + SEED = toggle gate output mode.
  //
  if (switches_.pressed(SWITCH_ASSIGN) && e.control_id == SWITCH_SEED) {
    State* state = settings_->mutable_state();
    state->gate_output_mode = !state->gate_output_mode;
    settings_->SaveState();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    ignore_release_[SWITCH_ASSIGN] = ignore_release_[SWITCH_SEED] = true;
    return;
  }

  // Combo
  //
  // Assign + FREEZE = toggle autosave.
  //
  if (switches_.pressed(SWITCH_ASSIGN) && e.control_id == SWITCH_FREEZE) {
    State* state = settings_->mutable_state();
    state->disable_autosave = !state->disable_autosave;
    settings_->SaveState();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    mode_ = MODE_DISPLAY_AUTOSAVE;
    ignore_release_[SWITCH_ASSIGN] = ignore_release_[SWITCH_FREEZE] = true;
    return;
  }

#ifdef ENABLE_ADVANCED_FEATURES
  // Combo
  //
  // Media + SEED = toogle monophonic mode.
  //
  if (switches_.pressed(SWITCH_MEDIA) && e.control_id == SWITCH_SEED) {
    State* state = settings_->mutable_state();
    state->monophonic_mode = !state->monophonic_mode;
    settings_->SaveState();
    assignable_cv_pot_[0].Unlock();
    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_SEED] = true;
    return;
  }

  // Combo
  //
  // Media + FREEZE = trim the buffer and unfreeze.
  //
  if (switches_.pressed(SWITCH_MEDIA) && \
      e.control_id == SWITCH_FREEZE) {
    trim_flag_ = true;
    freeze_button_state_ = false;
    assignable_cv_pot_[0].Unlock();
    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_FREEZE] = true;
    return;
  }
#endif  // ENABLE_ADVANCED_FEATURES
  
  // Combo
  //
  // Assign + Media = calibration mode.
  //
  if ((e.control_id == SWITCH_ASSIGN && switches_.pressed(SWITCH_MEDIA)) || \
      (e.control_id == SWITCH_MEDIA && switches_.pressed(SWITCH_ASSIGN))) {
    mode_ = MODE_CALIBRATION_1;
    calibrator_->Start();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_ASSIGN] = true;
    return;
  }
  
  switch (e.control_id) {
    case SWITCH_FREEZE:
      freeze_button_state_ = !freeze_button_state_;
      break;

    case SWITCH_SEED:
      if (seed_button_locked_) {
        seed_button_locked_ = false;
      }
      seed_button_state_ = true;
      break;
      
    case SWITCH_MEDIA:
      assignable_cv_pot_[0].Lock();
      break;
      
    case SWITCH_ASSIGN:
      for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
        assignable_cv_pot_[i].Lock();
      }
      break;
    
    default:
      break;
  }
}

void Ui::NextCalibrationStep() {
  switch (mode_) {
    case MODE_CALIBRATION_1:
      calibrator_->CalibrateOffsets();
      calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 0, 12.0f);
      mode_ = MODE_CALIBRATION_2;
      break;

    case MODE_CALIBRATION_2:
      mode_ = calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 1, 36.0f)
          ? MODE_CALIBRATION_3
          : MODE_PANIC;
      break;

    case MODE_CALIBRATION_3:
      calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 0, 0.2f);
      mode_ = MODE_CALIBRATION_4;
      break;
    
    case MODE_CALIBRATION_4:
      mode_ = calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 1, 0.6f)
          ? MODE_NORMAL
          : MODE_PANIC;
      if (mode_ == MODE_NORMAL) {
        settings_->SavePersistentData();
      }
      break;
      
    default:
      break;
  }
}

void Ui::OnSwitchReleased(const Event& e) {
  switch (e.control_id) {
    case SWITCH_SEED:
      {
        if (uint32_t(e.data) >= long_press_duration[SWITCH_SEED]) {
          seed_button_locked_ = true;
        }
        seed_button_state_ = false;
      }
      break;

    case SWITCH_MEDIA:
      {
        if (!assignable_cv_pot_[0].editing_hidden_parameter()) {
          if (mode_ >= MODE_CALIBRATION_1 && mode_ <= MODE_CALIBRATION_4) {
            NextCalibrationStep();
          } else {
            if (uint32_t(e.data) >= long_press_duration[SWITCH_MEDIA]) {
              State* state = settings_->mutable_state();
              if (state->gain != 0) {
                state->gain = 0;
                settings_->SaveState();
              }
              gain_control_->Restart();
            } else {
              State* state = settings_->mutable_state();
              state->media = (state->media + 3) % 4;
              settings_->SaveState();
            }
          }
        } else {
          // Save the manual gain settings.
          settings_->SaveState();
        }
        assignable_cv_pot_[0].Unlock();
      }
      break;

    case SWITCH_ASSIGN:
      {
        bool pot_touched = false;
        for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
          pot_touched = pot_touched || \
              assignable_cv_pot_[i].editing_hidden_parameter();
          assignable_cv_pot_[i].Unlock();
        }
        if (!pot_touched) {
          State* state = settings_->mutable_state();
          state->assignable_cv_destination = \
              state->assignable_cv_destination == 3
                  ? 0
                  : ((state->assignable_cv_destination + 1) % 3);
        }
        settings_->SaveState();
      }
      break;

    default:
      break;
  }
}

void Ui::DoEvents() {
  while (queue_.available()) {
    Event e = queue_.PullEvent();
    if (e.control_type == CONTROL_SWITCH) {
      if (e.data == 0) {
        OnSwitchPressed(e);
      } else {
        OnSwitchReleased(e);
      }
    }
  }
  
  if (queue_.idle_time() > 1000 && (mode_ == MODE_PANIC || mode_ == MODE_DISPLAY_AUTOSAVE)) {
    mode_ = MODE_NORMAL;
  }

  if (queue_.idle_time() > 3000) {
    queue_.Touch();
  }
}

}  // namespace beads