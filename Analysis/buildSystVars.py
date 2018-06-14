#!/usr/bin/env python

"""Constructs relative systematic variations.

Configuration that defines systematic variations is provided in the form
of a JSON file.  Stores the relative systematic variations, in trigger
bins, as well as the combined relative uncertainty, which is needed for
smoothing of the variations.
"""

import argparse
from array import array
import json
import itertools
import math
from uuid import uuid4

import numpy as np

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True

from triggerbins import TriggerBins


class BalanceBuilder:
    """A class to provide mean balance in trigger bins.
    
    Mean balance is provided in the form of nested dictionaries with
    keys [variable][triggerName], where variable is 'PtBal' or 'MPF'.
    The values of the last dictionary are either NumPy arrays of values
    of the mean balance in bins of pt or tuples of arrays of values of
    the mean balance and corresponding uncertainties.
    """
    
    def __init__(self, triggerBins, binning):
        """Initialize from TriggerBins object and pt binning.
        
        The binning in pt must be aligned with boundaries of trigger
        bins and the underlying binning used in TProfile in data.
        
        Arguments:
            triggerBins:  TriggerBins object.
            binning:  Binning in pt.
        """
        
        self.triggerBins = triggerBins
        self.binning = binning
        
        self.clippedBinnings = {}
        
        for triggerName, triggerBin in self.triggerBins.items():
            self.clippedBinnings[triggerName] = self.clip_binning(*triggerBin['corrPtRange'])
    
    
    def build_data(self, dataFilePath, errors=False):
        """Retrieve mean balance in data.
        
        Arguments:
            dataFilePath:  Path to a ROOT file with data.
            errors:  Flag controlling whether errors need to be
                retrieved as well.
        
        Return value:
            Mean balance in data in the format described in the class
            documentation.
        """
        
        dataFile = ROOT.TFile(dataFilePath)
        distrs = {'PtBal': {}, 'MPF': {}}
        
        for triggerName, triggerBin in self.triggerBins.items():
            
            profPtBal = dataFile.Get(triggerName + '/PtBalProfile')
            profMPF = dataFile.Get(triggerName + '/MPFProfile')
            
            clippedBinning = self.clip_binning(*triggerBin['corrPtRange'])
            distrs['PtBal'][triggerName] = self.hist_to_np(
                profPtBal.Rebin(len(clippedBinning) - 1, uuid4().hex, clippedBinning),
                errors=errors
            )
            distrs['MPF'][triggerName] = self.hist_to_np(
                profMPF.Rebin(len(clippedBinning) - 1, uuid4().hex, clippedBinning),
                errors=errors
            )
        
        dataFile.Close()
        
        return distrs


    def build_sim(self, simFilePath, weightFilePath, errors=False):
        """Compute mean balance in simulation.
        
        Arguments:
            simFilePath:  Path to a ROOT file with simulation.
            weightFilePath:  Path to a ROOT file with event weights.
            errors:  Flag controlling whether errors need to be
                retrieved as well.
        
        Return value:
            Mean balance in simulation in the format described in the
            class documentation.
        """
        
        simFile = ROOT.TFile(simFilePath)
        weightFile = ROOT.TFile(weightFilePath)
        
        distrs = {'PtBal': {}, 'MPF': {}}
        
        for triggerName, triggerBin in self.triggerBins.items():
            
            clippedBinning = self.clippedBinnings[triggerName]
            profPtBal = ROOT.TProfile(uuid4().hex, '', len(clippedBinning) - 1, clippedBinning)
            profMPF = profPtBal.Clone(uuid4().hex)
            
            for obj in [profPtBal, profMPF]:
                obj.SetDirectory(ROOT.gROOT)
            
            tree = simFile.Get(triggerName + '/BalanceVars')
            tree.AddFriend(triggerName + '/Weights', weightFile)
            tree.SetBranchStatus('*', False)
            
            for branchName in ['PtJ1', 'PtBal', 'MPF', 'TotalWeight']:
                tree.SetBranchStatus(branchName, True)
            
            ROOT.gROOT.cd()
            tree.Draw('PtBal:PtJ1>>' + profPtBal.GetName(), 'TotalWeight[0]', 'goff')
            tree.Draw('MPF:PtJ1>>' + profMPF.GetName(), 'TotalWeight[0]', 'goff')
            
            distrs['PtBal'][triggerName] = self.hist_to_np(profPtBal, errors=errors)
            distrs['MPF'][triggerName] = self.hist_to_np(profMPF, errors=errors)
        
        weightFile.Close()
        simFile.Close()
        
        return distrs
    
    
    def clip_binning(self, minPt, maxPt):
        """Select a subrange of the binning.
        
        Arguments:
            minPt, maxPt:  Desired range.  Boundaries are included.
        
        Return value:
            Array of bin edges included in the given range.
        
        Assume an accurate alignment of given boundaries with the
        binning, i.e. perform the check for equality exactly and not
        accounting for floating-point errors.
        """
        
        return array('d', [edge for edge in self.binning if minPt <= edge <= maxPt])
    
    
    @staticmethod
    def hist_to_np(hist, errors=False):
        """Convert ROOT histogram into NumPy array.
        
        Ignore under- and overflows.
        
        Arguments:
            hist:  ROOT histogram to convert.
            errors:  Flag indicating whether errors should be retrieved.
        
        Return value:
            NumPy array of bin contents of the histogram or a tuple of
            such an array and another array with bin errors.
        """
        
        numBins = hist.GetNbinsX()
        values = np.empty(numBins)
        
        for bin in range(1, numBins + 1):
            values[bin - 1] = hist.GetBinContent(bin)
        
        useErrors = errors
        
        if not useErrors:
            return values
        else:
            errors = np.empty(numBins)
            
            for bin in range(1, numBins + 1):
                errors[bin - 1] = hist.GetBinError(bin)
            
            return values, errors


if __name__ == '__main__':
    
    argParser = argparse.ArgumentParser(epilog=__doc__)
    argParser.add_argument('config', help='Configuration JSON file.')
    argParser.add_argument(
        '-t', '--triggers', default='triggerBins.json',
        help='JSON file with definition of triggers bins.'
    )
    argParser.add_argument(
        '-b', '--binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    argParser.add_argument(
        '-o', '--output', default='syst.root',
        help='Name for output ROOT file.'
    )
    args = argParser.parse_args()
    
    
    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    
    
    # Read configuration files
    with open(args.config) as f:
        config = json.load(f)
    
    with open(args.binning) as f:
        binning = json.load(f)
        binning.sort()
    
    triggerBins = TriggerBins(args.triggers, clip=binning[-1])
    
    
    # Analysis does not support overflow bins.  Check that the last
    # edge is finite.
    if not math.isfinite(binning[-1]):
        raise RuntimeError('Inclusive overflow bins are not supported.')
    
    triggerBins.check_alignment(binning)
    
    
    # Validate the configuration.  Add default weight files if missing.
    if 'Nominal' not in config:
        raise RuntimeError('Configuration does not contain required entry "Nominal".')
    
    for key in ['data', 'sim']:
        if key not in config['Nominal']:
            raise RuntimeError(
                'Entry "Nominal" in configuration does not contain required key "{}".'.format(key)
            )
    
    if 'weights' not in config['Nominal']:
        config['Nominal']['weights'] = \
            os.path.splitext(config['Nominal']['sim'])[0] + '_weights.root'
    
    for syst, systConfig in config.items():
        if syst == 'Nominal':
            continue
        
        for direction in ['Up', 'Down']:
            if direction not in systConfig:
                raise RuntimeError(
                    'Entry "{}" in configuration does not contain required key "{}".'.format(
                        syst, direction
                    )
                )
            
            if 'sim' not in systConfig[direction]:
                raise RuntimeError(
                    'Entry "{}/{}" in configuration does not contain required key "{}".'.format(
                        syst, direction, 'sim'
                    )
                )
            
            if 'weights' not in systConfig[direction]:
                systConfig[direction]['weights'] = \
                    os.path.splitext(systConfig[direction]['sim'])[0] + '_weights.root'
        
        if ('data' in systConfig['Up']) != ('data' in systConfig['Down']):
            raise RuntimeError(
                'In entry "{}" key "data" is provided for one variation but not the other.'.format(
                    syst
                )
            )
    
    
    # Output file and in-file directory structure
    outputFile = ROOT.TFile(args.output, 'recreate')
    
    for triggerName in triggerBins:
        outputFile.mkdir(triggerName)
    
    
    builder = BalanceBuilder(triggerBins, binning)
    
    # Compute mean balance with nominal configuration
    dataNominal = builder.build_data(config['Nominal']['data'], errors=True)
    simNominal = builder.build_sim(
        config['Nominal']['sim'], config['Nominal']['weights'], errors=True
    )
    
    
    # Compute combined relative uncertainty and store it in the output
    # file
    histsToStore = []
    
    for triggerName, variable in itertools.product(builder.triggerBins, dataNominal):
        relErrors = np.hypot(
            dataNominal[variable][triggerName][1],
            simNominal[variable][triggerName][1]
        ) / simNominal[variable][triggerName][0]
        
        hist = ROOT.TH1D(
            'RelUnc_{}'.format(variable), '',
            len(builder.clippedBinnings[triggerName]) - 1, builder.clippedBinnings[triggerName]
        )
        
        for i in range(len(relErrors)):
            hist.SetBinContent(i + 1, relErrors[i])
        
        hist.SetDirectory(outputFile.Get(triggerName))
        histsToStore.append(hist)  # Prevent garbage collection
    
    
    # Process all systematic variations
    for syst, systConfig in config.items():
        if syst == 'Nominal':
            continue
        
        for direction in ['Up', 'Down']:
            simSyst = builder.build_sim(
                systConfig[direction]['sim'], systConfig[direction]['weights']
            )
            
            if 'data' in systConfig[direction]:
                dataSyst = builder.build_data(systConfig[direction]['data'])
            else:
                dataSyst = None
            
            
            for triggerName, variable in itertools.product(triggerBins, simNominal):
                
                # Compute the total variation in the difference between
                # data and simulation.  This shift will be applied to
                # simulation, and its sign is chosen accordingly.
                shift = simSyst[variable][triggerName] - simNominal[variable][triggerName][0]
                
                if dataSyst:
                    shift -= \
                        dataSyst[variable][triggerName] - dataNominal[variable][triggerName][0]
                
                deviation = shift / simNominal[variable][triggerName][0]
                
                
                # Convert into a histogram and store in the output file
                hist = ROOT.TH1D(
                    'RelVar_{}_{}{}'.format(variable, syst, direction), '',
                    len(builder.clippedBinnings[triggerName]) - 1,
                    builder.clippedBinnings[triggerName]
                )
                
                for i in range(len(deviation)):
                    hist.SetBinContent(i + 1, deviation[i])
                
                hist.SetDirectory(outputFile.Get(triggerName))
                histsToStore.append(hist)  # Prevent garbage collection
    
    
    outputFile.Write()
    outputFile.Close()
            
