# Multijet analysis

This repository contains software code used for jet p<sub>T</sub> calibration with multijet events with the [CMS detector](https://cms.cern). On the technical side, the analysis is split into several logically separate steps, which is reflected in the directory structure of the repository:

 * [`grid`](grid/) Processing of MiniAOD data sets over Grid.
 * [`events`](events/) Event selection and production of ROOT trees with balance observables.
 * [`analysis`](analysis/) Comparison of data and simulation, construction of inputs for the global fit of jet p<sub>T</sub> corrections.

Further information about each of the steps is given in corresponding README files. The setup to fit the corrections is provided in a [dedicated repository](https://github.com/andrey-popov/jec-fit-prototype).
