#!/usr/bin/env ksh
#
# requires ksh93, bash 2.x or greater, or a similar shell

########################################
# parameters
########################################
readonly NUM_SERVES=10 # agent will serve this many times
########################################

set -o errexit
set -o nounset

# semaphore array indexes
readonly MATCHES=0 PAPER=1 TOBACCO=2 # supplies
readonly SMOKER_DONE=3 # smoker is done smoking
readonly CLOSING_TIME=4 # agent will supply no more

agent() {
  for ((serve=0 ; serve < NUM_SERVES ; serve++))
  do
    hasnt=$((RANDOM%3)) # choose ingredient to not supply
    case $hasnt in
      $MATCHES) echo "agent supplying paper & tobacco"
                ipcmd semop $PAPER=+1 $TOBACCO=+1 ;;
        $PAPER) echo "agent supplying matches & tobacco"
                ipcmd semop $MATCHES=+1 $TOBACCO=+1 ;;
      $TOBACCO) echo "agent supplying matches & paper"
                ipcmd semop $MATCHES=+1 $PAPER=+1 ;;
    esac
    ipcmd semop $SMOKER_DONE=-1 # wait for smoker to grab stuff
  done
  echo "agent done serving..."
  ipcmd semop $MATCHES=+2 $PAPER=+2 $TOBACCO=+2 $CLOSING_TIME=+1
}

smoker() {
  what_i_got=$1
  while true
  do
    case $what_i_got in
      matches) ipcmd semop $PAPER=-1 $TOBACCO=-1 ;;
      paper) ipcmd semop $MATCHES=-1 $TOBACCO=-1 ;;
      tobacco) ipcmd semop $MATCHES=-1 $PAPER=-1 ;;
    esac
    if [[ $(ipcmd semctl getval $CLOSING_TIME) == 1 ]]
    then
      echo "smoker with $what_i_got exiting..."
      exit
    else
      echo "smoker with $what_i_got rolled a cig"
      ipcmd semop $SMOKER_DONE=+1 # let agent know
    fi
  done
}

# setup: Create a set of 5 semaphores
export IPCMD_SEMID=$(ipcmd semget -N $((CLOSING_TIME+1)))

# remove the semaphore set when this process exits
trap 'ipcrm -s $IPCMD_SEMID' EXIT 

# initialize semaphores to 0
ipcmd semctl setall 0 

# start processes...
smoker matches &
smoker paper &
smoker tobacco &
agent &

# wait for all processes to finish before exiting
# and removing the semaphore set
wait
