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
    'globalTag', '', VarParsing.multiplicity.singleton, VarParsing.varType.string,
    'Global tag to be used'
)
options.register(
    'runOnData', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Indicates whether the job processes data or simulation'
)
options.register(
    'period', '2016', VarParsing.multiplicity.singleton,
    VarParsing.varType.string, 'Data-taking period'
)
options.register(
    'isPromptReco', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'In case of data, distinguishes PromptReco and ReReco. Ignored for simulation'
)
options.register(
    'isLegacy2016', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
    'Flags legacy ReReco fo 2016 data. Ignored for simulation'
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


# Check provided data-taking period
if options.period not in ['2016', '2017']:
    raise RuntimeError(
        'Data-taking period "{}" is not supported.'.format(options.period)
    )

# Make shortcuts to access some of the configuration options easily
runOnData = options.runOnData


# Provide a default global tag if user has not given any.  Chosen
# according to [1].
# [1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/PdmVAnalysisSummaryTable?rev=10
if not options.globalTag:
    if options.period == '2016':
        if runOnData:
            options.globalTag = '94X_dataRun2_v10'
        else:
            options.globalTag = '94X_mcRun2_asymptotic_v3'
    elif options.period == '2017':
        if runOnData:
            options.globalTag = '94X_dataRun2_v11'
        else:
            options.globalTag = '94X_mc2017_realistic_v17'
    
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
    test_file = None

    if options.period == '2016':
        if runOnData:
            # A file from era 2016H, which contains multiple certified
            # runs, including 283885.
            test_file = '/store/data/Run2016H/JetHT/MINIAOD/17Jul2018-v1/50000/F2DFE866-0E90-E811-B8FC-0025905C5430.root'
        else:
            test_file = '/store/mc/RunIISummer16MiniAODv3/QCD_HT500to700_TuneCUETP8M1_13TeV-madgraphMLM-pythia8/MINIAODSIM/PUMoriond17_94X_mcRun2_asymptotic_v3-v2/110000/42BEEC72-02EA-E811-94ED-003048CB7DA2.root'
    elif options.period == '2017':
        if runOnData:
            # A file from era 2017F, which contains lumi sections from
            # multiple certified runs, including 305064.
            test_file = '/store/data/Run2017F/JetHT/MINIAOD/31Mar2018-v1/80000/A4BA7F54-0137-E811-819B-B496910A9A2C.root'
        else:
            test_file = '/store/mc/RunIIFall17MiniAODv2/QCD_HT500to700_TuneCP5_13TeV-madgraph-pythia8/MINIAODSIM/PU2017_12Apr2018_94X_mc2017_realistic_v14-v2/100000/48B9B41E-D197-E811-949E-A4BF0112BD6A.root'

    process.source.fileNames = cms.untracked.vstring(test_file)

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
    setup_egamma_preconditions, define_jets, define_METs
)
process.analysisTask = cms.Task()

setup_egamma_preconditions(process, process.analysisTask, options.period)
recorrectedJetsLabel, jetQualityCuts = define_jets(
    process, process.analysisTask, reapplyJEC=True, runOnData=runOnData
)
metTag = define_METs(
    process, process.analysisTask, runOnData=runOnData,
    legacy2016=options.isLegacy2016
)

process.analysisPatJets.minPt = 0
process.analysisPatJets.preselection = ''


# Apply event filters recommended for analyses involving MET
from Analysis.PECTuples.EventFilters_cff import apply_event_filters
apply_event_filters(
    process, paths, options.period, runOnData=runOnData,
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

if options.period == '2016':
    electron_id = 'cutBasedElectronID-Summer16-80X-V1-veto'
elif options.period == '2017':
    electron_id = 'cutBasedElectronID-Fall17-94X-V2-veto'

process.looseElectrons = cms.EDFilter('PATElectronSelector',
    src = cms.InputTag('slimmedElectrons'),
    cut = cms.string(
        'pt > 20. & abs(superCluster.eta) < 2.5 & ' \
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
        'pt > 20. & abs(superCluster.eta) < 2.5 & ' \
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
    'PFJet140', 'PFJet200', 'PFJet260', 'PFJet320', 'PFJet400', 'PFJet450', 'PFJet500'
]
trigger_results_tag = cms.InputTag(
    'TriggerResults', processName=options.triggerProcessName
)

if runOnData:
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
process.pecTriggerObjects = cms.EDAnalyzer('PECTriggerObjects',
    triggerResults = trigger_results_tag,
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
    jetIDVersion = cms.string(options.period),
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
