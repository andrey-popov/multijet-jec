#!/usr/bin/env python

"""Constructs inputs for the fit of multijet data.

Information in the output ROOT file is organized into directories by
trigger bins.  For simulation this script constructs a spline
approximation for mean values of balance observables as a function of
the (natural) logarithm of the pt of the leading jet.  Histograms and
profiles for data are copied from the input file without modification.
In each trigger bin they have a much finer binning, which is needed for
the rebinning during the fit.  If the number of data events in some bin
of the target binning is smaller than a threshold, a warning is printed.
"""

import argparse
from array import array
from collections import OrderedDict
import json
import math
import os
from uuid import uuid4

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True

from regularization import SplineSimFitter
from triggerbins import TriggerBins


if __name__ == '__main__':
    
    arg_parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    arg_parser.add_argument(
        'data', help='Input data file.'
    )
    arg_parser.add_argument(
        'sim', help='Input simulation file.'
    )
    arg_parser.add_argument(
        '-w', '--weights',
        help='File with weights for simulation. '
            'By default constructed from path to simulation file.'
    )
    arg_parser.add_argument(
        '-t', '--triggers', default='triggerBins.json',
        help='JSON file with definition of triggers bins.'
    )
    arg_parser.add_argument(
        '-b', '--binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    arg_parser.add_argument(
        '-o', '--output', default='multijet.root',
        help='Name for output ROOT file.'
    )
    arg_parser.add_argument('--plots', default='fig', help='Directory for diagnostic plots.')
    arg_parser.add_argument('--era', help='Era label for diagnostic plots.')
    args = arg_parser.parse_args()
    
    if args.weights is None:
        if args.sim_file.endswith('.root'):
            args.weights = args.sim_file[:-5] + '_weights.root'
        else:
            args.weights = args.sim_file + '_weights.root'
    
    
    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    
    
    # Read configuration files
    with open(args.binning) as f:
        binning = json.load(f)
        binning.sort()
    
    trigger_bins = TriggerBins(args.triggers, clip=binning[-1])
    
    
    # Analysis does not support overflow bins.  Check that the last
    # edge is finite.
    if not math.isfinite(binning[-1]):
        raise RuntimeError('Inclusive overflow bins are not supported.')
    
    
    trigger_bins.check_alignment(binning)
    
    
    # Construct continuous model for mean balance observables in
    # simulation
    sim_fitter = SplineSimFitter(
        args.sim, args.weights, trigger_bins,
        diagnostic_plots_dir=args.plots + '/sim_fit', era=args.era
    )
    sim_fitter.fit(['PtBal', 'MPF'])
    
    
    data_file = ROOT.TFile(args.data)
    out_file = ROOT.TFile(args.output, 'recreate')
    
    
    # Write jet pt thresholds
    pt_threshold = ROOT.TVectorD(1)
    pt_threshold[0] = 30.
    pt_threshold.Write('MinPtPtBal')
    
    pt_threshold[0] = 15.
    pt_threshold.Write('MinPtMPF')
    
    
    # Loop over triggers
    for trigger_bin in trigger_bins:
        out_directory = out_file.mkdir(trigger_bin.name)
        out_directory.cd()
        
        
        # Store definition for pt bins in the current trigger bin
        clipped_binning = [
            edge for edge in binning if trigger_bin.pt_range[0] <= edge <= trigger_bin.pt_range[1]
        ]
        clipped_binning_store = ROOT.TVectorD(len(clipped_binning))
        
        for i in range(len(clipped_binning)):
            clipped_binning_store[i] = clipped_binning[i]
        
        clipped_binning_store.Write('Binning')
        
        
        # Store splines constructed for simulation after converting them
        # to ROOT classes
        for variable in ['PtBal', 'MPF']:
            root_spline = sim_fitter.spline_to_root(
                sim_fitter.fit_results[variable][trigger_bin.name]
            )
            root_spline.Write('Sim' + variable)
        
        
        # In case of data simply copy profiles and histograms
        hist_pt_lead = data_file.Get('{}/PtLead'.format(trigger_bin.name))
        hist_pt_lead.SetDirectory(out_directory)
        
        for name in [
            'PtLeadProfile', 'PtBalProfile', 'MPFProfile', 'PtJet', 'PtJetSumProj',
            'RelPtJetSumProj'
        ]:
            obj = data_file.Get('{}/{}'.format(trigger_bin.name, name))
            obj.SetDirectory(out_directory)
        
        out_directory.Write()
        
        
        # Check if there are poorly populated bins in data.  Do not
        # perform the same check for simulation as it is assumed to
        # have larger effective intergrated luminosity
        hist_pt_lead_rebinned = hist_pt_lead.Rebin(
            len(clipped_binning) - 1, uuid4().hex, array('d', clipped_binning)
        )
        underpopulated_bins = []
        
        for bin in range(1, hist_pt_lead_rebinned.GetNbinsX() + 1):
            if hist_pt_lead_rebinned.GetBinContent(bin) < 10:
                underpopulated_bins.append(bin)
        
        if underpopulated_bins:
            print('There were under-populated bins when producing file "{}".'.format(args.output))
            print('  Bin in ptLead   Events in data')
            
            for bin in underpopulated_bins:
                print('  {:13}   {}'.format(
                    '({:g}, {:g})'.format(clipped_binning[bin - 1], clipped_binning[bin]),
                    round(hist_pt_lead_rebinned.GetBinContent(bin))
                ))
    
    
    for f in [out_file, data_file]:
        f.Close()
