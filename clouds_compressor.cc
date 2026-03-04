// =============================================================================
// clouds_compressor.cc
// Main entry point — replaces clouds.cc
// Wires CompressorEngine + UI to Clouds hardware
//
// Hardware notes (STM32F405 @ 168MHz):
//   Audio codec  : WM8731 via I2S, 32000Hz, int16 frames (short l, short r)
//   ADC channels : 10 total (5 pots + 5 CVs, see AdcChannel enum)
//   Gate inputs  : freeze() = Freeze jack, trigger() = Trigger jack
//   Switches     : 3 total (indices 0,1,2)
//   LEDs         : 4x bicolour via 595 shift reg + 1 freeze LED
//   Bootloader   : Preserved at 0x08000000-0x08003FFF
//   Firmware     : Starts at 0x08008000 (large application linker script)
//
// ADC channel mapping (from clouds/drivers/adc.h):
//   0  ADC_POSITION_POTENTIOMETER_CV  → we use as: Threshold + CV
//   1  ADC_DENSITY_POTENTIOMETER_CV   → we use as: Ratio + CV
//   2  ADC_SIZE_POTENTIOMETER         → we use as: Attack
//   3  ADC_SIZE_CV                    → we use as: Attack CV
//   4  ADC_PITCH_POTENTIOMETER        → we use as: Release
//   5  ADC_V_OCT_CV                   → we use as: Release CV
//   6  ADC_BLEND_POTENTIOMETER        → we use as: Input gain
//   7  ADC_BLEND_CV                   → we use as: Bypass CV (gate)
//   8  ADC_TEXTURE_POTENTIOMETER      → we use as: SC HPF Filter
//   9  ADC_TEXTURE_CV                 → we use as: Output gain (make-up)
//
// Switch mapping:
//   0 = Freeze button  → IN (engage/bypass)
//   1 = Button 1       → Shift
//   2 = Button 2       → SC / model cycle
//
// Gate inputs:
//   freeze()  → SC1 jack (sidechain/clock detection via normalling)
//   trigger() → SC2 jack
//
// Codec DMA buffer layout (6 channels):
//   input[i].l  = In L
//   input[i].r  = In R
//   Sidechain jacks are on the second input pair — accessed via
//   rx_dma_buffer offset (see Codec::Fill). For now SC audio is
//   handled via the gate_input jack detection and internal keying.
// =============================================================================

#include "clouds/drivers/codec.h"
#include "clouds/drivers/leds.h"
#include "clouds/drivers/switches.h"
#include "clouds/drivers/adc.h"
#include "clouds/drivers/gate_input.h"
#include "clouds/drivers/system.h"

#include "compressor/compressor_engine.h"
#include "compressor/ui.h"

using namespace compressor;
using namespace clouds;

// ---------------------------------------------------------------------------
// Global instances — static allocation, no heap used
// ---------------------------------------------------------------------------
CompressorEngine engine;
Parameters       params;
UI               ui_handler;
MeterValues      meter;

// Clouds hardware drivers
Codec      codec;
Leds       leds;
Switches   switches;
Adc        adc;
GateInput  gate_input;

// ---------------------------------------------------------------------------
// Scaling constant — int16 codec frames to float -1..+1
// ---------------------------------------------------------------------------
static const float kShortToFloat = 1.0f / 32768.0f;
static const float kFloatToShort = 32767.0f;

// ---------------------------------------------------------------------------
// Audio callback — called from DMA ISR via Codec::Fill()
// Codec::Frame = struct { short l; short r; }
// Stride unpacking already handled by Codec::Fill() before we're called.
//
// Sidechain hardware note:
//   WM8731 is a single stereo codec — one L/R input pair only.
//   Clouds SC jacks are normalled to the main inputs on the PCB.
//   When SC jack is patched: that signal replaces main input on that channel.
//   Jack presence detected via GateInput normalling detection pins.
//   So input[] contains program audio OR sidechain audio depending on patch.
//   We pass it as both program and SC — engine sc_mode controls usage.
// ---------------------------------------------------------------------------
void FillBuffer(Codec::Frame* input, Codec::Frame* output, size_t size) {
  static float in_l[kBlockSize], in_r[kBlockSize];
  static float out_l[kBlockSize], out_r[kBlockSize];

  for (size_t i = 0; i < size; i++) {
    in_l[i] = input[i].l * kShortToFloat;
    in_r[i] = input[i].r * kShortToFloat;
  }

  engine.Process(
    in_l, in_r,
    in_l, in_r,
    ui_handler.clock_trig,
    out_l, out_r,
    &meter,
    params);

  for (size_t i = 0; i < size; i++) {
    float l = out_l[i];
    float r = out_r[i];
    if (l >  1.0f) l =  1.0f;
    if (l < -1.0f) l = -1.0f;
    if (r >  1.0f) r =  1.0f;
    if (r < -1.0f) r = -1.0f;
    output[i].l = static_cast<short>(l * kFloatToShort);
    output[i].r = static_cast<short>(r * kFloatToShort);
  }
}

// ---------------------------------------------------------------------------
// Write metering to LEDs
//
// LED assignment (fixed):
//   led 0 = Left input level   green→red
//   led 1 = Right input level  green→red
//   led 2 = Left GR            green→red (green=no GR)
//   led 3 = Right GR           green→red
//
// set_status(channel, red, green) takes uint8_t 0-255
// We use PWM values for smooth metering
// ---------------------------------------------------------------------------
void WriteLeds() {
  // Helper: convert 0..1 level to red/green PWM values
  // Input LEDs: green at low, yellow in middle, red at top
  auto InputToRG = [](float level, uint8_t& r, uint8_t& g) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    if (level < 0.5f) {
      // Green only
      g = static_cast<uint8_t>(level * 2.0f * 255.0f);
      r = 0;
    } else if (level < 0.85f) {
      // Yellow — both on
      g = 255;
      r = static_cast<uint8_t>((level - 0.5f) / 0.35f * 255.0f);
    } else {
      // Red only
      r = 255;
      g = static_cast<uint8_t>((1.0f - level) / 0.15f * 128.0f);
    }
  };

  // GR LEDs: green=no compression, yellow=moderate, red=heavy
  auto GrToRG = [](float gr, uint8_t& r, uint8_t& g) {
    if (gr < 0.0f) gr = 0.0f;
    if (gr > 1.0f) gr = 1.0f;
    if (gr < 0.15f) {
      // Green — little GR
      g = 255;
      r = 0;
    } else if (gr < 0.5f) {
      // Yellow — moderate GR
      g = 255;
      r = static_cast<uint8_t>((gr - 0.15f) / 0.35f * 255.0f);
    } else {
      // Red — heavy GR
      r = 255;
      g = static_cast<uint8_t>((1.0f - gr) * 255.0f);
    }
  };

  uint8_t r, g;

  // LED 0 — Left input
  InputToRG(meter.input_l, r, g);
  leds.set_status(0, r, g);

  // LED 1 — Right input
  InputToRG(meter.input_r, r, g);
  leds.set_status(1, r, g);

  // LED 2 — Left GR
  GrToRG(meter.gr_l, r, g);
  leds.set_status(2, r, g);

  // LED 3 — Right GR
  GrToRG(meter.gr_r, r, g);
  leds.set_status(3, r, g);

  // IN button LED — freeze LED on Clouds hardware
  leds.set_freeze(params.engaged);

  leds.Write();
}

// ---------------------------------------------------------------------------
// Default interrupt handlers (required by STM32 startup)
// ---------------------------------------------------------------------------
extern "C" {
  void NMI_Handler()         { }
  void HardFault_Handler()   { while (1); }
  void MemManage_Handler()   { while (1); }
  void BusFault_Handler()    { while (1); }
  void UsageFault_Handler()  { while (1); }
  void SVC_Handler()         { }
  void DebugMon_Handler()    { }
  void PendSV_Handler()      { }
}

// ---------------------------------------------------------------------------
// SysTick — fires at ~1kHz, handles UI polling
// ---------------------------------------------------------------------------
extern "C" {
void SysTick_Handler() {
  // Read ADC
  adc.Convert();

  // Read gate inputs (SC jack presence)
  gate_input.Read();

  // Read switches and debounce
  switches.Debounce();

  // Map ADC channels to knobs and CVs
  // Knobs: position, density, size, pitch, blend, texture pots
  // CVs:   position_cv, size_cv, v_oct_cv, blend_cv, texture_cv
  float knob[7];
  knob[0] = adc.float_value(ADC_BLEND_POTENTIOMETER);     // Input gain
  knob[1] = adc.float_value(ADC_POSITION_POTENTIOMETER_CV); // Threshold
  knob[2] = adc.float_value(ADC_DENSITY_POTENTIOMETER_CV);  // Ratio
  knob[3] = adc.float_value(ADC_SIZE_POTENTIOMETER);       // Attack
  knob[4] = adc.float_value(ADC_PITCH_POTENTIOMETER);      // Release
  knob[5] = adc.float_value(ADC_TEXTURE_POTENTIOMETER);    // SC HPF Filter
  knob[6] = adc.float_value(ADC_TEXTURE_CV);               // Output gain

  float cv[6];
  cv[0] = adc.float_value(ADC_BLEND_CV);      // Bypass CV gate
  cv[1] = 0.0f;                                // Ratio CV (no jack — reuse if needed)
  cv[2] = adc.float_value(ADC_SIZE_CV);        // Attack CV
  cv[3] = adc.float_value(ADC_V_OCT_CV);       // Release CV
  cv[4] = 0.0f;                                // VCA L CV
  cv[5] = 0.0f;                                // VCA R CV

  // Buttons
  // Switch 0 = Freeze button = IN
  // Switch 1 = Button left of LEDs = Shift
  // Switch 2 = Button top right = SC/model
  bool btn_in    = switches.pressed(0);
  bool btn_shift = switches.pressed(1);
  bool btn_sc    = switches.pressed(2);

  // Jack detection via gate inputs
  // freeze() = Freeze/SC1 jack, trigger() = Trigger/SC2 jack
  bool jack_sc1 = gate_input.freeze();
  bool jack_sc2 = gate_input.trigger();

  // Block duration in seconds (32 samples at 32kHz)
  static const float kDt = 32.0f / 32000.0f;

  // Process UI
  ui_handler.Process(
    knob, cv,
    btn_in, btn_sc, btn_shift,
    jack_sc1, jack_sc2,
    false, false,   // VCA CV jacks not separately detectable on stock hardware
    meter,
    kDt);

  // Write LEDs
  WriteLeds();
}
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
  System sys;
  sys.Init(true);

  // Init hardware drivers
  adc.Init();
  leds.Init();
  switches.Init();
  gate_input.Init();

  // Init DSP engine
  engine.Init();
  ui_handler.Init(&params);

  // Safe power-up defaults
  params.model        = MODEL_SSL_BUS;
  params.stereo_link  = true;
  params.engaged      = false;
  params.soft_knee    = true;
  params.alt_mode     = false;
  params.sc_mode      = SC_INTERNAL;
  params.clock_period = 0.5f;
  params.vca_cv_l     = 1.0f;
  params.vca_cv_r     = 1.0f;

  // Clouds v1: WM8731 is I2S master, STM32 is slave → mcu_is_master = false
  // This means stride=3 at 32kHz — Codec::Fill() handles the unpacking for us
  if (!codec.Init(false, 32000)) {
    // Codec init failed — flash all LEDs red
    leds.set_status(0, 255, 0);
    leds.set_status(1, 255, 0);
    leds.set_status(2, 255, 0);
    leds.set_status(3, 255, 0);
    leds.Write();
    while (1);
  }

  if (!codec.Start(32, &FillBuffer)) {
    while (1);
  }

  // Start SysTick timer — fires UI poll at 1kHz
  sys.StartTimers();

  // Main loop — audio runs from DMA ISR, UI from SysTick
  // Nothing to do here except keep the processor alive
  while (1) {
    __WFI(); // Wait for interrupt — saves power
  }
}
