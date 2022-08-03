#!/bin/bash

if [ "$NERSC_HOST" == "cori" ]; then
    NODE_CONSTRAINTS="-C haswell"
fi

NODE_CONSTRAINTS+="-p standard"
#hmmer
#sbatch --wait -N 1 --job-name=$PROG -t 240 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-$PROG.out --error $DARSHAN_TMP/%A_%a-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"

#mpi-io-test
#sbatch --wait -N 1 --job-name=$PROG -t 60 $NODE_CONSTRAINTS --output $DARSHAN_TMP/$$-$PROG.out --error $DARSHAN_TMP/$$-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
sbatch --wait -N 22 --job-name=$PROG -t 60 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-$PROG.out --error $DARSHAN_TMP/%A_%a-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"

#sbatch --wait -N 16 --job-name=$PROG -t 60 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-$PROG.out --error $DARSHAN_TMP/%A_%a-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
