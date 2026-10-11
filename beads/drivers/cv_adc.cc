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
// Driver for ADC1 - used for scanning CVs.

#include "beads/drivers/cv_adc.h"

namespace beads {

/* static */
uint16_t CvAdc::values_[CV_ADC_CHANNEL_LAST];

bool CvAdc::Init() {
  return InitializeADC() && InitializeGPIO();
}
 
bool CvAdc::InitializeADC() {
  bool old_silicon = HAL_GetREVID() <= REV_ID_Y;
  
  adc_.Instance = ADC1;
  adc_.Init.ClockPrescaler = old_silicon
      ? ADC_CLOCK_ASYNC_DIV8
      : ADC_CLOCK_ASYNC_DIV4;
  adc_.Init.Resolution = ADC_RESOLUTION_16B;
  adc_.Init.ScanConvMode = ADC_SCAN_ENABLE;
  adc_.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc_.Init.LowPowerAutoWait = DISABLE;
  adc_.Init.ContinuousConvMode = DISABLE;
  adc_.Init.NbrOfConversion = CV_ADC_CHANNEL_LAST;
  adc_.Init.DiscontinuousConvMode = ENABLE;
  adc_.Init.NbrOfDiscConversion = CV_ADC_CHANNEL_LAST;
  adc_.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc_.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc_.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  adc_.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  adc_.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  adc_.Init.OversamplingMode = ENABLE;
  adc_.Init.Oversampling.Ratio = 8;
  adc_.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_3;
  adc_.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  adc_.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  
  if (HAL_ADC_Init(&adc_) != HAL_OK)
    return false;

  // Configure the ADC multi-mode
  ADC_MultiModeTypeDef multimode;
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&adc_, &multimode) != HAL_OK)
    return false;

  // Configure Regular Channel
  ADC_ChannelConfTypeDef channel_config;
  channel_config.Channel = ADC_CHANNEL_16;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // Configure Regular Channel
  channel_config.Channel = ADC_CHANNEL_17;
  channel_config.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // Configure Regular Channel
  channel_config.Channel = ADC_CHANNEL_14;
  channel_config.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // Configure Regular Channel
  channel_config.Channel = ADC_CHANNEL_15;
  channel_config.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // Configure Regular Channel
  channel_config.Channel = ADC_CHANNEL_10;
  channel_config.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // Configure Regular Channel
  channel_config.Channel = ADC_CHANNEL_11;
  channel_config.Rank = ADC_REGULAR_RANK_6;
  if (HAL_ADC_ConfigChannel(&adc_, &channel_config) != HAL_OK)
    return false;

  // ADC1 DMA Init
  dma_adc_.Instance = DMA1_Stream2;
  dma_adc_.Init.Request = DMA_REQUEST_ADC1;
  dma_adc_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_adc_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_adc_.Init.MemInc = DMA_MINC_ENABLE;
  dma_adc_.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma_adc_.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma_adc_.Init.Mode = DMA_CIRCULAR;
  dma_adc_.Init.Priority = DMA_PRIORITY_LOW;
  dma_adc_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_adc_) != HAL_OK)
    return false;

  __HAL_LINKDMA(&adc_, DMA_Handle, dma_adc_);

  // Calibrate the ADC
  while(HAL_ADCEx_Calibration_Start(&adc_,
                                    ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK);

  return true;
}

bool CvAdc::InitializeGPIO() {
  GPIO_InitTypeDef gpio_init;

  gpio_init.Pin = SIZE_CV_GPIO_PIN|SHAPE_CV_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_ANALOG;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SIZE_CV_GPIO_PORT, &gpio_init);

  gpio_init.Pin = ASSIGN_CV_GPIO_PIN|DENSITY_CV_GPIO_PIN|PITCH_CV_GPIO_PIN|TIME_CV_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_ANALOG;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ASSIGN_CV_GPIO_PORT, &gpio_init);

  return true;
}

void CvAdc::Convert() {  
  HAL_ADC_Start_DMA(&adc_, (uint32_t*)(&values_[0]), CV_ADC_CHANNEL_LAST);
}

void CvAdc::DeInit() {
  HAL_ADC_Stop(&adc_);
  HAL_ADC_Stop_DMA(&adc_);
}

}  // namespace beads
