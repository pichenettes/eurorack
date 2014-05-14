#!/usr/bin/python2.5
#
# Copyright 2014 Olivier Gillet.
#
# Author: Olivier Gillet (ol.gillet@gmail.com)
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
# Waveform definitions.

import numpy

WAVETABLE_SIZE = 256

# Waveforms for the trigger shaper ---------------------------------------------

def scale(x):
  if x.min() > 0:
    x = (x - x.min()) / (x.max() - x.min()) * 32767.0
  else:
    abs_max = numpy.abs(x).max()
    x = x / abs_max * 32767.0
  return numpy.round(x)


t = numpy.arange(0, WAVETABLE_SIZE + 1) / float(WAVETABLE_SIZE)

exponential = numpy.exp(-4.0 * t)
ring = numpy.exp(-3.0 * t) * numpy.cos(8.0 * t * numpy.pi)
steps = numpy.sign(numpy.sin(4.0 * t * numpy.pi)) * (2 ** (-numpy.round(t * 2.0)))
noise = numpy.random.randn(WAVETABLE_SIZE + 1, 1).ravel() * ((1 - t) ** 2)

waveforms = [('exponential', scale(exponential))]
waveforms += [('ring', scale(ring))]
waveforms += [('steps', scale(steps))]
waveforms += [('noise', scale(noise))]



"""----------------------------------------------------------------------------
Band-limited waveforms
----------------------------------------------------------------------------"""

WAVETABLE_SIZE = 1024
SAMPLE_RATE = 48000

# The Juno-6 / Juno-60 waveforms have a brighter harmonic content, which can be
# recreated by adding to the signal a 1-pole high-pass filtered version of
# itself.
JUNINESS = 0.0


# Sine wave.
numpy.random.seed(21)
sine = -numpy.sin(numpy.arange(WAVETABLE_SIZE + 1) / float(WAVETABLE_SIZE) * 2 * numpy.pi) * 32767

# Band limited waveforms.
num_zones = (107 - 24) / 16 + 2
bl_pulse_tables = []
bl_square_tables = []
bl_saw_tables = []
bl_tri_tables = []

wrap = numpy.fmod(numpy.arange(WAVETABLE_SIZE + 1) + WAVETABLE_SIZE / 2, WAVETABLE_SIZE)
quadrature = numpy.fmod(numpy.arange(WAVETABLE_SIZE + 1) + WAVETABLE_SIZE / 4, WAVETABLE_SIZE)
fill = numpy.fmod(numpy.arange(WAVETABLE_SIZE + 1), WAVETABLE_SIZE)

waveforms.append(('sine', scale(sine)))

for zone in range(num_zones):
  f0 = 440.0 * 2.0 ** ((18 + 16 * zone - 69) / 12.0)
  period = SAMPLE_RATE / f0
  m = 2 * numpy.floor(period / 2) + 1.0
  i = numpy.arange(-WAVETABLE_SIZE / 2, WAVETABLE_SIZE / 2) / float(WAVETABLE_SIZE)
  pulse = numpy.sin(numpy.pi * i * m) / (m * numpy.sin(numpy.pi * i) + 1e-9)
  pulse[WAVETABLE_SIZE / 2] = 1.0
  pulse = pulse[fill]

  square = numpy.cumsum(pulse - pulse[wrap])
  rectangle = numpy.cumsum(pulse - pulse[quadrature])
  triangle = -numpy.cumsum(square[::-1] - square.mean()) / WAVETABLE_SIZE

  square -= JUNINESS * triangle
  if zone == num_zones - 1:
    square = sine
    rectangle = sine
  bl_pulse_tables.append(('bandlimited_pulse_%d' % zone, scale(rectangle)))
  bl_square_tables.append(('bandlimited_square_%d' % zone, scale(square)))

  triangle = triangle[quadrature]
  if zone == num_zones - 1:
    triangle = sine
  bl_tri_tables.append(('bandlimited_triangle_%d' % zone, scale(triangle)))

  saw = -numpy.cumsum(pulse[wrap] - pulse.mean())
  saw -= JUNINESS * numpy.cumsum(saw - saw.mean()) / WAVETABLE_SIZE
  if zone == num_zones - 1:
    saw = sine
  bl_saw_tables.append(('bandlimited_saw_%d' % zone, scale(saw)))


triangle_lowest_octave = bl_tri_tables[0]
for i in xrange(3):
  bl_tri_tables[i] = triangle_lowest_octave

waveforms.extend(bl_pulse_tables)
waveforms.extend(bl_square_tables)
waveforms.extend(bl_saw_tables)
waveforms.extend(bl_tri_tables)