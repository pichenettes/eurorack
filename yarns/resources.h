// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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
// Resources definitions.
//
// Automatically generated with:
// make resources


#ifndef YARNS_RESOURCES_H_
#define YARNS_RESOURCES_H_


#include "stmlib/stmlib.h"



namespace yarns {

typedef uint8_t ResourceId;

extern const char* string_table[];

extern const uint16_t* lookup_table_table[];

extern const int16_t* lookup_table_signed_table[];

extern const int16_t* waveform_table[];

extern const uint32_t* lookup_table_32_table[];

extern const uint16_t* char_table[];

extern const uint16_t lut_env_expo[];
extern const uint16_t lut_arpeggiator_patterns[];
extern const uint16_t lut_consonance[];
extern const int16_t lut_scale_pythagorean[];
extern const int16_t lut_scale_1_4_eb[];
extern const int16_t lut_scale_1_4_e[];
extern const int16_t lut_scale_1_4_ea[];
extern const int16_t lut_scale_bhairav[];
extern const int16_t lut_scale_gunakri[];
extern const int16_t lut_scale_marwa[];
extern const int16_t lut_scale_shree[];
extern const int16_t lut_scale_purvi[];
extern const int16_t lut_scale_bilawal[];
extern const int16_t lut_scale_yaman[];
extern const int16_t lut_scale_kafi[];
extern const int16_t lut_scale_bhimpalasree[];
extern const int16_t lut_scale_darbari[];
extern const int16_t lut_scale_rageshree[];
extern const int16_t lut_scale_khamaj[];
extern const int16_t lut_scale_mimal[];
extern const int16_t lut_scale_parameshwari[];
extern const int16_t lut_scale_rangeshwari[];
extern const int16_t lut_scale_gangeshwari[];
extern const int16_t lut_scale_kameshwari[];
extern const int16_t lut_scale_pa__kafi[];
extern const int16_t lut_scale_natbhairav[];
extern const int16_t lut_scale_m_kauns[];
extern const int16_t lut_scale_bairagi[];
extern const int16_t lut_scale_b_todi[];
extern const int16_t lut_scale_chandradeep[];
extern const int16_t lut_scale_kaushik_todi[];
extern const int16_t lut_scale_jogeshwari[];
extern const int16_t wav_exponential[];
extern const int16_t wav_ring[];
extern const int16_t wav_steps[];
extern const int16_t wav_noise[];
extern const int16_t wav_sine[];
extern const int16_t wav_bandlimited_pulse_0[];
extern const int16_t wav_bandlimited_pulse_1[];
extern const int16_t wav_bandlimited_pulse_2[];
extern const int16_t wav_bandlimited_pulse_3[];
extern const int16_t wav_bandlimited_pulse_4[];
extern const int16_t wav_bandlimited_pulse_5[];
extern const int16_t wav_bandlimited_square_0[];
extern const int16_t wav_bandlimited_square_1[];
extern const int16_t wav_bandlimited_square_2[];
extern const int16_t wav_bandlimited_square_3[];
extern const int16_t wav_bandlimited_square_4[];
extern const int16_t wav_bandlimited_square_5[];
extern const int16_t wav_bandlimited_saw_0[];
extern const int16_t wav_bandlimited_saw_1[];
extern const int16_t wav_bandlimited_saw_2[];
extern const int16_t wav_bandlimited_saw_3[];
extern const int16_t wav_bandlimited_saw_4[];
extern const int16_t wav_bandlimited_saw_5[];
extern const int16_t wav_bandlimited_triangle_0[];
extern const int16_t wav_bandlimited_triangle_3[];
extern const int16_t wav_bandlimited_triangle_4[];
extern const int16_t wav_bandlimited_triangle_5[];
extern const uint32_t lut_lfo_increments[];
extern const uint32_t lut_portamento_increments[];
extern const uint32_t lut_oscillator_increments[];
extern const uint32_t lut_euclidean[];
extern const uint16_t chr_characters[];
#define STR_DUMMY 0  // dummy
#define LUT_ENV_EXPO 0
#define LUT_ENV_EXPO_SIZE 257
#define LUT_ARPEGGIATOR_PATTERNS 1
#define LUT_ARPEGGIATOR_PATTERNS_SIZE 22
#define LUT_CONSONANCE 2
#define LUT_CONSONANCE_SIZE 1536
#define LUT_SCALE_PYTHAGOREAN 0
#define LUT_SCALE_PYTHAGOREAN_SIZE 12
#define LUT_SCALE_1_4_EB 1
#define LUT_SCALE_1_4_EB_SIZE 12
#define LUT_SCALE_1_4_E 2
#define LUT_SCALE_1_4_E_SIZE 12
#define LUT_SCALE_1_4_EA 3
#define LUT_SCALE_1_4_EA_SIZE 12
#define LUT_SCALE_BHAIRAV 4
#define LUT_SCALE_BHAIRAV_SIZE 12
#define LUT_SCALE_GUNAKRI 5
#define LUT_SCALE_GUNAKRI_SIZE 12
#define LUT_SCALE_MARWA 6
#define LUT_SCALE_MARWA_SIZE 12
#define LUT_SCALE_SHREE 7
#define LUT_SCALE_SHREE_SIZE 12
#define LUT_SCALE_PURVI 8
#define LUT_SCALE_PURVI_SIZE 12
#define LUT_SCALE_BILAWAL 9
#define LUT_SCALE_BILAWAL_SIZE 12
#define LUT_SCALE_YAMAN 10
#define LUT_SCALE_YAMAN_SIZE 12
#define LUT_SCALE_KAFI 11
#define LUT_SCALE_KAFI_SIZE 12
#define LUT_SCALE_BHIMPALASREE 12
#define LUT_SCALE_BHIMPALASREE_SIZE 12
#define LUT_SCALE_DARBARI 13
#define LUT_SCALE_DARBARI_SIZE 12
#define LUT_SCALE_BAGESHREE 14
#define LUT_SCALE_BAGESHREE_SIZE 12
#define LUT_SCALE_RAGESHREE 15
#define LUT_SCALE_RAGESHREE_SIZE 12
#define LUT_SCALE_KHAMAJ 16
#define LUT_SCALE_KHAMAJ_SIZE 12
#define LUT_SCALE_MIMAL 17
#define LUT_SCALE_MIMAL_SIZE 12
#define LUT_SCALE_PARAMESHWARI 18
#define LUT_SCALE_PARAMESHWARI_SIZE 12
#define LUT_SCALE_RANGESHWARI 19
#define LUT_SCALE_RANGESHWARI_SIZE 12
#define LUT_SCALE_GANGESHWARI 20
#define LUT_SCALE_GANGESHWARI_SIZE 12
#define LUT_SCALE_KAMESHWARI 21
#define LUT_SCALE_KAMESHWARI_SIZE 12
#define LUT_SCALE_PA__KAFI 22
#define LUT_SCALE_PA__KAFI_SIZE 12
#define LUT_SCALE_NATBHAIRAV 23
#define LUT_SCALE_NATBHAIRAV_SIZE 12
#define LUT_SCALE_M_KAUNS 24
#define LUT_SCALE_M_KAUNS_SIZE 12
#define LUT_SCALE_BAIRAGI 25
#define LUT_SCALE_BAIRAGI_SIZE 12
#define LUT_SCALE_B_TODI 26
#define LUT_SCALE_B_TODI_SIZE 12
#define LUT_SCALE_CHANDRADEEP 27
#define LUT_SCALE_CHANDRADEEP_SIZE 12
#define LUT_SCALE_KAUSHIK_TODI 28
#define LUT_SCALE_KAUSHIK_TODI_SIZE 12
#define LUT_SCALE_JOGESHWARI 29
#define LUT_SCALE_JOGESHWARI_SIZE 12
#define LUT_SCALE_RASIA 30
#define LUT_SCALE_RASIA_SIZE 12
#define WAV_EXPONENTIAL 0
#define WAV_EXPONENTIAL_SIZE 257
#define WAV_RING 1
#define WAV_RING_SIZE 257
#define WAV_STEPS 2
#define WAV_STEPS_SIZE 257
#define WAV_NOISE 3
#define WAV_NOISE_SIZE 257
#define WAV_SINE 4
#define WAV_SINE_SIZE 1025
#define WAV_BANDLIMITED_PULSE_0 5
#define WAV_BANDLIMITED_PULSE_0_SIZE 1025
#define WAV_BANDLIMITED_PULSE_1 6
#define WAV_BANDLIMITED_PULSE_1_SIZE 1025
#define WAV_BANDLIMITED_PULSE_2 7
#define WAV_BANDLIMITED_PULSE_2_SIZE 1025
#define WAV_BANDLIMITED_PULSE_3 8
#define WAV_BANDLIMITED_PULSE_3_SIZE 1025
#define WAV_BANDLIMITED_PULSE_4 9
#define WAV_BANDLIMITED_PULSE_4_SIZE 1025
#define WAV_BANDLIMITED_PULSE_5 10
#define WAV_BANDLIMITED_PULSE_5_SIZE 1025
#define WAV_BANDLIMITED_PULSE_6 11
#define WAV_BANDLIMITED_PULSE_6_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_0 12
#define WAV_BANDLIMITED_SQUARE_0_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_1 13
#define WAV_BANDLIMITED_SQUARE_1_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_2 14
#define WAV_BANDLIMITED_SQUARE_2_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_3 15
#define WAV_BANDLIMITED_SQUARE_3_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_4 16
#define WAV_BANDLIMITED_SQUARE_4_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_5 17
#define WAV_BANDLIMITED_SQUARE_5_SIZE 1025
#define WAV_BANDLIMITED_SQUARE_6 18
#define WAV_BANDLIMITED_SQUARE_6_SIZE 1025
#define WAV_BANDLIMITED_SAW_0 19
#define WAV_BANDLIMITED_SAW_0_SIZE 1025
#define WAV_BANDLIMITED_SAW_1 20
#define WAV_BANDLIMITED_SAW_1_SIZE 1025
#define WAV_BANDLIMITED_SAW_2 21
#define WAV_BANDLIMITED_SAW_2_SIZE 1025
#define WAV_BANDLIMITED_SAW_3 22
#define WAV_BANDLIMITED_SAW_3_SIZE 1025
#define WAV_BANDLIMITED_SAW_4 23
#define WAV_BANDLIMITED_SAW_4_SIZE 1025
#define WAV_BANDLIMITED_SAW_5 24
#define WAV_BANDLIMITED_SAW_5_SIZE 1025
#define WAV_BANDLIMITED_SAW_6 25
#define WAV_BANDLIMITED_SAW_6_SIZE 1025
#define WAV_BANDLIMITED_TRIANGLE_0 26
#define WAV_BANDLIMITED_TRIANGLE_0_SIZE 1025
#define WAV__BANDLIMITED_TRIANGLE_0 27
#define WAV__BANDLIMITED_TRIANGLE_0_SIZE 1025
#define WAV___BANDLIMITED_TRIANGLE_0 28
#define WAV___BANDLIMITED_TRIANGLE_0_SIZE 1025
#define WAV_BANDLIMITED_TRIANGLE_3 29
#define WAV_BANDLIMITED_TRIANGLE_3_SIZE 1025
#define WAV_BANDLIMITED_TRIANGLE_4 30
#define WAV_BANDLIMITED_TRIANGLE_4_SIZE 1025
#define WAV_BANDLIMITED_TRIANGLE_5 31
#define WAV_BANDLIMITED_TRIANGLE_5_SIZE 1025
#define WAV_BANDLIMITED_TRIANGLE_6 32
#define WAV_BANDLIMITED_TRIANGLE_6_SIZE 1025
#define LUT_LFO_INCREMENTS 0
#define LUT_LFO_INCREMENTS_SIZE 128
#define LUT_PORTAMENTO_INCREMENTS 1
#define LUT_PORTAMENTO_INCREMENTS_SIZE 128
#define LUT_OSCILLATOR_INCREMENTS 2
#define LUT_OSCILLATOR_INCREMENTS_SIZE 97
#define LUT_EUCLIDEAN 3
#define LUT_EUCLIDEAN_SIZE 1024
#define CHR_CHARACTERS 0
#define CHR_CHARACTERS_SIZE 256

}  // namespace yarns

#endif  // YARNS_RESOURCES_H_
