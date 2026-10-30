#!/usr/bin/python2.5
#
# Copyright 2021 Emilie Gillet.
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
#
# -----------------------------------------------------------------------------
#
# My favorite DX7 patches.

def Read(name):
  data = file(name, 'rb').read()
  #for i in xrange(32):
  #  print data[6 + 118 + i * 128:6 + 118 + i * 128 + 10]
  # Skip the SysEx header and footer
  return map(ord, data[6:-2])

def make_bank(patches):
  data = []
  cache = {}
  for rom_name, index in patches:
    if not rom_name in cache:
      rom_data = file('plaits/resources/syx/%s.syx' % rom_name, 'rb').read()
      cache[rom_name] = rom_data[6:-2]
    patch = cache[rom_name][index * 128:(index + 1) * 128]
    # print rom_name, index, patch[118:128]
    data += map(ord, patch)
  return data
  
banks = [
  [
    # Basses
    ('MISC', 0), # Solid bass
    ('MISC', 21), # Mooger Low
    ('MISC', 2), # LeaderTape
    ('Guit_Clav5', 19), # Morhol TB1
    ('ROM1B', 30), # Bass 3
    ('KV04B', 23), # Bill bass
    ('ROM1A', 14), # Bass 1
    ('Guit_Clav5', 2), # Elec Bass

    # More basses
    ('MISC', 1), # S.Bas 27.7
    ('Guit_Clav2', 30), # Resonances
    ('ROM2B', 15), # Syn-bass 2
    ('ROM3A', 15), # Prc synth1
    ('Guit_Clav4', 11), # Croma 2
    ('MISC', 3), # Analog 4 SQUAREWAVY BRASS
    ('KV04A', 0), # Analog A
    ('MISC', 4), # Analog 6 SAWY

    # Synths
    ('Guit_Clav4', 18), # CS-80
    ('Guit_Clav4', 22), # Insert 1 BRASSY
    ('Guit_Clav2', 31), # Spiral
    ('Guit_Clav4', 9), # Dx-Trott BASS
    ('MISC', 5), # GasHaus
    ('Guit_Clav3', 31), # Ring ding
    ('Guit_Clav4', 29), # Papagayo
    ('KV04B', 14), # Wineglass

    # Synths, metallic
    ('Guit_Clav2', 17), # Amytal THROATY PAD
    ('Guit_Clav4', 2), # Fairlight
    ('PPGVOCAL', 0), # PPG Vol 1
    ('PPGVOCAL', 1), # PPG Vol 2
    ('PPGVOCAL', 26), # *Fairl. 3
    ('PPGVOCAL', 19), # *Vocoder 2
    ('PPGVOCAL', 21), # * Sequence
    ('MISC', 13), # Bounce 4
  ],
  [
    # Keys
    ('ROM1A', 10), # E piano 1
    ('Guit_Clav3', 22), # Fender 1
    ('MISC', 6), # WintrRhodes
    ('KV04B', 18), # RS-EP C
    ('MISC', 7), # Mark III
    ('ROM4B', 0), # Clav E pno
    ('Guit_Clav1', 13), # Syn Clav
    ('KV04B', 20), # Clavinet

    # Piano / plucked
    ('ROM1B', 1), # Piano 5
    ('Guit_Clav5', 3), # Grd Piano
    ('Guit_Clav1', 21), # Steinway
    ('Guit_Clav5', 16), # Guit acous
    #('ROM1B', 24), # Guitar 5
    ('ROM1B', 21), # Sitar
    ('ROM1A', 22), # Koto
    ('ROM3A', 1), # Harpsich
    ('ROM1B', 11), # Clav 3

    # Chroma percussions
    ('ROM2A', 23), # Xylophone
    ('ROM3A', 6), # Marimba
    ('ROM1A', 20), # Vibe 1
    ('ROM2A', 21), # Glockenspl
    ('KV04B', 15), # Bell C
    ('ROM4A', 20), # Bells 
    ('ROM1A', 25), # Tub Bells
    ('ROM2A', 26), # Gong 2

    # Percussions
    ('SYN9', 8), # Kettle
    ('SYN9', 27), # Mid drum 3
    ('SYN9', 10), # Ori Drum
    ('SYN9', 3), # Wood 6
    ('SYN9', 17), # Latin Drum
    ('SYN9', 24), # Cimbal
    ('MISC', 8), # SYNDM 25.8
    ('ROM2B', 21) # B Drm - Snar
  ],
  [
    ('Guit_Clav3', 1), # Click 124
    ('MISC', 22), # Hammond
    ('ROM1B', 13), # E organ 3
    ('ROM3B', 14), # 60s organ
    ('Guit_Clav3', 19), # Optic 28
    ('ROM1A', 17), # Pipes 1
    ('ROM1B', 17), # Pipes 3
    ('ROM3B', 15), # Pipes 2

    ('MISC', 9), # JX-33-P
    ('Guit_Clav4', 20), # Soundtrack
    ('MISC', 11), # Ice pad 2
    ('MISC', 12), # M1 PADS
    ('MISC', 14), # CARLOS 2
    ('MISC', 16), # Soft touch
    ('PPGVOCAL', 30), # *Planets
    ('MISC', 17), # Cirrus
    ('MISC', 18), # ENTRIX
    ('Guit_Clav4', 27), # Mal Poly
    ('MISC', 20), # Textures 6
    ('MISC', 10), # Etherial5a
    ('MISC', 15), # Airy
    ('MISC', 19), # Boron A
    ('Guit_Clav4', 5), # Vangelis 1
    ('KV04B', 5), # Strings C

    ('ROM1A', 5), # Strings 3
    ('ROM1A', 4), # Strings 2
    ('ROM2A', 9), # Strings 7
    ('Guit_Clav1', 7), # Full strin
    ('Guit_Clav1', 2), # Syn orch
    ('ROM1A', 0), # Brass 1
    ('ROM2A', 13), # Brass 6 BC
    ('ROM3A', 5) # Br trumpet
  ]
]

patches = []
for i, bank in enumerate(banks):
  bank_data = make_bank(bank)
  file('bank_%d.raw' % i, 'wb').write(''.join(map(chr, bank_data)))
  patches += [('bank_%d' % i, bank_data)]
