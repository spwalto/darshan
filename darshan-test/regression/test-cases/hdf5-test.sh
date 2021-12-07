#!/bin/bash

PROG=hdf5-test

# set log file path; remove previous log if present
export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}.darshan
rm -f ${DARSHAN_LOGFILE}

# compile
$DARSHAN_CC $DARSHAN_TESTDIR/test-cases/src/${PROG}.c -o $DARSHAN_TMP/${PROG}
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

echo ${DARSHAN_LOGFILE}
# parse log
$DARSHAN_PATH/bin/darshan-parser $DARSHAN_LOGFILE > $DARSHAN_TMP/${PROG}.darshan.txt
if [ $? -ne 0 ]; then
    echo "Error: failed to parse ${DARSHAN_LOGFILE}" 1>&2
    exit 1
fi

# check results

# also, ensure that darshan-dxt-parser doesn't complain if given a log file that
# does not have DXT data present
#$DARSHAN_PATH/bin/darshan-dxt-parser $DARSHAN_LOGFILE > /dev/null
#if [ $? -ne 0 ]; then
#    echo "Error: darshan-dxt-parser failed to handle ${DARSHAN_LOGFILE}" 1>&2
#    exit 1
#fi

exit 0
