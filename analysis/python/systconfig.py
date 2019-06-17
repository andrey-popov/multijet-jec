"""Tools for configuration files for systematic variations."""

from collections import OrderedDict
import json
import os


class VariationSamples:
    """Data and simulation samples for a single systematic variation.

    This class provides fully qualified paths to input files with data
    and simulation, as well as event weights for simulation.  This
    allows to fully define inputs to construct data and simulation
    distributions for the nominal configuration or a single systematic
    variation alike.  When dealing with systematic variations, either
    members related to real data or those for simulation might not be
    set, which indicates that the systematic uncertainty does not affect
    the corresponding part.
    """

    __slots__ = ['data_paths', 'sim_paths', 'period_weight', 'add_weight']

    def __init__(
        self, data_paths, sim_paths, period_weight,
        eras=[], directory='', add_weight=''
    ):
        """Initialize from file paths.
        
        Arguments:
            data_paths:  List of paths to data files.
            sim_paths:   List of paths to simulation files.
            period_weight:  Era label to identify period weight to be
                used with simulation.
            eras:  List of era labels to format paths to input files.
            directory:    Directory with respect to which paths to input
                files should be resolved.
            add_weights:  Additional weight to be applied to simulation.

        List of paths to input files are expanded using method
        _qualify_paths.
        """

        self.data_paths = self._qualify_paths(data_paths, directory, eras)
        self.sim_paths = self._qualify_paths(sim_paths, directory, eras)
        self.period_weight = period_weight
        self.add_weight = add_weight

        if (self.add_weight or self.period_weight) and not self.sim_paths:
            raise RuntimeError(
                'Weights can only be specified together with simulation files.'
            )

        if self.sim_paths and not self.period_weight:
            raise RuntimeError(
                'When using simulation, period weight must be specified.'
            )


    @classmethod
    def from_config(cls, config, period_weight, eras=[], directory=''):
        """Initialize from a configuration.

        Arguments:
            config:  Configuration describing a single variation.  Must
                be a part of the full configuration described in class
                SystConfig.
            period_weight:  Era label to identify period weight to be
                used with simulation.
            eras:  List of era labels to format paths to input files.
            directory:    Directory with respect to which paths to input
                files should be resolved.
        """
        
        data_paths = config['data'] if 'data' in config else []
        sim_paths = config['sim'] if 'sim' in config else []
        add_weight = config['add_weight'] if 'add_weight' in config else None

        return cls(
            data_paths, sim_paths, period_weight,
            eras=eras, directory=directory, add_weight=add_weight
        )


    @staticmethod
    def _qualify_paths(paths, directory='', eras=[]):
        """Prepend paths with directory and format them for eras.

        If a path contains a substring "{era}", copy it multiple times
        replacing the substring with actual era labels.  Prepend each
        resulting path with the given directory.
        """

        # For each path that contains a fragment "{era}", replace it
        # with all provided era labels
        expanded_paths = []

        for path in paths:
            if '{era}' in path:
                if not eras:
                    raise RuntimeError(
                        'Path "{}" requires a substitution with era label '
                        'but not eras have been provided.'.format(path)
                    )

                for era in eras:
                    expanded_paths.append(path.format(era=era))
            else:
                expanded_paths.append(path)

        full_paths = [
            os.path.join(directory, path)
            for path in expanded_paths
        ]

        return full_paths



class SystConfig:
    """Configuration for systematic variations.
    
    The configuration is described by a JSON file of the following
    format:
      {
        "directory":  <string, optional>,
        "eras":  <list of strings with names of eras>,
        "period_weight": <name of era for period weights>,
        "nominal":  <samples>,
        "variations": {
          <label>: {
            "up":  <samples>,
            "down":  <samples>,
            "legend_label": <label to be used in plots, optional>
          },
          // ...
        }
      }
    Each group <samples> above define a set of files for one specific
    variation.  It it described by the following dictionary:
      {
        "data":  <list of paths>,
        "sim":  <list of paths>,
        "add_weight": <string>
      }
    which gives location of ROOT files with data and simulation.  Either
    of the two entries can be omitted if the systematic variation
    applies only to simulation or only to data.  All paths will be
    prepended with the optional source directory, given at the top level
    of the configuration.  If a path contains a substring "{era}", it
    will be replaced with names of all eras specified at the top level
    of the configuration (unless this behaviour is overridden in the
    initializer).  Optional parameter "add_weight" specifies an
    additional weight to be applied to simulation.
    """

    def __init__(self, path, era=None):
        """Initialize from a configuration file.
        
        Arguments:
            path:  Path to a JSON configuration file.
            era:   Era to override the list of eras specified in the
                configuration file.  If given, only this era will be
                considered, and the parameter "period_weight" will also
                be overwritten with this era name.
        """
        
        with open(path) as f:
            config = json.load(f)

        if era:
            self.eras = [era]
            self.period_weight = era
        else:
            self.eras = config['eras']
            self.period_weight = config['period_weight']


        if 'directory' in config:
            directory = config['directory']
        else:
            directory = ''

        self.nominal = VariationSamples.from_config(
            config['nominal'], self.period_weight,
            eras=self.eras, directory=directory
        )

        self.variations = OrderedDict()
        self.legend_labels = {}

        for syst_label, entry in config['variations'].items():
            self.variations[syst_label] = {
                direction: VariationSamples.from_config(
                    entry[direction], self.period_weight,
                    eras=self.eras, directory=directory
                )
                for direction in ['up', 'down']
            }

            if 'legend_label' in entry:
                self.legend_labels[syst_label] = entry['legend_label']


    def get_legend_label(self, syst_label):
        """Return legend label for given systematic variation.

        If no legend label has been provided in the configuration,
        return syst_label.
        """

        if syst_label in self.legend_labels:
            return self.legend_labels[syst_label]
        else:
            return syst_label


    def iter_group(self, group='all'):
        """Return an iterator for uncertainties in a given group.

        Construct an iterator over labels of systematic uncertainties in
        a given group.

        Arguments:
            group:  Requested group of uncertainties.  Allowed values
                are 'data', 'sim', or 'all' (default).

        Return value:
            Iterator over labels of selected uncertainties.
        """

        if group == 'data':
            return filter(
                lambda label: self.variations[label]['up'].data_paths,
                self.variations
            )
        elif group == 'sim':
            return filter(
                lambda label: self.variations[label]['up'].sim_paths,
                self.variations
            )
        elif group == 'all':
            return iter(self.variations)
        else:
            raise RuntimeError('Unknown group "{}".'.format(group))

