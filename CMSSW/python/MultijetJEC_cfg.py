"""Configuration for cmsRun to produce PEC tuples from MiniAOD.

This configuration exploits object definitions and modules from package
PEC-tuples [1] to produce customized tuples intended for measurements of
JEC with the multijet method.  Some information is stored using
dedicated plugins defined in this package.

Stored information mostly comprises reconstructed jets and MET,
generator-level jets, and trigger decisions.  Events accepted by none of
the selected triggers are rejected.  In addition, an event it rejected
if it contains a loosely defined muon, electron, or photon.

[1] https://github.com/andrey-popov/PEC-tuples
"""

from __future__ import print_function
import random
import string

import FWCore.ParameterSet.Config as cms


# Create a process
process = cms.Process('Analysis')


# Enable MessageLogger and reduce its verbosity
process.load('FWCore.MessageLogger.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 1000


# Ask to print a summary in the log
process.options = cms.untracked.PSet(
    wantSummary = cms.untracked.bool(True)
)


# Parse command-line options.  In addition to the options defined below,
# use several standard ones: inputFiles, outputFile, maxEvents.
from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing('analysis')

options.register(
    'globalTag', '', VarParsing.multiplicity.singleton,
    VarParsing.varType.string, 'Global tag to be used'
)
options.register(
    'runOnData', False, VarParsing.multiplicity.singleton,
    VarParsing.varType.bool,
    'Indicates whether the job processes data or simulation'
)
options.register(
    'period', '2016', VarParsing.multiplicity.singleton,
    VarParsing.varType.string, 'Data-taking period'
)
options.register(
    'triggerProcessName', 'HLT', VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    'Name of the process that evaluated trigger decisions'
)

# Override defaults for automatically defined options
options.setDefault('maxEvents', 100)
options.setType('outputFile', VarParsing.varType.string)
options.setDefault('outputFile', '')

options.parseArguments()


# Check provided data-taking period
if options.period not in ['2016', '2017']:
    raise RuntimeError(
        'Data-taking period "{}" is not supported.'.format(options.period)
    )

# Make shortcuts to access some of the configuration options easily
is_data = options.runOnData


# Provide a default global tag if user has not given any.  Chosen
# according to [1].
# [1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/PdmVAnalysisSummaryTable?rev=10
if not options.globalTag:
    if options.period == '2016':
        if is_data:
            options.globalTag = '94X_dataRun2_v10'
        else:
            options.globalTag = '94X_mcRun2_asymptotic_v3'
    elif options.period == '2017':
        if is_data:
            options.globalTag = '94X_dataRun2_v11'
        else:
            options.globalTag = '94X_mc2017_realistic_v17'
    
    print(
        'No global tag provided. Will use the default one: "{}".'.format(
            options.globalTag
        )
    )

# Set the global tag
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_condDBv2_cff')
from Configuration.AlCa.GlobalTag_condDBv2 import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, options.globalTag)


# Define the input files
process.source = cms.Source('PoolSource')

if len(options.inputFiles) > 0:
    process.source.fileNames = cms.untracked.vstring(options.inputFiles)
else:
    # Default input files for testing
    test_file = None

    if options.period == '2016':
        if is_data:
            # A file from era 2016H, which contains multiple certified
            # runs, including 283885.
            test_file = '/store/data/Run2016H/JetHT/MINIAOD/17Jul2018-v1/50000/F2DFE866-0E90-E811-B8FC-0025905C5430.root'
        else:
            test_file = '/store/mc/RunIISummer16MiniAODv3/QCD_HT500to700_TuneCUETP8M1_13TeV-madgraphMLM-pythia8/MINIAODSIM/PUMoriond17_94X_mcRun2_asymptotic_v3-v2/110000/42BEEC72-02EA-E811-94ED-003048CB7DA2.root'
    elif options.period == '2017':
        if is_data:
            # A file from era 2017F, which contains lumi sections from
            # multiple certified runs, including 305064.
            test_file = '/store/data/Run2017F/JetHT/MINIAOD/31Mar2018-v1/80000/A4BA7F54-0137-E811-819B-B496910A9A2C.root'
        else:
            test_file = '/store/mc/RunIIFall17MiniAODv2/QCD_HT500to700_TuneCP5_13TeV-madgraph-pythia8/MINIAODSIM/PU2017_12Apr2018_94X_mc2017_realistic_v14-v2/100000/48B9B41E-D197-E811-949E-A4BF0112BD6A.root'

    process.source.fileNames = cms.untracked.vstring(test_file)

# Set a specific event range here (useful for debugging)
# process.source.eventsToProcess = cms.untracked.VEventRange('1:5')

# Set the maximum number of events to process for a local run (it is overiden by CRAB)
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))


# Set indices of alternative generator-level weights to be stored.
# The vector is parsed with C++ class IndexIntervals.
if not is_data:
    # Alternative LHE weights for scale variations [1]
    # [1] https://github.com/andrey-popov/PEC-tuples/issues/86#issuecomment-481698177
    alt_lhe_weight_indices = cms.vint32(1, 8)

    # Alternative parton shower weights. Only store variations in the
    # main ISR and FSR scales [1].
    # [1] https://github.com/cms-sw/cmssw/blob/e580f628505e08ac1577040d47fa2f041125e250/PhysicsTools/NanoAOD/plugins/GenWeightsTableProducer.cc#L240-L248
    alt_ps_weight_indices = cms.vint32(6, 9)


# Information about geometry is also needed to evaluate egamma ID
process.load('Configuration.Geometry.GeometryRecoDB_cff')


# Create the processing path.  It is wrapped in a manager that
# simplifies working with multiple paths (and also provides the
# interface expected by Python tools in the PEC-tuples package).
from Analysis.PECTuples.Utils_cff import PathManager
process.p = cms.Path()
paths = PathManager(process.p)


# Include an event counter before any selection is applied.  It is only
# needed for simulation.
if not is_data:
    process.eventCounter = cms.EDAnalyzer('EventCounter',
        generator = cms.InputTag('generator'),
        saveAltLHEWeights = alt_lhe_weight_indices,
        saveAltPSWeights = alt_ps_weight_indices,
        puInfo = cms.InputTag('slimmedAddPileupInfo')
    )
    paths.append(process.eventCounter)


# Filter on properties of the first vertex
process.goodOfflinePrimaryVertices = cms.EDFilter('FirstVertexFilter',
    src = cms.InputTag('offlineSlimmedPrimaryVertices'),
    cut = cms.string('!isFake & ndof > 4. & abs(z) < 24. & position.rho < 2.')
)
paths.append(process.goodOfflinePrimaryVertices)


# Customization of physics objects
process.analysisTask = cms.Task()
from Analysis.Multijet.ObjectsDefinitions_cff import (
    setup_egamma_preconditions
)
setup_egamma_preconditions(process, process.analysisTask, options.period)


# Apply event filters recommended for analyses involving MET
from Analysis.PECTuples.EventFilters_cff import apply_event_filters
apply_event_filters(
    process, paths, options.period, runOnData=is_data,
    processName='RECO' if is_data else 'PAT'
)


# Apply lepton veto
process.looseMuons = cms.EDFilter('PATMuonSelector',
    src = cms.InputTag('slimmedMuons'),
    cut = cms.string(
        'pt > 10. & isLooseMuon & (pfIsolationR04.sumChargedHadronPt + ' \
        'max(0., pfIsolationR04.sumNeutralHadronEt + pfIsolationR04.sumPhotonEt - ' \
        '0.5 * pfIsolationR04.sumPUPt)) / pt < 0.25'
    )
)

if options.period == '2016':
    electron_id = 'cutBasedElectronID-Summer16-80X-V1-veto'
elif options.period == '2017':
    electron_id = 'cutBasedElectronID-Fall17-94X-V2-veto'

process.looseElectrons = cms.EDFilter('PATElectronSelector',
    src = cms.InputTag('slimmedElectrons'),
    cut = cms.string(
        'pt > 10. & ' \
        'electronID("{}") > 0.5'.format(electron_id)
    )
)
process.analysisTask.add(process.looseMuons, process.looseElectrons)

process.vetoMuons = cms.EDFilter('PATCandViewCountFilter',
    src = cms.InputTag('looseMuons'),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
process.vetoElectrons = cms.EDFilter('PATCandViewCountFilter',
    src = cms.InputTag('looseElectrons'),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
paths.append(process.vetoMuons, process.vetoElectrons)


# Apply photon veto
process.loosePhotons = cms.EDFilter('PATPhotonSelector',
    src = cms.InputTag('slimmedPhotons'),
    cut = cms.string(
        'pt > 10. & ' \
        'photonID("cutBasedPhotonID-Fall17-94X-V2-loose") > 0.5'
    )
)
process.analysisTask.add(process.loosePhotons)
process.vetoPhotons = cms.EDFilter('PATCandViewCountFilter',
    src = cms.InputTag('loosePhotons'),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
paths.append(process.vetoPhotons)


# Save decisions of selected triggers.  The lists are based on menu [1],
# which was used in the re-HLT campaign with RunIISpring16MiniAODv2.
# [1] /frozen/2016/25ns10e33/v2.1/HLT/V3
triggerNames = [
    'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450',
    'PFJet500'
]
trigger_results_tag = cms.InputTag(
    'TriggerResults', processName=options.triggerProcessName
)

if is_data:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(triggerNames),
        filter = cms.bool(True),
        savePrescales = cms.bool(True),
        triggerBits = trigger_results_tag,
        hltPrescales = cms.InputTag('patTrigger'),
        l1tPrescales = cms.InputTag('patTrigger', 'l1min')
    )
else:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(triggerNames),
        filter = cms.bool(True),
        savePrescales = cms.bool(False),
        triggerBits = trigger_results_tag
    )

paths.append(process.pecTrigger)


# Save trigger objects that pass last filters in the single-jet triggers
process.unpackedPatTrigger = cms.EDProducer(
    'PATTriggerObjectStandAloneUnpacker',
    patTriggerObjectsStandAlone = cms.InputTag('slimmedPatTrigger'),
    triggerResults = trigger_results_tag,
    unpackFilterLabels = cms.bool(True)
)
process.analysisTask.add(process.unpackedPatTrigger)

process.pecTriggerObjects = cms.EDAnalyzer('PECTriggerObjects',
    triggerResults = trigger_results_tag,
    triggerObjects = cms.InputTag('unpackedPatTrigger'),
    filters = cms.vstring(
        'hltSinglePFJet140', 'hltSinglePFJet200', 'hltSinglePFJet260',
        'hltSinglePFJet320', 'hltSinglePFJet400', 'hltSinglePFJet450',
        'hltSinglePFJet500'
    )
)
paths.append(process.pecTriggerObjects)


# Save event ID and relevant objects
process.pecEventID = cms.EDAnalyzer('PECEventID')

process.basicJetMET = cms.EDAnalyzer('BasicJetMET',
    runOnData = cms.bool(is_data),
    jets = cms.InputTag('slimmedJets'),
    jetIDVersion = cms.string(options.period),
    met = cms.InputTag('slimmedMETs')
)

process.pecPileUp = cms.EDAnalyzer('PECPileUp',
    primaryVertices = cms.InputTag('goodOfflinePrimaryVertices'),
    rho = cms.InputTag('fixedGridRhoFastjetAll'),
    runOnData = cms.bool(is_data),
    puInfo = cms.InputTag('slimmedAddPileupInfo'),
    saveMaxPtHat = cms.bool(True)
)

paths.append(process.pecEventID, process.basicJetMET, process.pecPileUp)


# Save global generator information
if not is_data:
    process.pecGenerator = cms.EDAnalyzer('PECGenerator',
        generator = cms.InputTag('generator'),
        saveAltLHEWeights = alt_lhe_weight_indices,
        lheEventProduct = cms.InputTag('externalLHEProducer'),
        saveAltPSWeights = alt_ps_weight_indices
    )
    paths.append(process.pecGenerator)
    
    process.pecGenParticles = cms.EDAnalyzer('PECGenParticles',
        genParticles = cms.InputTag('prunedGenParticles')
    )
    paths.append(process.pecGenParticles)


# Save information on generator-level jets and MET
if not is_data:
    process.pecGenJetMET = cms.EDAnalyzer('PECGenJetMET',
        jets = cms.InputTag('slimmedGenJets'),
        cut = cms.string(''),
        saveFlavourCounters = cms.bool(True),
        met = cms.InputTag('slimmedMETs')
    )
    paths.append(process.pecGenJetMET)


# Associate with the paths the analysis-specific task and the task
# filled by PAT tools automatically
paths.associate(process.analysisTask)

from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask
paths.associate(getPatAlgosToolsTask(process))


# The output file for the analyzers
if options.outputFile:
    output_file_name = options.outputFile

    if not output_file_name.endswith('.root'):
        output_file_name += '.root'
else:
    output_file_name = 'multijet_{}_{}_{}.root'.format(
        'data' if is_data else 'sim', options.period,
        ''.join(random.choice(string.letters) for i in range(5))
    )

print('Output file: "{}".'.format(output_file_name))

process.TFileService = cms.Service('TFileService',
    fileName = cms.string(output_file_name))

