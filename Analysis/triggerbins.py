from collections import OrderedDict
import json
import math
import os


class TriggerBins(OrderedDict):
    """Facilitates working with configuration for trigger bins.
    
    This class reads a configuration that defines trigger bins and
    implements some sanity checks.  The configuration is exposed as an
    ordered dictionary, which this class subclasses.  Triggers bins are
    sorted in the increasing order in pt.
    """
    
    def __init__(self, path, clip=math.inf):
        """Initialize from a file path.
        
        Read trigger bins configuration from the given path.  If the
        file does not exist, try to resolve the path with respect to a
        standard location.
        
        Arguments:
            path:  Path to JSON file with the configuration.
            clip:  Upper boundaries of trigger bins in terms of
                corrected pt will be clipped using this value.
        """
        
        super().__init__()
        
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
        
        
        # Read the configuration.  Sort trigger bins in pt.
        with open(path) as f:
            bins = list(json.load(f).items())
            bins.sort(key=lambda b: b[1]['ptRange'][0])
            self.update(bins)
        
        
        # One of the trigger bins normally extends to very large pt (but
        # the value is finite as infinities are not supported in other
        # code that uses that file).  Clip it.
        for trigger_name, trigger_bin in self.items():
            corr_range = trigger_bin['ptRange']
            
            if corr_range[1] > clip:
                corr_range[1] = clip
            
            if corr_range[0] >= corr_range[1]:
                raise RuntimeError('Illegal corrected pt range for trigger "{}": {}.'.format(
                    trigger_name, corr_range
                ))
    
    
    def check_alignment(self, binning, silent=False):
        """Check alignment with given binning.
        
        If all boundaries of trigger bins expressed in corrected pt are
        found in the given binning, the two are aligned.
        
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
        
        for trigger_bin in self.values():
            for edge in trigger_bin['ptRange']:
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
