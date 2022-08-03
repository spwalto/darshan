#!/bin/bash

export PROG=mpi-io-test_lC_noldms
exe_name=mpi-io-test

# set log file path; remove previous log if present
export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}-dxt.darshan
rm -f ${DARSHAN_LOGFILE}

# compile
$DARSHAN_CC $DARSHAN_TESTDIR/test-cases/src/mpi-io-test.c -o $DARSHAN_TMP/${exe_name}
if [ $? -ne 0 ]; then
    echo "Error: failed to compile ${PROG}" 1>&2
    exit 1
fi

# enable dxt tracing
export DXT_ENABLE_IO_TRACE=

# execute
# luster
$DARSHAN_RUNJOB --ntasks-per-node=32 $DARSHAN_TMP/${exe_name} -C -f $DARSHAN_TMP/${PROG}.tmp.dat
#$DARSHAN_RUNJOB --ntasks-per-node=32 $DARSHAN_TMP/${exe_name} -f $DARSHAN_TMP/${PROG}.tmp.dat
# nfs
#$DARSHAN_RUNJOB --ntasks-per-node=32 $DARSHAN_TMP/${exe_name} -C -f /projects/ovis/tmp/${PROG}.tmp.dat
#$DARSHAN_RUNJOB --ntasks-per-node=32 $DARSHAN_TMP/${exe_name} -f /projects/ovis/tmp/${PROG}.tmp.dat
if [ $? -ne 0 ]; then
    echo "Error: failed to execute ${PROG}" 1>&2
    exit 1
fi

# TODO: check results

# also, ensure that darshan-parser doesn't complain if given a log file that
# has DXT data present
$DARSHAN_PATH/bin/darshan-parser $DARSHAN_LOGFILE > /dev/null
if [ $? -ne 0 ]; then
    echo "Error: darshan-parser failed to handle ${DARSHAN_LOGFILE}" 1>&2
    exit 1
fi

unset DXT_ENABLE_IO_TRACE

exit 0
