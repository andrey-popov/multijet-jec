#!/usr/bin/env python

"""Constructs inputs for the fit of multijet data.

Information in the output ROOT file is organized into directories by
trigger bins.  For simulation this script builts profiles of balance
observables versus pt of the leading jet.  This profiles also define the
target binning to be used for the fit.  Histograms and profiles for data
are copied from the input file without modification.  In each trigger
bin they have a much finer binning and cover larger regions in pt, which
is needed for the rebinning during the fit.  If the number of data
events in some bin of the target binning is smaller than a threshold, a
warning is printed.
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
    
    
    data_file = ROOT.TFile(args.data)
    sim_file = ROOT.TFile(args.sim_file)
    weight_file = ROOT.TFile(args.weights)
    
    out_file = ROOT.TFile(args.output, 'recreate')
    
    
    # Write jet pt thresholds
    pt_threshold = ROOT.TVectorD(1)
    pt_threshold[0] = 30.
    pt_threshold.Write('MinPtPtBal')
    
    pt_threshold[0] = 15.
    pt_threshold.Write('MinPtMPF')
    
    
    # Loop over triggers
    for trigger_name, trigger_bin in trigger_bins.items():
        out_directory = out_file.mkdir(trigger_name)
        
        
        # Create profiles in simulation
        pt_range = trigger_bin['ptRange']
        clipped_binning = array('d', [
            edge for edge in binning if pt_range[0] <= edge <= pt_range[1]
        ])
        
        prof_pt_bal = ROOT.TProfile(
            'SimPtBalProfile', ';p_{T}^{lead} [GeV];<p_{T} balance>',
            len(clipped_binning) - 1, clipped_binning
        )
        prof_mpf = ROOT.TProfile(
            'SimMPFProfile', ';p_{T}^{lead} [GeV];<MPF>',
            len(clipped_binning) - 1, clipped_binning
        )
        
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
        
        for obj in [prof_pt_bal, prof_mpf]:
            obj.SetDirectory(out_directory)
        
        
        # In case of data simply copy profiles and histograms
        hist_pt_lead = data_file.Get('{}/PtLead'.format(trigger_name))
        hist_pt_lead.SetDirectory(out_directory)
        
        for name in [
            'PtLead', 'PtLeadProfile', 'PtBalProfile', 'MPFProfile', 'PtJet', 'PtJetSumProj',
            'RelPtJetSumProj'
        ]:
            obj = data_file.Get('{}/{}'.format(trigger_name, name))
            obj.SetDirectory(out_directory)
        
        
        # Check if there are poorly populated bins in data.  Do not
        # perform the same check for simulation as it is assumed to
        # have larger effective intergrated luminosity
        hist_pt_lead_rebinned = hist_pt_lead.Rebin(
            len(clipped_binning) - 1, uuid4().hex, clipped_binning
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
        
        
        out_directory.Write()
    
    
    for f in [out_file, data_file, sim_file, weight_file]:
        f.Close()
