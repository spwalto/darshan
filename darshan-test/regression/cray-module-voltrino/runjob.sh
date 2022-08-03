#!/bin/bash

if [ "$NERSC_HOST" == "cori" ]; then
    NODE_CONSTRAINTS="-C haswell"
fi

NODE_CONSTRAINTS="-p ldms"

sbatch --wait -N 1 -t 10 $NODE_CONSTRAINTS --output $DARSHAN_TMP/$$-tmp.out --error $DARSHAN_TMP/$$-tmp.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
#sbatch --wait -N 16 --array=[1-24] -t 40 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-SWFFT.out --error $DARSHAN_TMP/%A_%a-SWFFT.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
