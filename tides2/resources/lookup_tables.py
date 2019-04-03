#!/usr/bin/python2.5
#
# Copyright 2017 Emilie Gillet.
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
# Lookup table definitions.

import numpy
import pylab

lookup_tables = []
lookup_tables_i16 = []

sample_rate = 48000

"""----------------------------------------------------------------------------
Sine table.
----------------------------------------------------------------------------"""

size = 1024
t = numpy.arange(0, size + size / 4 + 1) / float(size) * numpy.pi * 2
lookup_tables.append(('sine', numpy.sin(t)))



"""----------------------------------------------------------------------------
Waveshaper for audio rate
----------------------------------------------------------------------------"""

WAVESHAPER_SIZE = 1024

x = numpy.arange(0, WAVESHAPER_SIZE) / float(WAVESHAPER_SIZE)

linear = x
sin = 0.5 - 0.5 * numpy.cos(x * numpy.pi)
tan = numpy.arctan(8 * numpy.cos(numpy.pi * x))
tan_scale = tan.max()
fade_crop = numpy.minimum(1.0, 4.0 - 4.0 * x)
bump = (1.0 - numpy.cos(numpy.pi * x * 1.5)) * (1.0 - numpy.cos(numpy.pi * fade_crop)) / 4.5
inverse_sin = numpy.arccos(1 - 2 * x) / numpy.pi
inverse_tan = numpy.arccos(numpy.tan(tan_scale * (1.0 - 2.0 * x)) / 8.0) / numpy.pi
inverse_tan[0] = 0

def scale(x):
  x = numpy.array(list(x) + [x[-1]])
  # pylab.plot(x)
  # pylab.show()
  return list(numpy.round((x * 32767.0)).astype(int))

wavetable = []
wavetable += scale(inverse_tan)
wavetable += scale(inverse_sin)
wavetable += scale(linear)
wavetable += scale(sin)
wavetable += scale(bump)

"""----------------------------------------------------------------------------
Waveshaper for control rate
----------------------------------------------------------------------------"""

x = numpy.arange(0, WAVESHAPER_SIZE / 2) / float(WAVESHAPER_SIZE / 2)
linear = x
sin = (1.0 - numpy.cos(numpy.pi * x)) / 2.0
inverse_sin = numpy.arccos(1 - 2 * x) / numpy.pi
expo = 1.0 - numpy.exp(-5 * x)
expo_max = expo.max()
expo /= expo_max
log = numpy.log(1.0 - x * expo_max) / -5.0

def scale_flip(x, flip=True):
  if flip:
    y = x[::-1]
  else:
    y = 1.0 - x
  x = numpy.array(list(x) + [1] + list(y))
  return list(numpy.round((x * 32767.0)).astype(int))

wavetable += scale_flip(log, False)
wavetable += scale_flip(log, True)
wavetable += scale_flip(inverse_sin, False)
wavetable += scale_flip(linear, False)
wavetable += scale_flip(sin, False)
wavetable += scale_flip(expo, True)
wavetable += scale_flip(expo, False)

lookup_tables_i16.append(('wavetable', wavetable))



"""----------------------------------------------------------------------------
Post waveshaper
----------------------------------------------------------------------------"""

x = numpy.arange(0, WAVESHAPER_SIZE + 4) / (WAVESHAPER_SIZE / 2.0) - 1.0
x[-1] = x[-2]
sine = numpy.sin(8 * numpy.pi * x)
window = numpy.exp(-x * x * 4) ** 2
bipolar_fold = sine * window + numpy.arctan(3 * x) * (1 - window)
bipolar_fold /= numpy.abs(bipolar_fold).max()
lookup_tables.append(('bipolar_fold', bipolar_fold))

x = numpy.arange(0, WAVESHAPER_SIZE + 4) / float(WAVESHAPER_SIZE)
x[-1] = x[-3]
x[-2] = x[-3]
sine = numpy.sin(16 * numpy.pi * x)
window = numpy.exp(-x * x * 4) ** 2
unipolar_fold = (0.38 * sine + 4 * x) * window + numpy.arctan(4 * x) * (1 - window)
unipolar_fold /= numpy.abs(unipolar_fold).max()
lookup_tables.append(('unipolar_fold', unipolar_fold))
