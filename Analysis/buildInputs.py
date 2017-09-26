#!/usr/bin/env python

"""Constructs inputs for the fit of multijet data.

Information in the output ROOT file is organized into directories by
trigger bins.  For simulation this script builts profiles of balance
observables versus pt of the leading jet.  This profiles also define the
target binning to be used for the fit.  Histograms and profiles for data
are copied from the input file without modification.  In each trigger
bin they have a much finning and cover larger regions in pt, which is
needed for the rebinning during the fit.
"""

import argparse
from array import array
from collections import OrderedDict
import json
import math
import os

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True


if __name__ == '__main__':
    
    argParser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    argParser.add_argument(
        'dataFile', help='Input data file.'
    )
    argParser.add_argument(
        'simFile', help='Input simulation file.'
    )
    argParser.add_argument(
        '-w', '--weights', default=None,
        help='File with weights for simulation. '
            'By default constructed from path to simulation file.'
    )
    argParser.add_argument(
        '-t', '--triggers', default='triggerBins.json',
        help='JSON file with definition of triggers bins.'
    )
    argParser.add_argument(
        '-b', '--binning', metavar='binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    argParser.add_argument(
        '-o', '--output', metavar='multijet.root', default='multijet.root',
        help='Name for output ROOT file.'
    )
    args = argParser.parse_args()
    
    if args.weights is None:
        if args.simFile.endswith('.root'):
            args.weights = args.simFile[:-5] + '_weights.root'
        else:
            args.weights = args.simFile + '_weights.root'
    
    if not os.path.exists(args.triggers):
        path = None
        
        if 'MULTIJET_JEC_INSTALL' in os.environ:
            path = os.path.join(os.environ['MULTIJET_JEC_INSTALL'], 'config', args.triggers)
        
        if path is None or not os.path.exists(path):
            raise RuntimeError('Failed to find file "{}".'.format(args.triggers))
        else:
            args.triggers = path
    
    
    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    
    
    # Read configuration files
    with open(args.binning) as f:
        binning = json.load(f)
        binning.sort()
    
    with open(args.triggers) as f:
        triggerBins = json.load(f, object_pairs_hook=OrderedDict)
    
    
    # Analysis does not support overflow bins.  Check that the last
    # edge is finite.
    if not math.isfinite(binning[-1]):
        raise RuntimeError('Inclusive overflow bins are not supported.')
    
    
    # One of the trigger bins normally extends to very large pt (but the
    # value is finite as infinities are not implemented in other code
    # that uses that file).  Clip it.
    for triggerName, triggerBin in triggerBins.items():
        corrRange = triggerBin['corrPtRange']
        
        if corrRange[1] > binning[-1]:
            corrRange[1] = binning[-1]
        
        if corrRange[0] >= corrRange[1]:
            raise RuntimeError('Illegal corrected pt range for trigger "{}": {}.'.format(
                triggerName, corrRange
            ))
    
    
    # Verify alignment of boundaries of trigger bins
    mismatchedEdges = []
    
    for triggerBin in triggerBins.values():
        for edge in triggerBin['corrPtRange']:
            if edge not in binning:
                mismatchedEdges.append(edge)
    
    if mismatchedEdges:
        raise RuntimeError(
            'Following boundaries of trigger bins are not aligned with the binning: {}.'.format(
                mismatchedEdges
            )
        )
    
    
    dataFile = ROOT.TFile(args.dataFile)
    simFile = ROOT.TFile(args.simFile)
    weightFile = ROOT.TFile(args.weights)
    
    outFile = ROOT.TFile(args.output, 'recreate')
    
    
    # Write jet pt thresholds
    ptThreshold = ROOT.TVectorD(1)
    ptThreshold[0] = 30.
    ptThreshold.Write('MinPtPtBal')
    
    ptThreshold[0] = 15.
    ptThreshold.Write('MinPtMPF')
    
    
    # Loop over triggers
    for triggerName, triggerBin in triggerBins.items():
        outDirectory = outFile.mkdir(triggerName)
        
        
        # Create profiles in simulation
        ptRange = triggerBin['corrPtRange']
        clippedBinning = array('d', [edge for edge in binning if ptRange[0] <= edge <= ptRange[1]])
        
        profPtBal = ROOT.TProfile(
            'SimPtBalProfile', ';p_{T}^{lead} [GeV];<p_{T} balance>',
            len(clippedBinning) - 1, clippedBinning
        )
        profMPF = ROOT.TProfile(
            'SimMPFProfile', ';p_{T}^{lead} [GeV];<MPF>',
            len(clippedBinning) - 1, clippedBinning
        )
        
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
        
        for obj in [profPtBal, profMPF]:
            obj.SetDirectory(outDirectory)
        
        
        # In case of data simply copy profiles and histograms
        for name in [
            'PtLead', 'PtLeadProfile', 'PtBalProfile', 'MPFProfile', 'PtJet', 'PtJetSumProj'
        ]:
            obj = dataFile.Get('{}/{}'.format(triggerName, name))
            obj.SetDirectory(outDirectory)
        
        
        outDirectory.Write()
    
    
    for f in [outFile, dataFile, simFile, weightFile]:
        f.Close()
