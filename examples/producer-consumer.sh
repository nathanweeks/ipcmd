#!/bin/sh

########################################
# parameters
########################################
readonly ITEMS_PER_PRODUCER=5
readonly PRODUCERS="A B"
readonly CONSUMERS="1 2 3"
########################################

producer() {
  me=$1
  item=1
  while [ $item -le $ITEMS_PER_PRODUCER ]
  do
    ipcmd msgsnd "item $item from producer $me"
    item=$((item+1))
  done
}

consumer() {
  me=$1
  while true
  do
    item=$(ipcmd msgrcv)
    if [ "$item" = 'poison pill' ]
    then
      echo "Consumer $me consumed poison pill... exiting"
      exit 
    else
      echo "Consumer $me consumed: $item"
    fi
  done
}

# create a message queue
export IPCMD_MSQID=$(ipcmd msgget)

# remove the message queue when this process exits
trap 'ipcrm -q $IPCMD_MSQID' EXIT 

# spawn consumers
for i in $CONSUMERS
do
  consumer $i &  
done

# spawn producers
for i in $PRODUCERS
do
  producer $i &
  producer_PIDs="$producer_pids ${!}"
done

# wait for producer processes to terminate
wait $producer_PIDs

# instruct consumer processes to terminate
for i in $CONSUMERS
do
  ipcmd msgsnd 'poison pill'
done

# wait for consumers processes to terminate
wait
