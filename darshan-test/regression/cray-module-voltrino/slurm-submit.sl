#!/bin/bash -l
export PBS_JOBID=$SLURM_JOB_ID
export LD_LIBRARY_PATH=/opt/intel/2020.4.912/lib/intel64/libimf.so:/opt/intel/2020.4.912/compilers_and_libraries_2020.4.304/linux/compiler/lib/intel64_lin:/opt/intel/2020.4.912/compilers_and_libraries_2020.4.304/linux/mkl/lib/intel64_lin:$LD_LIBRARY_PATH 

echo "JOBID IS: $PBS_JOBID"
# HACC
#rm -f /lscratch/ovis/darshanTest/darshan*
#rm -f /projects/ovis/tmp/darshan*

# hmmer
#rm -rf /lscratch/ovis/darshanTest/Pfam/*
#rm -rf /projects/ovis/tmp/Pfam/*

START=$(date +%s.%N)
srun $@
END=$(date +%s.%N)

DIFF=$(echo "$END - $START" | bc)
echo "The DiffOfTime = $DIFF"

# HACC
#du -h /lscratch/ovis/darshanTest/darshan*
#du -h /projects/ovis/tmp/darshan*

# hmmer
#du -h /lscratch/ovis/darshanTest/Pfam/*
#du -h /projects/ovis/tmp/Pfam/*

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
