# Clouds Stereo Compressor Firmware

A full firmware replacement for the Mutable Instruments Clouds (STM32F405),
turning it into a 4-model stereo compressor/limiter with CV control and
sidechain/autopump functionality.

The original Mutable bootloader is fully preserved.

---

## Hardware

**Module:** Mutable Instruments Clouds  
**MCU:** STM32F405RGT6 @ 168MHz  
**Codec:** WM8731, 48kHz, stereo  
**Flash:** 1MB (bootloader at 0x08000000, firmware at 0x08004000)

---

## Controls

### Knobs
| # | Label | Range |
|---|-------|-------|
| 1 | Input | -12 to +12 dB |
| 2 | Threshold | -40 to 0 dBFS |
| 3 | Ratio | 1:1 to ∞:1 |
| 4 | Attack | 0.1ms to 300ms |
| 5 | Release | 10ms to 3s |
| 6 | Filter | SC HPF 20Hz to 500Hz |
| 7 | Output | 0 to +20 dB make-up gain |

### Buttons
| Button | Action | Shift + Action |
|--------|--------|----------------|
| IN (Freeze) | Toggle compressor in/out | — |
| SC | Toggle stereo link | Cycle compressor model |
| Shift | Hold for secondary | — |

### LEDs (4x Red/Green, fixed assignment)
| LED | Channel | Parameter |
|-----|---------|-----------|
| 1 | Left | Input level |
| 2 | Right | Input level |
| 3 | Left | Gain Reduction |
| 4 | Right | Gain Reduction |

**Colour coding:**
- Green = nominal / no GR
- Yellow (both) = moderate / warning
- Red = hot / heavy GR

**IN button LED:** Lit = compressor active

### Jacks
| Jack | Function |
|------|----------|
| In 1 | Audio input Left |
| In 2 | Audio input Right |
| SC1 | Sidechain input Left (or mono SC) |
| SC2 | Clock input (autopump) |
| Out 1 | Audio output Left |
| Out 2 | Audio output Right |
| CV Bypass | Gate — bypasses compressor at 5V |
| CV Ratio | CV modulation over Ratio |
| CV Attack | CV modulation over Attack |
| CV Release | CV modulation over Release |
| VCA L | Post-compressor VCA Left (0V=off, 5V=unity) |
| VCA R | Post-compressor VCA Right |

---

## Sidechain Jack Logic

| SC1 | SC2 | Mode |
|-----|-----|------|
| Empty | Empty | Internal (keys from program audio) |
| Patched | Empty | External mono sidechain → both channels |
| Empty | Patched | Clock/autopump mode |
| Patched | Patched | Dual mono independent sidechain |

---

## Compressor Models

### SSL Bus
- RMS detection
- Program-dependent release (speeds up under sustained compression)
- Soft knee (6dB) — toggle with Shift held
- Classic 4000G bus compressor character

### 1176
- Peak detection
- Discrete ratio steps: 4:1 / 8:1 / 12:1 / 20:1
- Very fast FET ballistics
- **Shift held = All-buttons mode** (infamous British mode, extreme limiting)

### Opto (LA-2A style)
- RMS detection with frequency-weighted response
- Program-dependent dual-time-constant release
- Photocell-accurate slow attack character
- **Shift held = Tube colour** (LA-2A vs LA-3A character)

### Distressor
- Hybrid RMS + peak detection
- Tight 2dB knee
- **Shift held = Dist 3** (3rd harmonic saturation vs default Dist 2)

---

## Autopump / Clock Mode

Patch a clock signal (gate/trigger) into SC2.

- The clock drives a synthetic transient envelope into the detector
- Attack knob controls how fast the duck hits
- Release knob controls pump feel (the main character knob)
- Threshold controls pump depth
- Filter HPF keeps low-frequency content from muddying the pump

**Tap tempo:** With SC2 unpatched, tap the Shift button in time.
Averages last 4 taps, times out after 2 seconds.

---

## Signal Path

```
In1/In2
  │
  ▼
Input Gain (knob 1)
  │
  ├──→ Sidechain HPF (knob 6) → Detector → Gain Computer
  │              ↑
  │         SC1/SC2 jacks (or clock)
  │
  ▼
VCA (gain reduction applied)
  │
  ▼
Make-up Gain (knob 7)
  │
  ▼
Post VCA (VCA L/R CV jacks)
  │
  ▼
Out1/Out2
```

---

## Building

### Requirements
- `arm-none-eabi-gcc` (version 9+ recommended)
- Mutable Instruments eurorack repository (forked)

### Setup
```bash
# Fork https://github.com/pichenettes/eurorack on GitHub, then:
git clone https://github.com/YOUR_USERNAME/eurorack.git
cd eurorack
git checkout -b stereo-compressor

# Copy compressor/ folder and makefile into eurorack root
# Then build:
make -f compressor/makefile
```

### Output files
```
build/clouds_compressor.bin   ← flash this
build/clouds_compressor.hex   ← alternative
build/clouds_compressor.map   ← symbol map for debugging
```

### Flashing

**Via bootloader (no programmer needed):**
1. Hold the Clouds button while powering on → enters DFU mode
2. Run:
```bash
dfu-util -a 0 -s 0x08004000:leave -D build/clouds_compressor.bin
```

**Via ST-Link:**
```bash
st-flash write build/clouds_compressor.bin 0x08004000
```

---

## Power-up Defaults

| Parameter | Default |
|-----------|---------|
| Compressor | **OFF** (IN button unlit) |
| Model | SSL Bus |
| Stereo link | Linked |
| Soft knee | On |
| VCA | Unity (transparent) |

---

## File Structure

```
compressor/
├── compressor_engine.h     Core DSP — parameters, state, engine class
├── compressor_engine.cc    DSP implementation — all 4 models
├── ui.h                    UI — buttons, LEDs, tap tempo, CV scaling
├── clouds_compressor.cc    Main entry point — hardware wiring
└── makefile                Build system
```
