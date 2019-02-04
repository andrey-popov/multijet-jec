import json
import math
import os


class TriggerBin:
    """Auxiliary class that represents a single trigger bin."""
    
    __slots__ = ['name', 'filter_name', 'pt_range', 'pt_range_margined']
    
    def __init__(self, trigger_name, config):
        """Initialize from configuration read from JSON file."""
        
        self.name = trigger_name
        self.filter_name = config['filter']
        self.pt_range = tuple(config['ptRange'])
        self.pt_range_margined = tuple(config['ptRangeMargined'])
    
    
    def __getitem__(self, field_name):
        """Provide an alternative way to access data members.
        
        Needed for backward compatibility with code in which trigger
        bins were represented with dictionaries.
        """
        
        if field_name == 'ptRange':
            return self.pt_range
        elif field_name == 'ptRangeMargined':
            return self.pt_range_margined
        elif field_name == 'filter':
            return self.filter_name
        else:
            raise AttributeError('Unknown attribute "{}".'.format(filter_name))


class TriggerBins:
    """Collection of triggers bins.
    
    This class represents a collection of trigger bins, which can be
    accessed by index or by name.  The bins are sorted in the increasing
    order in pt.  Definitions of triggers bins are read from a JSON
    file.
    """
    
    def __init__(self, path, clip=math.inf):
        """Initialize from a file path.
        
        Read trigger bins configuration from the given path.  If the
        file does not exist, try to resolve the path with respect to a
        standard location.
        
        Arguments:
            path:  Path to JSON file with the configuration.
            clip:  Upper boundaries of trigger bins in pt (without the
                margin) will be clipped using this value.  Trigger bins
                whose pt range without the margin is fully above this
                value, will be skipped.
        """
        
        # Attempt to resolve the path with respect to a standard
        # location if it does not match any file
        if not os.path.exists(path):
            try_path = None
            
            if 'MULTIJET_JEC_INSTALL' in os.environ:
                try_path = os.path.join(os.environ['MULTIJET_JEC_INSTALL'], 'config', path)
            
            if try_path is None or not os.path.exists(try_path):
                raise RuntimeError('Failed to find file "{}".'.format(path))
            else:
                path = try_path
        
        
        # Read the configuration.  Skip trigger bins whose pt range
        # (without the margin) is fully above the clip value.
        with open(path) as f:
            self.bins = list(TriggerBin(k, v) for k, v in json.load(f).items())

        self.bins = list(filter(lambda bin: bin.pt_range[0] < clip, self.bins))
        
        
        # Make sure the bins are sorted in pt
        self.bins.sort(key=lambda b: b.pt_range[0])
        
        
        # One of the trigger bins normally extends to very large pt (but
        # the value is finite as infinities are not supported in other
        # code that uses that file).  Clip it.
        for bin in self.bins:
            r = bin.pt_range
            
            if r[1] > clip:
                r = (r[0], clip)
                bin.pt_range = r
            
            if r[0] >= r[1]:
                raise RuntimeError('Illegal pt range for trigger "{}": {}.'.format(
                    trigger_name, r
                ))
        
        
        self.names = [b.name for b in self.bins]
        self._lookup_by_name = {b.name: b for b in self.bins}
    
    
    def __getitem__(self, index):
        """Access trigger bin by index."""
        
        return self.bins[index]
    
    
    def __iter__(self):
        """Return iterator over trigger bins."""
        
        return iter(self.bins)
    
    
    def __len__(self):
        """Return number of available trigger bins."""
        
        return len(self.bins)
    
    
    def check_alignment(self, binning, silent=False):
        """Check alignment with given binning.
        
        If all boundaries of trigger bins without the margins are found
        in the given binning, the two are aligned.
        
        Arguments:
            binning:  Binning against which to check the alignment.
            silent:  If false, raise an exception in case of
                misalignment.  If true, indicate the result of the check
                with boolean return value.
        
        Return value:
            True if alignment is correct, false if the alignment is
            wrong and caller has not requested to raise an exception.
        """
        
        mismatched_edges = []
        
        for trigger_bin in self.bins:
            for edge in trigger_bin.pt_range:
                if edge not in binning:
                    mismatched_edges.append(edge)
        
        if mismatched_edges:
            if silent:
                return False
            else:
                raise RuntimeError(
                    'Following boundaries of trigger bins are not aligned with the binning: {}.'.format(
                        mismatched_edges
                    )
                )
        else:
            return True
    
    
    def find(self, name):
        """Access trigger bin by trigger name."""
        
        return self._lookup_by_name[name]
