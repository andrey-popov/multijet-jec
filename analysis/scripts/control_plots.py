#!/usr/bin/env python

"""Produces control plots for the multijet analysis."""

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

from plotting import plot_distribution
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
    
    
    ROOT.gROOT.SetBatch(True)
    plt.style.use(mpl_style)
    
    
    with open(args.config) as f:
        config = json.load(f)
    
    era_label = '{} {} fb$^{{-1}}$ (13 TeV)'.format(
        config['eras'][args.era]['label'], config['eras'][args.era]['lumi']
    )
    
    
    # 2D histograms for balance observables.  Their slices will be
    # plotted.
    binning_pt = config['control']['binning']
    binning_bal = {'range': [0., 2.], 'step': 0.025}
    
    hist_pt_bal = RDFHists(
        ROOT.TH2D, [binning_bal, binning_pt], ['PtBal', 'PtJ1', 'weight'],
        ['data', 'sim']
    )
    hist_mpf = RDFHists(
        ROOT.TH2D, [binning_bal, binning_pt], ['MPF', 'PtJ1', 'weight'],
        ['data', 'sim']
    )

    rdf_hists = [hist_pt_bal, hist_mpf]
    
    
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
    
    
    # Add under- and overflows in balance
    for d in [hist_pt_bal, hist_mpf]:
        for hist in d.hists.values():
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
    
    
    # Plot distributions of balance observables
    for hist_bal, label, x_label in [
        (hist_pt_bal, 'PtBal', r'$p_\mathrm{T}$ balance'),
        (hist_mpf, 'MPF', 'MPF')
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
            
            
            fig.savefig(os.path.join(args.fig_dir, '{}_ptBin{}.pdf'.format(label, pt_bin)))
            plt.close(fig)