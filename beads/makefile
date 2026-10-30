# Copyright 2019 Emilie Gillet.
# 
# Author: Emilie Gillet (emilie.o.gillet@gmail.com)
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
# See http://creativecommons.org/licenses/MIT/ for more information.

# System specifications
F_CRYSTAL      = 8000000L
F_CPU          = 400000000L
SYSCLOCK       = SYSCLK_FREQ_400MHz
FAMILY         = h7xx
# USB            = enabled

APPLICATION_LARGE = TRUE
BOOTLOADER        = beads_bootloader

# Prefered upload command
# UPLOAD_COMMAND  = upload_combo_jtag_erase_first_sector_and_settings
UPLOAD_COMMAND  = upload_combo_jtag_erase_first_sector

# Packages to build
TARGET         = beads
PACKAGES       = beads \
		beads/drivers \
		beads/dsp \
		stmlib/dsp \
		stmlib/utils \
		stmlib/ui \
		stmlib/system
RESOURCES      = beads/resources

# Also tested with latest gcc version (8.3.1). Improvement of code size, no
# improvement of CPU use, much longer compile time.
TOOLCHAIN_PATH ?= /usr/local/arm-4.9/
OPENOCD_BIN_PATH ?= /usr/local/openocd/0.10.0-13/bin/
OPENOCD_SCRIPT_PATH ?= /usr/local/openocd/0.10.0-13/scripts/
GOLDEN_FIRMWARE = beads/hardware_design/golden_firmware/beads_golden_bootloader_combo.bin

include stmlib/makefile.inc

# Rule for building the firmware update file
wav:  $(TARGET_BIN)
	python stm_audio_bootloader/qpsk/encoder.py \
		-t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 \
		$(TARGET_BIN)

# Rule for dumping the settings block
image_settings:
	$(BASE_JTAG_CMD) \
		-c "dump_image beads_settings.bin 0x081e0000 0x020000" \
		$(POSTLUDE)

# Rule for dumping the settings block
erase_settings:
	$(BASE_JTAG_CMD) \
		-c "flash erase_address 0x081e0000 0x20000" \
		$(POSTLUDE)

write_settings: $(COMBO_BIN)
	$(BASE_JTAG_CMD) \
				-c "flash write_image erase beads_settings_ok.bin 0x081e0000" \
				-c "verify_image beads_settings_ok.bin 0x081e0000" \
				$(POSTLUDE)

# Rule for dumping the buffer save block
buffer_image_jtag:
	$(BASE_JTAG_CMD) \
		-c "dump_image beads_buffer.bin 0x08100000 0x0c0000" \
		$(POSTLUDE)

# Rule for dumping a whole image
image_all:
	$(BASE_JTAG_CMD) \
		-c "dump_image beads_full.bin 0x08000000 0x200000" \
		$(POSTLUDE)

upload_combo_jtag_erase_first_sector_and_settings: $(COMBO_BIN)
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase $(COMBO_BIN) 0x08000000" \
				-c "verify_image $(COMBO_BIN) 0x08000000" \
				$(POSTLUDE)

# Factory write
factory_write:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase $(GOLDEN_FIRMWARE) 0x08000000" \
				-c "verify_image $(GOLDEN_FIRMWARE) 0x08000000" \
				$(POSTLUDE)

factory_write_keep_calibration:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase $(GOLDEN_FIRMWARE) 0x08000000" \
				-c "verify_image $(GOLDEN_FIRMWARE) 0x08000000" \
				$(POSTLUDE)

# Factory writes for older golden images
factory_write_batch_1:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_1.bin 0x08000000" \
				-c "verify_image beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_1.bin 0x08000000" \
				$(POSTLUDE)

factory_write_batch_2_aborted:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_aborted.bin 0x08000000" \
				-c "verify_image beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_aborted.bin 0x08000000" \
				$(POSTLUDE)

factory_write_batch_2_candidate:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate.bin 0x08000000" \
				-c "verify_image beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate.bin 0x08000000" \
				$(POSTLUDE)

factory_write_batch_2_candidate_safe:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate_safe.bin 0x08000000" \
				-c "verify_image beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate_safe.bin 0x08000000" \
				$(POSTLUDE)

factory_write_batch_2_candidate_eraser:
	$(BASE_JTAG_CMD) \
				-c "flash erase_address 0x08000000 0x20000" \
				-c "flash erase_address 0x081e0000 0x20000" \
				-c "reset halt" \
				-c "flash write_image erase beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate_eraser.bin 0x08000000" \
				-c "verify_image beads/hardware_design/golden_firmware/beads_golden_bootloader_combo_batch_2_candidate_eraser.bin 0x08000000" \
				$(POSTLUDE)

