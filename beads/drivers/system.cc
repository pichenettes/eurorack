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
// System-level initialization.

#include "beads/drivers/system.h"

#include <stm32h7xx_hal.h>

namespace beads {

bool System::Init(bool application) {
  // Set vector table according to AN5044
  if (application)
    SCB->VTOR = 0x08000000U | (0x20000U & 0x1fffff80U);

  // Treat denormals as 0 (shows no practical performance improvement).
  uint32_t fpscr_value = __get_FPSCR();
  const uint32_t flush_to_zero_bit = 1U << 24;
  __set_FPSCR(fpscr_value | flush_to_zero_bit);  
  
  __HAL_RCC_D2SRAM1_CLK_ENABLE();
  __HAL_RCC_D2SRAM2_CLK_ENABLE();
  __HAL_RCC_D2SRAM3_CLK_ENABLE();
  __HAL_RCC_BKPRAM_CLK_ENABLE();
  
  __HAL_RCC_BKPRAM_CLKAM_ENABLE();
  __HAL_RCC_D3SRAM1_CLKAM_ENABLE();
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_ADC3_CLK_ENABLE();
  
  InitializeMPU();
  
  SCB_EnableICache();
  SCB_EnableDCache();

  HAL_Init();

  return InitializeDefaultHandlers() && \
      InitializeClocks();
}

bool System::InitializeDefaultHandlers() {
  HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
  HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
  HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
  HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
  HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
  
  return true;
}

void System::InitializeMPU() {
  HAL_MPU_Disable();

  // RAM_D2_DMA (SRAM3) region
  MPU_Region_InitTypeDef mpu_init;
  mpu_init.Enable = MPU_REGION_ENABLE;
  mpu_init.Number = MPU_REGION_NUMBER0;
  mpu_init.BaseAddress = 0x30040000;		
  mpu_init.Size = MPU_REGION_SIZE_32KB;
  mpu_init.SubRegionDisable = 0x0;
  mpu_init.TypeExtField = MPU_TEX_LEVEL0;
  mpu_init.AccessPermission = MPU_REGION_FULL_ACCESS;
  mpu_init.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  mpu_init.IsShareable = MPU_ACCESS_SHAREABLE;
  mpu_init.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  mpu_init.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&mpu_init);

  // Enables the MPU (stays in privileged mode)
  HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
}

bool System::InitializeClocks() {
  RCC_OscInitTypeDef rcc_osc_init;
  RCC_ClkInitTypeDef rcc_clk_init;
  RCC_PeriphCLKInitTypeDef periph_clk_init;

  // Supply configuration update enable
  MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);

  // Configure the main internal regulator output voltage
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while ((PWR->D3CR & (PWR_D3CR_VOSRDY)) != PWR_D3CR_VOSRDY);

  // Macro to configure the PLL clock source
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);

  // Initializes the CPU, AHB and APB busses clocks
  rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  rcc_osc_init.HSEState = RCC_HSE_ON;
  rcc_osc_init.PLL.PLLState = RCC_PLL_ON;
  rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  rcc_osc_init.PLL.PLLM = 1;
  rcc_osc_init.PLL.PLLN = 120;
  rcc_osc_init.PLL.PLLP = 2;
  rcc_osc_init.PLL.PLLQ = 2;
  rcc_osc_init.PLL.PLLR = 2;
  rcc_osc_init.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  rcc_osc_init.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  rcc_osc_init.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK)
    return false;

  // Initializes the CPU, AHB and APB busses clocks
  rcc_clk_init.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  rcc_clk_init.SYSCLKDivider = RCC_SYSCLK_DIV1;
  rcc_clk_init.AHBCLKDivider = RCC_HCLK_DIV2;
  rcc_clk_init.APB3CLKDivider = RCC_APB3_DIV2;
  rcc_clk_init.APB1CLKDivider = RCC_APB1_DIV2;
  rcc_clk_init.APB2CLKDivider = RCC_APB2_DIV2;
  rcc_clk_init.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_2) != HAL_OK)
    return false;

  periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_SAI1
                              |RCC_PERIPHCLK_ADC;
  periph_clk_init.PLL2.PLL2M = 1;
  // SAI Clock = 172 MHz.
  // Clock ticks per sample = 256
  // -> 172M / 14 / 256 = 47.991kHz
  // -> 172M / 21 / 256 = 31.994kHz
  // -> 172M / 42 / 256 = 15.997kHz
  periph_clk_init.PLL2.PLL2N = 43;
  periph_clk_init.PLL2.PLL2P = 2;
  periph_clk_init.PLL2.PLL2Q = 2;
  periph_clk_init.PLL2.PLL2R = 2;
  periph_clk_init.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  periph_clk_init.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  periph_clk_init.PLL2.PLL2FRACN = 0;
  periph_clk_init.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
  periph_clk_init.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
  periph_clk_init.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    return false;

  HAL_SYSTICK_Config(SystemCoreClock / 1000);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  
  return true;
}

} // namespace beads


extern "C" {

void NMI_Handler() {
  while (1);
}

void HardFault_Handler() {
  while (1);
}

void MemManage_Handler() {
  while (1);
}

void BusFault_Handler() {
  while (1);
}

void UsageFault_Handler() {
  while (1);
}

void SVC_Handler() {
  while (1);
}

void DebugMon_Handler() {
  while (1);
}

void PendSV_Handler() {
  while (1);
}

void MemoryManagement_Handler() {
  while (1);
}

}