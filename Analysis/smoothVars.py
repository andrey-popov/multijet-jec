#!/usr/bin/env python

"""Suppresses fluctuations in systematic variations.

The fluctuations are smoothed out and resulting smoothed deviations are
written to a ROOT file.  Also plots input and smoothed deviations.
"""

import argparse
from collections import OrderedDict
import itertools
import os
from uuid import uuid4

import numpy as np

import matplotlib as mpl
mpl.use('agg')
from matplotlib import pyplot as plt

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True


class Reader:
    """A class to simplify reading of input file.
    
    Implements a conversion from a set of ROOT histograms in different
    trigger bins to a combined NumPy array and the inverse conversion.
    """
    
    def __init__(self, input_file_path):
        """Initialize from path to ROOT file with deviations."""
        
        self.input_file = ROOT.TFile(input_file_path)
        
        # Read names of trigger bins.  Assume they are ordered properly
        # in the input file.
        self.trigger_names = []
        
        for key in self.input_file.GetListOfKeys():
            if key.GetClassName() != 'TDirectoryFile':
                continue
            
            self.trigger_names.append(key.GetName())
        
        
        # Extract the binning combined over all trigger bins and find
        # first indices of bins in each trigger bin
        binning = []
        self.trigger_boundaries = [0]
        
        for trigger_name in self.trigger_names:
            hist = self.input_file.Get(trigger_name + '/RelUnc_PtBal')
            num_bins = hist.GetNbinsX()
            
            for bin in range(1, num_bins + 1):
                binning.append(hist.GetBinLowEdge(bin))
            
            self.trigger_boundaries.append(self.trigger_boundaries[-1] + num_bins)
        
        binning.append(hist.GetBinLowEdge(num_bins + 1))
        self.binning = np.asarray(binning)
        
        if not np.all(self.binning[:-1] < self.binning[1:]):
            raise RuntimeError('Extracted binning is not ordered.')
        
        
        # Extract uncertainties
        self.unc = {'PtBal': self.read('RelUnc_PtBal'), 'MPF': self.read('RelUnc_MPF')}
    
    
    def read(self, label):
        """Read histograms with given label.
        
        Arguments:
            label:  Label that identifies histograms in all trigger
                bins.
        
        Return value:
            NumPy array with bin contents of all histograms stiched
            together.
        """
        
        values = np.empty(len(self.binning) - 1)
        
        for itrigger, trigger_name in enumerate(self.trigger_names):
            hist = self.input_file.Get('{}/{}'.format(trigger_name, label))
            
            for bin in range(1, hist.GetNbinsX() + 1):
                values[self.trigger_boundaries[itrigger] + bin - 1] = hist.GetBinContent(bin)
        
        return values
    
    
    def to_hists(self, array, name=uuid4().hex):
        """Convert NumPy array into a set of histograms.
        
        Split NumPy array by trigger bin and convert into a ROOT
        histogram in each trigger bin.  The histograms are not assigned
        to any directory.
        
        Arguments:
            array:  NumPy array to be converted.
            name:  Name to be used for all histograms.
        
        Return value:
            Dictionary that maps trigger names into newly created
            histograms.
        """
        
        if len(array) != len(self.binning) - 1:
            raise RuntimeError('Unexpected length of the array.')
        
        hists = {}
        
        for itrigger, trigger_name in enumerate(self.trigger_names):
            
            first_bin = self.trigger_boundaries[itrigger]
            num_bins = self.trigger_boundaries[itrigger + 1] - first_bin
            hist = ROOT.TH1D(name, '', num_bins, self.binning[first_bin:first_bin + num_bins + 1])
            hist.SetDirectory(None)
            
            for bin in range(1, num_bins + 1):
                hist.SetBinContent(bin, array[first_bin + bin - 1])
            
            hists[trigger_name] = hist
        
        return hists


class Smoother:
    """A class to smooth fluctuations in a systematic variations.
    
    Smooths up and down relative deviations using LOWESS algorithm [1].
    The relative deviations are assumed to be symmetric in shape but
    allowed to scale differently.
    [1] https://en.wikipedia.org/wiki/Local_regression
    """
    
    def __init__(self, nominal, up, down, weights):
        """Initializer from reference templates.
        
        Arguments:
            nominal, up, down:  Nominal template and templates that
                describe up and down variations.
            weights:  Relative weights for bins of the templates.
        """
        
        self.nominal = nominal
        self.up = up
        self.down = down
        self.weights = weights
        
        self.is_relative = False
        self.smooth_averaged_deviation = None
    
    
    @classmethod
    def fromrelative(cls, up, down, weights):
        """Initialize from relative deviations.
        
        Unlike in the standard initializer, start from up and down
        relative deviations instead of full templates.  The nominal
        template is then zero everywhere by construction.
        
        Arguments:
            up, down:  Up and down relative deviations from nominal.
            weights:  Relative weights for bins of the templates.
        """
        
        smoother = cls(np.zeros_like(up), up, down, weights)
        smoother.is_relative = True
        
        return smoother
    
    
    @staticmethod
    def lowess(y, external_weights, bandwidth):
        """Peform LOWESS smoothing.
        
        Arguments:
            y:  Input sequence to be smoothed.
            external_weights:  Weights of points in the sequence.
            bandwidth:  Smoothing bandwidth defined in terms of indices.
        
        Return value:
            Smoothed sequence.
        """
        
        y_smooth = np.empty_like(y)
        
        for i in range(len(y_smooth)):
            
            # Point indices, centred at the current point, will be used
            # as x coordinate
            x = np.arange(len(y_smooth)) - i
            
            # Compute standard weights for LOWESS
            distances = np.abs(x) / bandwidth
            weights = (1 - distances ** 3) ** 3
            np.clip(weights, 0., None, out=weights)
            
            # Include weights provided by the caller and rescale weights
            # to simplify computation of various mean values below
            weights *= external_weights
            weights /= np.sum(weights)
            
            
            # Compute smoothed value for the current point with weighted
            # least-squares fit with a linear function.  Since x
            # coordinates are centred at the current point, only need to
            # find the constant term in the linear function.
            mean_x = np.dot(weights, x)
            mean_y = np.dot(weights, y)
            mean_x2 = np.dot(weights, x ** 2)
            mean_xy = np.dot(weights, x * y)
            
            y_smooth[i] = (mean_x2 * mean_y - mean_x * mean_xy) / (mean_x2 - mean_x ** 2)
        
        return y_smooth
    
    
    def smooth(self, bandwidth=5, symmetric=False):
        """Construct smoothed templates.
        
        Arguments:
            bandwidth:  Bandwidth for smoothing, in terms of bins.
            symmetric:  Flag to request symmetric variations.
        
        Return value:
            Tuple with smoothed templates for up and down variations.
        
        Relative up and down deviations are assumed to be symmetric in
        shape but allowed to scale differently.  The combined shape is
        smoothed using LOWESS algorithm.  The scale factors are chosen
        by minimizing chi^2 difference between smoothed and input
        templates, unless called with flag symmetric set to true.
        """
        
        averaged_deviation = 0.5 * (self.up - self.down)
        
        if not self.is_relative:
            averaged_deviation /= self.nominal
        
        self.smooth_averaged_deviation = self.lowess(
            averaged_deviation, self.weights, bandwidth
        )
        
        if symmetric:
            sf_up, sf_down = 1., -1.
        else:
            sf_up = self._scale_factor(self.up)
            sf_down = self._scale_factor(self.down)
        
        if self.is_relative:
            return sf_up * self.smooth_averaged_deviation, sf_down * self.smooth_averaged_deviation
        else:
            return (
                self.nominal * (1 + sf_up * self.smooth_averaged_deviation),
                self.nominal * (1 + sf_down * self.smooth_averaged_deviation)
            )
    
    
    def _scale_factor(self, template):
        """Compute scale factor for smoothed relative deviation.
        
        Find a scale factor that gives the best match (smallest chi^2)
        between smoothed and given template.
        
        Arguments:
            template:  Template to match.
        
        Return value:
            Scale factor to be applied to smoothed relative deviation.
        """
        
        if self.is_relative:
            smooth_abs_dev = self.smooth_averaged_deviation
        else:
            smooth_abs_dev = self.smooth_averaged_deviation * self.nominal
        
        # This is result of an analytical computation
        return np.sum(smooth_abs_dev * (template - self.nominal) * self.weights) / \
            np.sum(smooth_abs_dev ** 2 * self.weights)
        


if __name__ == '__main__':
    
    arg_parser = argparse.ArgumentParser(epilog=__doc__)
    arg_parser.add_argument('input_file', help='ROOT file with input variations and uncertainties.')
    arg_parser.add_argument(
        '-o', '--output', default=None,
        help='Name for output ROOT file with smoothed variations.'
    )
    arg_parser.add_argument(
        '--fig-dir', default='figSmooth',
        help='Name for directory with plots.'
    )
    args = arg_parser.parse_args()
    
    if args.output is None:
        args.output = os.path.splitext(args.input_file)[0] + '_smooth.root'
    
    if not os.path.exists(args.fig_dir):
        os.makedirs(args.fig_dir)
    
    
    mpl.rc('xtick', top=True, direction='in')
    mpl.rc('ytick', right=True, direction='in')
    mpl.rc('axes', labelsize='large')
    mpl.rc('axes.formatter', limits=[-2, 4], use_mathtext=True)
    
    
    reader = Reader(args.input_file)
    smooth_deviations = OrderedDict()
    
    for (variable, variable_label), syst_name in itertools.product(
        [('PtBal', '$p_\\mathrm{T}$ balance'), ('MPF', 'MPF')],
        ['L1Res', 'L2Res', 'JER']
    ):
        # Read input relative deviations
        up = reader.read('RelVar_{}_{}Up'.format(variable, syst_name))
        down = reader.read('RelVar_{}_{}Down'.format(variable, syst_name))
        
        
        # Smooth them
        num_bins = len(reader.binning) - 1
        smoother = Smoother.fromrelative(up, down, 1 / reader.unc[variable] ** 2)
        up_smooth, down_smooth = smoother.smooth(bandwidth=0.1 * num_bins)
        
        smooth_deviations['RelVar_{}_{}Up'.format(variable, syst_name)] = up_smooth
        smooth_deviations['RelVar_{}_{}Down'.format(variable, syst_name)] = down_smooth
        
        
        # Plot input and smoothed deviations
        fig = plt.figure()
        axes = fig.add_subplot(111)
        
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=up * 100, histtype='step',
            color='#a8d2f0', label='Up, input'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=up_smooth * 100, histtype='step',
            color='#1f77b4', label='Up, smoothed'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=down * 100, histtype='step',
            color='#ffc999', label='Down, input'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=down_smooth * 100, histtype='step',
            color='#ff7f0e', label='Down, smoothed'
        )
        
        axes.axhline(0., color='black', lw=0.8, ls='dashed')
        
        axes.margins(x=0.)
        axes.set_ylim(-1, 1)
        axes.set_xscale('log')
        
        axes.legend()
        axes.set_xlabel('$p_\\mathrm{T}^\\mathrm{lead}$ [GeV]')
        axes.set_ylabel('Relative deviation from nominal [%]')
        
        axes.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
        axes.xaxis.set_minor_formatter(mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4)))
        
        axes.text(
            0.5, 1.005, '{}, {}'.format(syst_name, variable_label), size='large',
            ha='center', va='bottom', transform=axes.transAxes
        )
        
        fig.savefig(os.path.join(args.fig_dir, '{}_{}.pdf'.format(syst_name, variable)))
        plt.close(fig)
    
    
    # Save smoothed deviations to a file
    output_file = ROOT.TFile(args.output, 'recreate')
    hists_to_store = []
    
    for trigger_name in reader.trigger_names:
        output_file.mkdir(trigger_name)
    
    for label, deviation in smooth_deviations.items():
        hists = reader.to_hists(deviation, label)
        
        for trigger_name in reader.trigger_names:
            hist = hists[trigger_name]
            hist.SetDirectory(output_file.Get(trigger_name))
            hists_to_store.append(hist)  # Prevent garbage collection
    
    output_file.Write()
    output_file.Close()
