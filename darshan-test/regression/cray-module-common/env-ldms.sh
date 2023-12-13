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

export DARSHAN_CC=mpicc
export DARSHAN_CXX=CC
export DARSHAN_F77=ftn
export DARSHAN_F90=ftn

export LD_LIBRARY_PATH=/usr/lib64/:$LD_LIBRARY_PATH
export LD_PRELOAD=/projects/ovis/darshanConnector/common/darshan/build/install/lib/libdarshan.so
#export LD_PRELOAD=/projects/ovis/darshanConnector/common/darshan_original/build/install/lib/libdarshan.so
export PATH=$PATH:/projects/ovis/darshanConnector/stress-ng/build/usr/bin:/projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/
export DXT_ENABLE_IO_TRACE=1
export HDF5_USE_FILE_LOCKING=1

export DARSHAN_RUNJOB=$DARSHAN_TESTDIR/$DARSHAN_PLATFORM/runjob-apps.sh

#module unload darshan >& /dev/null

# to set env variables for ldms_streams daemon testing
export DARSHAN_LDMS_STREAM=darshanConnector
export DARSHAN_LDMS_PORT=412
export DARSHAN_LDMS_HOST=localhost
export DARSHAN_LDMS_XPRT=sock
export DARSHAN_LDMS_AUTH=munge

#export DARSHAN_LDMS_VERBOSE=
export DARSHAN_LDMS_ENABLE_ALL=
#export DARSHAN_LDMS_ENABLE_MPIIO=
#export DARSHAN_LDMS_ENABLE_POSIX=
#export DARSHAN_LDMS_ENABLE_STDIO=
#export DARSHAN_LDMS_ENABLE_HDF5=

