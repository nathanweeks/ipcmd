#!/bin/sh

set -o errexit
set -o nounset

print_usage() {
  printf 'usage: parallelpipe.sh [options] partitioner_cmd [args] : '
  printf 'filter_cmd [args]\n'
  printf 'Options:\n'
  printf '  -n NUM_PARTITIONS : queue at most NUM_PARTITIONS partitions\n'
  printf '                      (must be >= NPROCS; default NPROCS)\n'
  printf '  -p NPROCS         : spawn NPROCS filter processes (default 1)\n'
}

########################################
# process command-line arguments
########################################

# defaults
nprocs=1

while getopts n:p: option
do
  case $option in
  n) num_partitions="$OPTARG" ;;
  p) nprocs="$OPTARG" ;;
  ?) print_usage 1>&2
     exit 2 ;;
  esac
done

shift $(($OPTIND - 1))

partitioner=''

while [ $# -ne 0 ] && [ "$1" != ':' ]
do
  if [ -z "$partitioner" ]
  then
    partitioner="$1"
  else
    partitioner="$partitioner $1"
  fi
  shift
done

shift # discard ':'

if [ $# -eq 0 ]
then
  print_usage
  exit 2
fi

########################################
# setup
########################################
# export the chosen partition path in case the user's partitioner will get
# this from an environment variable
readonly PARTITION_DIR=${TMPDIR:-/tmp}/${0##*/}.$$
mkdir -p $PARTITION_DIR
export PARTITION_PATH=$PARTITION_DIR/partition

# partitioner's semaphore set
# semaphore 0: partition queue counting semaphore to throttle partitioner; 
#              assumes values in {0,1,...,$num_partitions}
# semaphore 1: write (binary semaphore)
# semaphore 2: read (binary semaphore)
readonly partition_slot_sem=0 write_sem=1 read_sem=2
export IPCMD_SEMID=$(ipcmd semget -N 3)

# filter's message queue
readonly filter_msqid=$(ipcmd msgget)

# output message queue
readonly output_msqid=$(ipcmd msgget)

trap 'ipcrm -s $IPCMD_SEMID; ipcrm -q $filter_msqid; ipcrm -q $output_msqid; rm -rf $PARTITION_DIR' EXIT

# if terminated by a CTRL-C
trap 'ipcrm -s $IPCMD_SEMID; ipcrm -q $filter_msqid; ipcrm -q $output_msqid; rm -rf $PARTITION_DIR; kill 0' INT

ipcmd semctl setall $partition_slot_sem=${num_partitions:-$nprocs} \
                    $write_sem=1 \
                    $read_sem=0

# executes the user-specified partitioner
partitioner() {
  # read stdin from fd 3 (stdin of parent process)
  exec 0<&3
  "$@" 
  ipcmd semop $read_sem=+1 # in case the user code didn't "signal" coordinator
}

# executes the user-specified filter
filter() {
  while true
  do
    partition=$(ipcmd msgrcv -q $filter_msqid)
    if [ "$partition" = 'poison pill' ]
    then
      #ipcmd msgsnd -q $output_msqid 'poison pill'
      exit
    fi
    "$@" < $PARTITION_PATH.$partition > $PARTITION_PATH.$partition.out
    ipcmd msgsnd -q $output_msqid -t $partition $partition
    rm $PARTITION_PATH.$partition
    ipcmd semop $partition_slot_sem=+1
  done
}

# Make file descripter 3 a copy of stdin for inputProcess (asynchronous lists
# have /dev/null for stdin by default)
exec 3<&0 
eval partitioner "$partitioner" &

# spawn filter processes
p=0
while [ $p -lt $nprocs ]
do
  filter "$@" &
  p=$((p+1))
done

########################################
# output process
########################################

partition_num=1
while true
do
  partition=$(ipcmd msgrcv -q $output_msqid -t $partition_num)
  partition_num=$((partition_num+1))
  if [ "$partition" = 'poison pill' ]
  then
     exit
  else
    cat $PARTITION_PATH.$partition.out
    rm $PARTITION_PATH.$partition.out
  fi
done &

########################################
# coordinate partitioner process
########################################
partition_num=1

while true
do
  ipcmd semop $read_sem=-1 # wait for partitioner to finish writing
  if [ ! -s $PARTITION_PATH ]    # if no more partitions
  then
    p=0
    while [ $p -lt $nprocs ] # send poison pill to filter processes
    do
      ipcmd msgsnd -q $filter_msqid 'poison pill'
      p=$((p+1))
    done
    # last message output process receives will be poison pill
    ipcmd msgsnd -q $output_msqid -t $partition_num 'poison pill'
    break
  fi
  mv $PARTITION_PATH $PARTITION_PATH.$partition_num
  ipcmd semop $write_sem=+1 # partitioner may write next partition
  ipcmd msgsnd -q $filter_msqid $partition_num
  partition_num=$((partition_num+1))
done

wait
