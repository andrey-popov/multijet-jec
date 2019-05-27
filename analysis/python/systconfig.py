"""Tools for configuration files for systematic variations."""

from collections import OrderedDict
import json
import os


class _Samples:
    """Auxiliary class to aggregate files for a single variation.
    
    Contains names of input files for data, simulation, and weights for
    simulation.
    """

    __slots__ = ['data', 'sim', 'weights']

    def __init__(self, data, sim, weights, directory='', era=''):
        """Initialize from file paths.
        
        Prepend all paths with the given directory and format for the
        given era.  See _qualify_path for details.
        """

        self.data = self._qualify_path(data, directory, era)
        self.sim = self._qualify_path(sim, directory, era)
        self.weights = self._qualify_path(weights, directory, era)

        if bool(self.sim) != bool(self.weights):
            raise RuntimeError(
                'Files for simulation and weights must either both be present '
                'or none should be given.'
            )


    @classmethod
    def from_config(cls, config, directory='', era=''):
        """Initialize from a configuration.

        The configuration must be the relevant part of the full JSON
        configuration described in class SystConfig.
        """
        
        data = config['data'] if 'data' in config else ''
        sim = config['sim'] if 'sim' in config else ''
        weights = config['weights'] if 'weights' in config else ''

        return cls(data, sim, weights, directory=directory, era=era)


    @staticmethod
    def _qualify_path(path, directory='', era=''):
        """Prepend path with directory and format it for era.

        Prepend the path with the given directory (using os.path.join).
        If the path contains a substring "{era}", replace it with given
        era.  If the path is empty or None, return None.
        """

        if not path:
            return None
        else:
            return os.path.join(directory, path.format(era=era))



class SystConfigEra:
    """Configuration for systematic variations in a specific era."""

    def __init__(self, config, era):
        """Initialize from a configuration.

        Arguments:
            config:  Dictionary with configuration described in
                documentation for class SystConfig.
            era:  Era for which the configuration is to be instantiated.
        """

        if 'directory' in config:
            directory = config['directory']
        else:
            directory = ''

        self.nominal = _Samples.from_config(
            config['nominal'], directory=directory, era=era
        )

        self.variations = OrderedDict()

        for syst_label, entry in config['variations'].items():
            self.variations[syst_label] = {
                direction: _Samples.from_config(
                    entry[direction], directory=directory, era=era
                )
                for direction in ['up', 'down']
            }


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
                lambda label: self.variations[label]['up'].data is not None,
                self.variations
            )
        elif group == 'sim':
            return filter(
                lambda label: self.variations[label]['up'].sim is not None,
                self.variations
            )
        elif group == 'all':
            return iter(self.variations)
        else:
            raise RuntimeError('Unknown group "{}".'.format(group))



class SystConfig:
    """Configuration for systematic variations in multiple eras.
    
    The configuration is described by a JSON file of the following
    format:
      {
        "directory":  <string, optional>,
        "eras":  <list of strings with names of eras>,
        "nominal":  <samples>,
        "variations": {
          <label>: {
            "up":  <samples>,
            "down":  <samples>
          },
          // ...
        }
      }
    Each group <samples> above define a set of files for one specific
    variation.  It it described by the following dictionary:
      {
        "data":  <path>,
        "sim":  <path>,
        "weights":  <path>
      }
    which gives location of ROOT files with data, simulation, and
    weights for simulation.  Either entry "data" or the pair "sim" and
    "weights" can be omitted if the systematic variation applies only to
    simulation or only to data.  All paths will be prepended with the
    optional source directory, given at the top level of the
    configuration.  If a path contains a substring "{era}", it will be
    replaced with the label of the era when the configuration is
    instantiated for a specific era.
    """

    def __init__(self, path, eras=None):
        """Initialize from a configuration file.
        
        Arguments:
            path:  Path to a JSON configuration file.
            eras:  Eras for which configurations are to be instantiated.
                If given, overrides the list of eras provided in the
                configuration file.  If None (default), the list from
                the configuration file is used.
        """
        
        with open(path) as f:
            config = json.load(f)

        if eras is None:
            self.eras = config['eras']
        else:
            self.eras = eras

        # Construct configuration objects for individual eras in the
        # same order as they are listed in the file.  User might want to
        # process eras in a specific order.
        self.configs = OrderedDict()

        for era in self.eras:
            self.configs[era] = SystConfigEra(config, era)


    def __getitem__(self, era=None):
        """Return configuration for given era.
        
        Arguments:
            era:  Label of the desired era.  If None (default), return
                configuration for the first era (as given in the
                configuration file).

        Return value:
            SystConfigEra for requested era.
        """

        if era is None:
            return self.configs[self.eras[0]]
        else:
            return self.configs[era]


    def __iter__(self):
        """Return iterator over configurations for different eras."""

        return iter(self.configs)


    def __len__(self):
        """Return the number of eras."""

        return len(self.eras)

