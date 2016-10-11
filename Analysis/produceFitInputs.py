#!/usr/bin/env python

"""Produces inputs for fit of multijet data.

For data fills a 3D histogram with PtJ1 * cos(Alpha), MJB, and F as the
coordinates.  For simulation produces 1D histogram with mean MJB as a
function of PtJ1 * cos(Alpha), usually with a wider binning.  The
histogram with simulation also has uncertainties computed.
"""

import argparse
from array import array
import json
import math
import os
import sys

import ROOT
ROOT.gROOT.SetBatch(True)

from triggerclip import TriggerClip


if __name__ == '__main__':
    
    # Parse arguments
    argParser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    argParser.add_argument(
        'dataFiles',
        help='ROOT files with data; comma-separated list is allowed'
    )
    argParser.add_argument(
        'simFile',
        help='ROOT file with simulation'
    )
    argParser.add_argument(
        '-w', '--weights', metavar='simFile_weights.root', required=True, dest='weightsFile',
        help='ROOT file with additional weights for simulation'
    )
    argParser.add_argument(
        '-b', '--binning', metavar='binning.json', default='binning.json',
        help='JSON file defining desired binning'
    )
    argParser.add_argument(
        '-t', '--trigger-bins', metavar='triggerBins.json',
        default='triggerBins.json', dest='triggerBins',
        help='JSON file defining pt cuts for different trigger bins'
    )
    argParser.add_argument(
        '-o', '--output', metavar='output.root', default='multijet.root',
        help='Name for the output ROOT file'
    )
    argParser.add_argument(
        '-f', '--function', metavar='linear', default='linear',
        help='Functional form for JEC to choose appropriate version of F'
    )
    
    args = argParser.parse_args()
    
    if args.function == 'linear':
        branchF = 'F_Linear'
    elif args.function == 'loglinear':
        branchF = 'F_LogLinear'
    else:
        print('Unknown functional form "{}" has been requested.'.format(args.function))
        sys.exit(1)
    
    
    # Read binning and create histograms to be filled.  The 3D histogram
    # for data will filled directly, but mean MJB in simulation will be
    # computed using three lists of counters, which aggregate various
    # quantities in bins of the histogram for simulation.
    with open(args.binning) as binningFile:
        binning = json.load(binningFile)
    
    histData3D = ROOT.TH3I(
        'Data', 'Distribution of data;p_{T,lead} #upoint cos #alpha [GeV];R_{MJB};F',
        len(binning['Input']['PtLeadProj']) - 1, array('d', binning['Input']['PtLeadProj']),
        len(binning['Input']['MJB']) - 1, array('d', binning['Input']['MJB']),
        len(binning['Input']['F']) - 1, array('d', binning['Input']['F'])
    )
    histSimMJB = ROOT.TH1D(
        'Simulation', 'Mean R_{MJB} in simulation;p_{T,lead} #upoint cos #alpha [GeV];R_{MJB}',
        len(binning['Fit']['PtLeadProj']) - 1, array('d', binning['Fit']['PtLeadProj'])
    )
    
    simCounts = [0. for bin in range(histSimMJB.GetNbinsX())]
    simSumMJB = list(simCounts)
    simSumMJB2 = list(simCounts)
    
    
    # Create object to verify clipping to trigger bins
    clip = TriggerClip(args.triggerBins)
    
    
    # Create a TChain to read data files
    chain = ROOT.TChain('BalanceVarsPt30')
    for dataFileName in args.dataFiles.split(','):
        chain.Add(dataFileName)
    
    chain.SetBranchStatus('*', False)
    for branchName in ['PtJ1', 'Alpha', 'TriggerBin', 'MJB', branchF]:
        chain.SetBranchStatus(branchName, True)
    
    
    # Fill the data histogram
    for event in chain:
        
        ptLeadProj = event.PtJ1 * math.cos(event.Alpha)
        if not clip.check_pt(event.TriggerBin, ptLeadProj):
            continue
        
        histData3D.Fill(ptLeadProj, event.MJB, getattr(event, branchF))
    
    del chain
    
    
    # Open file with simulation
    simFile = ROOT.TFile(args.simFile)
    tree = simFile.Get('BalanceVarsPt30')
    tree.AddFriend('WeightsPt30', args.weightsFile)
    
    tree.SetBranchStatus('*', False)
    for branchName in ['PtJ1', 'Alpha', 'TriggerBin', 'MJB', 'TotalWeight', branchF]:
        tree.SetBranchStatus(branchName, True)
    
    
    # Ranges to emulate dropping of events in over- and underflow bins
    # in data
    rangePtLeadProj = (binning['Input']['PtLeadProj'][0], binning['Input']['PtLeadProj'][-1])
    rangeMJB = (binning['Input']['MJB'][0], binning['Input']['MJB'][-1])
    rangeF = (binning['Input']['F'][0], binning['Input']['F'][-1])
    
    
    # Fill the histograms for simulation
    for event in tree:
        
        ptLeadProj = event.PtJ1 * math.cos(event.Alpha)
        MJB = event.MJB
        
        if not clip.check_pt(event.TriggerBin, ptLeadProj):
            continue
        
        if not rangePtLeadProj[0] < ptLeadProj < rangePtLeadProj[1] or \
                not rangeMJB[0] < MJB < rangeMJB[1] or \
                not rangeF[0] < getattr(event, branchF) < rangeF[1]:
            continue
        
        weight = event.TotalWeight[0]
        i = histSimMJB.FindFixBin(ptLeadProj) - 1
        
        simCounts[i] += weight
        simSumMJB[i] += MJB * weight
        simSumMJB2[i] += MJB**2 * weight
    
    simFile.Close()
    
    
    # Fill the histogram with mean values of MJB in simulation
    for i in range(histSimMJB.GetNbinsX()):
        
        meanMJB = simSumMJB[i] / simCounts[i]
        meanMJBUnc = math.sqrt((simSumMJB2[i] / simCounts[i] - meanMJB**2) / simCounts[i])
        
        histSimMJB.SetBinContent(i + 1, meanMJB)
        histSimMJB.SetBinError(i + 1, meanMJBUnc)
    
    
    # Save the two histograms to a file
    outFile = ROOT.TFile(args.output, 'recreate')
    
    dirName = 'MJB_Pt30'
    outFile.mkdir(dirName)
    outFile.cd(dirName)
    
    histData3D.Write()
    histSimMJB.Write()
    
    outFile.Close()
