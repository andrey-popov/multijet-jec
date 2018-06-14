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
    
    def __init__(self, inputFilePath):
        """Initialize from path to ROOT file with deviations."""
        
        self.inputFile = ROOT.TFile(inputFilePath)
        
        # Read names of trigger bins.  Assume they are ordered properly
        # in the input file.
        self.triggerNames = []
        
        for key in self.inputFile.GetListOfKeys():
            if key.GetClassName() != 'TDirectoryFile':
                continue
            
            self.triggerNames.append(key.GetName())
        
        
        # Extract the binning combined over all trigger bins and find
        # first indices of bins in each trigger bin
        binning = []
        self.triggerBoundaries = [0]
        
        for triggerName in self.triggerNames:
            hist = self.inputFile.Get(triggerName + '/RelUnc_PtBal')
            numBins = hist.GetNbinsX()
            
            for bin in range(1, numBins + 1):
                binning.append(hist.GetBinLowEdge(bin))
            
            self.triggerBoundaries.append(self.triggerBoundaries[-1] + numBins)
        
        binning.append(hist.GetBinLowEdge(numBins + 1))
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
        
        for iTrigger, triggerName in enumerate(self.triggerNames):
            hist = self.inputFile.Get('{}/{}'.format(triggerName, label))
            
            for bin in range(1, hist.GetNbinsX() + 1):
                values[self.triggerBoundaries[iTrigger] + bin - 1] = hist.GetBinContent(bin)
        
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
        
        for iTrigger, triggerName in enumerate(self.triggerNames):
            
            firstBin = self.triggerBoundaries[iTrigger]
            numBins = self.triggerBoundaries[iTrigger + 1] - firstBin
            hist = ROOT.TH1D(name, '', numBins, self.binning[firstBin:firstBin + numBins + 1])
            hist.SetDirectory(None)
            
            for bin in range(1, numBins + 1):
                hist.SetBinContent(bin, array[firstBin + bin - 1])
            
            hists[triggerName] = hist
        
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
        
        self.isRelative = False
        self.smoothAveragedDeviation = None
    
    
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
        smoother.isRelative = True
        
        return smoother
    
    
    @staticmethod
    def lowess(y, externalWeights, bandwidth):
        """Peform LOWESS smoothing.
        
        Arguments:
            y:  Input sequence to be smoothed.
            externalWeights:  Weights of points in the sequence.
            bandwidth:  Smoothing bandwidth defined in terms of indices.
        
        Return value:
            Smoothed sequence.
        """
        
        ySmooth = np.empty_like(y)
        
        for i in range(len(ySmooth)):
            
            # Point indices, centred at the current point, will be used
            # as x coordinate
            x = np.arange(len(ySmooth)) - i
            
            # Compute standard weights for LOWESS
            distances = np.abs(x) / bandwidth
            weights = (1 - distances ** 3) ** 3
            np.clip(weights, 0., None, out=weights)
            
            # Include weights provided by the caller and rescale weights
            # to simplify computation of various mean values below
            weights *= externalWeights
            weights /= np.sum(weights)
            
            
            # Compute smoothed value for the current point with weighted
            # least-squares fit with a linear function.  Since x
            # coordinates are centred at the current point, only need to
            # find the constant term in the linear function.
            meanX = np.dot(weights, x)
            meanY = np.dot(weights, y)
            meanX2 = np.dot(weights, x ** 2)
            meanXY = np.dot(weights, x * y)
            
            ySmooth[i] = (meanX2 * meanY - meanX * meanXY) / (meanX2 - meanX ** 2)
        
        return ySmooth
    
    
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
        
        averagedDeviation = 0.5 * (self.up - self.down)
        
        if not self.isRelative:
            averagedDeviation /= self.nominal
        
        self.smoothAveragedDeviation = self.lowess(
            averagedDeviation, self.weights, bandwidth
        )
        
        if symmetric:
            sfUp, sfDown = 1., -1.
        else:
            sfUp = self._scale_factor(self.up)
            sfDown = self._scale_factor(self.down)
        
        if self.isRelative:
            return sfUp * self.smoothAveragedDeviation, sfDown * self.smoothAveragedDeviation
        else:
            return (
                self.nominal * (1 + sfUp * self.smoothAveragedDeviation),
                self.nominal * (1 + sfDown * self.smoothAveragedDeviation)
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
        
        if self.isRelative:
            smoothAbsDev = self.smoothAveragedDeviation
        else:
            smoothAbsDev = self.smoothAveragedDeviation * self.nominal
        
        # This is result of an analytical computation
        return np.sum(smoothAbsDev * (template - self.nominal) * self.weights) / \
            np.sum(smoothAbsDev ** 2 * self.weights)
        


if __name__ == '__main__':
    
    argParser = argparse.ArgumentParser(epilog=__doc__)
    argParser.add_argument('inputFile', help='ROOT file with input variations and uncertainties.')
    argParser.add_argument(
        '-o', '--output', default=None,
        help='Name for output ROOT file with smoothed variations.'
    )
    argParser.add_argument(
        '--fig-dir', dest='figDir', default='figSmooth',
        help='Name for directory with plots.'
    )
    args = argParser.parse_args()
    
    if args.output is None:
        args.output = os.path.splitext(args.inputFile)[0] + '_smooth.root'
    
    if not os.path.exists(args.figDir):
        os.makedirs(args.figDir)
    
    
    mpl.rc('xtick', top=True, direction='in')
    mpl.rc('ytick', right=True, direction='in')
    mpl.rc('axes', labelsize='large')
    mpl.rc('axes.formatter', limits=[-2, 4], use_mathtext=True)
    
    
    reader = Reader(args.inputFile)
    smoothDeviations = OrderedDict()
    
    for (variable, variableLabel), systName in itertools.product(
        [('PtBal', '$p_\\mathrm{T}$ balance'), ('MPF', 'MPF')],
        ['L1Res', 'L2Res', 'JER']
    ):
        # Read input relative deviations
        up = reader.read('RelVar_{}_{}Up'.format(variable, systName))
        down = reader.read('RelVar_{}_{}Down'.format(variable, systName))
        
        
        # Smooth them
        numBins = len(reader.binning) - 1
        smoother = Smoother.fromrelative(up, down, 1 / reader.unc[variable] ** 2)
        upSmooth, downSmooth = smoother.smooth(bandwidth=0.1 * numBins)
        
        smoothDeviations['RelVar_{}_{}Up'.format(variable, systName)] = upSmooth
        smoothDeviations['RelVar_{}_{}Down'.format(variable, systName)] = downSmooth
        
        
        # Plot input and smoothed deviations
        fig = plt.figure()
        axes = fig.add_subplot(111)
        
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=up * 100, histtype='step',
            color='#a8d2f0', label='Up, input'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=upSmooth * 100, histtype='step',
            color='#1f77b4', label='Up, smoothed'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=down * 100, histtype='step',
            color='#ffc999', label='Down, input'
        )
        axes.hist(
            reader.binning[:-1], bins=reader.binning, weights=downSmooth * 100, histtype='step',
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
            0.5, 1.005, '{}, {}'.format(systName, variableLabel), size='large',
            ha='center', va='bottom', transform=axes.transAxes
        )
        
        fig.savefig(os.path.join(args.figDir, '{}_{}.pdf'.format(systName, variable)))
        plt.close(fig)
    
    
    # Save smoothed deviations to a file
    outputFile = ROOT.TFile(args.output, 'recreate')
    histsToStore = []
    
    for triggerName in reader.triggerNames:
        outputFile.mkdir(triggerName)
    
    for label, deviation in smoothDeviations.items():
        hists = reader.to_hists(deviation, label)
        
        for triggerName in reader.triggerNames:
            hist = hists[triggerName]
            hist.SetDirectory(outputFile.Get(triggerName))
            histsToStore.append(hist)  # Prevent garbage collection
    
    outputFile.Write()
    outputFile.Close()
