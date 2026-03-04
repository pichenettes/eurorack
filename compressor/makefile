# =============================================================================
# Makefile — Clouds Stereo Compressor Firmware
# =============================================================================

TOOLCHAIN_PATH ?= /opt/homebrew/bin/
TOOLCHAIN_PREFIX = arm-none-eabi-

CC      = $(TOOLCHAIN_PATH)$(TOOLCHAIN_PREFIX)gcc
CXX     = $(TOOLCHAIN_PATH)$(TOOLCHAIN_PREFIX)g++
OBJCOPY = $(TOOLCHAIN_PATH)$(TOOLCHAIN_PREFIX)objcopy
AR      = $(TOOLCHAIN_PATH)$(TOOLCHAIN_PREFIX)ar
SIZE    = $(TOOLCHAIN_PATH)$(TOOLCHAIN_PREFIX)size

TARGET    = clouds_compressor
BUILD_DIR = build/compressor

MCU_FLAGS = \
  -mcpu=cortex-m4 \
  -mthumb \
  -mfpu=fpv4-sp-d16 \
  -mfloat-abi=hard \
  -DSTM32F4XX \
  -DSTM32F40_41xxx \
  -DUSE_STDPERIPH_DRIVER \
  -DGCC_ARMCM4 \
  -DARM_MATH_CM4 \
  -D__FPU_PRESENT \
  -DAPPLICATION \
  -DF_CPU=168000000L \
  -DF_CRYSTAL=8000000L \
  -DHSE_VALUE=8000000L \
  -DSYSCLK_FREQ_168MHz=168000000L

OPT = -O2

INCLUDES = \
  -I. \
  -Istmlib \
  -Istmlib/third_party/STM \
  -Istmlib/third_party/STM/CMSIS/CM3_f4xx \
  -Istmlib/third_party/STM/CMSIS/CM3_f4xx/startup/gcc \
  -Istmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/inc

CFLAGS = \
  $(MCU_FLAGS) \
  $(OPT) \
  $(INCLUDES) \
  -std=c11 \
  -Wall \
  -Wno-register \
  -Wno-deprecated \
  -ffast-math \
  -fdata-sections \
  -ffunction-sections \
  -fno-exceptions

CXXFLAGS = \
  $(MCU_FLAGS) \
  $(OPT) \
  $(INCLUDES) \
  -std=c++11 \
  -Wall \
  -Wno-register \
  -Wno-deprecated \
  -ffast-math \
  -fdata-sections \
  -ffunction-sections \
  -fno-exceptions \
  -fno-rtti

# Use the same linker script as stock Clouds
LDSCRIPT = stmlib/linker_scripts/stm32f4xx_flash_application_large.ld

LDFLAGS = \
  -T$(LDSCRIPT) \
  $(MCU_FLAGS) \
  -Wl,--gc-sections \
  -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
  --specs=nano.specs \
  -lm

# Compressor source files
SOURCES_CXX = \
  compressor/compressor_engine.cc \
  clouds/drivers/codec.cc \
  clouds/drivers/leds.cc \
  clouds/drivers/switches.cc \
  clouds/drivers/adc.cc \
  clouds/drivers/gate_input.cc \
  clouds/drivers/system.cc \
  clouds_compressor.cc

# STM32 stdlib — match exactly what clouds/makefile uses
SOURCES_C = \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_spi.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_dma.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_i2c.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_adc.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_tim.c \
  stmlib/third_party/STM/STM32F4xx_StdPeriph_Driver/src/misc.c \
  stmlib/third_party/STM/CMSIS/CM3_f4xx/system_stm32f4xx.c

SOURCES_ASM = \
  stmlib/third_party/STM/CMSIS/CM3_f4xx/startup/gcc/startup_stm32f4xx.s

OBJECTS_CXX = $(SOURCES_CXX:%.cc=$(BUILD_DIR)/%.o)
OBJECTS_C   = $(SOURCES_C:%.c=$(BUILD_DIR)/%.o)
OBJECTS_ASM = $(SOURCES_ASM:%.s=$(BUILD_DIR)/%.o)
OBJECTS     = $(OBJECTS_CXX) $(OBJECTS_C) $(OBJECTS_ASM)

# =============================================================================
all: $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo ">>> Build complete: $@"
	@echo ">>> Flash with: dfu-util -a 0 -s 0x08008000 -D $@"

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	@echo ">>> Build complete: $@"

$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	dfu-util -a 0 -s 0x08008000:leave -D $<

flash_stlink: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08008000

.PHONY: all clean flash flash_stlink
