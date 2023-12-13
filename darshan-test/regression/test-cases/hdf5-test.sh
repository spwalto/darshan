#!/bin/bash

export PROG=hdf5-test

# setup for hdf5
module use /apps/modules/modulefiles-apps/cde/v3/hdf5
module load cde/v3/hdf5/1.10.6-gcc-10.3.0-openmpi-4.1.2

# set log file path; remove previous log if present
export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}.darshan
rm -f ${DARSHAN_LOGFILE}

# compile
$DARSHAN_CC -L/projects/cde/v3/2023-03/x86_64/spack/opt/spack/linux-rhel7-x86_64/gcc-10.3.0/hdf5-1.10.6-xaapjijkqikzzc5hlqo36tl6vngytusy/lib -lhdf5 $DARSHAN_TESTDIR/test-cases/src/${PROG}.c -o $DARSHAN_TMP/${PROG}
if [ $? -ne 0 ]; then
    echo "Error: failed to compile ${PROG}" 1>&2
    exit 1
fi

# execute with gdb
$DARSHAN_RUNJOB $DARSHAN_TMP/${PROG} -f $DARSHAN_TMP/${PROG}.tmp.dat
if [ $? -ne 0 ]; then
    echo "Error: failed to execute ${PROG}" 1>&2
    exit 1
fi
#process_id=$(ps -ef | grep 'slurm-submit.s' | grep -v 'grep' | awk '{ printf $2 }')
#echo "this is the process id: $process_id \n" 


exit 0
