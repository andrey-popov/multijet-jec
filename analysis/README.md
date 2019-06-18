# Physics analysis

In this component the physics analysis is performed, starting from files with plain ROOT trees produced in [`events`](../events). Various plots with comparisons between data and simulation are produced, and input files for the global fit are prepared.


## Environment

Dependencies:

 * Python 3.6
 * NumPy 1.14
 * Matplotlib 2.2
 * ROOT 6.16

As in component [`events`](../events), it is recommended to use [LCG_95apython3](http://lcginfo.cern.ch/release/95apython3/) environment. From `/cvmfs/`, it can be set up with the following command (change the architecture if needed):

```sh
. /cvmfs/sft.cern.ch/lcg/views/setupViews.sh LCG_95apython3 x86_64-slc6-gcc8-opt
```

On top of this, file [`env.sh`](env.sh) needs to be sourced.


## Comparison between data and simulation

Most important p<sub>T</sub> distributions and mean values of the balance observables as a function of p<sub>T</sub> of the leading jet are plotted with script [`plot_data_sim.py`](scripts/plot_data_sim.py). It also produces control plots with distributions of balance observables in bins of p<sub>T</sub> of the leading jet. These are useful to check for possible outliers in the tails of the distributions.

Here is an example command to produce the plots:

```sh
plot_data_sim.py --data JetHT-Run2016*.root --sim QCD-Ht-*.root \
    --config "$config_dir/plot_config.json" --era 2016All -o fig/2016All
```

A number of settings, including the binnings, are read from the [configuration file](config/plot_config.json).


## Inputs for the fit of L3Res corrections

Input files for the fit of L3Res jet corrections are constructed in two steps. First, the nominal inputs are created with script [`build_fit_inputs.py`](scripts/build_fit_inputs.py), as in the example below:

```sh
build_fit_inputs.py --data 2016BCD.root --sim simulation.root \
   --binning "$config_dir/binning.json" --era 2016BCD --plots fig/2016All \
   -o multijet.root
```

The produced ROOT file contains the following entries:

* `Binning`: Binning in p<sub>T</sub> of the leading jet (hereafter denoted with &tau;<sub>1</sub>) to be used for the &chi;<sup>2</sup> in the fit.
* `PtBalProfile`, `MPFProfile`: Profiles of the two balance observables as a function of &tau;<sub>1</sub>.
* `PtBalThreshold`, `MPFThreshold`: Reference values that define [smooth jet p<sub>T</sub> thresholds](https://indico.cern.ch/event/780845/#16-multijet-analysis-with-craw) used for the two balance observables.
* `PtLead`, `PtLeadProfile`: Distribution of &tau;<sub>1</sub> and profile of &tau;<sub>1</sub> versus &tau;<sub>1</sub>.
* `RelPtJetSumProj`: Matrix S used at slide&nbsp;8 [here](https://indico.cern.ch/event/780845/#16-multijet-analysis-with-craw).

All the histograms and profiles above are filled with data. Information about simulation is provided in in-file directories `PFJet*`. Each of them corresponds to one trigger bin and contains the following entries:

* `Range`: Range in &tau;<sub>1</sub> for that trigger bin.
* `SimPtBal`, `SimMPF`: Mean values of the two balance observables in simulation versus &tau;<sub>1</sub>. They are represented with splines.

In the current version of the code, the splines describing &langle;B&rangle;(&tau;<sub>1</sub>) in simulation are identical between all trigger bins.

Script `build_fit_inputs.py` reports in the standard output under-populated bins. Typically, bins with &tau;<sub>1</sub>&nbsp;&gt;&nbsp;1.6&nbsp;TeV are affected. These must not be considered in the fit because uncertainties become not reliable for them. The script also produces diagnostic plots that illustrate the quality of the fit of &langle;B&rangle;(&tau;<sub>1</sub>) in simulation with splines.

Variations due to systematic uncertainties are constructed with script [`build_syst_vars.py`](scripts/build_syst_vars.py) as in

```sh
build_syst_vars.py $config_dir/syst_config.json \
    --binning $config_dir/binning.json --plots fig/2016All -o syst.root
```

The variations to be produced are described in the dedicated configuration file ([example](config/syst_config.json)). They can be represented with a dedicated set of input files (created by supplying appropriate flags to the program `multijet` from component [`events`](../events)) or with a set of additional per-event weights. Uncertainties that affect data or simulation are stored in the output file `syst.root` in different ways. In case of data, ROOT histograms representing up and down relative deviations from nominal &langle;B&rangle; are stored in the root of the output file. In simulation, similar relative deviations are described with splines and saved in in-file directories for different trigger bins. If some uncertainty affects both data and simulation, both types of entries are saved.

Saving systematic variations in the form of relative deviations in a separate file allows to include uncertainties from an older version of the analysis in case up-to-date uncertainties are not available for some reason. Of course, this is only an approximation, and the uncertainties need to be recomputed to obtain reliable results.

In addition to storing the (smoothed) relative variations in a ROOT file, script `build_syst_vars.py` also plots them, together with the inputs that have been fitted to produce the variations.

Finally, the files produced by the two scripts above are combined using `hadd`. The resulting file is used in the fit of the L3Res correction, see the [dedicated repository](https://github.com/andrey-popov/jec-fit-prototype).
