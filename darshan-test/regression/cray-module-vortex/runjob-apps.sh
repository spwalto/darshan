#!/bin/bash

#hmmer
#sbatch --wait -N 1 --job-name=$PROG -t 240 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-$PROG.out --error $DARSHAN_TMP/%A_%a-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"

#mpi-io-test
bsub -nnodes 1 -J $PROG -W 60 -o $DARSHAN_TMP/$$-$PROG.out -e $DARSHAN_TMP/$$-$PROG.err -i $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
#bsub --wait -N 22 --job-name=$PROG -t 60 $NODE_CONSTRAINTS --output $DARSHAN_TMP/%A_%a-$PROG.out --error $DARSHAN_TMP/%A_%a-$PROG.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
