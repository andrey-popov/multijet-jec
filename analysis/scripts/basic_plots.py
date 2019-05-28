#!/usr/bin/env python

"""Produces plots of basic observables.

Plots distributions of pt of the leading jet and the recoil and the
missing pt in data and simulation.  Also plots mean balance observables
in bins of pt of the leading jet.  In all cases the ratio between data
and simulation is included.
"""

import argparse
import json
import math
import os

import matplotlib as mpl
mpl.use('Agg')  # Use a non-interactive backend
from matplotlib import pyplot as plt

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True

from plotting import plot_distribution, plot_balance
from utils import RDFHists, mpl_style


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
        '-c', '--config', default='plot_config.json',
        help='JSON file with configuration for plotting'
    )
    arg_parser.add_argument(
        '-e', '--era', default=None,
        help='Data-taking period'
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
    
    
    ROOT.gROOT.SetBatch(True)
    plt.style.use(mpl_style)
    
    
    with open(args.config) as f:
        config = json.load(f)
    
    era_label = '{} {} fb$^{{-1}}$ (13 TeV)'.format(
        config['eras'][args.era]['label'], config['eras'][args.era]['lumi']
    )


    # Define histograms to be filled
    hist_pt_lead = RDFHists(
        ROOT.TH1D, config['binning']['ptLead'], ['PtJ1', 'weight'],
        ['data', 'sim']
    )
    hist_pt_recoil = RDFHists(
        ROOT.TH1D, config['binning']['ptRecoil'], ['PtRecoil', 'weight'],
        ['data', 'sim']
    )
    hist_pt_miss = RDFHists(
        ROOT.TH1D, config['binning']['ptMiss'], ['MET', 'weight'],
        ['data', 'sim']
    )

    bal_binning = config['balance']['binning']
    prof_pt_lead = RDFHists(
        ROOT.TProfile, bal_binning, ['PtJ1', 'PtJ1', 'weight'],
        ['data', 'sim']
    )
    prof_pt_bal = RDFHists(
        ROOT.TProfile, bal_binning, ['PtJ1', 'PtBal', 'weight'],
        ['data', 'sim']
    )
    prof_mpf = RDFHists(
        ROOT.TProfile, bal_binning, ['PtJ1', 'MPF', 'weight'],
        ['data', 'sim']
    )

    rdf_hists = [
        hist_pt_lead, hist_pt_recoil, hist_pt_miss,
        prof_pt_lead, prof_pt_bal, prof_mpf
    ]
    
    
    # Fill the histograms
    data_file = ROOT.TFile(args.data)
    sim_file = ROOT.TFile(args.sim)
    
    for trigger, pt_range in config['triggers'].items():
        tree_data = data_file.Get(trigger + '/BalanceVars')
        tree_sim = sim_file.Get(trigger + '/BalanceVars')

        for weight_tree_name in ['GenWeights', 'PeriodWeights']:
            tree_sim.AddFriend('{}/{}'.format(trigger, weight_tree_name))
        
        
        pt_selection = 'PtJ1 > {}'.format(pt_range[0])
        
        if not math.isinf(pt_range[1]):
            pt_selection += ' && PtJ1 < {}'.format(pt_range[1])
        
        for label, tree, selection in [
            ('data', tree_data, pt_selection),
            (
                'sim', tree_sim,
                '({}) * WeightGen * Weight_{}'.format(pt_selection, args.era)
            )
        ]:
            df = ROOT.RDataFrame(tree)
            df_filtered = df.Define('weight', selection).Filter('weight != 0')

            for hist in rdf_hists:
                hist.register(df_filtered)

            for hist in rdf_hists:
                hist.add(label)

    data_file.Close()
    sim_file.Close()
    
    
    # In distributions, include under- and overflow bins
    for p in [hist_pt_lead, hist_pt_recoil, hist_pt_miss]:
        for hist in p.hists.values():
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
        x_label=r'$p_\mathrm{T}^\mathrm{recoil}$ [GeV]', era_label=era_label, mark_underflow=True
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
        y_label=r'Mean $p_\mathrm{T}$ balance', era_label=era_label, balance_range=(0.85, 1.)
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
