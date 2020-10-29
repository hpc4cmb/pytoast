#!/usr/bin/env python3

# Copyright (c) 2015-2020 by the parties listed in the AUTHORS file.
# All rights reserved.  Use of this source code is governed by
# a BSD-style license that can be found in the LICENSE file.

"""
Prototype of TOAST 3.0 interfaces
"""

import os
import sys

import argparse

import traceback

import numpy as np

import toast

from toast import qarray as qa

from toast import Telescope

from toast.mpi import get_world, Comm

from toast.data import Data

from toast.utils import Logger, Environment

from toast.timing import GlobalTimers, gather_timers

from toast.timing import dump as dump_timing

from toast.config import dump_toml, parse_config, create

from toast import future_ops as ops

from toast.future_ops.sim_focalplane import fake_hexagon_focalplane


def main():
    env = Environment.get()
    log = Logger.get()
    gt = GlobalTimers.get()
    gt.start("toast_future (total)")

    mpiworld, procs, rank = get_world()

    # The operators used in this script:
    operators = [
        ops.SimSatellite(name="sim_satellite"),
        ops.DefaultNoiseModel(name="noise_model"),
        ops.SimNoise(name="sim_noise"),
    ]

    # Argument parsing
    parser = argparse.ArgumentParser(description="Demo of TOAST future features.")

    # Add some custom arguments specific to this script.

    parser.add_argument(
        "--focalplane_pixels",
        required=False,
        type=int,
        default=1,
        help="Number of focalplane pixels",
    )

    parser.add_argument(
        "--group_size",
        required=False,
        type=int,
        default=procs,
        help="Size of a process group assigned to an observation",
    )

    # Build a config dictionary starting from the operator defaults, overriding with any
    # config files specified with the '--config' commandline option, followed by any
    # individually specified parameter overrides.
    config, args = parse_config(parser, operators=operators)
    print(config)
    print(args)

    # The satellite simulation operator requires a Telescope object.  Make a fake
    # focalplane and telescope
    focalplane = fake_hexagon_focalplane(
        args.focalplane_pixels,
        10.0,
        samplerate=10.0,
        epsilon=0.0,
        net=1.0,
        fmin=1.0e-5,
        alpha=1.0,
        fknee=0.05,
    )
    print(focalplane)

    # Log the config that was actually used at runtime.
    out = "future_config_log.toml"
    dump_toml(out, config)

    # Instantiate our operators
    run = create(config)

    # Add the telescope class to the satellite simulation operator (For a real
    # experiment, this kind of thing would be done automatically based on other
    # options to the operator).

    run["operators"]["sim_satellite"].telescope = Telescope(
        name="fake", focalplane=focalplane
    )

    # Put our operators into a pipeline in a specific order, running all detectors at
    # once.

    pipe = ops.Pipeline(
        detector_sets="ALL",
        operators=[
            run["operators"][x] for x in ["sim_satellite", "noise_model", "sim_noise"]
        ],
    )

    # Set up the communicator
    comm = Comm(world=mpiworld, groupsize=args.group_size)

    # Start with an empty data object (the first operator in our pipeline will create
    # Observations in the data).
    data = Data(comm=comm)

    # Run the pipeline
    pipe.exec(data)
    pipe.finalize(data)

    # Print the resulting data
    for ob in data.obs:
        group_rank = 0
        if ob.mpicomm is not None:
            group_rank = ob.mpicomm.rank
        if group_rank == 0:
            print(ob)

    # Cleanup
    gt.stop_all()

    alltimers = gather_timers(comm=comm.comm_world)
    if comm.world_rank == 0:
        dump_timing(alltimers, "future_timing")

    return


if __name__ == "__main__":
    try:
        main()
    except Exception:
        # We have an unhandled exception on at least one process.  Print a stack
        # trace for this process and then abort so that all processes terminate.
        mpiworld, procs, rank = get_world()
        if procs == 1:
            raise
        exc_type, exc_value, exc_traceback = sys.exc_info()
        lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
        lines = ["Proc {}: {}".format(rank, x) for x in lines]
        print("".join(lines), flush=True)
        if mpiworld is not None:
            mpiworld.Abort()
