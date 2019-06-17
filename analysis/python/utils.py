from array import array
import collections
from uuid import uuid4

import numpy as np

import ROOT


class Hist1D:
    """NumPy-based representation of 1D histogram.
    
    Includes under- and overflow bins.
    """
    
    def __init__(self, *args, binning=None, contents=None, errors=None):
        """Construct from binning or ROOT histogram."""
        
        if len(args) in [0, 3]:
            if len(args) == 3:
                # Assume arguments define an equidistant binning
                self.binning = np.linspace(args[1], args[2], num=args[0])
                
                if binning is not None:
                    raise RuntimeError('Conflicting definitions of binning.')
            else:
                if binning is None:
                    raise RuntimeError('Binning must be provided.')

                self.binning = np.asarray(binning, dtype=np.float64)
            
            # With the binning specified, set bin contents and errors
            self.contents = np.zeros(len(self.binning) + 1, dtype=np.float64)
            self.errors = np.zeros_like(self.contents)
            
            if contents is not None:
                if len(contents) == len(self.contents):
                    self.contents[:] = contents
                elif len(contents) == len(self.contents) - 2:
                    # Assume under- and overflows are missing
                    self.contents[1:-1] = contents
                else:
                    raise RuntimeError('Unexpected length of array of bin contents.')
            
            if errors is not None:
                if len(errors) == len(self.errors):
                    self.errors[:] = errors
                elif len(errors) == len(self.errors) - 2:
                    # Assume under- and overflows are missing
                    self.errors[1:-1] = errors
                else:
                    raise RuntimeError('Unexpected length of array of bin errors.')
                
                if contents is not None and len(errors) != len(contents):
                    raise RuntimeError('Inconsistent arrays of bin contents and errors.')
            
            elif contents is not None:
                self.errors = np.sqrt(self.contents)
        
        elif len(args) == 1:
            # Initialize from a ROOT histogram
            if not isinstance(args[0], ROOT.TH1):
                raise TypeError('ROOT histogram expected, got {}.'.format(type(args[0])))
            
            hist = args[0]
            
            if hist.GetDimension() != 1:
                raise RuntimeError('1D histogram is expected.')
            
            numbins = hist.GetNbinsX()
            self.binning = np.zeros(numbins + 1, dtype=np.float64)
            self.contents = np.zeros(numbins + 2, dtype=np.float64)
            self.errors = np.zeros_like(self.contents)
            
            for bin in range(1, numbins + 2):
                self.binning[bin - 1] = hist.GetBinLowEdge(bin)
            
            for bin in range(numbins + 2):
                self.contents[bin] = hist.GetBinContent(bin)
                self.errors[bin] = hist.GetBinError(bin)
    
    
    @property
    def numbins(self):
        """Return number of bins in the histogram.
        
        Under- and overflow bins are not counted.
        """
        
        return len(self.binning) - 1


    def to_root(self, name=None):
        """Convert to a ROOT histogram.

        Create a new ROOT.TH1D with the same content as self.  It is not
        associtated with any ROOT directory.

        Arguments:
            name:  Name for the ROOT histogram.  If not given, use a
                unique random name.

        Return value:
            Instance of ROOT.TH1D.
        """

        if not name:
            name = uuid4().hex

        hist = ROOT.TH1D(name, '', len(self.binning) - 1, self.binning)

        for b in range(self.numbins + 2):
            hist.SetBinContent(b, self.contents[b])
            hist.SetBinError(b, self.errors[b])

        return hist
    
    
    def __getitem__(self, index):
        """Return content and error for bin with given index.
        
        Under- and overflow bins have indices 0 and -1.
        """
        
        return self.contents[index], self.errors[index]


class RDFHists:
    """Simplifies filling of histograms with ROOT.RDataFrame.

    This class provides a less verbose syntax to fill histogram-like
    objects from one or more ROOT.RDataFrame.  Both ROOT histograms and
    profiles are supported.  A single instance of RDFHists can contain
    multiple versions of the histogram-like objects, which are
    accessible with operator [].  This is normally expected to be used
    to associate histograms in data and simulation.
    """

    def __init__(self, cls, binning_def, branches, versions):
        """Initialize.

        Arguments:
            cls:  ROOT class for the contained histogram-like objects.
            binning_def:  Binning definition (see below).
            branches:  Names of branches to be given to the method of
                ROOT.RDataFrame that will construct a histogram-like
                object (such as ROOT.RDataFrame.Histo1D).
            versions:  Iterable that defines versions of stored
                histogram-like objects.

        The binning definition can be specified in two ways: (1) a
        dictionary that contains fields "range" and "step" and thus
        defines a uniform binning and (2) a sequence of bin edges.  In
        case of multiple dimensions, a sequence of the above should be
        provided.
        """

        self.cls = cls
        self.model = self._build_model(binning_def)
        self.hists = {}

        for version in versions:
            hist = cls(*self.model)
            hist.SetDirectory(None)

            # Protect against a bug in ROOT [1]
            # [1] https://sft.its.cern.ch/jira/browse/ROOT-10153
            hist.Sumw2()

            self.hists[version] = hist

        self.branches = branches
        self._proxy = None


    def __getitem__(self, version):
        """Return histogram-like with given version."""
        return self.hists[version]


    def add(self, version):
        """Add registered proxy to histogram like with given label.

        The proxy must have been registered earlier with method
        register.  The lazy evaluation of ROOT.RDataFrame will be
        triggered by this method.
        """
        self.hists[version].Add(self._proxy.GetValue())


    def register(self, dataframe):
        """Register this histogram-like to the given data frame.

        The proxy object provided by the data frame is saved internally.
        It will be accessed when method add is called.
        """

        if issubclass(self.cls, ROOT.TProfile):
            self._proxy = dataframe.Profile1D(self.model, *self.branches)
        elif issubclass(self.cls, ROOT.TH2):
            self._proxy = dataframe.Histo2D(self.model, *self.branches)
        elif issubclass(self.cls, ROOT.TH1):
            self._proxy = dataframe.Histo1D(self.model, *self.branches)
        else:
            raise NotImplementedError(
                'Type {} is not supported.'.format(self.cls)
            )


    @staticmethod
    def _build_model(binning_def):

        # Harmonize the format between 1D and multidimensional cases
        if (isinstance(binning_def, collections.abc.Sequence) and
            isinstance(binning_def[0], collections.abc.Container)):
            # This is a sequence of binning definitions for multiple
            # dimensions
            binning_defs = binning_def
        else:
            # This is a binning definition for a single dimension
            binning_defs = [binning_def]

        model = ['', '']  # Empty name and title

        for bd in binning_defs:
            if isinstance(bd, collections.abc.Mapping):
                r = bd['range']
                num_bins = round((r[1] - r[0]) / bd['step'])
                model += [num_bins, r[0], r[1]]
            else:
                binning = array('d', bd)
                model += [len(binning) - 1, binning]

        return tuple(model)


def spline_to_root(spline):
    """Convert a SciPy spline into ROOT.TSpline3.
    
    Arguments:
        spline:  SciPy one-dimensional spline such as
            scipy.interpolate.UnivariateSpline.

    Return value:
        Equivalent ROOT.TSpline3.
    """
    
    # Force the ROOT spline to pass through all knots and provide
    # boundary conditions for second derivatives.
    knots = spline.get_knots()
    boundaries_der2 = spline.derivative(2)(knots[[0, -1]])
    root_spline = ROOT.TSpline3(
        '', knots, spline(knots), len(knots),
        'b2 e2', boundaries_der2[0], boundaries_der2[-1]
    )
    
    return root_spline


mpl_style = {
    'figure.figsize': (6.0, 4.8),
    
    'axes.labelsize':              'large',
    'axes.formatter.use_mathtext': True,
    'axes.formatter.limits':       (-2, 4),
    
    'xtick.top':          True,
    'xtick.direction':    'in',
    'xtick.minor.top':    True,
    'xtick.minor.bottom': True,
    
    'ytick.right':        True,
    'ytick.direction':    'in',
    'ytick.minor.left':   True,
    'ytick.minor.right':  True,
    
    'lines.linewidth':   1.,
    'lines.markersize':  3.,
    'errorbar.capsize':  1.
}
