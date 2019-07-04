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
// Factory test mode.

#ifndef TIDES_FACTORY_TEST_H_
#define TIDES_FACTORY_TEST_H_

#include "stmlib/stmlib.h"

#include "tides2/drivers/debug_port.h"
#include "tides2/io_buffer.h"

namespace tides {

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

class Settings;
class CvReader;
class GateInputs;
class Switches;

class FactoryTest {
 public:
  FactoryTest() { instance_ = NULL; }
  ~FactoryTest() { }
  
  void Init(
      Settings* settings,
      CvReader* cv_reader,
      GateInputs* gate_inputs,
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
  
  static void ProcessFn(IOBuffer::Block* block, size_t size);
  void Process(IOBuffer::Block* block, size_t size);
  bool Calibrate(int step, float v1, float v2);
  
  inline bool running() const { return instance_ != NULL; }
  static FactoryTest* GetInstance() { return instance_; }

 private:
  uint8_t HandleRequest(uint8_t command);
  
  static FactoryTest* instance_;

  DebugPort debug_port_;
  
  Settings* settings_;
  CvReader* cv_reader_;
  GateInputs* gate_inputs_;
  const Switches* switches_;
  
  uint16_t forced_dac_code_[kNumCvOutputs];
  uint32_t calibration_data_;
  float calibration_first_adc_value_;
  
  DISALLOW_COPY_AND_ASSIGN(FactoryTest);
};

}  // namespace tides

#endif  // TIDES_FACTORY_TEST_H_
