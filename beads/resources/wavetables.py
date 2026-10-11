#!/usr/bin/python2.5
#
# Copyright 2016 Emilie Gillet.
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
# Waveform definitions.

import numpy
import pylab


WAVETABLE_SIZE = 256
BRAIDS_WAVES = numpy.fromstring(
  file('beads/resources/waves.bin', 'rb').read(), numpy.uint8)

wavetables = []

def sine(frequency):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  if frequency >= WAVETABLE_SIZE / 2:
    return t * 0
  x = numpy.sin(2 * numpy.pi * t * frequency)
  return x


def comb(n):
  x = 0
  for i in xrange(n):
    x += sine(i + 1)
  return x


def pair(n):
  x = 0
  for i in xrange(n):
    x += sine(i + 1) * (i + 0.5) / (n - 1.0)
    x += sine((i + 1) * 4) * (i + 0.5) / (n - 1.0) * 0.5
  return x


def tri(n, f=1):
  x = 0
  for i in xrange(n):
    x += sine((2 * i + 1) * f) / (2 * i + 1) ** 2.0
  return x


def tri_stack(n):
  x = 0
  for i in xrange(n):
    x += tri(15 + 5 * n, i + n / 3)
  return x


def saw(n, f=1):
  x = 0
  for i in xrange(n):
    x += sine((i + 1) * f) / (i + 1)
  return x


def saw_stack(n):
  x = 0
  for i in xrange(n):
    x += saw(1 + 6 * i, i + 1) / ((i + 1) ** 0.5)
  return x


def square(n):
  x = 0
  for i in xrange(n):
    x += sine(2 * i + 1) / (2 * i + 1)
  return x


def quadra(n):
  x = 0
  for harmonic, amplitude in zip(xrange(4), [1, 0.5, 1, 0.5]):
    x += sine(2 * n + 2 * harmonic + 1) * amplitude
  return x


def drawbars(bars):
  pipes = [1.0, 3.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 16.0]
  x = 0
  for intensity, frequency in zip(bars, pipes):
    x += int(intensity) / 8.0 * sine(frequency)
  return x


def pulse(duty):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  t[-1] = t[0]
  t[t < duty] = -1.0
  t[t >= duty] = 1.0
  return -t


def burst(duty):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  t[-1] = t[0]
  d = duty ** 0.5
  t[t < d] = -1.0
  t[t >= d] = 0.0
  return -t * sine(1.0 / duty)


def hybrid(duty):
  cycle = (numpy.arange(0, WAVETABLE_SIZE) + int((duty - 0.5) * WAVETABLE_SIZE)) % WAVETABLE_SIZE
  x = pulse(duty)
  x += saw(80)[cycle]
  x -= (x.mean())
  return x


def trisaw(harmonic):
  return tri(80) + saw(80, harmonic) * (1 if harmonic != 1 else 0.25) * 0.5


def sawtri(harmonic):
  return saw(80) * 0.5 + tri(80, harmonic) * (1 if harmonic != 1 else 0.25)


def square_formant(ratio):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  phase = t * (ratio ** 0.5) * 0.5
  phase[phase >= 1.0] = 1.0
  amplitude = numpy.cos(phase * numpy.pi) + 1
  formant = (sine(ratio * 0.75) + 1.0) * amplitude * 0.5
  formant -= (formant.max() + formant.min()) / 2.0
  return formant
  

def saw_formant(ratio):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  amplitude = 1.0 - t
  formant = (sine(ratio) + 1.0) * amplitude * 0.5
  formant -= (formant.max() + formant.min()) / 2.0
  return formant


def bandpass_formant(ratio):
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  amplitude = 1.0 - t
  formant = sine(ratio * 1.5) * amplitude * 0.5
  return formant


def sine_power(power):
  x = sine(1.0)
  x += saw(16)
  power = 2.0 ** power
  return numpy.sign(x) * (numpy.abs(x) ** power)


def formant_f(index):
  formant_1 = 3.9 * (index + 1) / 8.0
  formant_2 = formant_1 * (1.0 - numpy.cos(formant_1 * numpy.pi * 0.8))
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  amplitude_1 = (1.0 - t) ** 0.2 * numpy.exp(-4.0 * t) 
  amplitude_2 = (1.0 - t) ** 0.2 * numpy.exp(-2.0 * t) 
  formant_3 = sine(1 + 2.8 * (formant_2 + formant_1)) * amplitude_2 * 1.7
  formant_1 = sine(1 + 3 * formant_1) * amplitude_1
  formant_2 = sine(1 + 4 * formant_2) * amplitude_2 * 1.5
  f = formant_1 + formant_2 + formant_3
  return f - (f.max() + f.min()) / 2.0


def distort(x):
  return numpy.arctan(x * 8.0) / numpy.pi


def digi_formant_f(index):
  formant_1 = 3.9 * (index + 1) / 8.0
  formant_2 = formant_1 * (1.0 - numpy.cos(formant_1 * numpy.pi * 0.8))
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  amplitude_1 = (1.0 - t) ** 0.2 * numpy.exp(-4.0 * t) 
  amplitude_2 = (1.0 - t) ** 0.2 * numpy.exp(-2.0 * t) 
  formant_3 = distort(sine(1 + 2.9 * (formant_2 + formant_1))) * amplitude_2 * 0.7
  formant_1 = distort(sine(1 + 3.2 * formant_1)) * amplitude_1
  formant_2 = distort(sine(1 + 4.1 * formant_2)) * amplitude_2 * 0.7
  f = formant_1 + formant_2 + formant_3
  return f - (f.max() + f.min()) / 2.0


def fm(carrier, modulator, amount):
  modulator = sine(modulator)
  t = numpy.arange(0, WAVETABLE_SIZE) / float(WAVETABLE_SIZE)
  return numpy.sin(2 * numpy.pi * (t * carrier + modulator * amount))


def make_family(fn, arguments):
  return map(fn, arguments)


def make_braids_family(indices, fix=False):
  family = []
  for i in indices:
    start = i * 129
    end = start + 128
    s = BRAIDS_WAVES[start:end] - 128.0
    if fix:
      sf = numpy.fft.rfft(s)
    else:
      si = numpy.zeros((WAVETABLE_SIZE, ))
      si = s
      # si[::2] = s
      # si[1::2] = s  # Ewwwwww
      sf = numpy.fft.rfft(si)
    sf = numpy.abs(sf) * numpy.exp(-1j * numpy.pi / 2.0)
    interpolated = numpy.fft.irfft(sf, WAVETABLE_SIZE)
    family += [interpolated]
  return family


waves = []

# A
waves += make_family(sine, range(1, 17))

# B
waves += make_family(tri_stack, range(2, 18))

# C
waves += make_family(drawbars, [
    '688600000',
    '686040000',
    '666806000',
    '655550600',
    '665560060', 
    '688500888',
    '660000888',
    '060000046',
    '867000006', 
    '888876788', 
    '668744354',
    '448644054',
    '327645222',
    '204675300',
    '002478500',
    '002050321'])

# D
waves += make_family(trisaw, [1, 1.5, 2, 3, 4, 4.5, 5, 8])
waves += make_family(sawtri, [8, 5, 4.5, 4, 3, 2, 1.5, 1])

# E
waves += make_family(burst, [0.5, 0.4, 1/3.0, 0.25, 0.2, 0.125, 1/16.0, 1/32.0])
waves += make_family(bandpass_formant, [16.0, 10.0, 9.0, 8.0, 6.0, 4.0, 3.0, 2.0])

# F
waves += make_family(formant_f, xrange(8))
waves += make_family(digi_formant_f, [7, 6, 5, 4, 3, 2, 1, 0])

# G
waves += make_braids_family([176, 189, 191, 193, 195, 197, 199, 201])  # Digi
waves += make_braids_family([220, 222, 224, 226, 228, 230, 232, 234])  # Metal

# H
waves += make_braids_family(range(172, 188), False)

# I, J, K, L
sweep = numpy.linspace(0, 1.0, 16)
waves += make_family(lambda x: fm(2.0, 1.0, x), 4 * sweep ** 2)
waves += make_family(lambda x: fm(1.0, 0.5, x), 8 * sweep ** 2)
waves += make_family(lambda x: fm(1.0, 3.0, x), 3 * sweep ** 2)
waves += make_family(lambda x: fm(1.0, 8.0, x), 2 * sweep ** 2)

# New wavetable code uses integrated wavetables
# Reference:
# "Higher-order integrated Wavetable Synthesis", Franck & Valimaki, DAFX-12.
#
# Here we use K = 1 (first order), N = 1 (linear interpolation).
data = []
for wave in waves:
  n = len(wave)
  x = numpy.array(list(wave) * 2 + wave[0] + wave[1] + wave[2] + wave[3])
  x -= x.mean()
  x /= numpy.abs(x).max()
  
  x = numpy.cumsum(x)
  x -= x.mean()
  x = list(numpy.round(x * (4 * 32768.0 / WAVETABLE_SIZE)).astype(int))
  data += list(x[-n-4:])
  
wavetables.append(('integrated_waves', data))
