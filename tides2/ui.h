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
// User interface.

#ifndef TIDES_UI_H_
#define TIDES_UI_H_

#include "stmlib/stmlib.h"

#include "stmlib/ui/event_queue.h"

#include "tides2/drivers/leds.h"
#include "tides2/drivers/switches.h"

#include "tides2/settings.h"

namespace tides {

enum UiMode {
  UI_MODE_NORMAL,
  UI_MODE_CALIBRATION_C1,
  UI_MODE_CALIBRATION_C3,
  UI_MODE_FACTORY_TEST
};

class FactoryTest;

class Ui {
 public:
  Ui() { }
  ~Ui() { }
  
  void Init(Settings* settings, FactoryTest* factory_test);
  void Poll();
  void DoEvents();
  
  inline const Switches& switches() const { return switches_; }
  void set_factory_test(bool factory_test) {
    mode_ = factory_test ? UI_MODE_FACTORY_TEST : UI_MODE_NORMAL;
  }

 private:
  void OnSwitchPressed(const stmlib::Event& e);
  void OnSwitchReleased(const stmlib::Event& e);
   
  LedColor MakeColor(uint8_t value, bool color_blind);
  void UpdateLEDs();

  stmlib::EventQueue<16> queue_;
  
  Leds leds_;
  Switches switches_;
  uint32_t press_time_[SWITCH_LAST];
  bool ignore_release_[SWITCH_LAST];
  
  Settings* settings_;
  FactoryTest* factory_test_;
  
  UiMode mode_;

  static const LedColor palette_[4];
  
  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace tides

#endif  // TIDES_UI_H_
