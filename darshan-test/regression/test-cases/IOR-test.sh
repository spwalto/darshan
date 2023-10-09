#!/bin/bash

# note: put the application name
export PROG=IOR_pscratch

export DXT_ENABLE_IO_TRACE=

# remove existing files
rm -rf /pscratch/spwalto/iorTest/darshan*

# execute the application

# pscratch
$DARSHAN_RUNJOB --ntasks=16 /projects/ovis/darshanConnector/apps/rhel9.7/ior/src/ior -i 1 -b 2m -t 4k -s 1024 -F -k -o /pscratch/spwalto/iorTest/darshan
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
