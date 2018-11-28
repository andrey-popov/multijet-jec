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
    keys [variable][trigger_name], where variable is 'PtBal' or 'MPF'.
    The values of the last dictionary are either NumPy arrays of values
    of the mean balance in bins of pt or tuples of arrays of values of
    the mean balance and corresponding uncertainties.
    """
    
    def __init__(self, trigger_bins, binning):
        """Initialize from TriggerBins object and pt binning.
        
        The binning in pt must be aligned with boundaries of trigger
        bins and the underlying binning used in TProfile in data.
        
        Arguments:
            trigger_bins:  TriggerBins object.
            binning:  Binning in pt.
        """
        
        self.trigger_bins = trigger_bins
        self.binning = binning
        
        self.clipped_binnings = {}
        
        for trigger_name, trigger_bin in self.trigger_bins.items():
            self.clipped_binnings[trigger_name] = self.clip_binning(*trigger_bin['ptRange'])
    
    
    def build_data(self, data_file_path, errors=False):
        """Retrieve mean balance in data.
        
        Arguments:
            data_file_path:  Path to a ROOT file with data.
            errors:  Flag controlling whether errors need to be
                retrieved as well.
        
        Return value:
            Mean balance in data in the format described in the class
            documentation.
        """
        
        data_file = ROOT.TFile(data_file_path)
        distrs = {'PtBal': {}, 'MPF': {}}
        
        for trigger_name, trigger_bin in self.trigger_bins.items():
            
            prof_pt_bal = data_file.Get(trigger_name + '/PtBalProfile')
            prof_mpf = data_file.Get(trigger_name + '/MPFProfile')
            
            clipped_binning = self.clip_binning(*trigger_bin['ptRange'])
            distrs['PtBal'][trigger_name] = self.hist_to_np(
                prof_pt_bal.Rebin(len(clipped_binning) - 1, uuid4().hex, clipped_binning),
                errors=errors
            )
            distrs['MPF'][trigger_name] = self.hist_to_np(
                prof_mpf.Rebin(len(clipped_binning) - 1, uuid4().hex, clipped_binning),
                errors=errors
            )
        
        data_file.Close()
        
        return distrs


    def build_sim(self, sim_file_path, weight_file_path, errors=False):
        """Compute mean balance in simulation.
        
        Arguments:
            sim_file_path:  Path to a ROOT file with simulation.
            weight_file_path:  Path to a ROOT file with event weights.
            errors:  Flag controlling whether errors need to be
                retrieved as well.
        
        Return value:
            Mean balance in simulation in the format described in the
            class documentation.
        """
        
        sim_file = ROOT.TFile(sim_file_path)
        weight_file = ROOT.TFile(weight_file_path)
        
        distrs = {'PtBal': {}, 'MPF': {}}
        
        for trigger_name, trigger_bin in self.trigger_bins.items():
            
            clipped_binning = self.clipped_binnings[trigger_name]
            prof_pt_bal = ROOT.TProfile(uuid4().hex, '', len(clipped_binning) - 1, clipped_binning)
            prof_mpf = prof_pt_bal.Clone(uuid4().hex)
            
            for obj in [prof_pt_bal, prof_mpf]:
                obj.SetDirectory(ROOT.gROOT)
            
            tree = sim_file.Get(trigger_name + '/BalanceVars')
            tree.AddFriend(trigger_name + '/Weights', weight_file)
            tree.SetBranchStatus('*', False)
            
            for branch_name in ['PtJ1', 'PtBal', 'MPF', 'TotalWeight']:
                tree.SetBranchStatus(branch_name, True)
            
            ROOT.gROOT.cd()
            tree.Draw('PtBal:PtJ1>>' + prof_pt_bal.GetName(), 'TotalWeight[0]', 'goff')
            tree.Draw('MPF:PtJ1>>' + prof_mpf.GetName(), 'TotalWeight[0]', 'goff')
            
            distrs['PtBal'][trigger_name] = self.hist_to_np(prof_pt_bal, errors=errors)
            distrs['MPF'][trigger_name] = self.hist_to_np(prof_mpf, errors=errors)
        
        weight_file.Close()
        sim_file.Close()
        
        return distrs
    
    
    def clip_binning(self, min_pt, max_pt):
        """Select a subrange of the binning.
        
        Arguments:
            min_pt, max_pt:  Desired range.  Boundaries are included.
        
        Return value:
            Array of bin edges included in the given range.
        
        Assume an accurate alignment of given boundaries with the
        binning, i.e. perform the check for equality exactly and not
        accounting for floating-point errors.
        """
        
        return array('d', [edge for edge in self.binning if min_pt <= edge <= max_pt])
    
    
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
        
        num_bins = hist.GetNbinsX()
        values = np.empty(num_bins)
        
        for bin in range(1, num_bins + 1):
            values[bin - 1] = hist.GetBinContent(bin)
        
        use_errors = errors
        
        if not use_errors:
            return values
        else:
            errors = np.empty(num_bins)
            
            for bin in range(1, num_bins + 1):
                errors[bin - 1] = hist.GetBinError(bin)
            
            return values, errors


if __name__ == '__main__':
    
    arg_parser = argparse.ArgumentParser(epilog=__doc__)
    arg_parser.add_argument('config', help='Configuration JSON file.')
    arg_parser.add_argument(
        '-t', '--triggers', default='triggerBins.json',
        help='JSON file with definition of triggers bins.'
    )
    arg_parser.add_argument(
        '-b', '--binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    arg_parser.add_argument(
        '-o', '--output', default='syst.root',
        help='Name for output ROOT file.'
    )
    args = arg_parser.parse_args()
    
    
    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    
    
    # Read configuration files
    with open(args.config) as f:
        config = json.load(f)
    
    with open(args.binning) as f:
        binning = json.load(f)
        binning.sort()
    
    trigger_bins = TriggerBins(args.triggers, clip=binning[-1])
    
    
    # Analysis does not support overflow bins.  Check that the last
    # edge is finite.
    if not math.isfinite(binning[-1]):
        raise RuntimeError('Inclusive overflow bins are not supported.')
    
    trigger_bins.check_alignment(binning)
    
    
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
    
    for syst, syst_config in config.items():
        if syst == 'Nominal':
            continue
        
        for direction in ['Up', 'Down']:
            if direction not in syst_config:
                raise RuntimeError(
                    'Entry "{}" in configuration does not contain required key "{}".'.format(
                        syst, direction
                    )
                )
            
            if 'sim' not in syst_config[direction]:
                raise RuntimeError(
                    'Entry "{}/{}" in configuration does not contain required key "{}".'.format(
                        syst, direction, 'sim'
                    )
                )
            
            if 'weights' not in syst_config[direction]:
                syst_config[direction]['weights'] = \
                    os.path.splitext(syst_config[direction]['sim'])[0] + '_weights.root'
        
        if ('data' in syst_config['Up']) != ('data' in syst_config['Down']):
            raise RuntimeError(
                'In entry "{}" key "data" is provided for one variation but not the other.'.format(
                    syst
                )
            )
    
    
    # Output file and in-file directory structure
    output_file = ROOT.TFile(args.output, 'recreate')
    
    for trigger_name in trigger_bins:
        output_file.mkdir(trigger_name)
    
    
    builder = BalanceBuilder(trigger_bins, binning)
    
    # Compute mean balance with nominal configuration
    data_nominal = builder.build_data(config['Nominal']['data'], errors=True)
    sim_nominal = builder.build_sim(
        config['Nominal']['sim'], config['Nominal']['weights'], errors=True
    )
    
    
    # Compute combined relative uncertainty and store it in the output
    # file
    hists_to_store = []
    
    for trigger_name, variable in itertools.product(builder.trigger_bins, data_nominal):
        rel_errors = np.hypot(
            data_nominal[variable][trigger_name][1],
            sim_nominal[variable][trigger_name][1]
        ) / sim_nominal[variable][trigger_name][0]
        
        hist = ROOT.TH1D(
            'RelUnc_{}'.format(variable), '',
            len(builder.clipped_binnings[trigger_name]) - 1, builder.clipped_binnings[trigger_name]
        )
        
        for i in range(len(rel_errors)):
            hist.SetBinContent(i + 1, rel_errors[i])
        
        hist.SetDirectory(output_file.Get(trigger_name))
        hists_to_store.append(hist)  # Prevent garbage collection
    
    
    # Process all systematic variations
    for syst, syst_config in config.items():
        if syst == 'Nominal':
            continue
        
        for direction in ['Up', 'Down']:
            sim_syst = builder.build_sim(
                syst_config[direction]['sim'], syst_config[direction]['weights']
            )
            
            if 'data' in syst_config[direction]:
                data_syst = builder.build_data(syst_config[direction]['data'])
            else:
                data_syst = None
            
            
            for trigger_name, variable in itertools.product(trigger_bins, sim_nominal):
                
                # Compute the total variation in the difference between
                # data and simulation.  This shift will be applied to
                # simulation, and its sign is chosen accordingly.
                shift = sim_syst[variable][trigger_name] - sim_nominal[variable][trigger_name][0]
                
                if data_syst:
                    shift -= \
                        data_syst[variable][trigger_name] - data_nominal[variable][trigger_name][0]
                
                deviation = shift / sim_nominal[variable][trigger_name][0]
                
                
                # Convert into a histogram and store in the output file
                hist = ROOT.TH1D(
                    'RelVar_{}_{}{}'.format(variable, syst, direction), '',
                    len(builder.clipped_binnings[trigger_name]) - 1,
                    builder.clipped_binnings[trigger_name]
                )
                
                for i in range(len(deviation)):
                    hist.SetBinContent(i + 1, deviation[i])
                
                hist.SetDirectory(output_file.Get(trigger_name))
                hists_to_store.append(hist)  # Avoid garbage collection
    
    
    output_file.Write()
    output_file.Close()
