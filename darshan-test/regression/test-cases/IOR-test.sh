#!/bin/bash

# note: put the application name
export PROG=IOR_pscratch

export DXT_ENABLE_IO_TRACE=
#export DARSHAN_MODMEM=4500
module load openmpi-gnu/4.1

# remove existing files
rm -rf /pscratch/spwalto/iorTest/darshan*
# execute the application

export tempfile=$(mktemp)
# pscratch
$DARSHAN_RUNJOB --ntasks=36 --cpu-bind=v,RANK /projects/ovis/darshanConnector/apps/rhel9.7/ior/src/ior -i 6 -b 1024k -t 1024k -s 100 -F -C -e -k -o /pscratch/spwalto/iorTest/darshan
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
