"""Definitions of reconstructed physics objects.

This module exports several define_* functions that are used in the main
configuration.  At the moment it simply forwards definitions from
package PEC-tuples.
"""

import FWCore.ParameterSet.Config as cms

from Analysis.PECTuples.ObjectsDefinitions_cff import (
    setup_egamma_preconditions
)

