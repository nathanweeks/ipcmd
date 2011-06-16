#!/bin/ksh
#
# requires ksh93, bash 2.x or greater, or a similar shell

set -o errexit
set -o nounset

########################################
# parameters
########################################
readonly NUM_PHILOSOPHERS=5
readonly NUM_MEALS=3 # philosophers eat this many meals
########################################

philosopher() {
  left_chopstick=$((rank-1 < 0 ? NUM_PHILOSOPHERS-1 : rank-1))
  right_chopstick=$rank
  for ((meal=1 ; meal <= NUM_MEALS ; meal++))
  do
    echo "Philosopher $rank is hungry... reaching for chopsticks"
    ipcmd semop $left_chopstick=-1 $right_chopstick=-1
    echo "Philosopher $rank is eating meal $meal"
    sleep $((RANDOM%3)) # eat for 0-2 seconds
    echo "Philosopher $rank is done eating... thinking"
    ipcmd semop $left_chopstick=+1 $right_chopstick=+1
    sleep $((RANDOM%3)) # eat for 0-2 seconds
  done
}

# create a set of $NUM_PHILOSOPHERS semaphores, and use
# it as the default for ipcmd semaphore operations
export IPCMD_SEMID=$(ipcmd semget -N $NUM_PHILOSOPHERS)

# remove the semaphore set when this script exits
trap 'ipcrm -s $IPCMD_SEMID' EXIT 

# place one chopstick between each philosopher
ipcmd semctl setall 1

# start $NUM_PHILOSOPHERS philosopher processes
for ((rank=0 ; rank < NUM_PHILOSOPHERS ; rank++))
do
  philosopher &
done

# wait for background philosopher processes to terminate
# before exiting & removing the semaphore set
wait 
