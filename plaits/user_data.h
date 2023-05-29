// Copyright 2021 Emilie Gillet.
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
// User data manager.

#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

#ifdef TEST

// Mock flash saving functions for debugging purposes.
#define PAGE_SIZE 0x800

inline void FLASH_Unlock() { }

inline void FLASH_ErasePage(uint32_t address) {
  printf("--- Erasing page at %08x ---\n", address);
}

inline void FLASH_ProgramWord(uint32_t address, uint32_t word) {
  if (address % 32 == 0) {
    printf("%08x ", address);
  }
  printf(
      "%08x ",
      (word >> 24) | ((word & 0x00ff0000) >> 8) | ((word & 0x0000ff00) << 8) | (word << 24));
  if (address % 32 == 28) {
    printf("\n");
  }
}

#else

#include <stm32f37x_conf.h>
#include "stmlib/system/flash_programming.h"

#endif  // TEST

namespace plaits {

class UserData {
 public:
  enum {
    ADDRESS = 0x08007000,
    SIZE = 0x1000
  };

  UserData() { }
  ~UserData() { }

#ifdef TEST  
  inline const uint8_t* ptr(int slot) const {
    return NULL;
  }
#else
  inline const uint8_t* ptr(int slot) const {
    const uint8_t* data = (const uint8_t*)(ADDRESS);
    if (data[SIZE - 2] == 'U' && data[SIZE - 1] == (' ' + slot)) {
      return data;
    } else {
      return NULL;
    }
  }
#endif  // TEST
  
  inline bool Save(uint8_t* rx_buffer, int slot) {
    if (slot < rx_buffer[SIZE - 2] || slot > rx_buffer[SIZE - 1]) {
      return false;
    }
    
    // Tag the data to identify which engine it should be associated to.
    rx_buffer[SIZE - 2] = 'U';
    rx_buffer[SIZE - 1] = ' ' + slot;

    // Write to FLASH.
    const uint32_t* words = static_cast<const uint32_t*>(
        static_cast<const void*>(rx_buffer));
    for (uint32_t i = ADDRESS; i < ADDRESS + SIZE; i += 4) {
      if (i % PAGE_SIZE == 0) {
        FLASH_Unlock();
        FLASH_ErasePage(i);
      }
      FLASH_ProgramWord(i, *words++);
    }
    return true;
  }
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
