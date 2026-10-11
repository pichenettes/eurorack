// Copyright 2018 Fred's Lab and Mutable Instruments.
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
// Drivers for the PCM3060 24bit stereo ADC & DAC.

#ifndef BEADS_DRIVERS_CODEC_H_
#define BEADS_DRIVERS_CODEC_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/pins.h"

namespace beads {

// Audio buffer size in stereo samples (= frames)
const size_t kMaxCodecBlockSize = 24;

class Codec {
 public:
  Codec() { }
  ~Codec() { }

  struct Frame {
    int32_t l;
    int32_t r;
  } __attribute__((packed, aligned(4)));
  
  struct PaddedEvent {
    uint32_t event;
    uint32_t padding;
  } __attribute__((packed, aligned(4)));
  
  enum Event {
    EVENT_NONE,
    EVENT_SEED,
    EVENT_FREEZE,
  };
  
  typedef void (*FillBufferCallback)(
      const Event* events,
      const Frame* in,
      Frame* out,
      size_t size);
  
  bool Init();
  bool Start(int32_t sample_rate, size_t block_size, FillBufferCallback callback = NULL);
  void Stop();
  
  void Fill(size_t offset);
  inline static Codec* GetInstance() { return instance_; }
  
 private:
  bool InitializeGPIO();
  bool InitializeCodec(int32_t sample_rate);
  bool InitializeDMA();
  bool InitializeGPIODMA();

  static Codec* instance_;

  static Frame tx_dma_buffer_[kMaxCodecBlockSize * 2] __attribute__((section (".dma")));
  static Frame rx_dma_buffer_[kMaxCodecBlockSize * 2] __attribute__((section (".dma")));
  static PaddedEvent idr_buffer_[kMaxCodecBlockSize * 2] __attribute__((section (".dma")));
	
  size_t block_size_;
  
  Event event_buffer_[kMaxCodecBlockSize];

  FillBufferCallback callback_;
    
  SAI_HandleTypeDef sai_tx_;
  SAI_HandleTypeDef sai_rx_;
  DMA_HandleTypeDef dma_sai_tx_;
  DMA_HandleTypeDef dma_sai_rx_;
  DMA_HandleTypeDef dma_sai_idr_;
  
  DISALLOW_COPY_AND_ASSIGN(Codec);
};

}  // namespace beads

#endif // BEADS_DRIVERS_CODEC_H_