#!/usr/bin/env python

"""Produces plots with a comparison between data and simulation.

Plots distributions of pt of the leading jet and the recoil and the
missing pt in data and simulation.  Also plots mean balance observables
in bins of pt of the leading jet.  In addition, produces control
distributions of balance observables in wide bins in pt of the leading
jet.  In all cases the ratio between data and simulation is included.
"""

import argparse
import json
import math
import os
from uuid import uuid4

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
        '-d', '--data', nargs='+',
        help='Input files with data (required)'
    )
    arg_parser.add_argument(
        '-s', '--sim', nargs='+',
        help='Input files with simulation (required)'
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

    control_fig_dir = os.path.join(args.fig_dir, 'control')

    for fig_dir in [args.fig_dir, control_fig_dir]:
        try:
            os.makedirs(fig_dir)
        except FileExistsError:
            pass
    
    if args.era is None and len(args.data) == 1:
        # Try to figure the era label from the name of the data file
        args.era = os.path.splitext(os.path.basename(args.data[0]))[0]
    
    
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

    binning_pt = config['control']['binning']
    binning_bal = {'range': [0., 2.], 'step': 0.025}
    
    hist_2d_pt_bal = RDFHists(
        ROOT.TH2D, [binning_bal, binning_pt], ['PtBal', 'PtJ1', 'weight'],
        ['data', 'sim']
    )
    hist_2d_mpf = RDFHists(
        ROOT.TH2D, [binning_bal, binning_pt], ['MPF', 'PtJ1', 'weight'],
        ['data', 'sim']
    )

    rdf_hists = [
        hist_pt_lead, hist_pt_recoil, hist_pt_miss,
        prof_pt_lead, prof_pt_bal, prof_mpf,
        hist_2d_pt_bal, hist_2d_mpf
    ]
    
    
    # Fill the histograms
    for trigger, pt_range in config['triggers'].items():
        chain_data = ROOT.TChain(trigger + '/BalanceVars')

        for f in args.data:
            chain_data.AddFile(f)

        chain_sim = ROOT.TChain(trigger + '/BalanceVars')
        chain_sim_friends = [
            ROOT.TChain('{}/{}'.format(trigger, name))
            for name in ['GenWeights', 'PeriodWeights']
        ]

        for f in args.sim:
            for chain in [chain_sim] + chain_sim_friends:
                chain.AddFile(f)

        for friend in chain_sim_friends:
            chain_sim.AddFriend(friend)
        
        
        pt_selection = 'PtJ1 > {}'.format(pt_range[0])
        
        if not math.isinf(pt_range[1]):
            pt_selection += ' && PtJ1 < {}'.format(pt_range[1])
        
        for label, tree, selection in [
            ('data', chain_data, pt_selection),
            (
                'sim', chain_sim,
                '({}) * WeightGen * Weight_{}'.format(pt_selection, args.era)
            )
        ]:
            df = ROOT.RDataFrame(tree)
            df_filtered = df.Define('weight', selection).Filter('weight != 0')

            for hist in rdf_hists:
                hist.register(df_filtered)

            for hist in rdf_hists:
                hist.add(label)
    
    
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

    for hist in (
        h for d in [hist_2d_pt_bal, hist_2d_mpf] for h in d.hists.values()
    ):
        for pt_bin in range(0, hist.GetNbinsY() + 2):
            hist.SetBinContent(
                1, pt_bin,
                hist.GetBinContent(1, pt_bin) + hist.GetBinContent(0, pt_bin)
            )
            hist.SetBinError(
                1, pt_bin,
                math.hypot(hist.GetBinError(1, pt_bin), hist.GetBinError(0, pt_bin))
            )
            
            n = hist.GetNbinsX()
            hist.SetBinContent(
                n, pt_bin,
                hist.GetBinContent(n, pt_bin) + hist.GetBinContent(n + 1, pt_bin)
            )
            hist.SetBinError(
                n, pt_bin,
                math.hypot(hist.GetBinError(n, pt_bin), hist.GetBinError(n + 1, pt_bin))
            )
    
    
    # Plot 1D distributions
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


    # Plot distributions of balance observables in bins of pt of the
    # leading jet.  They are constructed from slices of the 2D
    # histograms.
    for hist_bal, label, x_label in [
        (hist_2d_pt_bal, 'PtBal', r'$p_\mathrm{T}$ balance'),
        (hist_2d_mpf, 'MPF', 'MPF')
    ]:
        num_bins_pt = hist_bal['data'].GetNbinsY()
        
        for pt_bin in range(1, num_bins_pt + 2):
            hist_data = hist_bal['data'].ProjectionX(uuid4().hex, pt_bin, pt_bin, 'e')
            hist_sim = hist_bal['sim'].ProjectionX(uuid4().hex, pt_bin, pt_bin, 'e')
            
            fig, axes_upper, axes_lower = plot_distribution(
                hist_data, hist_sim, x_label=x_label, era_label=era_label
            )
            
            if pt_bin == num_bins_pt + 1:
                pt_bin_label = r'$p_\mathrm{{T}}^\mathrm{{lead}} > {:g}$ GeV'.format(
                    hist_bal['data'].GetYaxis().GetBinLowEdge(pt_bin)
                )
            else:
                pt_bin_label = r'${:g} < p_\mathrm{{T}}^\mathrm{{lead}} < {:g}$ GeV'.format(
                    hist_bal['data'].GetYaxis().GetBinLowEdge(pt_bin),
                    hist_bal['data'].GetYaxis().GetBinLowEdge(pt_bin + 1)
                )
            
            axes_upper.text(
                0., 1., pt_bin_label, ha='left', va='bottom', transform=axes_upper.transAxes
            )
            
            
            # Move the common exponent so that it does not collide with
            # the pt bin label.  Technically, the actual exponent is
            # turned invisible, and a new text object is added.
            # Check the value of the common exponent as determined by
            # the tick formatter.  To do it, need to feed locations of
            # ticks to the formatter first.
            ax = axes_upper.get_yaxis()
            major_locs = ax.major.locator()
            formatter = ax.get_major_formatter()
            formatter.set_locs(major_locs)
            
            if formatter.orderOfMagnitude != 0:
                # There is indeed a common exponent
                ax.offsetText.set_visible(False)
                
                axes_upper.text(
                    0., 1.05, '$\\times 10^{}$'.format(formatter.orderOfMagnitude),
                    ha='right', va='bottom', transform=axes_upper.transAxes
                )
            
            
            fig.savefig(os.path.join(
                control_fig_dir, '{}_ptBin{}.pdf'.format(label, pt_bin)
            ))
            plt.close(fig)
