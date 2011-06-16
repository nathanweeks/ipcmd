#!/bin/sh
# SYNOPSIS
#     message_queues.sh

set -o errexit
set -o nounset

readonly NUM_MESSAGES=100

export IPCMD_MSQID=$(ipcmd msgget)

# clean up message queue upon (normal or abnormal) program termination
trap 'ipcrm -q $IPCMD_MSQID' EXIT

awk -v N=$NUM_MESSAGES 'BEGIN {for(i=1;i<=N;i++) print i}' |
  xargs ipcmd msgsnd &

sum=0
j=1
while [ $j -le $NUM_MESSAGES ]
do
    message=$(ipcmd msgrcv)
    sum=$((sum + message))
    j=$((j+1))
done

wait

EXPECTED_RESULT=$((NUM_MESSAGES*(NUM_MESSAGES+1)/2)) # 1+2+...+$NUM_MESSAGES

if [ $sum -ne $EXPECTED_RESULT ]
then
   echo "$0: failed - sum == $sum (expected $EXPECTED_RESULT)"
fi

