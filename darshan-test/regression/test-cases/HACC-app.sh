#!/bin/bash

# note: put the application name
export PROG=HACC_pscratch_10B

export DXT_ENABLE_IO_TRACE=

# execute the application

# pscratch
$DARSHAN_RUNJOB --ntasks-per-node=36 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 10000000000 /pscratch/spwalto/haccTest/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 40000000 /pscratch/spwalto/haccTest/darshan

# nfs
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 5000000 /projects/ovis/haccTest/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/darshanConnector/apps/rhel9.7/hacc-io/hacc_io 10000000 /projects/ovis/haccTest/darshan

if [ $? -ne 0 ]; then
    echo "Error: failed to execute ${PROG}" 1>&2
    exit 1
fi

# Confirm HACC-IO completed successfully
du -h /pscratch/spwalto/haccTest/darshan*

unset DXT_ENABLE_IO_TRACE

exit 0
