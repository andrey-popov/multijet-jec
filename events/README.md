# Event processing

This component performs computationally intensive per-event processing. It reads ROOT files created in [CMSSW](../CMSSW), implements event selection, and writes plain ROOT trees with a few selected variables, including balance observables.


## Build instructions

Software dependencies:

 * Compiler supporting C++17
 * CMake 3.11
 * Boost 1.63
 * ROOT 6
 * [mensura framework](https://github.com/andrey-popov/mensura) of version fff1a8c5 or later

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
