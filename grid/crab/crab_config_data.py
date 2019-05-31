from CRABClient.UserUtilities import config

config = config()

config.General.requestName = ''
config.General.transferOutputs = True
config.General.transferLogs = True

config.JobType.pluginName = 'Analysis'
config.JobType.psetName = '../MultijetJEC_cfg.py'
config.JobType.pyCfgParams = ['runOnData=true', 'period=2016']

config.Data.inputDataset = ''
config.Data.lumiMask = 'https://cms-service-dqm.web.cern.ch/cms-service-dqm/CAF/certification/Collisions16/13TeV/Final/Cert_271036-284044_13TeV_PromptReco_Collisions16_JSON.txt'
config.Data.splitting = 'LumiBased'
config.Data.unitsPerJob = 250
config.Data.publication = False
config.Data.outLFNDirBase = '/store/user/aapopov/stageout/JetMET190412/'

config.Site.storageSite = 'T2_BE_IIHE'

