"""Defines a class to perform clipping to trigger bins."""

import json
import math


class TriggerClip:
    """A class to apply pt cut based on trigger bin."""
    
    def __init__(self, triggerBinsFileName):
        
        with open(triggerBinsFileName) as triggerBinsFile:
            triggerBins = json.load(triggerBinsFile)
        
        self.thresholds = {}
        
        for triggerBin in triggerBins:
            self.thresholds[int(triggerBin['index'])] = float(triggerBin['threshold'])
    
    
    def check_pt(self, triggerBin, pt):
        return pt > self.thresholds[triggerBin]
    
    
    def check_event(self, event):
        return event.PtJ1 * math.cos(event.Alpha) > self.thresholds[event.TriggerBin]
