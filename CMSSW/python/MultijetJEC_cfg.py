"""Configuration for cmsRun to produce PEC tuples from MiniAOD.

This configuration exploits object definitions and modules from package
PEC-tuples [1] to produce customized tuples intended for measurements of
JEC with the multijet method.

Stored information mostly comprises reconstructed jets and MET,
generator-level jets, and trigger decisions.  Events accepted by none of
the selected triggers are rejected.  In addition, an event it rejected
if it contains a loosely defined muon, electron, or photon.

[1] https://github.com/andrey-popov/PEC-tuples
"""

import random
import string

import FWCore.ParameterSet.Config as cms


# Create a process
from Configuration.StandardSequences.Eras import eras
process = cms.Process('Analysis', eras.Run2_2017)


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
    'globalTag', '', VarParsing.multiplicity.singleton, VarParsing.varType.string,
    'Global tag to be used'
)
options.register(
    'runOnData', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Indicates whether the job processes data or simulation'
)
options.register(
    'period', '2017', VarParsing.multiplicity.singleton, VarParsing.varType.string,
    'Data-taking period'
)
# options.register(
#     'isPromptReco', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
#     'In case of data, distinguishes PromptReco and ReReco. Ignored for simulation'
# )
options.register(
    'triggerProcessName', 'HLT', VarParsing.multiplicity.singleton,
    VarParsing.varType.string, 'Name of the process that evaluated trigger decisions'
)
options.register(
    'saveGenJets', True, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Save information about generator-level jets'
)

# Override defaults for automatically defined options
options.setDefault('maxEvents', 100)
options.setType('outputFile', VarParsing.varType.string)
options.setDefault('outputFile', 'sample.root')

options.parseArguments()


# Check provided data-taking period.  At the moment only '2017' is
# supported.
if options.period not in ['2017']:
    raise RuntimeError('Data-taking period "{}" is not supported.'.format(options.period))


# Make shortcuts to access some of the configuration options easily
runOnData = options.runOnData


# Provide a default global tag if user has not given any.  Chosen
# according to [1].  They include outdated JEC Summer16_23Sep2016V4.
# [1] https://twiki.cern.ch/twiki/bin/view/CMSPublic/SWGuideFrontierConditions#Global_Tags_for_2017_Nov_re_reco?rev=615
if not options.globalTag:
    if runOnData:
        options.globalTag = '94X_dataRun2_ReReco_EOY17_v2'
    else:
        options.globalTag = '94X_mc2017_realistic_v10'
    
    print 'WARNING: No global tag provided. Will use the default one: {}.'.format(
        options.globalTag
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
    if runOnData:
        process.source.fileNames = cms.untracked.vstring('/store/data/Run2017F/JetHT/MINIAOD/17Nov2017-v1/50000/066DF59D-99DF-E711-9B58-02163E019C9B.root')
    else:
        process.source.fileNames = cms.untracked.vstring('/store/mc/RunIIFall17MiniAOD/QCD_HT500to700_TuneCP5_13TeV-madgraph-pythia8/MINIAODSIM/94X_mc2017_realistic_v10-v1/20000/02B9CF2E-B4FD-E711-8A43-0CC47A78A3F4.root')

# process.source.fileNames = cms.untracked.vstring('/store/relval/...')

# Set a specific event range here (useful for debugging)
# process.source.eventsToProcess = cms.untracked.VEventRange('1:5')

# Set the maximum number of events to process for a local run (it is overiden by CRAB)
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))


# Set up random-number service.  CRAB overwrites the seeds
# automatically.
process.RandomNumberGeneratorService = cms.Service('RandomNumberGeneratorService',
    analysisPatJets = cms.PSet(
        initialSeed = cms.untracked.uint32(372),
        engineName = cms.untracked.string('TRandom3')
    ),
    countGoodJets = cms.PSet(
        initialSeed = cms.untracked.uint32(3631),
        engineName = cms.untracked.string('TRandom3')
    )
)


# Information about geometry and magnetic field is needed to run DeepCSV
# b-tagging.  Geometry is also needed to evaluate electron ID.
process.load('Configuration.Geometry.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')


# Create the processing path.  It is wrapped in a manager that
# simplifies working with multiple paths (and also provides the
# interface expected by python tools in the PEC-tuples package).
process.p = cms.Path()

from Analysis.PECTuples.Utils_cff import PathManager
paths = PathManager(process.p)


# Include an event counter before any selection is applied.  It is only
# needed for simulation.
if not runOnData:
    process.eventCounter = cms.EDAnalyzer('EventCounter',
        generator = cms.InputTag('generator'),
        saveAltLHEWeights = cms.bool(False),
        puInfo = cms.InputTag('slimmedAddPileupInfo')
    )
    paths.append(process.eventCounter)


# Filter on properties of the first vertex
process.goodOfflinePrimaryVertices = cms.EDFilter('FirstVertexFilter',
    src = cms.InputTag('offlineSlimmedPrimaryVertices'),
    cut = cms.string('!isFake & ndof > 4. & abs(z) < 24. & position.rho < 2.')
)

paths.append(process.goodOfflinePrimaryVertices)


# Define and customize basic reconstructed objects
from Analysis.Multijet.ObjectsDefinitions_cff import (
    define_electrons, define_photons, define_jets, define_METs
)
process.analysisTask = cms.Task()

eleQualityCuts, eleEmbeddedCutBasedIDLabels, eleCutBasedIDMaps, eleMVAIDMaps = define_electrons(
    process, process.analysisTask
)
phoQualityCuts, phoCutBasedIDMaps = define_photons(process, process.analysisTask)
recorrectedJetsLabel, jetQualityCuts = \
    define_jets(process, process.analysisTask, reapplyJEC=True, runOnData=runOnData)
metTag = define_METs(process, process.analysisTask, runOnData=runOnData)

process.analysisPatElectrons.cut = 'pt > 20. & abs(superCluster.eta) < 2.5'
process.analysisPatPhotons.cut = process.analysisPatElectrons.cut

process.analysisPatJets.minPt = 0
process.analysisPatJets.preselection = ''


# Apply event filters recommended for analyses involving MET
from Analysis.PECTuples.EventFilters_cff import apply_event_filters
apply_event_filters(
    process, paths, runOnData=runOnData,
    processName='RECO' if runOnData else 'PAT'
)


# Apply lepton veto
process.looseMuons = cms.EDFilter('PATMuonSelector',
    src = cms.InputTag('slimmedMuons'),
    cut = cms.string(
        'pt > 10. & abs(eta) < 2.4 & isLooseMuon & (pfIsolationR04.sumChargedHadronPt + ' \
        'max(0., pfIsolationR04.sumNeutralHadronEt + pfIsolationR04.sumPhotonEt - ' \
        '0.5 * pfIsolationR04.sumPUPt)) / pt < 0.25'
    )
)
process.vetoMuons = cms.EDFilter('PATCandViewCountFilter',
    src = cms.InputTag('looseMuons'),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
process.analysisTask.add(process.looseMuons)
paths.append(process.vetoMuons)

process.vetoElectrons = cms.EDFilter('CandMapCountFilter',
    src = cms.InputTag('analysisPatElectrons'),
    acceptMap = cms.InputTag(eleCutBasedIDMaps[0]),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
paths.append(process.vetoElectrons)


# Apply photon veto
process.vetoPhotons = cms.EDFilter('CandMapCountFilter',
    src = cms.InputTag('analysisPatPhotons'),
    acceptMap = cms.InputTag(phoCutBasedIDMaps[0]),
    minNumber = cms.uint32(0), maxNumber = cms.uint32(0)
)
paths.append(process.vetoPhotons)


# Save decisions of selected triggers.  The lists are based on menu [1],
# which was used in the re-HLT campaign with RunIISpring16MiniAODv2.
# [1] /frozen/2016/25ns10e33/v2.1/HLT/V3
triggerNames = [
    'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450', 'PFJet500'
]

if runOnData:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(triggerNames),
        filter = cms.bool(True),
        savePrescales = cms.bool(True),
        triggerBits = cms.InputTag('TriggerResults', processName=options.triggerProcessName),
        hltPrescales = cms.InputTag('patTrigger'),
        l1tPrescales = cms.InputTag('patTrigger', 'l1min')
    )
else:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(triggerNames),
        filter = cms.bool(True),
        savePrescales = cms.bool(False),
        triggerBits = cms.InputTag('TriggerResults', processName=options.triggerProcessName)
    )

paths.append(process.pecTrigger)


# Save trigger objects that pass last filters in the single-jet triggers
process.pecTriggerObjects = cms.EDAnalyzer('PECTriggerObjects',
    triggerResults = cms.InputTag('TriggerResults', processName=options.triggerProcessName),
    triggerObjects = cms.InputTag('slimmedPatTrigger'),
    filters = cms.vstring(
        'hltSinglePFJet140', 'hltSinglePFJet200', 'hltSinglePFJet260', 'hltSinglePFJet320',
        'hltSinglePFJet400', 'hltSinglePFJet450', 'hltSinglePFJet500'
    )
)

paths.append(process.pecTriggerObjects)


# Save event ID and relevant objects
process.pecEventID = cms.EDAnalyzer('PECEventID')

process.pecJetMET = cms.EDAnalyzer('PECJetMET',
    runOnData = cms.bool(runOnData),
    jets = cms.InputTag('analysisPatJets'),
    jetSelection = jetQualityCuts,
    jetIDVersion = cms.string('2017'),
    met = metTag
)

process.pecPileUp = cms.EDAnalyzer('PECPileUp',
    primaryVertices = cms.InputTag('goodOfflinePrimaryVertices'),
    rho = cms.InputTag('fixedGridRhoFastjetAll'),
    runOnData = cms.bool(runOnData),
    puInfo = cms.InputTag('slimmedAddPileupInfo'),
    saveMaxPtHat = cms.bool(True)
)

paths.append(process.pecEventID, process.pecJetMET, process.pecPileUp)


# Save global generator information
if not runOnData:
    process.pecGenerator = cms.EDAnalyzer('PECGenerator',
        generator = cms.InputTag('generator'),
        saveAltLHEWeights = cms.bool(False),
        lheEventProduct = cms.InputTag('')
    )
    paths.append(process.pecGenerator)
    
    process.pecGenParticles = cms.EDAnalyzer('PECGenParticles',
        genParticles = cms.InputTag('prunedGenParticles')
    )
    paths.append(process.pecGenParticles)


# Save information on generator-level jets and MET
if not runOnData and options.saveGenJets:
    process.pecGenJetMET = cms.EDAnalyzer('PECGenJetMET',
        jets = cms.InputTag('slimmedGenJets'),
        cut = cms.string(''),
        saveFlavourCounters = cms.bool(True),
        met = metTag
    )
    paths.append(process.pecGenJetMET)


# Associate with the paths the analysis-specific task and the task
# filled by PAT tools automatically
paths.associate(process.analysisTask)

from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask
paths.associate(getPatAlgosToolsTask(process))


# The output file for the analyzers
postfix = '_' + string.join([random.choice(string.letters) for i in range(3)], '')

if options.outputFile.endswith('.root'):
    outputBaseName = options.outputFile[:-5] 
else:
    outputBaseName = options.outputFile

process.TFileService = cms.Service('TFileService',
    fileName = cms.string(outputBaseName + postfix + '.root'))
