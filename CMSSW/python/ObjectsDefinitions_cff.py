"""Definitions of reconstructed physics objects.

This module exports several define_* functions that are used in the main
configuration.  At the moment it simply forwards definitions from
package PEC-tuples.
"""

import FWCore.ParameterSet.Config as cms

from Analysis.PECTuples.ObjectsDefinitions_cff import (
    define_electrons, define_muons, define_photons, define_jets
)


def define_METs(process, runOnData=False):
    """Define reconstructed MET.
    
    Configure recalculation of corrected MET and its systematic
    uncertainties.  CHS MET is computed.  Only type-1 corrections are
    applied.
    
    Arguments:
        process: The process to which relevant MET producers are added.
        runOnData: Flag to distinguish processing of data and
            simulation.
    
    Return value:
        None.
    
    Among other things, add to the process producer slimmedMETs, which
    overrides the namesake collection from MiniAOD.  User must use this
    new collection.
    """
    
    # Apply charged hadron subtraction to the collection of PF
    # candidates.  At RECO level the CHS is performed by removing all
    # charged hadron PF candidates that are associated to a PV but the
    # first one [1].  With MiniAOD a different implementation is used
    # (see [2-3] for examples), which is based on flags indicating
    # quality of association with PV [4-5].  The results are identical,
    # though.  All PF candidates matched to a PV are identified in [6],
    # which is similar to the algorithm in [1].  In [7] those of them
    # that have a track are assigned quality bits UsedInFitTight or
    # UsedInFitLoose, while candidates without a track are always given
    # UsedInFitLoose [8].  In [5] candidates associated with the first
    # PV and also leptons are assigned good quality flags, and remaining
    # pileup particles are given quality NoPV = 0.  Any candidates not
    # associated to any vertices (thus having quality less than
    # UsedInFitLoose), are given at least PVLoose.
    # [1] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/CommonTools/ParticleFlow/src/PFPileUpAlgo.cc
    # [2] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/PhysicsTools/PatUtils/python/tools/runMETCorrectionsAndUncertainties.py#L1259
    # [3] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/PhysicsTools/PatAlgos/test/miniAOD/example_addJet.py#L16
    # [4] https://twiki.cern.ch/twiki/bin/view/CMSPublic/WorkBookMiniAOD2016?rev=13#PV_Assignment
    # [5] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/DataFormats/PatCandidates/interface/PackedCandidate.h#L429-L438
    # [6] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/CommonTools/RecoAlgos/src/PrimaryVertexAssignment.cc#L21-L32
    # [7] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/PhysicsTools/PatAlgos/plugins/PATPackedCandidateProducer.cc#L225-L228
    # [8] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_11/PhysicsTools/PatAlgos/plugins/PATPackedCandidateProducer.cc#L249
    process.packedPFCandidatesCHS = cms.EDFilter('CandPtrSelector',
        src = cms.InputTag('packedPFCandidates'),
        cut = cms.string('fromPV(0) > 0')
    )
    
    
    # Recompute MET together with corrections [1].  The function
    # runMetCorAndUncFromMiniAOD has arguments CHS and recoMetFromPFCs,
    # whose names sound interestingly, but they are misleading.
    # Uncorrected MET is first built by producer with label 'pfMet',
    # which is created by the function.  It is replaced with a module
    # that computes MET from the collection of CHS PF candidates.
    # [1] https://github.com/cms-sw/cmssw/blob/CMSSW_8_0_18/PhysicsTools/PatAlgos/test/corMETFromMiniAOD.py
    from PhysicsTools.PatUtils.tools.runMETCorrectionsAndUncertainties import \
        runMetCorAndUncFromMiniAOD
    
    runMetCorAndUncFromMiniAOD(
        process, isData=runOnData, pfCandColl='packedPFCandidatesCHS'
    )
    
    del(process.pfMet)
    process.pfMet = cms.EDProducer('PFMETProducer',
        alias = cms.string('pfMet'),
        calculateSignificance = cms.bool(False),
        globalThreshold = cms.double(0.0),
        src = cms.InputTag('packedPFCandidatesCHS')
    )
