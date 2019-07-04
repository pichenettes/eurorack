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
// Driver for DAC.

#include "tides2/drivers/dac.h"

#include <algorithm>

namespace tides {

/* static */
Dac* Dac::instance_;
  
void Dac::Init(int sample_rate, size_t block_size) {
  instance_ = this;
  block_size_ = block_size;
  callback_ = NULL;
  
  InitializeGPIO();
  InitializeAudioInterface(sample_rate);
  InitializeDMA(block_size);
}

void Dac::InitializeGPIO() {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

	GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Mode = GPIO_Mode_AF;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_init.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_10 | GPIO_Pin_11;
  GPIO_Init(GPIOA, &gpio_init);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_5);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_5);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_5);
}

void Dac::InitializeAudioInterface(int sample_rate) {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
  SPI_I2S_DeInit(SPI2);
  I2S_InitTypeDef i2s_init;
  i2s_init.I2S_Mode = I2S_Mode_MasterTx;
  i2s_init.I2S_Standard = I2S_Standard_PCMShort;
  i2s_init.I2S_DataFormat = I2S_DataFormat_32b;
  i2s_init.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
  i2s_init.I2S_AudioFreq = sample_rate * kNumCvOutputs >> 1;
  i2s_init.I2S_CPOL = I2S_CPOL_Low;
  I2S_Init(SPI2, &i2s_init);
  I2S_Cmd(SPI2, ENABLE);
}

void Dac::InitializeDMA(size_t block_size) {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
  
  DMA_Cmd(DMA1_Channel5, DISABLE);
  DMA_DeInit(DMA1_Channel5);

  DMA_InitTypeDef dma_init;
  dma_init.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR);
  dma_init.DMA_MemoryBaseAddr = (uint32_t)(&tx_dma_buffer_[0]);
  dma_init.DMA_DIR = DMA_DIR_PeripheralDST;
  dma_init.DMA_BufferSize = 2 * block_size * kNumCvOutputs * 2;
  dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
  dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  dma_init.DMA_Mode = DMA_Mode_Circular;
  dma_init.DMA_Priority = DMA_Priority_High;
  dma_init.DMA_M2M = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel5, &dma_init);
  
  // Enable the interrupts: half transfer and transfer complete.
	DMA_ITConfig(DMA1_Channel5, DMA_IT_TC | DMA_IT_HT, ENABLE);
  
  // Enable the IRQ.
  NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  
  SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
}

void Dac::Start(FillBufferCallback callback) {
  callback_ = callback;
  DMA_Cmd(DMA1_Channel5, ENABLE);
}

void Dac::Stop() {
  DMA_Cmd(DMA1_Channel5, DISABLE);
}

void Dac::Fill(size_t offset) {
  IOBuffer::Slice slice = (*callback_)(block_size_);
  uint16_t* p = &tx_dma_buffer_[offset * block_size_ * kNumCvOutputs * 2];
  for (size_t i = 0; i < block_size_; ++i) {
    for (size_t j = 0; j < kNumCvOutputs; ++j) {
      uint16_t sample = slice.block->output[j][slice.frame_index + i];
      *p++ = 0x1000 | (j << 9) | (sample >> 8);
      *p++ = sample << 8;
    }
  }
}

}  // namespace tides

extern "C" {

void DMA1_Channel5_IRQHandler() {
  uint32_t flags = DMA1->ISR;
  DMA1->IFCR = DMA1_FLAG_TC5 | DMA1_FLAG_HT5;
  if (flags & DMA1_FLAG_TC5) {
    tides::Dac::GetInstance()->Fill(1);
  } else if (flags & DMA1_FLAG_HT5) {
    tides::Dac::GetInstance()->Fill(0);
  }
}

}