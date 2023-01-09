#!/bin/bash

# note: put the application name
export PROG=HACC_nfs_round2_5

# set log file path; remove previous log if present
#export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}.darshan
#rm -f ${DARSHAN_LOGFILE}

# note: the application should be compiler prior to this stage
# enable dxt tracing
export DXT_ENABLE_IO_TRACE=

# execute the application

# luster
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/omar/stria_run/HACC-IO/hacc_io 20000000 /lustre/ovis/darshanTest/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/omar/stria_run/HACC-IO/hacc_io 40000000 /lustre/ovis/darshanTest/darshan

# nfs
$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/omar/stria_run/HACC-IO/hacc_io 5000000 /projects/ovis/tmp/darshan
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/omar/stria_run/HACC-IO/hacc_io 10000000 /projects/ovis/tmp/darshan

if [ $? -ne 0 ]; then
    echo "Error: failed to execute ${PROG}" 1>&2
    exit 1
fi

# check results
# in this case we want to confirm that both the MPI and POSIX open counters were triggered
#POSIX_OPENS=`grep POSIX_OPENS $DARSHAN_TMP/${PROG}.darshan.txt |grep -vE "^#" |cut -f 5`
#if [ ! "$POSIX_OPENS" -gt 0 ]; then
#    echo "Error: POSIX open count of $POSIX_OPENS is incorrect" 1>&2
#    exit 1
#fi
#MPI_OPENS=`grep INDEP_OPENS $DARSHAN_TMP/${PROG}.darshan.txt |grep -vE "^#" |cut -f 5`
#if [ ! "$MPI_OPENS" -gt 0 ]; then
#    echo "Error: MPI open count of $MPI_OPENS is incorrect" 1>&2
#    exit 1
#fi

# also, ensure that darshan-dxt-parser doesn't complain if given a log file that
# does not have DXT data present
#$DARSHAN_PATH/bin/darshan-dxt-parser $DARSHAN_LOGFILE > /dev/null
#if [ $? -ne 0 ]; then
#    echo "Error: darshan-dxt-parser failed to handle ${DARSHAN_LOGFILE}" 1>&2
#    exit 1
#fi

unset DXT_ENABLE_IO_TRACE

exit 0
