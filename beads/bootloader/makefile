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

# APPLICATION_LARGE = TRUE
BOOTLOADER        = beads_bootloader

# Prefered upload command
UPLOAD_COMMAND  = upload_jtag_erase_first_sector

# Packages to build
TARGET         = beads_bootloader
PACKAGES       = beads/bootloader \
		beads/drivers \
		stm_audio_bootloader/qpsk \
		stmlib/dsp \
		stmlib/utils \
		stmlib/system
RESOURCES      = beads/resources

TOOLCHAIN_PATH ?= /usr/local/arm-4.9/
OPENOCD_BIN_PATH ?= /usr/local/openocd/0.10.0-13/bin/
OPENOCD_SCRIPT_PATH ?= /usr/local/openocd/0.10.0-13/scripts/

include stmlib/makefile.inc
