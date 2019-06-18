# Configuration files

This directory contains multiple configuration files.

 * `main.json` <br />
   Master configuration for program `multijet`. Provides locations of specialized configuration files and defines groups of samples.
 * `period_weights.json` <br />
   Definitions for period-specific weights. These include target pileup profiles for the reweighting and integrated luminosities.
 * `trigger_bins.json` <br />
   Definitions of the trigger bins. For each single-jet trigger, provides the name of its last filter (which allows to identify corresponding trigger objects) and the range in p<sub>T</sub> of the leading jet, with and without a margin.
 * `samples_descriptions.json` <br />
   Descriptions of input data sets. Contains a list of their names, flags marking them as data or simulation (default), and cross sections (in pb).
