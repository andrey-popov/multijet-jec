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


# Create a process
import FWCore.ParameterSet.Config as cms
process = cms.Process('Analysis')


# Enable MessageLogger and reduce its verbosity
process.load('FWCore.MessageLogger.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 100


# Ask to print a summary in the log
process.options = cms.untracked.PSet(
    wantSummary = cms.untracked.bool(True),
    allowUnscheduled = cms.untracked.bool(True)
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
    'isPromptReco', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'In case of data, distinguishes PromptReco and ReReco. Ignored for simulation'
)
options.register(
    'triggerProcessName', 'HLT', VarParsing.multiplicity.singleton,
    VarParsing.varType.string, 'Name of the process that evaluated trigger decisions'
)
options.register(
    'saveGenJets', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Save information about generator-level jets'
)

# Override defaults for automatically defined options
options.setDefault('maxEvents', 100)
options.setType('outputFile', VarParsing.varType.string)
options.setDefault('outputFile', 'sample.root')

options.parseArguments()


# Make shortcuts to access some of the configuration options easily
runOnData = options.runOnData


# Provide a default global tag if user has not given any.  Chosen
# following recommendations of the JERC group [1].
# [1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/JECDataMC?rev=119
if len(options.globalTag) == 0:
    if runOnData:
        options.globalTag = '80X_dataRun2_Prompt_ICHEP16JEC_v0'
    else:
        options.globalTag = '80X_mcRun2_asymptotic_2016_miniAODv2_v1'
    
    print 'WARNING: No global tag provided. Will use the default one (' + options.globalTag + ')'

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
        process.source.fileNames = cms.untracked.vstring('/store/data/Run2016D/JetHT/MINIAOD/PromptReco-v2/000/276/315/00000/208AB2DD-0145-E611-910F-02163E0144AD.root')
    else:
        process.source.fileNames = cms.untracked.vstring('/store/mc/RunIISpring16MiniAODv2/QCD_HT500to700_TuneCUETP8M1_13TeV-madgraphMLM-pythia8/MINIAODSIM/PUSpring16_80X_mcRun2_asymptotic_2016_miniAODv2_v0-v1/00000/001FED44-BA3E-E611-8DAF-0025905C5474.root')

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


# Create the processing path.  It is wrapped in a manager that
# simplifies working with multiple paths (and also provides the
# interface expected by python tools in the PEC-tuples package).
class PathManager:
    
    def __init__(self, *paths):
        self.paths = []
        for p in paths:
            self.paths.append(p)
    
    def append(self, *modules):
        for p in self.paths:
            for m in modules:
                p += m

process.p = cms.Path()
paths = PathManager(process.p)


# Include an event counter before any selection is applied.  It is only
# needed for simulation.
if not runOnData:
    process.eventCounter = cms.EDAnalyzer('EventCounter',
        generator = cms.InputTag('generator'),
        saveAltLHEWeights = cms.bool(False)
    )
    paths.append(process.eventCounter)


# Filter on properties of the first vertex
process.goodOfflinePrimaryVertices = cms.EDFilter('FirstVertexFilter',
    src = cms.InputTag('offlineSlimmedPrimaryVertices'),
    cut = cms.string('!isFake & ndof > 4. & abs(z) < 24. & position.rho < 2.')
)

paths.append(process.goodOfflinePrimaryVertices)


# Define and customize basic reconstructed objects
from Analysis.Multijet.ObjectsDefinitions_cff import (define_photons, define_jets, define_METs)

(phoQualityCuts, phoCutBasedIDMaps) = define_photons(process)
(recorrectedJetsLabel, jetQualityCuts) = \
    define_jets(process, reapplyJEC=True, runOnData=runOnData)
define_METs(process, runOnData=runOnData)

process.analysisPatJets.minPt = 0
process.analysisPatJets.preselection = ''

process.analysisPatPhotons.cut = 'pt > 20. & abs(superCluster.eta) < 2.5'


# Apply event filters recommended for analyses involving MET
from Analysis.PECTuples.EventFilters_cff import apply_event_filters
apply_event_filters(
    process, paths, runOnData=runOnData,
    isPromptReco=options.isPromptReco
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
process.looseElectrons = cms.EDFilter('PATElectronSelector',
    src = cms.InputTag('slimmedElectrons'),
    cut = cms.string(
        'pt > 20. & abs(superCluster.eta) < 2.5 & ' \
        'electronID("cutBasedElectronID-Spring15-25ns-V1-standalone-veto")'
    )
)

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
    'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450', 'PFJet500',
    'PFHT350', 'PFHT400', 'PFHT475', 'PFHT600', 'PFHT650', 'PFHT800', 'PFHT900'
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
        filter = cms.bool(False),
        savePrescales = cms.bool(False),
        triggerBits = cms.InputTag('TriggerResults', processName=options.triggerProcessName)
    )

paths.append(process.pecTrigger)


# Save event ID and relevant objects
process.pecEventID = cms.EDAnalyzer('PECEventID')

process.pecJetMET = cms.EDAnalyzer('PECJetMET',
    runOnData = cms.bool(runOnData),
    jets = cms.InputTag('analysisPatJets'),
    jetSelection = jetQualityCuts,
    met = cms.InputTag('slimmedMETs', processName=process.name_())
)

process.pecPileUp = cms.EDAnalyzer('PECPileUp',
    primaryVertices = cms.InputTag('offlineSlimmedPrimaryVertices'),
    rho = cms.InputTag('fixedGridRhoFastjetAll'),
    runOnData = cms.bool(runOnData),
    puInfo = cms.InputTag('slimmedAddPileupInfo')
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


# Save information on generator-level jets and MET
if not runOnData and options.saveGenJets:
    process.pecGenJetMET = cms.EDAnalyzer('PECGenJetMET',
        jets = cms.InputTag('slimmedGenJets'),
        cut = cms.string(''),
        saveFlavourCounters = cms.bool(True),
        met = cms.InputTag('slimmedMETs', processName=process.name_())
    )
    paths.append(process.pecGenJetMET)


# The output file for the analyzers
postfix = '_' + string.join([random.choice(string.letters) for i in range(3)], '')

if options.outputFile.endswith('.root'):
    outputBaseName = options.outputFile[:-5] 
else:
    outputBaseName = options.outputFile

process.TFileService = cms.Service('TFileService',
    fileName = cms.string(outputBaseName + postfix + '.root'))
