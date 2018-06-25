#!/usr/bin/env python

"""Produces plots of basic observables.

Plots distributions of pt of the leading jet and the recoil and the
missing pt in data and simulation.  Also plots mean balance observables
in bins of pt of the leading jet.  In all cases the ratio between data
and simulation is included.
"""

from array import array
import argparse
import json
import math
import os
from uuid import uuid4

import numpy as np

import matplotlib as mpl
mpl.use('Agg')  # Use a non-interactive backend
from matplotlib import pyplot as plt

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True


def make_hists(binningDef):
    """Create a pair of 1D histograms from given binning."""
    
    r = binningDef['range']
    binning = np.linspace(r[0], r[1], num=(r[1] - r[0]) / binningDef['step'] + 1)
    
    hist1 = ROOT.TH1D(uuid4().hex, '', len(binning) - 1, binning)
    hist1.SetDirectory(ROOT.gROOT)
    
    hist2 = hist1.Clone(uuid4().hex)
    
    return {'data': hist1, 'sim': hist2}


def make_profiles(binning):
    """Create a pair of 1D profiles from given binning."""
    
    binning = array('d', binning)
    prof1 = ROOT.TProfile(uuid4().hex, '', len(binning) - 1, binning)
    prof1.SetDirectory(ROOT.gROOT)
    prof2 = prof1.Clone(uuid4().hex)
    
    return {'data': prof1, 'sim': prof2}


def plot_distribution(histData, histSim, xLabel='', yLabel='Events', eraLabel='', heightRatio=3.):
    """Plot distributions in data and simulation.
    
    Plot the two distributions and the deviations.  Under- and overflow
    bins of the histograms are ignored.
    """
    
    # Convert histograms to NumPy representations
    nBins = histData.GetNbinsX()
    binning = np.zeros(nBins + 1)
    binCentres = np.zeros(nBins)
    
    for bin in range(1, nBins + 2):
        binning[bin - 1] = histData.GetBinLowEdge(bin)
    
    for i in range(len(binCentres)):
        binCentres[i] = (binning[i] + binning[i + 1]) / 2
    
    dataValues, dataErrors = np.zeros(nBins), np.zeros(nBins)
    simValues, simErros = np.zeros(nBins), np.zeros(nBins)
    
    for bin in range(1, nBins + 1):
        dataValues[bin - 1] = histData.GetBinContent(bin)
        dataErrors[bin - 1] = histData.GetBinError(bin)
        simValues[bin - 1] = histSim.GetBinContent(bin)
        simErros[bin - 1] = histSim.GetBinError(bin)
    
    
    # Compute residuals, allowing for possible zero expectation
    residuals, resDataErrors, resBinCentres = [], [], []
    
    for i in range(len(simValues)):
        if simValues[i] == 0.:
            continue
        
        residuals.append(dataValues[i] / simValues[i] - 1)
        resDataErrors.append(dataErrors[i] / simValues[i])
        resBinCentres.append(binCentres[i])
    
    resSimErrorBand = []
    
    for i in range(len(simValues)):
        if simValues[i] == 0.:
            resSimErrorBand.append(0.)
        else:
            resSimErrorBand.append(simErros[i] / simValues[i])
    
    resSimErrorBand.append(resSimErrorBand[-1])
    resSimErrorBand = np.array(resSimErrorBand)
    
    
    # Plot the histograms
    fig = plt.figure()
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[heightRatio, 1])
    axesUpper = fig.add_subplot(gs[0, 0])
    axesLower = fig.add_subplot(gs[1, 0])
    
    axesUpper.errorbar(
        binCentres, dataValues, yerr=dataErrors,
        color='black', marker='o', ls='none', label='Data'
    )
    axesUpper.hist(
        binning[:-1], bins=binning, weights=simValues, histtype='stepfilled',
        color='#3399cc', label='Sim'
    )
    
    axesLower.fill_between(
        binning, resSimErrorBand, -resSimErrorBand,
        step='post', color='0.75', lw=0
    )
    axesLower.errorbar(
        resBinCentres, residuals, yerr=resDataErrors,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels on the x axis of the upper axes
    axesUpper.set_xticklabels([''] * len(axesUpper.get_xticklabels()))
    
    axesUpper.set_xlim(binning[0], binning[-1])
    axesLower.set_xlim(binning[0], binning[-1])
    axesLower.set_ylim(-0.35, 0.37)
    axesLower.grid(axis='y', color='black', ls='dotted')
    
    axesLower.set_xlabel(xLabel)
    axesLower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axesUpper.set_ylabel(yLabel)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axesUpper.get_yaxis().set_label_coords(-0.1, 0.5)
    axesLower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the overflow bin
    axesUpper.text(
        0.995, 0.5, 'Overflow', transform=axesUpper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    
    # Build legend ensuring desired ordering of the entries
    legendHandles, legendLabels = axesUpper.get_legend_handles_labels()
    legendHandleMap = {}
    
    for i in range(len(legendHandles)):
        legendHandleMap[legendLabels[i]] = legendHandles[i]
    
    axesUpper.legend(
        [legendHandleMap['Data'], legendHandleMap['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=False
    )
    
    axesUpper.text(1., 1., eraLabel, ha='right', va='bottom', transform=axesUpper.transAxes)
    
    return fig, axesUpper, axesLower


def plot_balance(
    profPtData, profPtSim, profBalData, profBalSim,
    xLabel=r'$p_\mathrm{T}^\mathrm{lead}$ [GeV]', yLabel='', eraLabel='', heightRatio=2.
):
    """Plot mean balance in data and simulation.
    
    Plot mean values of the balance observable in bins of pt of the
    leading jet, together with the ratio between data and simulation.
    In the upper panel x positions of markers are given by the profile
    of the leading jet's pt (and therefore might differ between data and
    simulation).  In the residuals panel mean pt in data is used as the
    position.  The overflow bin is plotted.
    """
    
    # Convert profiles to NumPy representations
    nBins = profPtData.GetNbinsX()
    dataX, dataY, dataYErr = np.zeros(nBins + 1), np.zeros(nBins + 1), np.zeros(nBins + 1)
    
    for bin in range(1, nBins + 2):
        dataX[bin - 1] = profPtData.GetBinContent(bin)
        dataY[bin - 1] = profBalData.GetBinContent(bin)
        dataYErr[bin - 1] = profBalData.GetBinError(bin)
    
    simX, simY, simYErr = np.zeros(nBins + 1), np.zeros(nBins + 1), np.zeros(nBins + 1)
    
    for bin in range(1, nBins + 2):
        simX[bin - 1] = profPtSim.GetBinContent(bin)
        simY[bin - 1] = profBalSim.GetBinContent(bin)
        simYErr[bin - 1] = profBalSim.GetBinError(bin)
    
    simErrBandX = np.zeros(nBins + 2)
    simErrBandYLow, simErrBandYHigh = np.zeros(nBins + 2), np.zeros(nBins + 2)
    
    for bin in range(1, nBins + 2):
        simErrBandX[bin - 1] = profPtData.GetBinLowEdge(bin)
    
    # Since the last bin is the overflow bin, there is no natural upper
    # boundary for the error band.  Set ptMax = <pt> + |ptMin - <pt>|.
    simErrBandX[-1] = 2 * profPtData.GetBinContent(nBins + 1) - profPtData.GetBinLowEdge(nBins + 1)
    
    simErrBandYLow = np.append(simY - simYErr, simY[-1] - simYErr[-1])
    simErrBandYHigh = np.append(simY + simYErr, simY[-1] + simYErr[-1])
    
    
    # Compute residuals
    residuals = dataY / simY - np.ones(nBins + 1)
    resDataYErr = dataYErr / simY
    resSimErrBandY = np.append(simYErr / simY, simYErr[-1] / simY[-1])
    
    
    # Plot the graphs
    fig = plt.figure()
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[heightRatio, 1])
    axesUpper = fig.add_subplot(gs[0, 0])
    axesLower = fig.add_subplot(gs[1, 0])
    
    axesUpper.set_xscale('log')
    axesLower.set_xscale('log')
    
    axesUpper.errorbar(
        dataX, dataY, yerr=dataYErr,
        color='black', marker='o', ls='none', label='Data'
    )
    axesUpper.fill_between(
        simErrBandX, simErrBandYLow, simErrBandYHigh, step='post',
        color='#3399cc', alpha=0.5, linewidth=0
    )
    axesUpper.plot(
        simX, simY,
        color='#3399cc', marker='o', mfc='none', ls='none', label='Sim'
    )
    
    axesLower.fill_between(simErrBandX, resSimErrBandY, -resSimErrBandY, step='post', color='0.75')
    axesLower.errorbar(
        dataX, residuals, yerr=resDataYErr,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels in the upper axes
    axesUpper.set_xticklabels([''] * len(axesUpper.get_xticklabels()))
    
    # Provide a formatter for minor ticks so that thay get labelled.
    # Also set a formatter for major ticks in order to obtain a
    # consistent formatting (1000 instead of 10^3).
    axesLower.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
    axesLower.xaxis.set_minor_formatter(mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4)))
    
    axesUpper.set_xlim(simErrBandX[0], simErrBandX[-1])
    axesLower.set_xlim(simErrBandX[0], simErrBandX[-1])
    axesUpper.set_ylim(0.9, 1.)
    axesLower.set_ylim(-0.02, 0.028)
    axesLower.grid(axis='y', color='black', ls='dotted')
    
    axesLower.set_xlabel(xLabel)
    axesLower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axesUpper.set_ylabel(yLabel)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axesUpper.get_yaxis().set_label_coords(-0.1, 0.5)
    axesLower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the overflow bin
    axesUpper.text(
        0.995, 0.5, 'Overflow', transform=axesUpper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    
    # Build legend ensuring desired ordering of the entries
    legendHandles, legendLabels = axesUpper.get_legend_handles_labels()
    legendHandleMap = {}
    
    for i in range(len(legendHandles)):
        legendHandleMap[legendLabels[i]] = legendHandles[i]
    
    axesUpper.legend(
        [legendHandleMap['Data'], legendHandleMap['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=True
    )
    
    axesUpper.text(1., 1., eraLabel, ha='right', va='bottom', transform=axesUpper.transAxes)
    
    return fig, axesUpper, axesLower


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
        help='Directory to store figures', default='fig', dest='figDir'
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
    
    
    # Construct histograms
    histPtLead = make_hists(config['binning']['ptLead'])
    histPtRecoil = make_hists(config['binning']['ptRecoil'])
    histPtMiss = make_hists(config['binning']['ptMiss'])
    
    profPtLead = make_profiles(config['balance']['binning'])
    profPtBal = make_profiles(config['balance']['binning'])
    profMPF = make_profiles(config['balance']['binning'])
    
    
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
            tree.SetBranchStatus('A', False)
            tree.SetBranchStatus('Alpha', False)
        
        treeSim.SetBranchStatus('WeightDataset', False)
        
        
        ROOT.gROOT.cd()
        
        ptSelection = 'PtJ1 > {}'.format(ptRange[0])
        
        if not math.isinf(ptRange[1]):
            ptSelection += ' && PtJ1 < {}'.format(ptRange[1])
        
        for label, tree, selection in [
            ('data', treeData, ptSelection),
            ('sim', treeSim, '({}) * TotalWeight[0]'.format(ptSelection))
        ]:
            tree.Draw('PtJ1>>+' + histPtLead[label].GetName(), selection, 'goff')
            tree.Draw('PtRecoil>>+' + histPtRecoil[label].GetName(), selection, 'goff')
            tree.Draw('MET>>+' + histPtMiss[label].GetName(), selection, 'goff')
            
            tree.Draw('PtJ1:PtJ1>>+' + profPtLead[label].GetName(), selection, 'goff')
            tree.Draw('PtBal:PtJ1>>+' + profPtBal[label].GetName(), selection, 'goff')
            tree.Draw('MPF:PtJ1>>+' + profMPF[label].GetName(), selection, 'goff')
    
    dataFile.Close()
    simFile.Close()
    weightFile.Close()
    
    
    # In distributions, include under- and overflow bins
    for p in [histPtLead, histPtRecoil, histPtMiss]:
        for hist in p.values():
            hist.SetBinContent(1, hist.GetBinContent(1) + hist.GetBinContent(0))
            hist.SetBinError(1, math.hypot(hist.GetBinError(1), hist.GetBinError(0)))
            
            nBins = hist.GetNbinsX()
            hist.SetBinContent(nBins, hist.GetBinContent(nBins) + hist.GetBinContent(nBins + 1))
            hist.SetBinError(
                nBins, math.hypot(hist.GetBinError(nBins), hist.GetBinError(nBins + 1))
            )
    
    
    # Plot distributions
    fig, axesUpper, axesLower = plot_distribution(
        histPtLead['data'], histPtLead['sim'],
        xLabel=r'$p_\mathrm{T}^\mathrm{lead}$ [GeV]', eraLabel=eraLabel
    )
    
    for triggerRange in config['triggers'].values():
        minPt = triggerRange[0]
        axesLower.axvline(minPt, color='#ff9933', ls='dashed')
        axesUpper.axvline(minPt, color='#ff9933', ls='dashed')
    
    fig.savefig(os.path.join(args.figDir, 'PtLead.pdf'))
    plt.close(fig)
    
    
    fig, axesUpper, axesLower = plot_distribution(
        histPtRecoil['data'], histPtRecoil['sim'],
        xLabel=r'$p_\mathrm{T}^\mathrm{recoil}$ [GeV]', eraLabel=eraLabel
    )
    fig.savefig(os.path.join(args.figDir, 'PtRecoil.pdf'))
    plt.close(fig)
    
    fig, axesUpper, axesLower = plot_distribution(
        histPtMiss['data'], histPtMiss['sim'],
        xLabel=r'$p_\mathrm{T}^\mathrm{miss}$ [GeV]', eraLabel=eraLabel
    )
    fig.savefig(os.path.join(args.figDir, 'PtMiss.pdf'))
    plt.close(fig)
    
    
    # Plot profiles
    fig, axesUpper, axesLower = plot_balance(
        profPtLead['data'], profPtLead['sim'],
        profPtBal['data'], profPtBal['sim'],
        yLabel=r'Mean $p_\mathrm{T}$ balance', eraLabel=eraLabel
    )
    fig.savefig(os.path.join(args.figDir, 'PtBal.pdf'))
    plt.close(fig)
    
    fig, axesUpper, axesLower = plot_balance(
        profPtLead['data'], profPtLead['sim'],
        profMPF['data'], profMPF['sim'],
        yLabel=r'Mean MPF', eraLabel=eraLabel
    )
    fig.savefig(os.path.join(args.figDir, 'MPF.pdf'))
    plt.close(fig)
