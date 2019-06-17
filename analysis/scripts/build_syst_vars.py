#!/usr/bin/env python

"""Constructs relative systematic variations.

For simulation, produced variations are stored in the form of
ROOT.TSpline3 inside in-file directories corresponding to different
trigger bins.
"""

import argparse
import itertools
import json
import math

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True

from regularization import DataVariationFitter, SimVariationFitter
from systconfig import SystConfig
from triggerbins import TriggerBins
import utils


if __name__ == '__main__':
    
    arg_parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    arg_parser.add_argument('config', help='Configuration JSON file.')
    arg_parser.add_argument('-e', '--era', help='Era to process')
    arg_parser.add_argument(
        '-t', '--triggers', default='trigger_bins.json',
        help='JSON file with definition of triggers bins.'
    )
    arg_parser.add_argument(
        '-b', '--binning', default='binning.json',
        help='JSON file with binning in ptlead.'
    )
    arg_parser.add_argument(
        '-o', '--output', default='syst.root',
        help='Name for output ROOT file.'
    )
    arg_parser.add_argument(
        '--plots', default='fig', help='Base directory for diagnostic plots.'
    )
    args = arg_parser.parse_args()
    
    
    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    
    
    # Read configuration files
    syst_config = SystConfig(args.config, era=args.era)

    with open(args.binning) as f:
        binning = json.load(f)
        binning.sort()
    
    trigger_bins = TriggerBins(args.triggers, clip=binning[-1])
    
    
    # Analysis does not support overflow bins.  Check that the last
    # edge is finite.
    if not math.isfinite(binning[-1]):
        raise RuntimeError('Inclusive overflow bins are not supported.')
    
    
    # Check alingment between given binning and trigger bins
    trigger_bins.check_alignment(binning)
    
    if trigger_bins[0].pt_range[0] != binning[0]:
        raise RuntimeError(
            'Left-most edge in given binning ({:g}) does not match left '
            'boundary of the first trigger bin ({:g}).'.format(
                binning[0], trigger_bins[0].pt_range[0]
            )
        )
    
    
    # Output file and in-file directory structure
    output_file = ROOT.TFile(args.output, 'recreate')
    
    for trigger_name in trigger_bins.names:
        output_file.mkdir(trigger_name)
    

    variables = ['PtBal', 'MPF']
    prevent_garbage_collection = []


    # Construct smoothed relative deviations in data
    data_fitter = DataVariationFitter(
        syst_config, trigger_bins, variables, binning,
        diagnostic_plots_dir=args.plots + '/data_syst'
    )

    for syst_label in syst_config.iter_group('data'):
        fit_results = data_fitter.fit(syst_label)
        
        for variable, direction in itertools.product(
            variables, ['up', 'down']
        ):
            hist = fit_results[variable][direction].to_root(
                'RelVar_{}_{}{}'.format(
                    variable, syst_label, direction.capitalize()
                )
            )
            hist.SetDirectory(output_file)
            prevent_garbage_collection.append(hist)


    # Construct smoothed relative variations in simulation
    sim_fitter = SimVariationFitter(
        syst_config, trigger_bins, variables,
        diagnostic_plots_dir=args.plots + '/sim_syst'
    )

    for syst_label in syst_config.iter_group('sim'):
        fit_results = sim_fitter.fit(syst_label)

        for trigger_name in trigger_bins.names:
            output_file.cd(trigger_name)

            for variable, direction in itertools.product(
                variables, ['up', 'down']
            ):
                root_spline = utils.spline_to_root(
                    fit_results[variable][trigger_name][direction]
                )
                root_spline.Write('RelVar_Sim{}_{}{}'.format(
                    variable, syst_label, direction.capitalize()
                ))
                prevent_garbage_collection.append(root_spline)
    

    output_file.Write()
    output_file.Close()

