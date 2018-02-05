#!/usr/bin/env python

"""Builds pileup profiles for single-jet trigger.

This script needs to be run in an environment with brilws and CMSSW.
"""

from __future__ import print_function
import argparse
import os
import Queue
import subprocess
from threading import Thread
from uuid import uuid4


def worker(queue, normtag, pileupInput):
    """Compute pileup profile for the next mask-trigger pair."""
    
    while True:
        
        # Read next mask and trigger combination
        try:
            mask, trigger = queue.get_nowait()
        except Queue.Empty:
            break
        
        
        # Run commands to compute pileup profile
        lumiCSVFile = 'lumi_{}.csv'.format(uuid4().hex)
        subprocess.check_call([
            'brilcalc', 'lumi', '--byls', '-i', mask, '--normtag', normtag,
            '--hltpath', 'HLT_{}_v*'.format(trigger), '-o', lumiCSVFile
        ])
        
        pileupJSONFile = 'pileup_{}.json'.format(uuid4().hex)
        subprocess.check_call([
            'pileupReCalc_HLTpaths.py', '-i', lumiCSVFile, '--inputLumiJSON', pileupInput,
            '--runperiod', 'Run2', '-o', pileupJSONFile
        ])
        
        subprocess.check_call([
            'pileupCalc.py', '-i', mask, '--inputLumiJSON', pileupJSONFile,
            '--calcMode', 'true', '--minBiasXsec', '69200', '--maxPileupBin', '100',
            '--numPileupBins', '1000',
            'pileup_{}_{}_finebin.root'.format(os.path.splitext(mask)[0], trigger)
        ])
        
        
        # Remove intermediate files
        for f in [lumiCSVFile, pileupJSONFile]:
            os.remove(f)
        
        queue.task_done()



if __name__ == '__main__':
    
    # Parse arguments
    argParser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    argParser.add_argument(
        'masks', metavar='masks', nargs='*',
        help='Lumi masks to select data'
    )
    argParser.add_argument(
        '--normtag', default='normtag_BRIL.json',
        help='Normtag for luminosity computation. Resolved w.r.t. standard location.'
    )
    argParser.add_argument(
        '--pileup-input', default=None, dest='pileupInput',
        help='Pileup JSON file'
    )
    args = argParser.parse_args()
    
    if not args.masks:
        raise RuntimeError('No lumi masks provided')
    
    normtag = '/cvmfs/cms-bril.cern.ch/cms-lumi-pog/Normtags/' + args.normtag
    
    if not args.pileupInput:
        raise RuntimeError('No pileup JSON file provided')
    
    
    # Define tasks to process
    triggers = ['PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450']
    queue = Queue.Queue()
    
    for mask in args.masks:
        for trigger in triggers:
            queue.put((mask, trigger))
    
    
    # Build pileup profiles for all tasks using a thread pool
    threads = []
    
    for i in range(16):
        t = Thread(target=worker, args=(queue, normtag, args.pileupInput))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
