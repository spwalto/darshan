#!/bin/bash

# note: put the application name
#export PROG=IOR_pscratch_8m-4m
export PROG=IOR_pscratch_fs_1024

export DXT_ENABLE_IO_TRACE=
#export DARSHAN_MODMEM=4500
#module load cde/v3/openmpi/4.1.2-gcc-10.3.0
#module load openmpi-gnu/4.1

# remove existing files
rm -rf /pscratch/spwalto/iorTest/darshan*
rm -rf /pscratch/spwalto/haccTest/darshan*
# execute the application

# pscratch
$DARSHAN_RUNJOB --ntasks=36 --cpu-bind=RANK /projects/ovis/darshanConnector/apps/rhel9.7/ior/build/bin/ior -b 8m -t 4m -s 1024 -F -C -e -k -o /pscratch/spwalto/iorTest/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 40000000 /pscratch/spwalto/haccTest/darshan

# nfs
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 5000000 /projects/ovis/haccTest/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 10000000 /projects/ovis/haccTest/darshan

if [ $? -ne 0 ]; then
    echo "Error: failed to execute ${PROG}" 1>&2
    exit 1
fi

unset DXT_ENABLE_IO_TRACE

exit 0
