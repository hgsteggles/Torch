import sys
import warnings
import numpy as np
from scipy import ndimage
from scipy.ndimage.filters import gaussian_filter

# Parse arguments.
import argparse
parser = argparse.ArgumentParser(description='Calculates max flux in cornish set.')
parser.add_argument('iden', metavar='iden', type=int, help='Density index in CORNISH param set.')
parser.add_argument('-ben', action='store_true', help='Use simple ben stromgren survey.')
parser.add_argument('-harry', action='store_true', help='Use simple harry stromgren survey.')
args = parser.parse_args()

def load_src(name, fpath):
    import os, imp
    return imp.load_source(name, os.path.join(os.path.dirname(__file__), fpath))

load_src("torch", "../torchpack/torch.py")
load_src("hgspy", "../torchpack/hgspy.py")

import torch
import hgspy

load_src("cdat", "../torchpack/cornishdata.py")
import cdat
cornish_data = cdat.CornishData(args.iden)
star_data = cornish_data.star_data
nstars = len(star_data[:,0])

DPI = 300
figformat = 'png'
plot_size = 10.0
fontsize = 16
torch.set_font_sizes(fontsize)

benstr = ""
if args.ben:
	benstr = "-ben"
if args.harry:
	benstr = "-harry"
outputfile = cornish_data.dirname + '/hist2d-flux-mass' + benstr + '.' + figformat

### Data
simulated_survey = np.genfromtxt(cornish_data.dirname + "/final-survey" + benstr + ".txt", skip_header=1)

### Plotting.
plotter = torch.Plotter(1, 1, plot_size, figformat, DPI)

###	Axes.
asp_rat = 1.0
grid = plotter.axes1D((1,1), aspect_ratio=asp_rat)

grid[0].set_xlabel(plotter.format_label(torch.VarType('\mathrm{Flux}', units='mJy')))

grid[0].set_ylabel(plotter.format_label(torch.VarType('M_\\star', units='M_{\odot}')))

grid[0].set_xlim([2, 10000])

grid[0].set_ylim([0.0, 70])

xbins = np.logspace(0.3, 4.3, 22)
ybins = np.arange(0, 72, 3)

counts_0, xedges_0, yedges_0 = np.histogram2d(simulated_survey[:,9], simulated_survey[:,1], bins=(xbins, ybins))

#counts_0 = ndimage.gaussian_filter(counts_0, sigma=(1, 1), order=0)

x_0 = []
for i in range(len(xedges_0) - 1):
	x_0.append(10.0**(0.5 * (np.log10(xedges_0[i] * xedges_0[i + 1]))))
y_0 = []
for i in range(len(yedges_0) - 1):
	y_0.append(0.5 * (yedges_0[i + 1] + yedges_0[i]))


nlevels = 4
levels = []
maxcounts = np.max(counts_0)
for ilev in range(nlevels):
	levels.append((ilev + 1) * maxcounts / float(nlevels + 1))

#grid[0].contour(x_0, y_0, np.swapaxes(counts_0, 0, 1), levels)

grid[0].hist2d(simulated_survey[:,9], simulated_survey[:,1], bins=(xbins, ybins))
grid[0].pcolormesh(xedges_0, yedges_0, np.swapaxes(counts_0, 0, 1))

grid[0].set_xscale("log")

###	Save figure.
with warnings.catch_warnings():
	warnings.simplefilter("ignore")
	plotter.save_plot(outputfile)

print sys.argv[0] + ': plotted in ' + outputfile
