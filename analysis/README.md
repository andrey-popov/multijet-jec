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


## Comparison between data and simulation

Most important p<sub>T</sub> distributions and mean values of the balance observables as a function of p<sub>T</sub> of the leading jet are plotted with script [`basic_plots.py`](scripts/basic_plots.py). Here is an example call:

```sh
basic_plots.py --data JetHT-Run2016*.root --sim QCD-Ht-*.root \
    --config "$config_dir/plot_config.json" --era 2016All -o fig/2016All
```

A number of settings, including the binnings, are read from the [configuration file](config/plot_config.json).

Script [`control_plots.py`](scripts/control_plots.py), with a similar usage, produces plots of distributions of the balance observables in bins of p<sub>T</sub> of the leading jet. These are useful to check for possible outliers in the tails of the distributions.
