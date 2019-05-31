# Grid

This component is responsible for running over MiniAOD data sets in Grid to produce custom ROOT files with selected events and tuned event content. It depends on the [PEC-tuples](https://github.com/andrey-popov/PEC-tuples) package.


## Installation

1. Set up CMSSW and PEC-tuples as described [here](https://github.com/andrey-popov/PEC-tuples/wiki/InstallSoftware). Use PEC-tuples of version 3.5.1.
1. Link this directory from the repository (`multijet-jec/grid`) as `$CMSSW_BASE/src/Analysis/Multijet` so that the package defined here is available to CMSSW.
1. Compile CMSSW with `scram b`.


## Running in Grid

Instructions for running in Grid are [provided](https://github.com/andrey-popov/PEC-tuples/wiki/GridCampaign) in the documentation for PEC-tuples. The `cmsRun` configuration file for this analysis is [`MultijetJEC_cfg.py`](python/MultijetJEC_cfg.py). Input files for script [`crabSubmit.py`](https://github.com/andrey-popov/PEC-tuples/blob/master/scripts/crabSubmit.py), which define the jobs to be run, are included in directory [`crab`](crab/). The jobs can be submitted with commands

```sh
crabSubmit.py -t crab_config_sim.py sim.json
crabSubmit.py -t crab_config_data.py data.json
```

Consult the documentation in [`crabSubmit.py`](https://github.com/andrey-popov/PEC-tuples/blob/master/scripts/crabSubmit.py) for a description of the format of these input files.


## Postprocessing

Finalize the CRAB tasks as described in the [same instructions](https://github.com/andrey-popov/PEC-tuples/wiki/GridCampaign). The computation of the integrated luminosities and the construction of pileup profiles for the `HLT_PFJet*_v*` triggers used in this analysis is automatized in scripts included in directory [`scripts`](scripts/):

```sh
computeLumis.py 2016BCD.json 2016EF1.json 2016F2GH.json \
    --normtag normtag_PHYSICS.json > lumi.txt
computePUProfiles.py 2016BCD.json 2016EF1.json 2016F2GH.json \
    --pileup-input /afs/cern.ch/cms/CAF/CMSCOMM/COMM_DQM/certification/Collisions16/13TeV/PileUp/pileup_latest.txt \
    --normtag normtag_PHYSICS.json
```

This needs to be run at `lxplus.cern.ch`, within an environment with [BRIL work suite](https://cms-service-lumi.web.cern.ch/cms-service-lumi/brilwsdoc.html). The luminosity masks given as inputs to these scipts correspond to the three periods considered in jet calibraion. They can be constructed from luminosity masks of individual jobs with commands like

```sh
mergeJSON.py \
    JetHT-Run2016E/processedLumis.json JetHT-Run2016F/processedLumis.json:0-278801 \
    --output 2016EF1.json
```

The pileup profiles for the full year are constructed by `hadd`ing the profiles for the individual periods:

```sh
for trigger in PFJet140 PFJet200 PFJet260 PFJet320 PFJet400 PFJet450; do
   hadd pileup_2016All_${trigger}_finebin.root pileup_2016*_${trigger}_finebin.root
done
```
