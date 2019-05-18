# Event processing

This component performs computationally intensive per-event processing. It reads ROOT files created in [CMSSW](../CMSSW), implements event selection, and writes plain ROOT trees with a few selected variables, including balance observables.


## Build instructions

Software dependencies:

 * Compiler supporting C++17
 * CMake 3.11
 * Boost 1.63
 * ROOT 6
 * [mensura framework](https://github.com/andrey-popov/mensura) of version 1b296ae8 or later

It is recommended to use [LCG_95apython3](http://lcginfo.cern.ch/release/95apython3/) environment. From `/cvmfs/`, it can be set up with the following command (change the architecture if needed):

```sh
. /cvmfs/sft.cern.ch/lcg/views/setupViews.sh LCG_95apython3 x86_64-slc6-gcc8-opt
```

Build mensura as explained in [its README]((https://github.com/andrey-popov/mensura)). Then, in the same environment, navigate into subdirectory `events` and execute

```sh
. ./env.sh
cd build
cmake .. && make
cd ..
```

Constructed executables are placed in directory `bin` and available in `$PATH`.


## Input files

Input files for this part of the analysis are constructed as decribed in [`grid`](../grid). However, further work is needed for era 2016F, which is split into two subperiods for the purpose of jet calibration. The files for it are split based on run numbers using program [`partition_runs`](prog/partition_runs.cpp):

```sh
partition_runs JetHT-Run2016F*.root -r 278802
```

In the following the two parts are treated as independent eras 2016F1 and 2016F2. Of course, it is also possible to produce files for the two parts separately by applying appropriate luminosity masks when running over input data sets in Grid.
