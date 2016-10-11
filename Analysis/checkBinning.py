#!/usr/bin/env python

"""Checks numbers of events in bins in several observables."""

from array import array
import json
import math

import ROOT
ROOT.gROOT.SetBatch(True)

from triggerclip import TriggerClip


if __name__ == '__main__':
    
    # Histograms to be filled
    with open('binning.json') as binningFile:
        binningInfo = json.load(binningFile)
        
    binning = array('d', binningInfo['Input']['PtLeadProj'])
    histPtLeadProj = ROOT.TH1D('histPtLeadProj', '', len(binning) - 1, binning)
    
    binning = array('d', binningInfo['Input']['MJB'])
    histMJB = ROOT.TH1D('histMJB', '', len(binning) - 1, binning)
    
    binning = array('d', binningInfo['Input']['F'])
    histF = ROOT.TH1D('histF', '', len(binning) - 1, binning)
    
    
    # Input file and object to verify clipping to trigger bins
    inputFile = ROOT.TFile('tuples/data-Run2016G.root')
    tree = inputFile.Get('BalanceVarsPt30')
    
    tree.SetBranchStatus('*', False)
    for branchName in ['PtJ1', 'Alpha', 'TriggerBin', 'F_Linear', 'MJB']:
        tree.SetBranchStatus(branchName, True)
    
    clip = TriggerClip('triggerBins.json')
    
    
    # Fill the histograms
    for event in tree:
        
        ptLeadHat = event.PtJ1 * math.cos(event.Alpha)
        if not clip.check_pt(event.TriggerBin, ptLeadHat):
            continue
        
        histPtLeadProj.Fill(ptLeadHat)
        histF.Fill(tree.F_Linear)
        histMJB.Fill(tree.MJB)
    
    inputFile.Close()
    
    
    # Print results
    for hist in [histPtLeadProj, histF, histMJB]:
        
        print('Results for histogram "{}":'.format(hist.GetName()))
        totalEvents = hist.GetEntries()
        
        for bin in range(hist.GetNbinsX() + 2):
            content = hist.GetBinContent(bin)
            print(
                '  Bin {bin:3d}: {content:6d}  ({fraction:6.3f}%)   {leftEdge} -- {rightEdge}'.\
                format(
                    bin=bin, content=int(content), fraction=content / totalEvents * 100.,
                    leftEdge=round(hist.GetBinLowEdge(bin), 5),
                    rightEdge=round(hist.GetBinLowEdge(bin) + hist.GetBinWidth(bin), 5)
                )
            )
        
        print()
