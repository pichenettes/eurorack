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
// The friendly state machine that saves the buffer in the background.

#ifndef BEADS_BACKGROUND_BUFFER_BACKUP_H_
#define BEADS_BACKGROUND_BUFFER_BACKUP_H_

#include "stmlib/stmlib.h"

namespace beads {
  
class Processor;
class Settings;

class BackgroundBufferBackup {
 public:
  BackgroundBufferBackup() { }
  ~BackgroundBufferBackup() { }
  
  void Init(Processor* processor, Settings* settings);
  bool Load();
  void Process(bool freeze_state);
  
  void EraseSector();
  void WriteBlock();
  void SaveState(bool freeze_state);
  
  inline bool busy() const { return remaining_bytes_ || remaining_sectors_; }
  
  struct BufferBackupHeader {
    uint32_t tag;
    int write_ptr;
    uint8_t media;
    uint8_t mono;
  };

 private:
  Processor* processor_;
  Settings* settings_;
  
  size_t write_offset_;
  size_t remaining_bytes_;
  size_t remaining_sectors_;
  uint32_t freeze_start_time_;
  
  bool previous_freeze_state_;
  bool busy_;
  
  DISALLOW_COPY_AND_ASSIGN(BackgroundBufferBackup);
};

}  // namespace beads

#endif  // BEADS_BACKGROUND_BUFFER_BACKUP_H_