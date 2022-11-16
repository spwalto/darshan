#!/bin/bash

if [ "$NERSC_HOST" == "cori" ]; then
    NODE_CONSTRAINTS="-C haswell"
fi

NODE_CONSTRAINTS="-p batch --account=fy140198"

sbatch --wait -N 1 -t 90 $NODE_CONSTRAINTS --output $DARSHAN_TMP/$$-${PROG}.tmp.out --error $DARSHAN_TMP/$$-${PROG}.tmp.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
