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

# Notes specific to this platform (cray-module-nersc)
########################
# Use Cray's default compiler wrappers and load the module associated with
# this darshan install
#
# RUNJOB is responsible for submitting a slurm job, waiting for its
# completion, and checking its return status

export DARSHAN_CC=cc
export DARSHAN_CXX=CC
export DARSHAN_F77=ftn
export DARSHAN_F90=ftn

export LD_LIBRARY_PATH=/projects/ovis/darshanConnector/ovis/LDMS_install/lib:/opt/ovis/lib64/
export LD_PRELOAD=/projects/darshan/install/lib/libdarshan.so
export DXT_ENABLE_IO_TRACE=1
export HDF5_USE_FILE_LOCKING=1

export DARSHAN_RUNJOB=$DARSHAN_TESTDIR/$DARSHAN_PLATFORM/runjob.sh

module unload darshan >& /dev/null
module load $DARSHAN_PATH/share/craype-2.x/modulefiles/

# to compile darshan on voltrino
module swap PrgEnv-intel/6.0.9 PrgEnv-gnu
module load cray-hdf5-parallel
export HDF5_USE_FILE_LOCKING=1

# to set env variables for ldms_streams daemon testing
export DARSHAN_LDMS_STREAM=darshanConnector
export DARSHAN_LDMS_PORT=412
export DARSHAN_LDMS_HOST=localhost
export DARSHAN_LDMS_XPRT=sock
export DARSHAN_LDMS_AUTH=munge

export DXT_ENABLE_LDMS=0
#export MPIIO_ENABLE_LDMS=
#export POSIX_ENABLE_LDMS=
#export MDHIM_ENABLE_LDMS=
export STDIO_ENABLE_LDMS=0
export HDF5_ENABLE_LDMS=0

