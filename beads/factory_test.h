// Copyright 2019 Emilie Gillet.
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
// Factory test mode.

#ifndef BEADS_FACTORY_TEST_H_
#define BEADS_FACTORY_TEST_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/codec.h"
#include "beads/drivers/debug_port.h"

namespace beads {

enum FactoryTestCommand {
  FACTORY_TEST_READ_POT,
  FACTORY_TEST_READ_CV,
  FACTORY_TEST_READ_GATE,
  FACTORY_TEST_GENERATE_TEST_SIGNALS,
  FACTORY_TEST_CALIBRATE,
  FACTORY_TEST_READ_NORMALIZATION,
  FACTORY_TEST_FORCE_DAC_CODE,
  FACTORY_TEST_WRITE_CALIBRATION_DATA_NIBBLE,
};

class Calibrator;
class CvReader;
class NormalizationDetector;
class Switches;

class FactoryTest {
 public:
  FactoryTest() { instance_ = NULL; }
  ~FactoryTest() { }
  
  void Init(
      CvReader* cv_reader,
      NormalizationDetector* normalization_detector,
      Calibrator* calibrator,
      const Switches* switches);
  
  void Start();
  
  inline void Poll() {
    if (running()) {
      if (debug_port_.readable()) {
        uint8_t command = debug_port_.Read();
        uint8_t response = HandleRequest(command);
        debug_port_.Write(response);
      }
    }
  }
  
  static void ProcessFn(
      const Codec::Event* events,
      const Codec::Frame* in,
      Codec::Frame* out,
      size_t size);
  
  void Process(
      const Codec::Event* events,
      const Codec::Frame* in,
      Codec::Frame* out,
      size_t size);
  
  bool Calibrate(int step);
  
  inline bool running() const { return instance_ != NULL; }
  static FactoryTest* GetInstance() { return instance_; }

 private:
  uint8_t HandleRequest(uint8_t command);
  
  static FactoryTest* instance_;

  DebugPort debug_port_;
  
  CvReader* cv_reader_;
  NormalizationDetector* normalization_detector_;
  Calibrator* calibrator_;
  const Switches* switches_;
  
  DISALLOW_COPY_AND_ASSIGN(FactoryTest);
};

}  // namespace beads

#endif  // BEADS_FACTORY_TEST_H_
