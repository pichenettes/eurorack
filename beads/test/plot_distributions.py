import numpy as np
import pylab
import sys

pitch_range = [-24.0,24.0]
value_range = [-0.1,1.1]
show_hist = True

data = np.loadtxt(sys.stdin)
_, axes_random = pylab.subplots(7, 3, sharex=True, figsize=(20,12))
_, axes_cv = pylab.subplots(7, 3, sharey=True, figsize=(20,12))
_, axes_pitch_random = pylab.subplots(7, 3, sharex=True, figsize=(20,12))
_, axes_pitch_cv = pylab.subplots(7, 3, sharey=True, figsize=(20,12))


for i in xrange(7):
  for j in xrange(3):
    axes_random[i][j].hist(data[:, i + j * 7], 25)
    axes_random[i][j].set_xlim(value_range)
    axes_pitch_random[i][j].hist(data[:, i + j * 7 + 84], 25)
    axes_pitch_random[i][j].set_xlim(pitch_range)

    axes_cv[i][j].plot(data[::16, i + j * 7 + 21])
    axes_cv[i][j].set_ylim(value_range)
    axes_pitch_cv[i][j].plot(data[::16, i + j * 7 + 105])
    axes_pitch_cv[i][j].set_ylim(pitch_range)

pylab.show()