#!/usr/bin/env python

"""Produces control plots for the multijet analysis."""

from array import array
import argparse
import json
import math
import os
from uuid import uuid4

import matplotlib as mpl
mpl.use('Agg')
from matplotlib import pyplot as plt

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True


from basicPlots import plot_distribution


if __name__ == '__main__':
    
    # Parse arguments
    argParser = argparse.ArgumentParser(description=__doc__)
    argParser.add_argument(
        'dataFile', help='Name of ROOT file with data'
    )
    argParser.add_argument(
        'simFile', help='Name of ROOT file with simulation'
    )
    argParser.add_argument(
        'weightFile', help='Name of ROOT file with weights for simulation'
    )
    argParser.add_argument(
        '-c', '--config', default='plotConfig.json',
        help='JSON file with configuration for plotting'
    )
    argParser.add_argument(
        '-e', '--era', default=None,
        help='Era to access luminosity and era label from configuration'
    )
    argParser.add_argument(
        '-o', '--fig-dir',
        help='Directory to store figures', default='figControl', dest='figDir'
    )
    args = argParser.parse_args()
    
    if not os.path.exists(args.figDir):
        os.makedirs(args.figDir)
    
    if args.era is None:
        # Try to figure the era label from the name of the data file
        args.era = os.path.splitext(os.path.basename(args.dataFile))[0]
    
    
    # Customization
    ROOT.gROOT.SetBatch(True)
    
    mpl.rc('figure', figsize=(6.0, 4.8))
    
    mpl.rc('xtick', top=True, direction='in')
    mpl.rc('ytick', right=True, direction='in')
    mpl.rc(['xtick.minor', 'ytick.minor'], visible=True)
    
    mpl.rc('lines', linewidth=1., markersize=2.)
    mpl.rc('errorbar', capsize=1.)
    
    mpl.rc('axes.formatter', limits=[-3, 4], use_mathtext=True)
    mpl.rc('axes', labelsize='large')
    
    
    with open(args.config) as f:
        config = json.load(f)
    
    eraLabel = '{} {} fb$^{{-1}}$ (13 TeV)'.format(
        config['eras'][args.era]['label'], config['eras'][args.era]['lumi']
    )
    
    
    # 2D histograms for balance observables.  Their slices will be
    # plotted.
    binning = array('d', config['control']['binning'])
    hist = ROOT.TH2D(uuid4().hex, '', 80, 0., 2., len(binning) - 1, binning)
    histPtBal = {'data': hist, 'sim': hist.Clone(uuid4().hex)}
    histMPF = {'data': hist.Clone(uuid4().hex), 'sim': hist.Clone(uuid4().hex)}
    
    for d in [histPtBal, histMPF]:
        for hist in d.values():
            hist.SetDirectory(ROOT.gROOT)
    
    
    # Fill the histograms
    dataFile = ROOT.TFile(args.dataFile)
    simFile = ROOT.TFile(args.simFile)
    weightFile = ROOT.TFile(args.weightFile)
    
    for trigger, ptRange in config['triggers'].items():
        
        treeData = dataFile.Get(trigger + '/BalanceVars')
        
        treeSim = simFile.Get(trigger + '/BalanceVars')
        treeWeight = weightFile.Get(trigger + '/Weights')
        treeSim.AddFriend(treeWeight)
        
        for tree in [treeData, treeSim]:
            tree.SetBranchStatus('*', False)
            
            for branchName in ['PtJ1', 'PtBal', 'MPF']:
                tree.SetBranchStatus(branchName, True)
        
        treeSim.SetBranchStatus('TotalWeight', True)
        
        
        ROOT.gROOT.cd()
        
        ptSelection = 'PtJ1 > {}'.format(ptRange[0])
        
        if not math.isinf(ptRange[1]):
            ptSelection += ' && PtJ1 < {}'.format(ptRange[1])
        
        for label, tree, selection in [
            ('data', treeData, ptSelection),
            ('sim', treeSim, '({}) * TotalWeight[0]'.format(ptSelection))
        ]:
            tree.Draw('PtJ1:PtBal>>+' + histPtBal[label].GetName(), selection, 'goff')
            tree.Draw('PtJ1:MPF>>+' + histMPF[label].GetName(), selection, 'goff')
    
    dataFile.Close()
    simFile.Close()
    weightFile.Close()
    
    
    # Add under- and overflows in balance
    for d in [histPtBal, histMPF]:
        for hist in d.values():
            for ptBin in range(0, hist.GetNbinsY() + 2):
                hist.SetBinContent(
                    1, ptBin,
                    hist.GetBinContent(1, ptBin) + hist.GetBinContent(0, ptBin)
                )
                hist.SetBinError(
                    1, ptBin,
                    math.hypot(hist.GetBinError(1, ptBin), hist.GetBinError(0, ptBin))
                )
                
                n = hist.GetNbinsX()
                hist.SetBinContent(
                    n, ptBin,
                    hist.GetBinContent(n, ptBin) + hist.GetBinContent(n + 1, ptBin)
                )
                hist.SetBinError(
                    n, ptBin,
                    math.hypot(hist.GetBinError(n, ptBin), hist.GetBinError(n + 1, ptBin))
                )
    
    
    # Plot distributions of balance observables
    for histBal, label, xLabel in [
        (histPtBal, 'PtBal', r'$p_\mathrm{T}$ balance'),
        (histMPF, 'MPF', 'MPF')
    ]:
        nBinsPt = histBal['data'].GetNbinsY()
        
        for ptBin in range(1, nBinsPt + 2):
            histData = histBal['data'].ProjectionX(uuid4().hex, ptBin, ptBin, 'e')
            histSim = histBal['sim'].ProjectionX(uuid4().hex, ptBin, ptBin, 'e')
            
            fig, axesUpper, axesLower = plot_distribution(
                histData, histSim, xLabel=xLabel, eraLabel=eraLabel
            )
            
            if ptBin == nBinsPt + 1:
                ptBinLabel = r'$p_\mathrm{{T}}^\mathrm{{lead}} > {:g}$ GeV'.format(
                    histBal['data'].GetYaxis().GetBinLowEdge(ptBin)
                )
            else:
                ptBinLabel = r'${:g} < p_\mathrm{{T}}^\mathrm{{lead}} < {:g}$ GeV'.format(
                    histBal['data'].GetYaxis().GetBinLowEdge(ptBin),
                    histBal['data'].GetYaxis().GetBinLowEdge(ptBin + 1)
                )
            
            axesUpper.text(
                0., 1., ptBinLabel, ha='left', va='bottom', transform=axesUpper.transAxes
            )
            
            
            # Move the common exponent so that it does not collide with
            # the pt bin label.  Technically, the actual exponent is
            # turned invisible, and a new text object is added.
            # Check the value of the common exponent as determined by
            # the tick formatter.  To do it, need to feed locations of
            # ticks to the formatter first.
            ax = axesUpper.get_yaxis()
            majorLocs = ax.major.locator()
            formatter = ax.get_major_formatter()
            formatter.set_locs(majorLocs)
            
            if formatter.orderOfMagnitude != 0:
                # There is indeed a common exponent
                ax.offsetText.set_visible(False)
                
                axesUpper.text(
                    0., 1.05, '$\\times 10^{}$'.format(formatter.orderOfMagnitude),
                    ha='right', va='bottom', transform=axesUpper.transAxes
                )
            
            
            fig.savefig(os.path.join(args.figDir, '{}_ptBin{}.pdf'.format(label, ptBin)))
            plt.close(fig)
