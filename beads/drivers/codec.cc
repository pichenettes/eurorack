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

#include "beads/drivers/codec.h"

#include <cstring>

// Internal hardware definitions / configuration
#define CODEC_SAI                   SAI1
#define CODEC_DMA                   DMA1

#define CODEC_DAC_SAI_BLOCK         SAI1_Block_A
#define CODEC_DAC_DMA_STREAM        DMA1_Stream0

#define CODEC_ADC_SAI_BLOCK         SAI1_Block_B
#define CODEC_ADC_DMA_STREAM        DMA1_Stream1
#define CODEC_ADC_DMA_ISR           CODEC_DMA->LISR
#define CODEC_ADC_DMA_IFCR          CODEC_DMA->LIFCR
#define CODEC_ADC_DMA_ISR_FLAG_TC   DMA_LISR_TCIF1
#define CODEC_ADC_DMA_ISR_FLAG_HT   DMA_LISR_HTIF1
#define CODEC_ADC_STREAM_IRQ        DMA1_Stream1_IRQn
#define CODEC_ADC_STREAM_HANDLER    DMA1_Stream1_IRQHandler

#define IDR_PORT                    SEED_GPIO_PORT
#define IDR_SEED                    SEED_GPIO_PIN
#define IDR_FREEZE                  FREEZE_GPIO_PIN

namespace beads {

using namespace std;

/* static */
Codec* Codec::instance_;

/* static */
Codec::Frame Codec::tx_dma_buffer_[kMaxCodecBlockSize * 2];

/* static */
Codec::Frame Codec::rx_dma_buffer_[kMaxCodecBlockSize * 2];

/* static */
Codec::PaddedEvent Codec::idr_buffer_[kMaxCodecBlockSize * 2];

bool Codec::Init() {
  bool success = InitializeGPIO() && \
      InitializeDMA() && \
      InitializeGPIODMA();
  if (success) {
    instance_ = this;
  } else {
    instance_ = NULL;
  }
  
  return success;
}

bool Codec::InitializeGPIO() {
  GPIO_InitTypeDef gpio_init;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init.Pin = CODEC_RESET_GPIO_PIN;
  HAL_GPIO_Init(CODEC_RESET_GPIO_PORT, &gpio_init);
  
  gpio_init.Pin = CODEC_MCK_GPIO_PIN | CODEC_WS_GPIO_PIN | \
      CODEC_SCK_GPIO_PIN | CODEC_MOSI_GPIO_PIN | CODEC_MISO_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF6_SAI1;
  HAL_GPIO_Init(GPIOE, &gpio_init);

  return true;
}

bool Codec::InitializeCodec(int32_t sample_rate) {
  __HAL_RCC_SAI1_CLK_ENABLE();

  memset(&sai_tx_, 0, sizeof(SAI_HandleTypeDef));
  sai_tx_.Instance = CODEC_DAC_SAI_BLOCK;
  sai_tx_.Init.AudioMode = SAI_MODEMASTER_TX;
  sai_tx_.Init.Synchro = SAI_ASYNCHRONOUS;
  sai_tx_.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  sai_tx_.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  sai_tx_.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  sai_tx_.Init.AudioFrequency = sample_rate;
  sai_tx_.Init.Mckdiv = 0;
  sai_tx_.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  sai_tx_.Init.MonoStereoMode = SAI_STEREOMODE;
  sai_tx_.Init.CompandingMode = SAI_NOCOMPANDING;
  sai_tx_.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&sai_tx_,
                           SAI_I2S_STANDARD,
                           SAI_PROTOCOL_DATASIZE_32BIT,
                           2) != HAL_OK)
    return false;

  memset(&sai_rx_, 0, sizeof(SAI_HandleTypeDef));
  sai_rx_.Instance = CODEC_ADC_SAI_BLOCK;
  sai_rx_.Init.AudioMode = SAI_MODESLAVE_RX;
  sai_rx_.Init.Synchro = SAI_SYNCHRONOUS;
  sai_rx_.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  sai_rx_.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  sai_rx_.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  sai_rx_.Init.MonoStereoMode = SAI_STEREOMODE;
  sai_rx_.Init.CompandingMode = SAI_NOCOMPANDING;
  sai_rx_.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&sai_rx_,
                           SAI_I2S_STANDARD,
                           SAI_PROTOCOL_DATASIZE_32BIT,
                           2) != HAL_OK)
    return false;

  return true;
}

bool Codec::InitializeDMA() {
  memset(&dma_sai_tx_, 0, sizeof(DMA_HandleTypeDef));
  dma_sai_tx_.Instance = CODEC_DAC_DMA_STREAM;
  dma_sai_tx_.Init.Request = DMA_REQUEST_SAI1_A;
  dma_sai_tx_.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dma_sai_tx_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_sai_tx_.Init.MemInc = DMA_MINC_ENABLE;
  dma_sai_tx_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_sai_tx_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma_sai_tx_.Init.Mode = DMA_CIRCULAR;
  dma_sai_tx_.Init.Priority = DMA_PRIORITY_HIGH;
  dma_sai_tx_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_sai_tx_) != HAL_OK)
    return false;

  memset(&dma_sai_rx_, 0, sizeof(DMA_HandleTypeDef));
  dma_sai_rx_.Instance = CODEC_ADC_DMA_STREAM;
  dma_sai_rx_.Init.Request = DMA_REQUEST_SAI1_B;
  dma_sai_rx_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_sai_rx_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_sai_rx_.Init.MemInc = DMA_MINC_ENABLE;
  dma_sai_rx_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_sai_rx_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma_sai_rx_.Init.Mode = DMA_CIRCULAR;
  dma_sai_rx_.Init.Priority = DMA_PRIORITY_HIGH;
  dma_sai_rx_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  
  if (HAL_DMA_Init(&dma_sai_rx_) != HAL_OK)
    return false;

  // Enable the IRQ for the ADC DMA transfers
  // The specific events (Half-transfer and Transfer-complete are 
  // enabled in Codec::Start
  HAL_NVIC_EnableIRQ(CODEC_ADC_STREAM_IRQ);
  HAL_NVIC_SetPriority(CODEC_ADC_STREAM_IRQ, 1, 0);

  return true;
}

bool Codec::InitializeGPIODMA() {
  HAL_DMA_MuxSyncConfigTypeDef pSyncConfig;
  pSyncConfig.SyncSignalID = 0;
  pSyncConfig.SyncPolarity = HAL_DMAMUX_SYNC_NO_EVENT;
  pSyncConfig.SyncEnable = DISABLE;
  pSyncConfig.EventEnable = ENABLE;
  pSyncConfig.RequestNumber = 1;
  if (HAL_DMAEx_ConfigMuxSync(&dma_sai_rx_, &pSyncConfig) != HAL_OK)
    return false;
  
  dma_sai_idr_.Instance = DMA1_Stream6;
  dma_sai_idr_.Init.Request = DMA_REQUEST_GENERATOR0;
  dma_sai_idr_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_sai_idr_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_sai_idr_.Init.MemInc = DMA_MINC_ENABLE;
  dma_sai_idr_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_sai_idr_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma_sai_idr_.Init.Mode = DMA_CIRCULAR;
  dma_sai_idr_.Init.Priority = DMA_PRIORITY_MEDIUM;
  dma_sai_idr_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_sai_idr_) != HAL_OK)
    return false;

  HAL_DMA_MuxRequestGeneratorConfigTypeDef pRequestGeneratorConfig;
  pRequestGeneratorConfig.SignalID = HAL_DMAMUX1_REQ_GEN_DMAMUX1_CH1_EVT;
  pRequestGeneratorConfig.Polarity = HAL_DMAMUX_REQ_GEN_RISING;
  pRequestGeneratorConfig.RequestNumber = 1;
  if (HAL_DMAEx_ConfigMuxRequestGenerator(&dma_sai_idr_,
                                          &pRequestGeneratorConfig) != HAL_OK)
    return false;
 
  return true;
}

bool Codec::Start(
    int32_t sample_rate,
    size_t block_size,
    FillBufferCallback callback) {
  if (!callback) return false;
  
  if (!InitializeCodec(sample_rate)) {
    return false;
  }
  
  callback_ = callback;

  // Enable the codec.
  CODEC_RESET_GPIO_PORT->BSRR = CODEC_RESET_GPIO_PIN;  

  block_size_ = block_size;
  const size_t buffer_size_in_words = \
      sizeof(Frame) / sizeof(uint32_t) * block_size * 2;

  // Enable the DMA transfer for the data sent to the codec, read from
  // the codec, and the data from GPIOB which contains the freeze/seed bits
  // read in sync with the audio samples.

  HAL_DMA_Start(
      &dma_sai_idr_,
      (uint32_t)&IDR_PORT->IDR,
      (uint32_t)idr_buffer_,
      buffer_size_in_words);
  HAL_DMAEx_EnableMuxRequestGenerator(&dma_sai_idr_);

  HAL_DMA_Start(
      &dma_sai_tx_,
      (uint32_t)(&tx_dma_buffer_[0]),
      (uint32_t)&sai_tx_.Instance->DR,
      buffer_size_in_words);

  HAL_DMA_Start(
      &dma_sai_rx_,
      (uint32_t)&sai_rx_.Instance->DR,
      (uint32_t)(&rx_dma_buffer_[0]),
      buffer_size_in_words);

  // Enable TC and HT interrupts for the DMA associated with data reception.
  ((DMA_Stream_TypeDef *) dma_sai_rx_.Instance)->CR |= DMA_IT_TC | DMA_IT_HT;

  // Enable the SAI peripherals (and their DMA requests) for TX and RX.
  HAL_SAI_DMAResume(&sai_tx_);
  HAL_SAI_DMAResume(&sai_rx_);
  
  return true;
}

void Codec::Stop() {
  HAL_DMA_Abort(&dma_sai_tx_);
  HAL_DMA_Abort(&dma_sai_rx_);
  HAL_DMA_Abort(&dma_sai_idr_);
  
  HAL_SAI_DeInit(&sai_tx_);
  HAL_SAI_DeInit(&sai_rx_);

  // Disable the codec.
  CODEC_RESET_GPIO_PORT->BSRR = CODEC_RESET_GPIO_PIN << 16U;  

  callback_ = NULL;
  block_size_ = 0;
}

void Codec::Fill(size_t offset) {
  if (callback_) {
    offset *= block_size_;
    
    const Frame* in = &rx_dma_buffer_[offset];
    Frame* out = &tx_dma_buffer_[offset];
    
    for (size_t i = 0; i < block_size_; ++i) {
      uint32_t pins = idr_buffer_[offset + i].event;
      int e = pins & SEED_GPIO_PIN ? 0 : EVENT_SEED;
      e |= pins & FREEZE_GPIO_PIN ? 0 : EVENT_FREEZE;
      event_buffer_[i] = static_cast<Event>(e);
    }

    (*callback_)(event_buffer_, in, out, block_size_);
  }
}

}  // namespace beads

// DMA & SAI related interrupts
extern "C" {

void CODEC_ADC_STREAM_HANDLER(void) {
  if (CODEC_ADC_DMA_ISR & CODEC_ADC_DMA_ISR_FLAG_TC) {
    beads::Codec::GetInstance()->Fill(1);
    CODEC_ADC_DMA_IFCR = CODEC_ADC_DMA_ISR_FLAG_TC;
  } else if (CODEC_ADC_DMA_ISR & CODEC_ADC_DMA_ISR_FLAG_HT) {
    beads::Codec::GetInstance()->Fill(0);
    CODEC_ADC_DMA_IFCR = CODEC_ADC_DMA_ISR_FLAG_HT;
  }
}

}
