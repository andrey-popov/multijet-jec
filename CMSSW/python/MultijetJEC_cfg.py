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
    'The relevant global tag'
)
options.register(
    'runOnData', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Indicates whether it runs on the real data'
)
options.register(
    'isPromptReco', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'In case of data, distinguishes PromptReco and ReReco. Ignored for simulation'
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


# Make the shortcuts to access some of the configuration options easily
runOnData = options.runOnData


# Provide a default global tag if user has not given any.  With data use
# the global tag for prompt reconstruction [1].  With simulation take
# the one recommended by JERC group [2].
# [1] https://twiki.cern.ch/twiki/bin/view/CMSPublic/SWGuideFrontierConditions?rev=568#Global_Tags_for_2016_data_taking
# [2] https://twiki.cern.ch/twiki/bin/viewauth/CMS/JECDataMC?rev=112
if len(options.globalTag) == 0:
    if runOnData:
        options.globalTag = '80X_dataRun2_Prompt_v8'
    else:
        options.globalTag = '80X_mcRun2_asymptotic_2016_miniAODv2'
    
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
        process.source.fileNames = cms.untracked.vstring('/store/data/Run2016B/JetHT/MINIAOD/PromptReco-v2/000/273/450/00000/1EAF9289-581C-E611-B23D-02163E014777.root')
    else:
        process.source.fileNames = cms.untracked.vstring('/store/mc/RunIISpring16MiniAODv1/QCD_HT500to700_TuneCUETP8M1_13TeV-madgraphMLM-pythia8/MINIAODSIM/PUSpring16_80X_mcRun2_asymptotic_2016_v3-v1/00000/0AEC156E-9418-E611-AAF7-0CC47A6C17FC.root')

# process.source.fileNames = cms.untracked.vstring('/store/relval/...')

# Set a specific event range here (useful for debugging)
# process.source.eventsToProcess = cms.untracked.VEventRange('1:5')

# Set the maximum number of events to process for a local run (it is overiden by CRAB)
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))


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


# Filter on properties of the first vertex
process.goodOfflinePrimaryVertices = cms.EDFilter('FirstVertexFilter',
    src = cms.InputTag('offlineSlimmedPrimaryVertices'),
    cut = cms.string('!isFake & ndof > 4. & abs(z) < 24. & position.rho < 2.')
)

paths.append(process.goodOfflinePrimaryVertices)


# Define  and customize basic reconstructed objects
from Analysis.PECTuples.ObjectsDefinitions_cff import (define_photons, define_jets, define_METs)

(phoQualityCuts, phoCutBasedIDMaps) = define_photons(process)
(recorrectedJetsLabel, jetQualityCuts, pileUpIDMap) = \
    define_jets(process, reapplyJEC=True, runOnData=runOnData)
define_METs(process, runOnData=runOnData)

process.analysisPatJets.cut = ''


# Apply event filters recommended for analyses involving MET
from Analysis.PECTuples.EventFilters_cff import apply_event_filters
apply_event_filters(
    process, paths, runOnData=runOnData,
    isPromptReco=options.isPromptReco
)


# Apply lepton veto
process.looseMuons = cms.EDFilter('PATMuonSelector',
    src = cms.InputTag('slimmedMuons'),
    cut = cms.string('pt() > 10. & isLooseMuon() & (pfIsolationR04().sumChargedHadronPt + ' + \
        'max(0., pfIsolationR04().sumNeutralHadronEt + pfIsolationR04().sumPhotonEt - ' + \
        '0.5 * pfIsolationR04().sumPUPt)) / pt() < 0.25')
)
process.looseElectrons = cms.EDFilter('PATElectronSelector',
    src = cms.InputTag('slimmedElectrons'),
    cut = cms.string('pt() > 20. & ' + \
        'electronID("cutBasedElectronID-Spring15-25ns-V1-standalone-veto")')
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


# Save decisions of selected triggers.  The lists are aligned with
# menu [1] used in some of 80X MC.  Event that do not fire any of the
# listed triggers, are rejected.
# [1] /dev/CMSSW_8_0_0/GRun/V8
if runOnData:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(
            'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450', 'PFJet500',
            'PFHT350', 'PFHT400', 'PFHT475', 'PFHT600', 'PFHT650', 'PFHT800'
        ),
        filter = cms.bool(True),
        savePrescales = cms.bool(True),
        triggerBits = cms.InputTag('TriggerResults', processName='HLT'),
        hltPrescales = cms.InputTag('patTrigger'),
        l1tPrescales = cms.InputTag('patTrigger', 'l1min')
    )
else:
    process.pecTrigger = cms.EDFilter('SlimTriggerResults',
        triggers = cms.vstring(
            'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450', 'PFJet500',
            'PFHT350', 'PFHT400', 'PFHT475', 'PFHT600', 'PFHT650', 'PFHT800'
        ),
        filter = cms.bool(False),
        savePrescales = cms.bool(False),
        triggerBits = cms.InputTag('TriggerResults', processName='HLT')
    )

paths.append(process.pecTrigger)


# Save event ID and relevant objects
process.pecEventID = cms.EDAnalyzer('PECEventID')

process.pecJetMET = cms.EDAnalyzer('PECJetMET',
    runOnData = cms.bool(runOnData),
    jets = cms.InputTag('analysisPatJets'),
    jetType = cms.string('AK4PFchs'),
    jetMinPt = cms.double(0.),
    jetSelection = jetQualityCuts,
    contIDMaps = cms.VInputTag(pileUpIDMap),
    met = cms.InputTag('slimmedMETs', processName=process.name_()),
    rho = cms.InputTag('fixedGridRhoFastjetAll')
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
        saveLHEWeightVars = cms.bool(False)
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
