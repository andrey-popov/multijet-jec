import itertools
import math
import os
from uuid import uuid4

import numpy as np
from scipy.interpolate import UnivariateSpline

import matplotlib as mpl
mpl.use('agg')
from matplotlib import pyplot as plt

import ROOT

from triggerbins import TriggerBins
from utils import Hist1D, mpl_style


class SimHistBuilder:
    """A class to construct profiles vs tau1 in simulation.
    
    It takes care of iterating over trigger bins.  All trigger bins are
    merged together (while eliminating the overlap) when the profiles
    are filled.
    """

    def __init__(self, trigger_bins, max_pt=math.inf):
        """Initialize from a TriggerBins object.

        Arguments:
            trigger_bins:  TriggerBins object.
            max_pt:  Trigger bins whose pt range (without the margin)
                lies fully above this value, are skipped.
        """

        self.trigger_bins = TriggerBins.from_bins(
            filter(lambda b: b.pt_range[0] < max_pt, trigger_bins.bins)
        )
        self.pt_binning = None


    def construct_binning(self, max_pt, num_bins):
        """Construct logarithmic binning in pt of the leading jet.

        Arguments:
            max_pt:  Cut-off for pt of the leading jet.
            num_bins:  Number of bins in pt of the leading jet.

        Return value:
            NumPy array representing the constructed binning.  It is
            also saved as self.pt_binning.
        """
        
        self.pt_binning = np.geomspace(
            self.trigger_bins[0].pt_range_margined[0], max_pt, num=num_bins + 1
        )
        return self.pt_binning


    def fill(self, sim_path, weight_path, variables):
        """Fill profiles from a simulation file.

        Construct profiles of given variables versus pt of the leading
        jet.  Use binning defined in self.pt_binning.

        Arguments:
            sim_path, weight_path:  Paths to ROOT files with balance
                observables for simulation and event weights.
            variables:  An iterable with a list of variables whose
                profiles are to be filled.  Each variable must be
                represented by a single branch of the input tree.

        Return value:
            Dictionary that maps names of the variables into their
            profiles represented by utils.Hist1D.
        """
        
        if self.pt_binning is None:
            raise RuntimeError('Binning is not defined.')

        # All trigger bins will be processed together.  Find pt ranges
        # for each bin that eliminate the overlap.  This means including
        # the margin only for the first and the last bins.
        trigger_pt_ranges = {}
        
        b = self.trigger_bins[0]
        trigger_pt_ranges[b.name] = (b.pt_range_margined[0], b.pt_range[1])
        
        b = self.trigger_bins[-1]
        trigger_pt_ranges[b.name] = (b.pt_range[0], b.pt_range_margined[1])
        
        for b in self.trigger_bins[1:-1]:
            trigger_pt_ranges[b.name] = b.pt_range
        
        
        # Construct and fill profiles for all requested variables
        profiles = {
            variable: ROOT.TProfile(
                uuid4().hex, '', len(self.pt_binning) - 1, self.pt_binning
            )
            for variable in variables
        }
        
        for profile in profiles.values(): 
            profile.SetDirectory(ROOT.gROOT)
        

        sim_file = ROOT.TFile(sim_path)
        weight_file = ROOT.TFile(weight_path)
        ROOT.gROOT.cd()
        
        for trigger_name in trigger_pt_ranges:
            tree = sim_file.Get(trigger_name + '/BalanceVars')
            tree.AddFriend(weight_file.Get(trigger_name + '/Weights'))
            tree.SetBranchStatus('*', False)
            
            for branch in itertools.chain(['PtJ1', 'TotalWeight'], variables):
                tree.SetBranchStatus(branch, True)
            
            weight_expression = \
                'TotalWeight[0] * (PtJ1 > {} && PtJ1 < {})'.format(
                    *trigger_pt_ranges[trigger_name]
                )
            
            for variable in variables:
                tree.Draw(
                    '{} : PtJ1 >>+ {}'.format(
                        variable, profiles[variable].GetName()
                    ),
                    weight_expression, 'goff'
                )
        
        
        # Convert all profiles into a NumPy representation
        for variable in variables:
            profiles[variable] = Hist1D(profiles[variable])

        return profiles


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
    
    def __init__(
        self, sim_path, weight_path, trigger_bins,
        diagnostic_plots_dir=None, era=None
    ):
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
        
        self.sim_path = sim_path
        self.weight_path = weight_path
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
        # Construct required profiles.
        hist_builder = SimHistBuilder(self.trigger_bins)
        hist_builder.construct_binning(max_pt, num_bins)
        profiles = hist_builder.fill(
            self.sim_path, self.weight_path, ['PtJ1'] + variables
        )
        
        
        # Build a smoothing spline for each variable.  Currently the
        # same spline is used for all trigger bins.
        cur_fit_results = {}
        
        for variable in variables:
            spline = UnivariateSpline(
                np.log(profiles['PtJ1'].contents[1:-1]),
                profiles[variable].contents[1:-1],
                w=1 / profiles[variable].errors[1:-1]
            )
            cur_fit_results[variable] = {
                trigger_name: spline
                for trigger_name in hist_builder.trigger_bins.names
            }
        
        self.fit_results.update(cur_fit_results)
        
        
        if self.diagnostic_plots_dir:
            for variable in variables:
                self._plot_diagnostics(
                    variable, profiles['PtJ1'], profiles[variable],
                    next(iter(cur_fit_results[variable].values()))
                )
        
        return cur_fit_results
    
    
    def _plot_diagnostics(
        self, variable, profile_pt, profile_bal, fit_spline
    ):
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
            pt_values = np.geomspace(
                profile_pt.binning[0], profile_pt.binning[-1], num=500
            )
            axes.plot(pt_values, fit_spline(np.log(pt_values)))
            
            axes.set_xscale('log')
            axes.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
            axes.xaxis.set_minor_formatter(
                mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4))
            )
            
            axes.set_xlabel('$\\tau_1$ [GeV]')
            axes.set_ylabel(ylabel)
            
            if self.era_label:
                axes.text(
                    1., 1.002, self.era_label,
                    ha='right', va='bottom', transform=axes.transAxes
                )
            
            fig.savefig(os.path.join(
                self.diagnostic_plots_dir, variable + '.pdf')
            )
            plt.close(fig)



class SimVariationFitter:
    """Class to construct systematic variations in simulation.

    Relative deviations in the mean values of the balance observables
    as functions of the logarithm of pt of the leading jet are fitted
    with splines.

    In the current implementation all trigger bins are merged for the
    construction of the spline, and the same spline is returned for
    every trigger bin.
    """

    def __init__(
        self, syst_config, trigger_bins,
        variables, max_pt=1700., num_bins=50,
        diagnostic_plots_dir=None, era=None
    ):
        """Initialize from a configuration object.
        
        Arguments:
            syst_config:  Configuration object of type
                systconfig.SystConfigEra.
            trigger_bins:  TriggerBins object.
            variables:  Iterable with names of variables that represent
                balance observables.
            max_pt:  Cut-off in pt of the leading jet.  Events with
                larger pt will not be used in the fit.
            num_bins:  Number of bins in pt of the leading jet to be
                used in internal histograms for the fit.
            diagnostic_plots_dir:  Directory in which figures with
                diagnostic plots are to be stored.  If None, the plots
                will not be produced.
            era:  Era label for diagnostic plots.
        """
        
        self.syst_config = syst_config

        self.variables = list(variables)
        self.max_pt = max_pt
        self.num_bins = num_bins

        self.era_label = era
        self.diagnostic_plots_dir = diagnostic_plots_dir


        # Fill nominal profiles, for all trigger bins simultaneously
        self.hist_builder = SimHistBuilder(trigger_bins, max_pt=max_pt)
        self.hist_builder.construct_binning(self.max_pt, self.num_bins)
        profiles = self.hist_builder.fill(
            self.syst_config.nominal.sim, self.syst_config.nominal.weights,
            ['PtJ1'] + self.variables
        )

        self.mean_pt_values = profiles['PtJ1'].contents[1:-1]
        self.nominal_profiles = {
            variable: profiles[variable] for variable in self.variables
        }


    def fit(self, syst_label):
        """Fit relative deviations for given uncertainty.
        
        Construct relative deviations in mean values of the balance
        observables, in bins of pt of the leading jet.  Fit the up and
        down deviations independently with smoothing splines, which
        depend on the logarithm of the pt of the leading jet.  While
        doing this, the statistical uncertainties in the relative
        deviations are set according to the numerator, as if the nominal
        histograms had no uncertainties.
        
        Arguments:
            syst_label:  Label of one of the systematic uncertainties
                defined in the configuration object provided at the
                initialization.

        Return value:
            Embedded dictionaries with constucted SciPy splines.  A
            specific spline can be accessed with
                result[variable][trigger][direction]
            where the direction of the variation is a string that takes
            values 'up' and 'down'.
        
        In the current implementation the same spline is returned for
        all trigger bins.
        """

        # Construct raw relative deviations
        deviations = {variable: {} for variable in self.variables}

        for direction in ['up', 'down']:
            samples = self.syst_config.variations[syst_label][direction]

            profiles = self.hist_builder.fill(
                samples.sim, samples.weights, self.variables
            )

            for variable in self.variables:
                # Construct relative deviation from the nominal.  Ignore
                # under- and overflow bins, which will not be used.
                nominal_values = self.nominal_profiles[variable].contents[1:-1]
                deviation = profiles[variable]
                
                deviation.contents[1:-1] /= nominal_values
                deviation.errors[1:-1] /= nominal_values
                deviation.contents[1:-1] -= 1

                deviations[variable][direction] = deviation


        # Regularize the relative deviations using smoothing splines
        smoothing_splines = {variable: {} for variable in self.variables}

        for variable, direction in itertools.product(
            self.variables, ['up', 'down']
        ):
            deviation = deviations[variable][direction]
            spline = UnivariateSpline(
                np.log(self.mean_pt_values), deviation.contents[1:-1],
                w=1 / deviation.errors[1:-1]
            )
            smoothing_splines[variable][direction] = spline

        if self.diagnostic_plots_dir:
            for variable in self.variables:
                self._plot_diagnostics(
                    variable, syst_label,
                    deviations[variable], smoothing_splines[variable]
                )
        
        
        cur_fit_results = {
            variable: {
                trigger: smoothing_splines[variable]
                for trigger in self.hist_builder.trigger_bins.names
            }
            for variable in self.variables
        }
        
        return cur_fit_results

    
    def _plot_diagnostics(
        self, variable, syst_label, raw_deviations, fit_splines
    ):
        """Produce a diagnostic plot.
        
        The plots shows the input relative deviations in the profile of
        the balance observable and the constructed smoothing splines.
        
        Arguments:
            variable:  Name of variable representing balance observable.
            raw_deviations:  Dictionaries with Hist1D that represent raw
                deviations from the nominal <B>.
            fit_splines:  Fitted splines for the deviations.
        
        Return value:
            None.
        """
        
        try:
            os.makedirs(self.diagnostic_plots_dir)
        except FileExistsError:
            pass
        
        var_template = r'$\langle B_\mathrm{{{}}}^\mathrm{{Sim}}\rangle$'

        if variable == 'PtBal':
            var_label = var_template.format('jet')
        elif variable == 'MPF':
            var_label = var_template.format('MPF')
        else:
            var_label = 'mean ' + variable

        ylabel = 'Rel. deviation in ' + var_label
        
        
        with mpl.style.context(mpl_style):
            fig = plt.figure()
            fig.patch.set_alpha(0.)
            axes = fig.add_subplot(111)
            
            for direction, colour in [('up', 'C1'), ('down', 'C0')]:
                deviation = raw_deviations[direction]
                axes.errorbar(
                    self.mean_pt_values, deviation.contents[1:-1],
                    yerr=deviation.errors[1:-1],
                    marker='o', color=colour, lw=0., elinewidth=0.8,
                    label='{}, {}'.format(syst_label, direction)
                )
                pt_values = np.geomspace(
                    deviation.binning[0], deviation.binning[-1], num=500
                )
                axes.plot(
                    pt_values, fit_splines[direction](np.log(pt_values)),
                    color=colour
                )
            
            axes.set_xscale('log')
            axes.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
            axes.xaxis.set_minor_formatter(
                mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4))
            )

            axes.axhline(0., color='black', ls='dashed', lw=0.8)
            
            axes.set_xlabel('$\\tau_1$ [GeV]')
            axes.set_ylabel(ylabel)
            axes.legend(loc='upper right')

            if self.era_label:
                axes.text(
                    1., 1.002, self.era_label,
                    ha='right', va='bottom', transform=axes.transAxes
                )
            
            fig.savefig(os.path.join(
                self.diagnostic_plots_dir,
                '{}_{}.pdf'.format(syst_label, variable))
            )
            plt.close(fig)
