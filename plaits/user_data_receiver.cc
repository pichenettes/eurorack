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
// User data receiver.

#include "plaits/user_data_receiver.h"

namespace plaits {
  
using namespace std;
using namespace stm_audio_bootloader;

void UserDataReceiver::Init(uint8_t* rx_buffer, size_t size) {
  rx_buffer_ = rx_buffer;
  size_ = size;
  state_ = PACKET_DECODER_STATE_SYNCING;
  Reset();
  threshold_.Init(0.001f, 0.005f);
}

void UserDataReceiver::Reset() {
  decoder_.Init();
  decoder_.Reset();
  demodulator_.Sync();
  received_ = 0;
}

PacketDecoderState UserDataReceiver::Process(float sample) {
  bool sign = threshold_.ProcessCosine(sample);
  uint8_t symbol = demodulator_.Process(sign);
  if (symbol == 0xff) {
    return state_;
  }
  
  state_ = decoder_.ProcessSymbol(symbol);
  switch (state_) {
    case PACKET_DECODER_STATE_OK:
      if (received_ < size_) {
        copy(
            decoder_.packet_data(),
            decoder_.packet_data() + kPacketSize,
            rx_buffer_ + received_);
        received_ += kPacketSize;
      }
      decoder_.Reset();
      demodulator_.Sync();
      break;

    case PACKET_DECODER_STATE_ERROR_SYNC:
    case PACKET_DECODER_STATE_ERROR_CRC:
      Reset();
      break;
    
    default:
      break;
  }
  
  return state_;
}

}  // namespace plaits
