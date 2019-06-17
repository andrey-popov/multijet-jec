#!/usr/bin/env python

"""Constructs inputs for the fit of multijet data.

Produced ROOT files contains the suggested binning for the computation
of the chi^2, as well as jet thresholds.  Histograms and profiles for
data from different trigger bins are merged together (since there is no
overlap) but otherwise they are copied from the input file without
modification.  For simulation, a spline approximation to the mean value
of the balance observables as a function of the logarithm of the pt of
the leading jet is constructed.  This is done for each trigger bin, and
the resulting splines are stored in separate directories.

The script also checks the number of events in each bin of the target
binning and prints a warning if this number is below a threshold.

Systematic uncertainties are not included.  They are constructed by a
dedicated script.
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
import utils


if __name__ == '__main__':
    
    arg_parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    arg_parser.add_argument(
        '-d', '--data', nargs='+',
        help='Input files with data (required)'
    )
    arg_parser.add_argument(
        '-s', '--sim', nargs='+',
        help='Input files with simulation (required)'
    )
    arg_parser.add_argument(
        '-t', '--triggers', default='trigger_bins.json',
        help='JSON file with definition of triggers bins.'
    )
    arg_parser.add_argument(
        '-b', '--binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    arg_parser.add_argument(
        '-e', '--era', default=None,
        help='Data-taking period'
    )
    arg_parser.add_argument(
        '-o', '--output', default='multijet.root',
        help='Name for output ROOT file.'
    )
    arg_parser.add_argument(
        '--plots', default='fig', help='Directory for diagnostic plots.'
    )
    args = arg_parser.parse_args()
    
    
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
    
    
    # Check alingment between given binning and trigger bins
    trigger_bins.check_alignment(binning)
    
    if trigger_bins[0].pt_range[0] != binning[0]:
        raise RuntimeError(
            'Left-most edge in given binning ({:g}) does not match left boundary of the first '
            'trigger bin ({:g}).'.format(binning[0], trigger_bins[0].pt_range[0])
        )
    
    
    out_file = ROOT.TFile(args.output, 'recreate')
    out_file.cd()
    
    
    # Write jet pt thresholds.  For both observables smooth thresholds
    # are used, and the two numbers define the range over which the
    # efficiency changes from 0 to 1.
    pt_threshold = ROOT.TVectorD(2)
    pt_threshold[0] = 30.
    pt_threshold[1] = 33.
    pt_threshold.Write('PtBalThreshold')
    
    pt_threshold[0] = 15.
    pt_threshold[1] = 20.
    pt_threshold.Write('MPFThreshold')
    
    
    # Store suggested binning for the computation of chi^2
    binning_store = ROOT.TVectorD(len(binning))
    
    for i in range(len(binning)):
        binning_store[i] = binning[i]
    
    binning_store.Write('Binning')
    
    
    # Store data histograms.  Since there is no overlap in pt of the
    # leading jet between different trigger bins in data, corresponding
    # histograms are combined.
    data_histograms = OrderedDict()
    
    for data_path in args.data:
        data_file = ROOT.TFile(data_path)

        for trigger_bin in trigger_bins:
            for name in [
                'PtLead', 'PtLeadProfile', 'PtBalProfile', 'MPFProfile', 'RelPtJetSumProj'
            ]:
                hist = data_file.Get('{}/{}'.format(trigger_bin.name, name))
                
                if name not in data_histograms:
                    hist.SetDirectory(out_file)
                    data_histograms[name] = hist
                else:
                    data_histograms[name].Add(hist)
        
        data_file.Close()

    out_file.Write()
    
    
    # Construct continuous model for mean balance observables in
    # simulation
    sim_fitter = SplineSimFitter(
        args.sim, args.era, trigger_bins,
        diagnostic_plots_dir=args.plots + '/sim_fit'
    )
    sim_fitter.fit(['PtBal', 'MPF'])
    
    
    # Write mean balance in simulation in all trigger bins
    for trigger_bin in trigger_bins:
        out_directory = out_file.mkdir(trigger_bin.name)
        out_directory.cd()
        
        range_store = ROOT.TVectorD(2)
        
        for i in range(2):
            range_store[i] = trigger_bin.pt_range[i]
        
        range_store.Write('Range')
        
        
        for variable in ['PtBal', 'MPF']:
            root_spline = utils.spline_to_root(
                sim_fitter.fit_results[variable][trigger_bin.name]
            )
            root_spline.Write('Sim' + variable)
        
        out_directory.Write()
    
    
    # Check for underpopulated bins in data
    hist_pt_lead_rebinned = data_histograms['PtLead'].Rebin(
        len(binning) - 1, uuid4().hex, array('d', binning)
    )
    hist_pt_lead_rebinned.SetDirectory(None)
    underpopulated_bins = []
    
    for bin in range(1, hist_pt_lead_rebinned.GetNbinsX() + 1):
        if hist_pt_lead_rebinned.GetBinContent(bin) < 100:
            underpopulated_bins.append(bin)
    
    if underpopulated_bins:
        print('There were under-populated bins when producing file "{}".'.format(args.output))
        print('  Bin in ptLead   Events in data')
        
        for bin in underpopulated_bins:
            print('  {:13}   {}'.format(
                '({:g}, {:g})'.format(binning[bin - 1], binning[bin]),
                round(hist_pt_lead_rebinned.GetBinContent(bin))
            ))
    
    out_file.Close()
