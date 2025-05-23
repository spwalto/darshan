#!/bin/bash

# General notes
#######################

# Script to set up the environment for tests on this platform.  Must export
# the following environment variables:
# 
# DARSHAN_CC: command to compile C programs
# DARSHAN_CXX: command to compile C++ programs
# DARSHAN_F90: command to compile Fortran90 programs
# DARSHAN_F77: command to compile Fortran77 programs
# DARSHAN_RUNJOB: command to execute a job and wait for its completion

# This script may load optional modules (as in a Cray PE), set LD_PRELOAD
# variables (as in a dynamically linked environment), or generate mpicc
# wrappers (as in a statically linked environment).

# Notes specific to this platform (alcf-aurora-ld-preload)
########################
# Use default compilers on Aurora and ultimately use LD_PRELOAD
# (in the pbs-submit script) to instrument binaries.
#
# RUNJOB is responsible for submitting a PBS job, waiting for its
# completion, and checking its return status

export DARSHAN_CC=mpicc
export DARSHAN_CXX=mpicxx
export DARSHAN_F77=mpifort
export DARSHAN_F90=mpifort

export DARSHAN_RUNJOB=$DARSHAN_TESTDIR/$DARSHAN_PLATFORM/runjob.sh
