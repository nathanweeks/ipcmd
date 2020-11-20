#!/bin/sh

readonly PARTITION_SLOT_SEM=0 WRITE_SEM=1 READ_SEM=2

while true
do
  ipcmd semop $PARTITION_SLOT_SEM=-1 $WRITE_SEM=-1
  dd of=$PARTITION_PATH bs=2048k count=1 2> /dev/null
  if [ ! -s $PARTITION_PATH ]
  then
    break
  fi
  ipcmd semop $READ_SEM=+1
done
