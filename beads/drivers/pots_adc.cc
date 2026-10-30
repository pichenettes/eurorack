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
// Driver for ADC1 and ADC2 - used for scanning pots.

#include "beads/drivers/pots_adc.h"
#include <cstring>
#include <algorithm>

namespace beads {

using namespace std;

const uint32_t kMaximumScanFrequency = 5000;

const uint32_t kPinResetMask = (POT_MUX_S0_GPIO_PIN | POT_MUX_S1_GPIO_PIN | POT_MUX_S2_GPIO_PIN) << 16;

/* static */
PotsAdc* PotsAdc::instance_;

/* static */
uint16_t PotsAdc::values_[POTS_ADC_CHANNEL_LAST];

/* static */
uint32_t PotsAdc::mux_pin_map_[];

bool PotsAdc::Init() {
  muxed_channel_count_ = 0;
  
  mux_pin_map_[0] = kPinResetMask | 0;
  mux_pin_map_[1] = kPinResetMask | POT_MUX_S0_GPIO_PIN;
  mux_pin_map_[2] = kPinResetMask | POT_MUX_S1_GPIO_PIN;
  mux_pin_map_[3] = kPinResetMask | POT_MUX_S1_GPIO_PIN | POT_MUX_S0_GPIO_PIN;
  mux_pin_map_[4] = kPinResetMask | POT_MUX_S2_GPIO_PIN;
  mux_pin_map_[5] = kPinResetMask | POT_MUX_S2_GPIO_PIN | POT_MUX_S0_GPIO_PIN;
  mux_pin_map_[6] = kPinResetMask | POT_MUX_S2_GPIO_PIN | POT_MUX_S1_GPIO_PIN;
  
  bool success = InitializeADC2() && \
      InitializeADC3() && \
      InitializeGPIO() && \
      InitializeTimer();
  if (success) {
    instance_ = this;
  }
  return success;
}

bool PotsAdc::InitializeADC2() {
  bool old_silicon = HAL_GetREVID() <= REV_ID_Y;
  
  adc2_.Instance = ADC2;
  adc2_.Init.ClockPrescaler = old_silicon
      ? ADC_CLOCK_ASYNC_DIV8
      : ADC_CLOCK_ASYNC_DIV4;
  adc2_.Init.Resolution = ADC_RESOLUTION_16B;
  adc2_.Init.ScanConvMode = ADC_SCAN_ENABLE;
  adc2_.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc2_.Init.LowPowerAutoWait = DISABLE;
  adc2_.Init.ContinuousConvMode = DISABLE;
  adc2_.Init.NbrOfConversion = kNumDirectlyScannedPots;
  adc2_.Init.DiscontinuousConvMode = ENABLE;
  adc2_.Init.NbrOfDiscConversion = kNumDirectlyScannedPots;
  adc2_.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc2_.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc2_.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  adc2_.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  adc2_.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  adc2_.Init.OversamplingMode = ENABLE;
  adc2_.Init.Oversampling.Ratio = 8;
  adc2_.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_3;
  adc2_.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  adc2_.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  
  if (HAL_ADC_Init(&adc2_) != HAL_OK)
    return false;

  // Configure regular channel.
  ADC_ChannelConfTypeDef channel_config;
  channel_config.Channel = ADC_CHANNEL_18;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  if (HAL_ADC_ConfigChannel(&adc2_, &channel_config) != HAL_OK)
    return false;

  // Configure regular channel.
  channel_config.Channel = ADC_CHANNEL_19;
  channel_config.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&adc2_, &channel_config) != HAL_OK)
    return false;

  // Configure regular channel.
  channel_config.Channel = ADC_CHANNEL_3;
  channel_config.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&adc2_, &channel_config) != HAL_OK)
    return false;

  // Configure regular channel.
  channel_config.Channel = ADC_CHANNEL_7;
  channel_config.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&adc2_, &channel_config) != HAL_OK)
    return false;

  // Configure regular channel.
  channel_config.Channel = ADC_CHANNEL_4;
  channel_config.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&adc2_, &channel_config) != HAL_OK)
    return false;

  // ADC2 DMA Init
  dma_adc2_.Instance = DMA1_Stream3;
  dma_adc2_.Init.Request = DMA_REQUEST_ADC2;
  dma_adc2_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_adc2_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_adc2_.Init.MemInc = DMA_MINC_ENABLE;
  dma_adc2_.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma_adc2_.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma_adc2_.Init.Mode = DMA_CIRCULAR;
  dma_adc2_.Init.Priority = DMA_PRIORITY_LOW;
  dma_adc2_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_adc2_) != HAL_OK)
    return false;

  __HAL_LINKDMA(&adc2_, DMA_Handle, dma_adc2_);

  // Calibrate the ADC
  while (HAL_ADCEx_Calibration_Start(&adc2_,
                                    ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK);

  return true;
}

bool PotsAdc::InitializeADC3() {
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_OPEN);
  
  bool old_silicon = HAL_GetREVID() <= REV_ID_Y;
  
  adc3_.Instance = ADC3;
  adc3_.Init.ClockPrescaler = old_silicon
      ? ADC_CLOCK_ASYNC_DIV8
      : ADC_CLOCK_ASYNC_DIV4;
  adc3_.Init.Resolution = ADC_RESOLUTION_16B;
  adc3_.Init.ScanConvMode = ADC_SCAN_DISABLE;
  adc3_.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc3_.Init.LowPowerAutoWait = DISABLE;
  adc3_.Init.ContinuousConvMode = DISABLE;
  adc3_.Init.NbrOfConversion = 1;
  adc3_.Init.DiscontinuousConvMode = ENABLE;
  adc3_.Init.NbrOfDiscConversion = 1;
  adc3_.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
  adc3_.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  adc3_.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  adc3_.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  adc3_.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  adc3_.Init.OversamplingMode = ENABLE;
  adc3_.Init.Oversampling.Ratio = 4;
  adc3_.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_2;
  adc3_.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  adc3_.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&adc3_) != HAL_OK)
    return false;

  // Configure Regular Channel
  ADC_ChannelConfTypeDef channel_config;
  channel_config.Channel = ADC_CHANNEL_1;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  if (HAL_ADC_ConfigChannel(&adc3_, &channel_config) != HAL_OK)
    return false;
  
  // ADC3 DMA Init
  dma_adc3_.Instance = DMA1_Stream4;
  dma_adc3_.Init.Request = DMA_REQUEST_ADC3;
  dma_adc3_.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_adc3_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_adc3_.Init.MemInc = DMA_MINC_ENABLE;
  dma_adc3_.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma_adc3_.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma_adc3_.Init.Mode = DMA_CIRCULAR;
  dma_adc3_.Init.Priority = DMA_PRIORITY_LOW;
  dma_adc3_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_adc3_) != HAL_OK)
    return false;

  __HAL_LINKDMA(&adc3_,DMA_Handle,dma_adc3_);

  while (HAL_ADCEx_Calibration_Start(&adc3_,
                                     ADC_CALIB_OFFSET_LINEARITY,
                                     ADC_SINGLE_ENDED) != HAL_OK);

  return true;
}

bool PotsAdc::InitializeTimer() {
  const uint32_t timer_period = ((HAL_RCC_GetPCLK2Freq() * 2) / 
      (kNumMuxedPots * kMaximumScanFrequency)) - 1;
  __HAL_RCC_TIM8_CLK_ENABLE();

  TIM_ClockConfigTypeDef clock_source_config;
  clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  
  TIM_MasterConfigTypeDef master_config;
  master_config.MasterOutputTrigger = TIM_TRGO_OC1;
  master_config.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  
  TIM_BreakDeadTimeConfigTypeDef break_dead_time_config;
  break_dead_time_config.OffStateRunMode = TIM_OSSR_DISABLE;
  break_dead_time_config.OffStateIDLEMode = TIM_OSSI_DISABLE;
  break_dead_time_config.LockLevel = TIM_LOCKLEVEL_OFF;
  break_dead_time_config.DeadTime = 0;
  break_dead_time_config.BreakState = TIM_BREAK_DISABLE;
  break_dead_time_config.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  break_dead_time_config.BreakFilter = 0;
  break_dead_time_config.Break2State = TIM_BREAK2_DISABLE;
  break_dead_time_config.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  break_dead_time_config.Break2Filter = 0;
  break_dead_time_config.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  
  tim8_.Instance = TIM8;
  tim8_.Init.Prescaler = 0;
  tim8_.Init.CounterMode = TIM_COUNTERMODE_UP;
  tim8_.Init.Period = timer_period;
  tim8_.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim8_.Init.RepetitionCounter = 0;
  tim8_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&tim8_) != HAL_OK)
    return false;

  if (HAL_TIM_ConfigClockSource(&tim8_, &clock_source_config) != HAL_OK)
    return false;

  if (HAL_TIMEx_MasterConfigSynchronization(&tim8_, &master_config) != HAL_OK)
    return false;
  
  if (HAL_TIMEx_ConfigBreakDeadTime(&tim8_, &break_dead_time_config) != HAL_OK)
    return false;

  HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM8_UP_TIM13_IRQn);

  memset(&dma_tim8_, 0, sizeof(DMA_HandleTypeDef));
  dma_tim8_.Instance = DMA1_Stream5;
  dma_tim8_.Init.Request = DMA_REQUEST_TIM8_UP;
  dma_tim8_.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dma_tim8_.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_tim8_.Init.MemInc = DMA_MINC_ENABLE;
  dma_tim8_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_tim8_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma_tim8_.Init.Mode = DMA_CIRCULAR;
  dma_tim8_.Init.Priority = DMA_PRIORITY_LOW;
  dma_tim8_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_tim8_) != HAL_OK)
    return false;
  
  __HAL_LINKDMA(&tim8_, hdma[TIM_DMA_ID_UPDATE], dma_tim8_);
  __HAL_TIM_ENABLE_DMA(&tim8_, TIM_DMA_UPDATE);
  
  return true;
}

void PotsAdc::StartMuxedPotsConversion() {
  HAL_ADC_Start_DMA(
      &adc3_,
      (uint32_t*)(&values_[0]),
      kNumMuxedPots);

  HAL_DMA_Start(
      &dma_tim8_,
      (uint32_t)(&mux_pin_map_[0]),
      (uint32_t)(&POT_MUX_S0_GPIO_PORT->BSRR),
      kNumMuxedPots);
}

bool PotsAdc::InitializeGPIO() {
  GPIO_InitTypeDef gpio_init;

  gpio_init.Pin = TIME_POT_GPIO_PIN | DENSITY_POT_GPIO_PIN | \
      PITCH_POT_GPIO_PIN | SIZE_POT_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_ANALOG;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TIME_POT_GPIO_PORT, &gpio_init);

  gpio_init.Pin = SHAPE_POT_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_ANALOG;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SHAPE_POT_GPIO_PORT, &gpio_init);

  gpio_init.Pin = POT_MUX_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_ANALOG;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(POT_MUX_GPIO_PORT, &gpio_init);

  gpio_init.Pin = POT_MUX_S0_GPIO_PIN | POT_MUX_S1_GPIO_PIN | \
      POT_MUX_S2_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(POT_MUX_S0_GPIO_PORT, &gpio_init);

  return true;
}

void PotsAdc::DeInit() {
  muxed_channel_count_ = 0;
  HAL_ADC_Stop_DMA(&adc2_);
  HAL_ADC_Stop_DMA(&adc3_);
  HAL_TIM_Base_Stop_IT(&tim8_);
}

void PotsAdc::Convert() {
  HAL_ADC_Start_DMA(
      &adc2_,
      (uint32_t*)(&values_[POTS_ADC_CHANNEL_FIRST_DIRECT_CHANNEL]),
      kNumDirectlyScannedPots);
  
  muxed_channel_count_ = kNumMuxedPots - 1;
  HAL_TIM_Base_Start_IT(&tim8_);
}

void PotsAdc::Cycle() {
  if (__HAL_TIM_GET_FLAG(&tim8_, TIM_FLAG_UPDATE) != RESET) {
    __HAL_TIM_CLEAR_IT(&tim8_, TIM_IT_UPDATE);
    if (!muxed_channel_count_) {
      HAL_TIM_Base_Stop_IT(&tim8_);
    } else {
      --muxed_channel_count_;
    }
  }
}

}  // namespace beads

extern "C" {

void TIM8_UP_TIM13_IRQHandler() {
  beads::PotsAdc* pots_adc = beads::PotsAdc::GetInstance();
  if (pots_adc) {
    pots_adc->Cycle();
  }
}

}