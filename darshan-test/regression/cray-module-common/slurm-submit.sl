#!/bin/bash -l
export PBS_JOBID=$SLURM_JOB_ID
export DARSHAN_LOGFILE=$DARSHAN_TMP/${PROG}.${PBS_JOBID}.darshan

# HACC
#rm -f /lustre/ovis/darshanTest/darshan*
#rm -f /projects/ovis/tmp/darshan*
START=$(date +%s.%N)
srun --mpi=pmi2 $@
END=$(date +%s.%N)

DIFF=$(echo "$END - $START" | bc)
echo "The DiffOfTime = $DIFF"

# HACC
#du -h /lustre/ovis/darshanTest/darshan*
#du -h /projects/ovis/tmp/darshan*

# parse log with the dxt parser
$DARSHAN_PATH/bin/darshan-dxt-parser --show-incomplete $DARSHAN_LOGFILE > $DARSHAN_TMP/${PROG}.${PBS_JOBID}-dxt.darshan.txt
if [ $? -ne 0 ]; then
    echo "Error: failed to parse ${DARSHAN_LOGFILE} for dxt tracing" 1>&2
    exit 1
fi

# parse log with normal parser
$DARSHAN_PATH/bin/darshan-parser --all $DARSHAN_LOGFILE > $DARSHAN_TMP/${PROG}.${PBS_JOBID}.darshan.txt
if [ $? -ne 0 ]; then
    echo "Error: failed to parse ${DARSHAN_LOGFILE}" 1>&2
    exit 1
fi
