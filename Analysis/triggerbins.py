from collections import OrderedDict
import json
import math
import os


class TriggerBins(OrderedDict):
    """Facilitates working with configuration for trigger bins.
    
    This class reads a configuration that defines trigger bins and
    implements some sanity checks.  The configuration is exposed as an
    ordered dictionary, which this class subclasses.
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
            tryPath = None
            
            if 'MULTIJET_JEC_INSTALL' in os.environ:
                tryPath = os.path.join(os.environ['MULTIJET_JEC_INSTALL'], 'config', path)
            
            if tryPath is None or not os.path.exists(tryPath):
                raise RuntimeError('Failed to find file "{}".'.format(path))
            else:
                path = tryPath
        
        
        # Read the configuration
        with open(path) as f:
            self.update(json.load(f, object_pairs_hook=OrderedDict))
        
        
        # One of the trigger bins normally extends to very large pt (but
        # the value is finite as infinities are not supported in other
        # code that uses that file).  Clip it.
        for triggerName, triggerBin in self.items():
            corrRange = triggerBin['ptRange']
            
            if corrRange[1] > clip:
                corrRange[1] = clip
            
            if corrRange[0] >= corrRange[1]:
                raise RuntimeError('Illegal corrected pt range for trigger "{}": {}.'.format(
                    triggerName, corrRange
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
        
        mismatchedEdges = []
        
        for triggerBin in self.values():
            for edge in triggerBin['ptRange']:
                if edge not in binning:
                    mismatchedEdges.append(edge)
        
        if mismatchedEdges:
            if silent:
                return False
            else:
                raise RuntimeError(
                    'Following boundaries of trigger bins are not aligned with the binning: {}.'.format(
                        mismatchedEdges
                    )
                )
        else:
            return True
