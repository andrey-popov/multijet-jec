import copy
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


    def fill(self, sim_paths, era, variables, add_weight=''):
        """Fill profiles from files with simulation.

        Construct profiles of given variables versus pt of the leading
        jet.  Use binning defined in self.pt_binning.

        Arguments:
            sim_paths:   Paths to ROOT files with balance observables for
                simulation.
            era:         Era label to access period-specific weights.
            variables:   An iterable with a list of variables whose
                profiles are to be filled.  Each variable must be
                represented by a single branch of the input tree.
            add_weight:  Expression with an additional event weight to
                be included.

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
            profile.SetDirectory(None)

            # Protect against a bug in ROOT [1]
            # [1] https://sft.its.cern.ch/jira/browse/ROOT-10153
            profile.Sumw2()
        

        for trigger_name in trigger_pt_ranges:
            chain = ROOT.TChain(trigger_name + '/BalanceVars')
            friend_chains = [
                ROOT.TChain('{}/{}'.format(trigger_name, tree_name))
                for tree_name in ['GenWeights', 'PeriodWeights']
            ]

            for path in sim_paths:
                for c in [chain] + friend_chains:
                    c.AddFile(path)

            for friend in friend_chains:
                chain.AddFriend(friend)

            
            pt_selection = 'PtJ1 > {} && PtJ1 < {}'.format(
                *trigger_pt_ranges[trigger_name]
            )
            weight_expression = ' WeightGen * Weight_{}'.format(era)

            if add_weight:
                weight_expression += ' * ({})'.format(add_weight)

            df = ROOT.RDataFrame(chain)
            df_filtered = df.Filter(pt_selection)\
                .Define('weight', weight_expression)

            proxies = {}

            for variable in variables:
                proxies[variable] = df_filtered.Profile1D(
                    ROOT.RDF.TProfile1DModel(profiles[variable]),
                    'PtJ1', variable, 'weight'
                )

            for variable in variables:
                profiles[variable].Add(proxies[variable].GetValue())


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
        self, sim_paths, era, trigger_bins,
        diagnostic_plots_dir=None
    ):
        """Initialize from paths to input files and trigger bins.
        
        Arguments:
            sim_paths:  Paths to ROOT files with balance observables for
                simulation.
            era:        Era label to access period-specific weights.
            trigger_bins:  TriggerBins object.
            diagnostic_plots_dir:  Directory in which figures with
                diagnostic plots are to be stored.  If None, the plots
                will not be produced.
        """
        
        self.sim_paths = sim_paths
        self.era_label = era
        self.trigger_bins = trigger_bins
        
        self.diagnostic_plots_dir = diagnostic_plots_dir
        
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
            self.sim_paths, self.era_label, ['PtJ1'] + variables
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
            ylabel = r'$\langle B_\mathrm{jet}^\mathrm{Sim}\rangle$'
        elif variable == 'MPF':
            ylabel = r'$\langle B_\mathrm{MPF}^\mathrm{Sim}\rangle$'
        else:
            ylabel = 'Mean ' + variable
        
        
        with mpl.style.context(mpl_style):
            fig = plt.figure()
            fig.patch.set_alpha(0.)
            gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[2, 1])
            axes_upper = fig.add_subplot(gs[0, 0])
            axes_lower = fig.add_subplot(gs[1, 0])
            
            # Plot input points and the fitted spline in the upper panel
            binning = profile_pt.binning
            mean_pt_values = profile_pt.contents[1:-1]
            axes_upper.errorbar(
                mean_pt_values, profile_bal.contents[1:-1],
                yerr=profile_bal.errors[1:-1],
                marker='o', color='black', lw=0., elinewidth=0.8
            )
            pt_values = np.geomspace(binning[0], binning[-1], num=500)
            axes_upper.plot(pt_values, fit_spline(np.log(pt_values)))

            # Plot residuals in the lower panel
            smoothed_bal = fit_spline(np.log(mean_pt_values))
            residuals = profile_bal.contents[1:-1] / smoothed_bal - 1
            residuals_errors = profile_bal.errors[1:-1] / smoothed_bal
            residuals *= 100         # Per cent
            residuals_errors *= 100  #

            axes_lower.errorbar(
                mean_pt_values, residuals, yerr=residuals_errors,
                marker='o', color='black', lw=0., elinewidth=0.8
            )


            for axes in [axes_upper, axes_lower]:
                axes.set_xlim(binning[0], binning[-1])
                axes.set_xscale('log')

            axes_lower.grid(axis='y', color='gray', ls='dotted')
            
            # Remove tick labels on the x axis of the upper axes and
            # adjust tick labels on the lower axes
            axes_upper.set_xticklabels(
                [''] * len(axes_upper.get_xticklabels())
            )
            axes_lower.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
            axes_lower.xaxis.set_minor_formatter(
                mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4))
            )
            
            axes_upper.set_ylabel(ylabel)
            axes_lower.set_xlabel('$\\tau_1$ [GeV]')
            axes_lower.set_ylabel('Rel. deviation [%]')
            
            if self.era_label:
                axes_upper.text(
                    1., 1.002, self.era_label,
                    ha='right', va='bottom', transform=axes_upper.transAxes
                )
            
            fig.savefig(os.path.join(
                self.diagnostic_plots_dir, variable + '.pdf')
            )
            plt.close(fig)



class VariationFitter:
    """Auxiliary class to fit systematic variations with splines.

    This class provides machinery to construct relative deviations from
    nominal profiles of the balance observables and to fit these
    deviations with smoothing splines.  A method to produce diagnostic
    plots is also provided.

    The actual computation is to be performed in a subclass.
    """

    def __init__(self, variables, diagnostic_plots_dir=None, era=None):
        """Initialize.

        Arguments:
            variables:  Iterable with names of variables that represent
                balance observables.
            diagnostic_plots_dir:  Directory in which figures with
                diagnostic plots are to be stored.  If None, the plots
                will not be produced.
            era:  Era label for diagnostic plots.
        """
        
        self.variables = list(variables)
        self.diagnostic_plots_dir = diagnostic_plots_dir
        self.era_label = era
        
        self.mean_pt_values = None
        self.nominal_profiles = None


    def set_nominal(self, mean_pt_values, nominal_profiles):
        """Set nominal <B>.

        Set mean values of the balance observables, in bins of pt of the
        leading jet, for the nominal case.

        Arguments:
            mean_pt_values:  Array-like with mean values of pt of the
                leading jet in bins used to compute relative deviations.
            nominal_profiles:  Profiles with mean values of the balance
                observables.  Represented by a dictionary that maps the
                name of the balance observable to a utils.Hist1D object.

        Return value:
            None.
        """

        self.mean_pt_values = mean_pt_values
        self.nominal_profiles = nominal_profiles
    

    def _construct_deviations(self, profiles):
        """Construct relative deviations from nominal profiles.

        Construct relative deviations in mean values of the balance
        observables, in bins of pt of the leading jet.  Set statistical
        uncertainties in the deviations according to the numerator, as
        if the nominal profiles had no uncertainties.

        Arguments:
            profiles:  Profiles with mean values of the balance
                observables for a systematic variation.  Represented by
                embedded dictionaries so that a specific profile, in the
                form of an utils.Hist1D object, is given by
                profiles[variable][direction].
        
        Return value:
            Relative deviations from the nominal profiles, in the same
            format as argument profiles.
        """

        deviations = {variable: {} for variable in self.variables}

        for variable, direction in itertools.product(
            self.variables, ['up', 'down']
        ):
            nominal_values = self.nominal_profiles[variable].contents[1:-1]
            deviation = copy.deepcopy(profiles[variable][direction])
            
            deviation.contents[1:-1] /= nominal_values
            deviation.errors[1:-1] /= nominal_values
            deviation.contents[1:-1] -= 1

            # Clear under- and overflows
            deviation.contents[[0,-1]] = 0.
            deviation.errors[[0, -1]] = 0.

            deviations[variable][direction] = deviation

        return deviations


    def _plot_diagnostics(
        self, syst_label, raw_deviations, fit_splines, superscript='',
        syst_label_legend=''
    ):
        """Produce diagnostic plots if enabled.
        
        The plots shows the input relative deviations in the profile of
        the balance observable and the constructed smoothing splines.
        If self.diagnostic_plots_dir is None, do nothing.
        
        Arguments:
            syst_label:  Label of systematic variation.
            raw_deviations:  Dictionaries with utils.Hist1D that
                represent raw deviations from the nominal <B>.
            fit_splines:  Fitted splines for the deviations.
            superscript:  Superscript for mean balance observable.
                Normally, it should be either "Sim" or "L2".
            syst_label_legend:  Label for systematic variation to be
                used in the legend.  If not given, will use syst_label.
        
        Return value:
            None.
        """
        
        # Do nothing if the plots have not been requested
        if not self.diagnostic_plots_dir:
            return
        

        try:
            os.makedirs(self.diagnostic_plots_dir)
        except FileExistsError:
            pass

        if not syst_label_legend:
            syst_label_legend = syst_label
        
        var_template = r'$\langle B_\mathrm{{{}}}^\mathrm{{{}}}\rangle$'

        for variable in self.variables:
            if variable == 'PtBal':
                var_label = var_template.format('jet', superscript)
            elif variable == 'MPF':
                var_label = var_template.format('MPF', superscript)
            else:
                var_label = 'mean ' + variable

            ylabel = 'Rel. deviation in ' + var_label
            
            
            with mpl.style.context(mpl_style):
                fig = plt.figure()
                fig.patch.set_alpha(0.)
                axes = fig.add_subplot(111)
                
                for direction, colour in [('up', 'C1'), ('down', 'C0')]:
                    deviation = raw_deviations[variable][direction]
                    spline = fit_splines[variable][direction]
                    axes.errorbar(
                        self.mean_pt_values, deviation.contents[1:-1],
                        yerr=deviation.errors[1:-1],
                        marker='o', color=colour, lw=0., elinewidth=0.8,
                        label='{}, {}'.format(syst_label_legend, direction)
                    )
                    pt_values = np.geomspace(
                        deviation.binning[0], deviation.binning[-1], num=500
                    )
                    axes.plot(
                        pt_values, spline(np.log(pt_values)), color=colour
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
    
    
    def _smooth_deviations(self, deviations):
        """Construct smoothing splines for given relative deviations.

        For each variable, fit the up and down deviations independently
        with smoothing splines.  The splines depend on the logarithm of
        the pt of the leading jet.

        Arguments:
            deviations:  Embedded dictionaries with relative deviations.
                The keys are the name of the balance observable and the
                label for the direction of the deviation ('up' or
                'down').  The relative deviations are represented with
                instances of utils.Hist1D.

        Return value:
            Embedded dictionaries with constructed SciPy splines.  The
            structure of the dictionaries is the same as in the argument
            deviations.
        """

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

        return smoothing_splines



class SimVariationFitter(VariationFitter):
    """Class to construct systematic variations in simulation.

    Relative deviations in the mean values of the balance observables
    as functions of the logarithm of pt of the leading jet are fitted
    with splines.

    In the current implementation all trigger bins are merged for the
    construction of the spline, and the same spline is returned for
    every trigger bin.
    """

    def __init__(
        self, syst_config, trigger_bins, variables,
        max_pt=1700., num_bins=50, **kwargs
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

        All additional keyword arguments are forwarded to the base
        class.
        """
        
        super().__init__(variables, era=syst_config.period_weight, **kwargs)

        self.syst_config = syst_config

        self.max_pt = max_pt
        self.num_bins = num_bins


        # Fill nominal profiles, for all trigger bins simultaneously
        self.hist_builder = SimHistBuilder(trigger_bins, max_pt=max_pt)
        self.hist_builder.construct_binning(self.max_pt, self.num_bins)
        profiles = self.hist_builder.fill(
            self.syst_config.nominal.sim_paths, self.syst_config.period_weight,
            ['PtJ1'] + self.variables
        )

        nominal_profiles = {
            variable: profiles[variable] for variable in self.variables
        }
        self.set_nominal(profiles['PtJ1'].contents[1:-1], nominal_profiles)


    def fit(self, syst_label):
        """Fit relative deviations for given uncertainty.
        
        Fit the deviations with smoothing splines.  See documentation
        for methods _construct_deviations and _smooth_deviations in the
        base class.
        
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

        profiles = {variable: {} for variable in self.variables}

        for direction in ['up', 'down']:
            samples = self.syst_config.variations[syst_label][direction]
            cur_profiles = self.hist_builder.fill(
                samples.sim_paths, self.syst_config.period_weight,
                self.variables, add_weight=samples.add_weight
            )

            for variable in self.variables:
                profiles[variable][direction] = cur_profiles[variable]

        deviations = self._construct_deviations(profiles)
        smoothing_splines = self._smooth_deviations(deviations)

        self._plot_diagnostics(
            syst_label, deviations, smoothing_splines, superscript='Sim',
            syst_label_legend=self.syst_config.get_legend_label(syst_label)
        )
        
        
        # Clone splines for all trigger bins
        cur_fit_results = {
            variable: {
                trigger: smoothing_splines[variable]
                for trigger in self.hist_builder.trigger_bins.names
            }
            for variable in self.variables
        }
        
        return cur_fit_results

    

class DataVariationFitter(VariationFitter):
    """Class to construct systematic variations in data.
    
    Relative deviations in the mean values of the balance observables
    as functions of the logarithm of pt of the leading jet are fitted
    with splines.  This is done in the analysis binning, and the
    resulting smoothed deviations are provided in the form of
    histograms.
    """

    def __init__(
        self, syst_config, trigger_bins, variables, binning, **kwargs
    ):
        """Initialize from a configuration object.
        
        Arguments:
            syst_config:  Configuration object of type
                systconfig.SystConfigEra.
            trigger_bins:  TriggerBins object.
            variables:  Iterable with names of variables that represent
                balance observables.
            binning:  Target binning for the variations.

        All additional keyword arguments are forwarded to the base
        class.
        """
        
        super().__init__(variables, era=syst_config.period_weight, **kwargs)

        self.syst_config = syst_config
        self.trigger_bins = trigger_bins

        self.binning = np.asarray(binning)


        # Read nominal profiles from the input file and rebin them to
        # the target binning.
        profiles = self._read_histograms(
            self.syst_config.nominal.data_paths,
            [name + 'Profile' for name in ['PtLead'] + self.variables]
        )
        nominal_profiles = {
            variable: profiles[variable + 'Profile']
            for variable in self.variables
        }
        self.set_nominal(
            profiles['PtLeadProfile'].contents[1:-1], nominal_profiles
        )


    def fit(self, syst_label):
        """Fit relative deviations for given uncertainty.
        
        Fit the deviations with smoothing splines.  See documentation
        for methods _construct_deviations and _smooth_deviations in the
        base class.
        
        Arguments:
            syst_label:  Label of one of the systematic uncertainties
                defined in the configuration object provided at the
                initialization.

        Return value:
            Embedded dictionaries with smoothed deviations, which are
            represented by instances of util.Hist1D.  A specific
            deviation can be accessed with
                result[variable][direction]
            where the direction of the variation is a string that takes
            values 'up' and 'down'.
        """

        profiles = {variable: {} for variable in self.variables}

        for direction in ['up', 'down']:
            samples = self.syst_config.variations[syst_label][direction]
            cur_profiles = self._read_histograms(
                samples.data_paths, [v + 'Profile' for v in self.variables]
            )

            for variable in self.variables:
                profiles[variable][direction] = \
                    cur_profiles[variable + 'Profile']

        deviations = self._construct_deviations(profiles)
        smoothing_splines = self._smooth_deviations(deviations)

        self._plot_diagnostics(
            syst_label, deviations, smoothing_splines, superscript='L2',
            syst_label_legend=self.syst_config.get_legend_label(syst_label)
        )


        # Convert smoothing splines into histograms
        smooth_deviations = {variable: {} for variable in self.variables}

        for variable, direction in itertools.product(
            self.variables, ['up', 'down']
        ):
            spline = smoothing_splines[variable][direction]
            smooth_deviations[variable][direction] = Hist1D(
                binning=self.binning,
                contents=spline(np.log(self.mean_pt_values)),
                errors=np.zeros(len(self.binning) - 1)
            )
        
        return smooth_deviations


    def _read_histograms(self, paths, names):
        """Read histograms from data files.

        Read requested ROOT histograms from data files.  Since trigger
        bins in data do not have overlaps in pt, merge the bins
        together.  Rebin the histograms to the binning given at
        initialization.

        Arguments:
            paths:  Paths to ROOT files with data.
            names:  Iterable with names of requested histograms.

        Return value:
            Dictionary that maps the names to utils.Hist1D objects that
            represent rebinned histograms.
        """

        root_histograms = {}

        for path in paths:
            data_file = ROOT.TFile(path)

            for name, trigger_bin in itertools.product(
                names, self.trigger_bins
            ):
                hist = data_file.Get('{}/{}'.format(trigger_bin.name, name))

                if name not in root_histograms:
                    hist.SetDirectory(None)
                    root_histograms[name] = hist
                else:
                    root_histograms[name].Add(hist)

            data_file.Close()

        converted_histograms = {
            name: Hist1D(hist.Rebin(
                len(self.binning) - 1, uuid4().hex, self.binning)
            )
            for name, hist in root_histograms.items()
        }

        return converted_histograms

