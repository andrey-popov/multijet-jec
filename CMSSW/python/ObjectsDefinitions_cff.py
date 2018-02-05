"""Definitions of reconstructed physics objects.

This module exports several define_* functions that are used in the main
configuration.  At the moment it simply forwards definitions from
package PEC-tuples.
"""

import FWCore.ParameterSet.Config as cms

from Analysis.PECTuples.ObjectsDefinitions_cff import (
    define_electrons, define_muons, define_photons, define_jets,
    define_METs
)
