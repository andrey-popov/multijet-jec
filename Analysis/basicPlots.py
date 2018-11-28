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


def make_hists(binning_def):
    """Create a pair of 1D histograms from given binning."""
    
    r = binning_def['range']
    binning = np.linspace(r[0], r[1], num=(r[1] - r[0]) / binning_def['step'] + 1)
    
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


def plot_distribution(
    hist_data, hist_sim, x_label='', y_label='Events', era_label='', height_ratio=3.
):
    """Plot distributions in data and simulation.
    
    Plot the two distributions and the deviations.  Under- and overflow
    bins of the histograms are ignored.
    """
    
    # Convert histograms to NumPy representations
    num_bins = hist_data.GetNbinsX()
    binning = np.zeros(num_bins + 1)
    bin_centres = np.zeros(num_bins)
    
    for bin in range(1, num_bins + 2):
        binning[bin - 1] = hist_data.GetBinLowEdge(bin)
    
    for i in range(len(bin_centres)):
        bin_centres[i] = (binning[i] + binning[i + 1]) / 2
    
    data_values, data_errors = np.zeros(num_bins), np.zeros(num_bins)
    sim_values, sim_errors = np.zeros(num_bins), np.zeros(num_bins)
    
    for bin in range(1, num_bins + 1):
        data_values[bin - 1] = hist_data.GetBinContent(bin)
        data_errors[bin - 1] = hist_data.GetBinError(bin)
        sim_values[bin - 1] = hist_sim.GetBinContent(bin)
        sim_errors[bin - 1] = hist_sim.GetBinError(bin)
    
    
    # Compute residuals, allowing for possible zero expectation
    residuals, res_data_errors, res_bin_centres = [], [], []
    
    for i in range(len(sim_values)):
        if sim_values[i] == 0.:
            continue
        
        residuals.append(data_values[i] / sim_values[i] - 1)
        res_data_errors.append(data_errors[i] / sim_values[i])
        res_bin_centres.append(bin_centres[i])
    
    res_sim_error_band = []
    
    for i in range(len(sim_values)):
        if sim_values[i] == 0.:
            res_sim_error_band.append(0.)
        else:
            res_sim_error_band.append(sim_errors[i] / sim_values[i])
    
    res_sim_error_band.append(res_sim_error_band[-1])
    res_sim_error_band = np.array(res_sim_error_band)
    
    
    # Plot the histograms
    fig = plt.figure()
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[height_ratio, 1])
    axes_upper = fig.add_subplot(gs[0, 0])
    axes_lower = fig.add_subplot(gs[1, 0])
    
    axes_upper.errorbar(
        bin_centres, data_values, yerr=data_errors,
        color='black', marker='o', ls='none', label='Data'
    )
    axes_upper.hist(
        binning[:-1], bins=binning, weights=sim_values, histtype='stepfilled',
        color='#3399cc', label='Sim'
    )
    
    axes_lower.fill_between(
        binning, res_sim_error_band, -res_sim_error_band,
        step='post', color='0.75', lw=0
    )
    axes_lower.errorbar(
        res_bin_centres, residuals, yerr=res_data_errors,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels on the x axis of the upper axes
    axes_upper.set_xticklabels([''] * len(axes_upper.get_xticklabels()))
    
    axes_upper.set_xlim(binning[0], binning[-1])
    axes_lower.set_xlim(binning[0], binning[-1])
    axes_lower.set_ylim(-0.35, 0.37)
    axes_lower.grid(axis='y', color='black', ls='dotted')
    
    axes_lower.set_xlabel(x_label)
    axes_lower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axes_upper.set_ylabel(y_label)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axes_upper.get_yaxis().set_label_coords(-0.1, 0.5)
    axes_lower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the overflow bin
    axes_upper.text(
        0.995, 0.5, 'Overflow', transform=axes_upper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    
    # Build legend ensuring desired ordering of the entries
    legend_handles, legend_labels = axes_upper.get_legend_handles_labels()
    legend_handle_map = {}
    
    for i in range(len(legend_handles)):
        legend_handle_map[legend_labels[i]] = legend_handles[i]
    
    axes_upper.legend(
        [legend_handle_map['Data'], legend_handle_map['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=False
    )
    
    axes_upper.text(1., 1., era_label, ha='right', va='bottom', transform=axes_upper.transAxes)
    
    return fig, axes_upper, axes_lower


def plot_balance(
    prof_pt_data, prof_pt_sim, prof_bal_data, prof_bal_sim,
    x_label=r'$p_\mathrm{T}^\mathrm{lead}$ [GeV]', y_label='', era_label='', height_ratio=2.
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
    num_bins = prof_pt_data.GetNbinsX()
    data_x = np.zeros(num_bins + 1)
    data_y, data_yerr = np.zeros(num_bins + 1), np.zeros(num_bins + 1)
    
    for bin in range(1, num_bins + 2):
        data_x[bin - 1] = prof_pt_data.GetBinContent(bin)
        data_y[bin - 1] = prof_bal_data.GetBinContent(bin)
        data_yerr[bin - 1] = prof_bal_data.GetBinError(bin)
    
    sim_x, sim_y, sim_yerr = np.zeros(num_bins + 1), np.zeros(num_bins + 1), np.zeros(num_bins + 1)
    
    for bin in range(1, num_bins + 2):
        sim_x[bin - 1] = prof_pt_sim.GetBinContent(bin)
        sim_y[bin - 1] = prof_bal_sim.GetBinContent(bin)
        sim_yerr[bin - 1] = prof_bal_sim.GetBinError(bin)
    
    sim_err_band_x = np.zeros(num_bins + 2)
    sim_err_band_y_low, sim_err_band_y_high = np.zeros(num_bins + 2), np.zeros(num_bins + 2)
    
    for bin in range(1, num_bins + 2):
        sim_err_band_x[bin - 1] = prof_pt_data.GetBinLowEdge(bin)
    
    # Since the last bin is the overflow bin, there is no natural upper
    # boundary for the error band.  Set ptMax = <pt> + |ptMin - <pt>|.
    sim_err_band_x[-1] = 2 * prof_pt_data.GetBinContent(num_bins + 1) - \
        prof_pt_data.GetBinLowEdge(num_bins + 1)
    
    sim_err_band_y_low = np.append(sim_y - sim_yerr, sim_y[-1] - sim_yerr[-1])
    sim_err_band_y_high = np.append(sim_y + sim_yerr, sim_y[-1] + sim_yerr[-1])
    
    
    # Compute residuals
    residuals = data_y / sim_y - np.ones(num_bins + 1)
    res_data_yerr = data_yerr / sim_y
    res_sim_err_band_y = np.append(sim_yerr / sim_y, sim_yerr[-1] / sim_y[-1])
    
    
    # Plot the graphs
    fig = plt.figure()
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[height_ratio, 1])
    axes_upper = fig.add_subplot(gs[0, 0])
    axes_lower = fig.add_subplot(gs[1, 0])
    
    axes_upper.set_xscale('log')
    axes_lower.set_xscale('log')
    
    axes_upper.errorbar(
        data_x, data_y, yerr=data_yerr,
        color='black', marker='o', ls='none', label='Data'
    )
    axes_upper.fill_between(
        sim_err_band_x, sim_err_band_y_low, sim_err_band_y_high, step='post',
        color='#3399cc', alpha=0.5, linewidth=0
    )
    axes_upper.plot(
        sim_x, sim_y,
        color='#3399cc', marker='o', mfc='none', ls='none', label='Sim'
    )
    
    axes_lower.fill_between(
        sim_err_band_x, res_sim_err_band_y, -res_sim_err_band_y, step='post', color='0.75'
    )
    axes_lower.errorbar(
        data_x, residuals, yerr=res_data_yerr,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels in the upper axes
    axes_upper.set_xticklabels([''] * len(axes_upper.get_xticklabels()))
    
    # Provide a formatter for minor ticks so that thay get labelled.
    # Also set a formatter for major ticks in order to obtain a
    # consistent formatting (1000 instead of 10^3).
    axes_lower.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
    axes_lower.xaxis.set_minor_formatter(mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4)))
    
    axes_upper.set_xlim(sim_err_band_x[0], sim_err_band_x[-1])
    axes_lower.set_xlim(sim_err_band_x[0], sim_err_band_x[-1])
    axes_upper.set_ylim(0.9, 1.)
    axes_lower.set_ylim(-0.02, 0.028)
    axes_lower.grid(axis='y', color='black', ls='dotted')
    
    axes_lower.set_xlabel(x_label)
    axes_lower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axes_upper.set_ylabel(y_label)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axes_upper.get_yaxis().set_label_coords(-0.1, 0.5)
    axes_lower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the overflow bin
    axes_upper.text(
        0.995, 0.5, 'Overflow', transform=axes_upper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    
    # Build legend ensuring desired ordering of the entries
    legend_handles, legend_labels = axes_upper.get_legend_handles_labels()
    legend_handle_map = {}
    
    for i in range(len(legend_handles)):
        legend_handle_map[legend_labels[i]] = legend_handles[i]
    
    axes_upper.legend(
        [legend_handle_map['Data'], legend_handle_map['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=True
    )
    
    axes_upper.text(1., 1., era_label, ha='right', va='bottom', transform=axes_upper.transAxes)
    
    return fig, axes_upper, axes_lower


if __name__ == '__main__':
    
    # Parse arguments
    arg_parser = argparse.ArgumentParser(description=__doc__)
    arg_parser.add_argument(
        'data', help='Name of ROOT file with data'
    )
    arg_parser.add_argument(
        'sim', help='Name of ROOT file with simulation'
    )
    arg_parser.add_argument(
        'weight_file', help='Name of ROOT file with weights for simulation'
    )
    arg_parser.add_argument(
        '-c', '--config', default='plotConfig.json',
        help='JSON file with configuration for plotting'
    )
    arg_parser.add_argument(
        '-e', '--era', default=None,
        help='Era to access luminosity and era label from configuration'
    )
    arg_parser.add_argument(
        '-o', '--fig-dir', default='fig',
        help='Directory to store figures'
    )
    args = arg_parser.parse_args()
    
    if not os.path.exists(args.fig_dir):
        os.makedirs(args.fig_dir)
    
    if args.era is None:
        # Try to figure the era label from the name of the data file
        args.era = os.path.splitext(os.path.basename(args.data))[0]
    
    
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
    
    era_label = '{} {} fb$^{{-1}}$ (13 TeV)'.format(
        config['eras'][args.era]['label'], config['eras'][args.era]['lumi']
    )
    
    
    # Construct histograms
    hist_pt_lead = make_hists(config['binning']['ptLead'])
    hist_pt_recoil = make_hists(config['binning']['ptRecoil'])
    hist_pt_miss = make_hists(config['binning']['ptMiss'])
    
    prof_pt_lead = make_profiles(config['balance']['binning'])
    prof_pt_bal = make_profiles(config['balance']['binning'])
    prof_mpf = make_profiles(config['balance']['binning'])
    
    
    # Fill the histograms
    data_file = ROOT.TFile(args.data)
    sim_file = ROOT.TFile(args.sim)
    weight_file = ROOT.TFile(args.weight_file)
    
    for trigger, pt_range in config['triggers'].items():
        
        tree_data = data_file.Get(trigger + '/BalanceVars')
        
        tree_sim = sim_file.Get(trigger + '/BalanceVars')
        tree_weight = weight_file.Get(trigger + '/Weights')
        tree_sim.AddFriend(tree_weight)
        
        for tree in [tree_data, tree_sim]:
            tree.SetBranchStatus('A', False)
            tree.SetBranchStatus('Alpha', False)
        
        tree_sim.SetBranchStatus('WeightDataset', False)
        
        
        ROOT.gROOT.cd()
        
        pt_selection = 'PtJ1 > {}'.format(pt_range[0])
        
        if not math.isinf(pt_range[1]):
            pt_selection += ' && PtJ1 < {}'.format(pt_range[1])
        
        for label, tree, selection in [
            ('data', tree_data, pt_selection),
            ('sim', tree_sim, '({}) * TotalWeight[0]'.format(pt_selection))
        ]:
            tree.Draw('PtJ1>>+' + hist_pt_lead[label].GetName(), selection, 'goff')
            tree.Draw('PtRecoil>>+' + hist_pt_recoil[label].GetName(), selection, 'goff')
            tree.Draw('MET>>+' + hist_pt_miss[label].GetName(), selection, 'goff')
            
            tree.Draw('PtJ1:PtJ1>>+' + prof_pt_lead[label].GetName(), selection, 'goff')
            tree.Draw('PtBal:PtJ1>>+' + prof_pt_bal[label].GetName(), selection, 'goff')
            tree.Draw('MPF:PtJ1>>+' + prof_mpf[label].GetName(), selection, 'goff')
    
    data_file.Close()
    sim_file.Close()
    weight_file.Close()
    
    
    # In distributions, include under- and overflow bins
    for p in [hist_pt_lead, hist_pt_recoil, hist_pt_miss]:
        for hist in p.values():
            hist.SetBinContent(1, hist.GetBinContent(1) + hist.GetBinContent(0))
            hist.SetBinError(1, math.hypot(hist.GetBinError(1), hist.GetBinError(0)))
            
            num_bins = hist.GetNbinsX()
            hist.SetBinContent(num_bins, hist.GetBinContent(num_bins) + hist.GetBinContent(num_bins + 1))
            hist.SetBinError(
                num_bins, math.hypot(hist.GetBinError(num_bins), hist.GetBinError(num_bins + 1))
            )
    
    
    # Plot distributions
    fig, axes_upper, axes_lower = plot_distribution(
        hist_pt_lead['data'], hist_pt_lead['sim'],
        x_label=r'$p_\mathrm{T}^\mathrm{lead}$ [GeV]', era_label=era_label
    )
    
    for trigger_range in config['triggers'].values():
        min_pt = trigger_range[0]
        axes_lower.axvline(min_pt, color='#ff9933', ls='dashed')
        axes_upper.axvline(min_pt, color='#ff9933', ls='dashed')
    
    fig.savefig(os.path.join(args.fig_dir, 'PtLead.pdf'))
    plt.close(fig)
    
    
    fig, axes_upper, axes_lower = plot_distribution(
        hist_pt_recoil['data'], hist_pt_recoil['sim'],
        x_label=r'$p_\mathrm{T}^\mathrm{recoil}$ [GeV]', era_label=era_label
    )
    fig.savefig(os.path.join(args.fig_dir, 'PtRecoil.pdf'))
    plt.close(fig)
    
    fig, axes_upper, axes_lower = plot_distribution(
        hist_pt_miss['data'], hist_pt_miss['sim'],
        x_label=r'$p_\mathrm{T}^\mathrm{miss}$ [GeV]', era_label=era_label
    )
    fig.savefig(os.path.join(args.fig_dir, 'PtMiss.pdf'))
    plt.close(fig)
    
    
    # Plot profiles
    fig, axes_upper, axes_lower = plot_balance(
        prof_pt_lead['data'], prof_pt_lead['sim'],
        prof_pt_bal['data'], prof_pt_bal['sim'],
        y_label=r'Mean $p_\mathrm{T}$ balance', era_label=era_label
    )
    fig.savefig(os.path.join(args.fig_dir, 'PtBal.pdf'))
    plt.close(fig)
    
    fig, axes_upper, axes_lower = plot_balance(
        prof_pt_lead['data'], prof_pt_lead['sim'],
        prof_mpf['data'], prof_mpf['sim'],
        y_label=r'Mean MPF', era_label=era_label
    )
    fig.savefig(os.path.join(args.fig_dir, 'MPF.pdf'))
    plt.close(fig)
