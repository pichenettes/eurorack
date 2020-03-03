// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef TIDES_TEST_FIXTURES_H_
#define TIDES_TEST_FIXTURES_H_

#include <cstdlib>
#include <vector>

#include "stmlib/utils/gate_flags.h"

namespace tides {

using namespace std;
using namespace stmlib;

class PulseGenerator {
 public:
  PulseGenerator() {
    counter_ = 0;
    previous_state_ = 0;
  }
  ~PulseGenerator() { }
  
  inline bool empty() const {
    return pulses_.size() == 0;
  }
  
  void AddPulses(int total_duration, int on_duration, int num_repetitions) {
    Pulse p;
    p.total_duration = total_duration;
    p.on_duration = on_duration;
    p.num_repetitions = num_repetitions;
    pulses_.push_back(p);
  }
  
  void CreateTestPattern() {
    AddPulses(6000, 1000, 16);
    AddPulses(6000, 3000, 6);
    AddPulses(12000, 1000, 6);
    AddPulses(12000, 6000, 6);
    AddPulses(24000, 1000, 3);
    AddPulses(24000, 12000, 3);
  }
  
  void Render(GateFlags* clock, size_t size) {
    while (size--) {
      bool current_state = pulses_.size() && counter_ < pulses_[0].on_duration;
      ++counter_;
      if (pulses_.size() && counter_ >= pulses_[0].total_duration) {
        counter_ = 0;
        --pulses_[0].num_repetitions;
        if (pulses_[0].num_repetitions == 0) {
          pulses_.erase(pulses_.begin());
        }
      }
      previous_state_ = *clock++ = ExtractGateFlags(previous_state_, current_state);
    }
  }
  
 private:
  struct Pulse {
    int total_duration;
    int on_duration;
    int num_repetitions;
  };
  int counter_;
  GateFlags previous_state_;
  
  vector<Pulse> pulses_;
  
  DISALLOW_COPY_AND_ASSIGN(PulseGenerator);
};

}  // namespace tides

#endif  // TIDES_TEST_FIXTURES_H_