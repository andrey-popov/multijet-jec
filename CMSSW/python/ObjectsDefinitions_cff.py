"""Definitions of reconstructed physics objects.

This module exports several define_* functions that are used in the main
configuration.  At the moment it simply forwards definitions from
package PEC-tuples.
"""

import FWCore.ParameterSet.Config as cms

from Analysis.PECTuples.ObjectsDefinitions_cff import (
    setup_egamma_preconditions, define_jets
)


def define_METs(process, task, runOnData=False, legacy2016=False):
    """Define reconstructed MET.
    
    Configure computation of corrected CHS PF MET and its systematic
    uncertainties.  Apply type-1 corrections as well as additional
    corrections for bad muons and ECAL gain switch.  What is stored as
    raw MET includes these additional corrections.  The additional
    corrections are not applied for the legacy 2016 data.
    
    Arguments:
        process: The process to which relevant MET producers are added.
        task: Task to which non-standard producers are attached.
        runOnData: Flag to distinguish processing of data and
            simulation.
        legacy2016: Flags legacy 2016 data for which there is no need
            for the correction for the ECAL gain switch.
    
    Return value:
        InputTag that defines MET collection to be used.
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
    
    task.add(process.packedPFCandidatesCHS)
    
    from PhysicsTools.PatUtils.tools.runMETCorrectionsAndUncertainties import \
        runMetCorAndUncFromMiniAOD
    runMetCorAndUncFromMiniAOD(
        process, isData=runOnData, pfCandColl='packedPFCandidatesCHS',
        recoMetFromPFCs=True
    )

    # Fix a bug in runMetCorAndUncFromMiniAOD, which does not add one of
    # producers to the PAT task
    from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask
    getPatAlgosToolsTask(process).add(process.pfNoPileUpJME)
    
    metTag = cms.InputTag('slimmedMETs', processName=process.name_())
    return metTag
