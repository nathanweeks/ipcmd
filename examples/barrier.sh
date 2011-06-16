#!/bin/sh

########################################
# parameters
########################################
readonly NUM_PROCESSES=4
readonly NUM_BARRIERS=3
########################################

set -o errexit
set -o nounset

barrier() {
  # decrement every semaphore in the semaphore set
  ipcmd semop -1

  # wait until my semaphore is 0, then reset to $NUM_PROCESSES
  # and continue
  ipcmd semop $rank=0 $rank=+$NUM_PROCESSES 
}

process() {
  b=0
  while [ $b -lt $NUM_BARRIERS ]
  do
    echo "$rank: approaching barrier $b"
    barrier
    echo "$rank: past barrier $b"
    b=$((b+1))
  done
}

# create a set of $NUM_PROCESSES semaphores
export IPCMD_SEMID=$(ipcmd semget -N $NUM_PROCESSES)

# remove the semaphore set when this process exits
trap 'ipcrm -s $IPCMD_SEMID' EXIT 

# set all semaphores in the semaphore set to $NUM_PROCESSES
ipcmd semctl setall $NUM_PROCESSES

# start $NUM_PROCESSES processes
rank=0
while [ $rank -lt $NUM_PROCESSES ]
do
  process &
  rank=$((rank+1))
done

# wait for background processes to terminate
# before exiting & removing the semaphore set
wait 
