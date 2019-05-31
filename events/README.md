# Event processing

This component performs computationally intensive per-event processing. It reads ROOT files created in [CMSSW](../grid), implements the event selection, and writes plain ROOT trees with a few selected variables, including balance observables.

The processing is done with program `multijet` ([source](prog/multijet.cpp)), which is in part configurable using [this](config/main.json) configuration file and files referenced therein. The code is factorized; typically, with each logically separate block implemented in a dedicated class. It relies heavily on the [mensura framework](https://github.com/andrey-popov/mensura).


## Build instructions

Software dependencies:

 * Compiler supporting C++17
 * CMake 3.11
 * Boost 1.63
 * ROOT 6
 * [mensura framework](https://github.com/andrey-popov/mensura) of version 1bec1ef2 or later

It is recommended to use [LCG_95apython3](http://lcginfo.cern.ch/release/95apython3/) environment. From `/cvmfs/`, it can be set up with the following command (change the architecture if needed):

```sh
. /cvmfs/sft.cern.ch/lcg/views/setupViews.sh LCG_95apython3 x86_64-slc6-gcc8-opt
```

Build mensura as explained in [its README]((https://github.com/andrey-popov/mensura)). Then, in the same environment, navigate into subdirectory `events` and execute

```sh
. ./env.sh
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..  # Change build type if needed
make
cd ..
```

Constructed executables are placed in directory `bin` and available in `$PATH`.


## Inputs

### Event files

Input files for this part of the analysis are constructed as decribed in [`grid`](../grid). However, further work is needed for era 2016F, which is split into two subperiods for the purpose of jet calibration. The files for it are split based on run numbers using program [`partition_runs`](prog/partition_runs.cpp):

```sh
partition_runs JetHT-Run2016F*.root -r 278802
```

In the following the two parts are treated as independent eras 2016F1 and 2016F2. Of course, it is also possible to produce files for the two parts separately by applying appropriate luminosity masks when running over input data sets in Grid.


### Database of samples

Available data sets need to be specified in a JSON file with a format described in documentation for class [`DatasetBuilder`](https://github.com/andrey-popov/mensura/blob/master/include/mensura/DatasetBuilder.hpp) from mensura. For each data set, it specifies the names of included ROOT files and metadata such as cross sections for simulated stamples. The cross sections have been taken from [samples_descriptions.json](config/samples_descriptions.json). This database file can be created with

```sh
compute_sample_norm.py $results_dir --drop-alt-weights
build_sample_database.py $results_dir -d $MULTIJET_JEC_INSTALL/config/samples_descriptions.json
```

where `$results_dir` is the directory with the input ROOT files and the scripts are from [mensura](https://github.com/andrey-popov/mensura/tree/master/scripts).

The path to the database file is [provided](https://github.com/andrey-popov/multijet-jec/blob/0b2ae13e09b4eccdc17782390844c72e9d2676f5/events/config/main.json#L3) in the main configuration. For user's convenience, the configuration also [defines](https://github.com/andrey-popov/multijet-jec/blob/0b2ae13e09b4eccdc17782390844c72e9d2676f5/events/config/main.json#L4-L15) several groups of data sets. Typically, a whole group would be processed together.


### Jet corrections

Jet corrections are applied using text files. They are searched for in standard locations (with the help of [`FileInPath`](https://github.com/andrey-popov/mensura/blob/master/include/mensura/FileInPath.hpp) class), which can be [specified](https://github.com/andrey-popov/multijet-jec/blob/0b2ae13e09b4eccdc17782390844c72e9d2676f5/events/config/main.json#L22) in the main configuration. New files with the corrections can be downloaded from the oficial repository with the help of script [`download_jec.sh`](scripts/download_jec.sh); their specific version needs to be specified in the script. Names of files with the corrections to be applied, together with their intervals of validity, are specified directly in the source code of `multijet`.


## Runnning main program

Program `multijet` can be run interactively or over a batch system.


### Running interactively

To run in the default configuration, only input event files need to be specified. There are two ways to do this. If a single positional argument is given, as in

```sh
multijet sim
```

it is interpreted as a group of data sets [specified](https://github.com/andrey-popov/multijet-jec/blob/0b2ae13e09b4eccdc17782390844c72e9d2676f5/events/config/main.json#L12-L15) in the main configuration file, and all data sets in that group (in this case ‘sim’) will be processed. If there is more than one positional argument, e.g.

```sh
multijet JetHT-Run2016G JetHT-Run2016G.part1.root JetHT-Run2016G.part2.root
```

then the first argument is taken as a data set ID, which is followed by the names of one or more ROOT files from that data set.

In both cases a number of options are supported; most notably, `--syst`, which requests applying a systematic variation. Here is an example:

```sh
multijet 2016All --syst jer_up --output output/jer_up
```


### Batch system

Jobs can be submitted to PBS with script [`submit_jobs.py`](scripts/submit_jobs.py). Jobs for a whole data set group are submitted at the same time. Examples:

```sh
submit_jobs.py 2016BCD --output output
submit_jobs.py sim --syst l2res_down --output sim/l2res_down
```
