import itertools
import os
from uuid import uuid4

import numpy as np
from scipy.interpolate import UnivariateSpline

import matplotlib as mpl
mpl.use('agg')
from matplotlib import pyplot as plt

import ROOT

from utils import Hist1D, mpl_style


class SplineSimFitter:
    """Class to construct continuous <B>(log(tau1)) in simulation.
    
    This class fits the mean of a balance observable in bins of pt of
    the leading jet in simulation with a smoothing spline.  A continuous
    model for the mean value is thus constructed, which can then be
    evaluated for arbitrary pt of the leading jet.  No uncertainties are
    assigned to the model.
    
    In the current implementation all trigger bins are merged for the
    construction of the spline, and the same spline is returned for
    every trigger bin.
    """
    
    def __init__(self, sim_path, weight_path, trigger_bins, diagnostic_plots_dir=None, era=None):
        """Initialize from paths to input files and trigger bins.
        
        Arguments:
            sim_path, weight_path:  Paths to ROOT files with balance
                observables for simulation and event weights.
            trigger_bins:  TriggerBins object.
            diagnostic_plots_dir:  Directory in which figures with
                diagnostic plots are to be stored.  If None, the plots
                will not be produced.
            era:  Era label for diagnostic plots.
        """
        
        self.sim_file = ROOT.TFile(sim_path)
        self.weight_file = ROOT.TFile(weight_path)
        self.trigger_bins = trigger_bins
        
        self.diagnostic_plots_dir = diagnostic_plots_dir
        self.era_label = era
        
        self.fit_results = {}
    
    
    def fit(self, variables, max_pt=1700., num_bins=100):
        """Fit given set of balance observables.
        
        Arguments:
            variables:  Iterable with names of variables that represent
                balance observables.
            max_pt:  Cut-off in pt of the leading jet.  Events with
                larger pt will not be used in the fit.
            num_bins:  Number of bins in pt of the leading jet to be
                used in internal histograms for the fit.
        
        Return value:
            Embedded dictionaries with constucted splines.  Keys of the
            outer dictionary are the names of the variables.  Its values
            are dictionaries that map names of trigger bins into SciPy
            splines.  The return value is also added into
            self.fit_results.
        
        In the current implementation the same spline is returned for
        all trigger bins.
        """
        
        # The fit will be done for all trigger bins simultaneously.
        # Find pt ranges for each bin that eliminate the overlap.  This
        # means including the margin only for the first and the last
        # bins.  In addition, respect the upper cut on pt.
        trigger_pt_ranges = {}
        last_bin_index = len(self.trigger_bins) - 1
        
        while self.trigger_bins[last_bin_index].pt_range[0] > max_pt:
            last_bin_index -= 1
        
        bin = self.trigger_bins[0]
        trigger_pt_ranges[bin.name] = (bin.pt_range_margined[0], bin.pt_range[1])
        
        bin = self.trigger_bins[last_bin_index]
        trigger_pt_ranges[bin.name] = (bin.pt_range[0], min(bin.pt_range_margined[1], max_pt))
        
        for bin in self.trigger_bins[1:last_bin_index]:
            trigger_pt_ranges[bin.name] = bin.pt_range
        
        
        # Construct and fill profiles of pt and all requested variables
        pt_binning = np.geomspace(
            self.trigger_bins[0].pt_range_margined[0], max_pt, num=num_bins + 1
        )
        profile_pt = ROOT.TProfile(uuid4().hex, '', num_bins, pt_binning)
        profiles_bal = {variable: profile_pt.Clone(uuid4().hex) for variable in variables}
        
        for profile in itertools.chain([profile_pt], profiles_bal.values()):
            profile.SetDirectory(ROOT.gROOT)
        
        
        ROOT.gROOT.cd()
        
        for trigger_name in trigger_pt_ranges:
            tree = self.sim_file.Get(trigger_name + '/BalanceVars')
            tree.AddFriend(self.weight_file.Get(trigger_name + '/Weights'))
            tree.SetBranchStatus('*', False)
            
            for branch in itertools.chain(['PtJ1', 'TotalWeight'], variables):
                tree.SetBranchStatus(branch, True)
            
            weight_expression = 'TotalWeight[0] * (PtJ1 > {} && PtJ1 < {})'.format(
                *trigger_pt_ranges[trigger_name]
            )
            tree.Draw('PtJ1 : PtJ1 >>+' + profile_pt.GetName(), weight_expression, 'goff')
            
            for variable in variables:
                tree.Draw(
                    '{} : PtJ1 >>+ {}'.format(variable, profiles_bal[variable].GetName()),
                    weight_expression, 'goff'
                )
        
        
        # Convert all profiles into a NumPy representation
        profile_pt = Hist1D(profile_pt)
        
        for variable in variables:
            profiles_bal[variable] = Hist1D(profiles_bal[variable])
        
        
        # Build a smoothing spline for each variable.  Currently the
        # same spline is used for all trigger bins.
        cur_fit_results = {}
        
        for variable, profile_bal in profiles_bal.items():
            spline = UnivariateSpline(
                np.log(profile_pt.contents[1:-1]), profile_bal.contents[1:-1],
                w=1 / profile_bal.errors[1:-1]
            )
            cur_fit_results[variable] = {
                trigger_name: spline for trigger_name in trigger_pt_ranges
            }
        
        self.fit_results.update(cur_fit_results)
        
        
        if self.diagnostic_plots_dir:
            for variable, profile_bal in profiles_bal.items():
                self._plot_diagnostics(
                    variable, profile_pt, profile_bal,
                    next(iter(cur_fit_results[variable].values()))
                )
        
        return cur_fit_results
    
    
    @staticmethod
    def spline_to_root(spline):
        """Convert a SciPy spline into ROOT.TSpline3."""
        
        # Force the ROOT spline to pass through all knots and provide
        # boundary conditions for second derivatives.
        knots = spline.get_knots()
        boundaries_der2 = spline.derivative(2)(knots[[0, -1]])
        root_spline = ROOT.TSpline3(
            '', knots, spline(knots), len(knots),
            'b2 e2', boundaries_der2[0], boundaries_der2[-1]
        )
        
        return root_spline
    
    
    def _plot_diagnostics(self, variable, profile_pt, profile_bal, fit_spline):
        """Produce a diagnostic plot.
        
        The plot shows the input profile of the balance observable and
        the constructed smoothing spline.
        
        Arguments:
            variable:  Name of variable representing balance observable.
            profile_pt, profile_bal:  Hist1D that represent profiles for
                pt of the leading jet and the balance observable.
            fit_spline:  Constructed approximating spline.
        
        Return value:
            None.
        """
        
        try:
            os.makedirs(self.diagnostic_plots_dir)
        except FileExistsError:
            pass
        
        if variable == 'PtBal':
            ylabel = r'$\langle B_\mathrm{jet}\rangle$'
        elif variable == 'MPF':
            ylabel = r'$\langle B_\mathrm{MPF}\rangle$'
        else:
            ylabel = 'Mean ' + variable
        
        
        with mpl.style.context(mpl_style):
            fig = plt.figure()
            fig.patch.set_alpha(0.)
            axes = fig.add_subplot(111)
            
            axes.errorbar(
                profile_pt.contents[1:-1], profile_bal.contents[1:-1],
                yerr=profile_bal.errors[1:-1],
                marker='o', color='black', lw=0., elinewidth=0.8
            )
            pt_values = np.geomspace(profile_pt.binning[0], profile_pt.binning[-1], num=500)
            axes.plot(pt_values, fit_spline(np.log(pt_values)))
            
            axes.set_xscale('log')
            axes.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
            axes.xaxis.set_minor_formatter(mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4)))
            
            axes.set_xlabel('$\\tau_1$ [GeV]')
            axes.set_ylabel(ylabel)
            
            if self.era_label:
                axes.text(
                    1., 1.002, self.era_label, ha='right', va='bottom', transform=axes.transAxes
                )
            
            fig.savefig(os.path.join(self.diagnostic_plots_dir, variable + '.pdf'))
            plt.close(fig)
