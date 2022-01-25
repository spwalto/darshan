#!/bin/bash

# note: put the application name
PROG=SWFFT
#PROG=sw4lite
#PROG=sw4
# set log file path; remove previous log if present
export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}.darshan
rm -f ${DARSHAN_LOGFILE}

# note: the application should be compiler prior to this stage

# execute the application
# SWFFT
$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/UCF/voltrino_run/SWFFT/SWFFT 5 512

# MPI_test
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/UCF/voltrino_run/SWFFT/mpi_test

# sw4lite
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/UCF/voltrino_run/sw4lite/sw4lite /projects/ovis/UCF/voltrino_run/sw4lite/new_gh_1node.in

# sw4
#$DARSHAN_RUNJOB --ntasks-per-node=32 /projects/ovis/UCF/voltrino_run/sw4/sw4 /projects/ovis/UCF/voltrino_run/sw4lite/new_gh_1node.in


#$DARSHAN_RUNJOB $DARSHAN_TMP/${PROG} -f $DARSHAN_TMP/${PROG}.tmp.dat
#if [ $? -ne 0 ]; then
#    echo "Error: failed to execute ${PROG}" 1>&2
#    exit 1
#fi
#sleep 10000

# parse log
$DARSHAN_PATH/bin/darshan-parser $DARSHAN_LOGFILE > $DARSHAN_TMP/${PROG}.darshan.txt
if [ $? -ne 0 ]; then
    echo "Error: failed to parse ${DARSHAN_LOGFILE}" 1>&2
    exit 1
fi

# check results
# in this case we want to confirm that both the MPI and POSIX open counters were triggered
POSIX_OPENS=`grep POSIX_OPENS $DARSHAN_TMP/${PROG}.darshan.txt |grep -vE "^#" |cut -f 5`
if [ ! "$POSIX_OPENS" -gt 0 ]; then
    echo "Error: POSIX open count of $POSIX_OPENS is incorrect" 1>&2
    exit 1
fi
MPI_OPENS=`grep INDEP_OPENS $DARSHAN_TMP/${PROG}.darshan.txt |grep -vE "^#" |cut -f 5`
if [ ! "$MPI_OPENS" -gt 0 ]; then
    echo "Error: MPI open count of $MPI_OPENS is incorrect" 1>&2
    exit 1
fi

# also, ensure that darshan-dxt-parser doesn't complain if given a log file that
# does not have DXT data present
$DARSHAN_PATH/bin/darshan-dxt-parser $DARSHAN_LOGFILE > /dev/null
if [ $? -ne 0 ]; then
    echo "Error: darshan-dxt-parser failed to handle ${DARSHAN_LOGFILE}" 1>&2
    exit 1
fi

exit 0
