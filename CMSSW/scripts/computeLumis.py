#!/usr/bin/env python

"""This script computes integrated luminosities for PFJet triggers.

For each given luminosity mask the script computes integrated
luminosities for a set of triggers of the form HLT_PFJet*_v*.
"""

from __future__ import print_function
import argparse
import Queue
import re
import subprocess
from threading import Thread


def worker(queue, lumis, normtag):
    """Read next mask-trigger pair and compute luminosity for it."""
    
    while True:
        # Read next mask and trigger combination
        try:
            mask, trigger = queue.get_nowait()
        except Queue.Empty:
            break
        
        
        # Issue command to compute luminosity
        brilcalcCall = subprocess.Popen(
            [
                'brilcalc', 'lumi', '--normtag', normtag, '-i', mask, '-u', '/pb',
                '--hltpath', 'HLT_{}_v*'.format(trigger)
            ],
            stdout=subprocess.PIPE, universal_newlines=True
        )
        
        
        # Parse its output.  Sum up luminosities for different versions.
        lumi = 0.
        summaryRegex = re.compile(
            r'^\|\s*HLT_{}_v\d+\s*\|(\s*\d+\s*\|){{3}}\s*\d+\.\d+\s*\|\s*(\d+\.\d+)\s*\|\s*$'.format(
                trigger    
            )
        )
        summaryFound = False
        
        # Skip until the summary table
        for line in brilcalcCall.stdout:
            if line.startswith('#Summary:'):
                break
        
        
        for line in brilcalcCall.stdout:
            match = summaryRegex.match(line)
            
            if match:
                summaryFound = True
                lumi += float(match.group(2))
        
        brilcalcCall.wait()
        
        
        if not summaryFound:
            raise RuntimeError(
                'Failed to find summary in brilcalc output for mask "{}" and trigger "{}".'.format(
                    mask, trigger
                )
            )
        
        
        # Store luminosity in the output list
        lumis.append((mask, trigger, lumi))
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
    args = argParser.parse_args()
    
    if not args.masks:
        raise RuntimeError('No lumi masks provided')
    
    normtag = '/cvmfs/cms-bril.cern.ch/cms-lumi-pog/Normtags/' + args.normtag
    
    
    # Define tasks to process
    triggers = ['PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450']
    queue = Queue.Queue()
    
    for mask in args.masks:
        for trigger in triggers:
            queue.put((mask, trigger))
    
    
    # Compute all tasks using a thread pool
    lumis = []
    threads = []
    
    for i in range(16):
        t = Thread(target=worker, args=(queue, lumis, normtag))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
    
    
    # Print results
    lumiMap = {}
    
    for mask, trigger, lumi in lumis:
        lumiMap[mask, trigger] = lumi
    
    for mask in args.masks:
        print('Recorded luminosities for mask {}, in 1/pb:'.format(mask))
        
        for trigger in triggers:
            print('  {}: {:10.3f}'.format(trigger, lumiMap[mask, trigger]))
        
        print()
