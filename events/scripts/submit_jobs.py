#!/usr/bin/env python

"""Submits jobs to PBS."""

import argparse
import fnmatch
import json
import math
import os
import subprocess
import sys

from termcolor import colored


class Samples:
    """Interface to access descriptions of samples."""

    def __init__(self, definition_file):
        """Construct from JSON file that describes samples."""

        with open(definition_file) as f:
            self.definitions = {
                entry['datasetId']: entry for entry in json.load(f)
            }

        self.samples_dir = os.path.dirname(definition_file)


    def get_files(self, dataset):
        """Return list of files for given data set ID.
        
        The paths are relative with respect to the samples directory.
        """

        all_files = [
            filename for filename in os.listdir(self.samples_dir)
            if os.path.isfile(os.path.join(self.samples_dir, filename))
        ]
        selected_files = []
        masks = self.definitions[dataset]['files']

        for mask in masks:
            selected_files.extend(
                filename for filename in all_files
                if fnmatch.fnmatch(filename, mask)
            )

        return sorted(selected_files)


job_script_template = """#!/bin/bash
. /cvmfs/sft.cern.ch/lcg/views/setupViews.sh LCG_95apython3 x86_64-slc6-gcc8-opt
cd $TMPDIR
multijet {sample_def} --config "{config}" --syst "{syst}"
cp *.root "{output_dir}"
"""


if __name__ == '__main__':

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument(
        'sample_group',
        help='Group of samples to process, as defined in configuration file.'
    )
    arg_parser.add_argument(
        '-c', '--config', default='main.json',
        help='Configuration file. Relative path is resolved with respect to '
        '$MULTIJET_JEC_INSTALL/config.'
    )
    arg_parser.add_argument(
        '-s', '--syst', default='',
        help='Requested systematic variation.'
    )
    arg_parser.add_argument(
        '-o', '--output', default='.',
        help='Directory to which to copy produced files.'
    )
    args = arg_parser.parse_args()


    if os.path.isabs(args.config):
        config_path = args.config
    else:
        try:
            config_path = os.path.join(
                os.environ['MULTIJET_JEC_INSTALL'], 'config', args.config
            )
        except KeyError:
            print('Environment variable "MULTIJET_JEC_INSTALL" is not set.')
            sys.exit(1)

    with open(config_path) as f:
        config = json.load(f)

    samples = Samples(config['samples']['definition_file'])

    try:
        datasets = config['samples']['groups'][args.sample_group]
    except KeyError:
        print('Unknown group of samples "{}".'.format(args.sample_group))
        sys.exit(1)

    datasets.sort()


    # Construct sample definitions for all jobs.  The entry for each job
    # consists of a data set ID and paths to input files to be processed
    # in that job.
    jobs = []

    for dataset in datasets:
        input_files = samples.get_files(dataset)
        files_per_job = 2
        num_jobs = math.ceil(len(input_files) / files_per_job)

        for ijob in range(num_jobs):
            start = ijob * files_per_job
            end = (ijob + 1) * files_per_job
            jobs.append((dataset, *input_files[start:end]))

    
    log_dir = 'logs'
    output_dir = os.path.abspath(args.output)

    for directory in [log_dir, output_dir]:
        try:
            os.makedirs(directory)
        except FileExistsError:
            pass


    # Submit all jobs
    for job in jobs:
        # Construct the name for the job from the stem of the first
        # input file
        job_name = os.path.splitext(job[1])[0]

        command = [
            'qsub', '-N', job_name,
            '-v', 'PATH,MULTIJET_JEC_INSTALL,MENSURA_INSTALL',
            '-j', 'oe', '-o', log_dir,
            '-q', 'localgrid', '-l', 'walltime=03:00:00'
        ]
        job_script = job_script_template.format(
            sample_def=' '.join(job), config=args.config,
            syst=args.syst, output_dir=output_dir
        )

        itry = 0
        max_tries = 3

        while (True):
            itry += 1

            try:
                subprocess.run(
                    command, input=job_script, encoding='ascii', check=True
                )
            except subprocess.CalledProcessError as error:
                print(colored(
                    'Failed to submit task "{}". Exit status is {}.'.format(
                        job_name, error.returncode
                    ),
                    'magenta'
                ))

                if itry < max_tries:
                    print(colored(
                        'Going to try again (attempt {} of {})...'.format(
                            itry, max_tries
                        ),
                        'magenta'
                    ))
                else:
                    print(colored(
                        'Giving up on this task. Following input files '
                        'will not be processed:',
                        'red'
                    ))

                    for filename in job[1:]:
                        print(' ', colored(filename, 'red'))

                    break

