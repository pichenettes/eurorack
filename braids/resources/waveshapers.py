#!/usr/bin/python2.5
#
# Copyright 2012 Olivier Gillet.
#
# Author: Olivier Gillet (ol.gillet@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# -----------------------------------------------------------------------------
#
# Waveshaper lookup tables.

import numpy

waveshapers = []

def scale(x, min=-32766, max=32766, center=True):
  if center:
    x -= x.mean()
  mx = numpy.abs(x).max()
  x = (x + mx) / (2 * mx)
  x = x * (max - min) + min
  x = numpy.round(x)
  return x.astype(int)


x = ((numpy.arange(0, 257) / 128.0 - 1.0))
x[-1] = x[-2]
violent_overdrive = numpy.tanh(8.0 * x)
overdrive = numpy.tanh(5.0 * x)
slight_overdrive = numpy.tanh(1.75 * x)
moderate_overdrive = numpy.tanh(2.5 * x)

window = numpy.exp(-x * x * 4) ** 1.5
sine = numpy.sin(8 * numpy.pi * x)
sine_fold = sine * window + numpy.arctan(3 * x) * (1 - window)
sine_fold /= numpy.abs(sine_fold).max()

tri_fold = numpy.sin(numpy.pi * (3 * x + (2 * x) ** 3))

waveshapers.append(('moderate_overdrive', scale(moderate_overdrive)))
waveshapers.append(('slight_overdrive', scale(slight_overdrive)))
waveshapers.append(('violent_overdrive', scale(violent_overdrive)))
waveshapers.append(('sine_fold', scale(sine_fold, center=False)))
waveshapers.append(('tri_fold', scale(tri_fold)))
