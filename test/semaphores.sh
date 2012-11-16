#!/bin/sh
# SYNOPSIS
#     semaphores.sh
#
# NOTES
#     Under Solaris 10, run this script using an at-least-POSIX shell like 
#     /bin/bash, /bin/ksh, or /usr/xpg4/bin/sh.
#
# TODO
#     * Take semume (max # of undo entries per process) into account

set -o errexit
set -o nounset
#set -o xtrace #DEBUG

# Attempt to determine the following values:
# SEMMSL: maximum semaphores per id
# SEMVMX: semaphore maximum value
case $(uname) in
  Darwin|*BSD) SEMMSL=$(ipcs -T | awk '/semmsl/ {print $2}') 
               SEMOPM=$(ipcs -T | awk '/semopm/ {print $2}')
               # OS X 10.6 has the (reported) bug of being unable to perform
               # an array of more than 5 semaphore operations, despite the
               # value of semopm. This has been fixed in OS X 10.8 (kernel
               # version 12.x.x).
               if [ $(uname) = Darwin ]
               then
                 release=$(uname -r)
                 SEMOPM=$((${release%%.*} < 12 ? 5 : SEMOPM))
               fi
               SEMUME=$(ipcs -T | awk '/semume/ {print $2}')
               SEMVMX=$(ipcs -T | awk '/semvmx/ {print $2}') ;;
  Linux) SEMMSL=$(cut -f 1 /proc/sys/kernel/sem) # per semget(2)
         SEMOPM=$(cut -f 3 /proc/sys/kernel/sem) # per semop(2)
         SEMVMX=32767 # per semctl(2) 
         SEMUME=$SEMOPM ;; # Linux has no intrinsic limit for SEMUME
  SunOS) SEMMSL=$(prctl -P -n process.max-sem-nsems -t privileged $$ | 
                  awk 'NR == 2 {print $3}') # Solaris 10 or greater
         SEMOPM=$(prctl -P -n process.max-sem-ops -t privileged $$ | 
                  awk 'NR == 2 {print $3}') # Solaris 10 or greater
         SEMVMX=65535 ;; # this seems to be the case for Solaris 10?
      *) SEMMSL=25
         SEMOPM=25
         SEMVMX=32767 ;; # safe (?) defaults for other systems
esac

# These tests assume all semaphores in the set can be operated on by a single
# semop(), so set SEMMSL=min(SEMOPM, SEMMSL)
if [ $SEMOPM -lt $SEMMSL ]
then
  SEMMSL=$SEMOPM
fi

# create semaphore set containing SEMMSL semaphores
semid=$(ipcmd semget -N $SEMMSL)

# remove semaphore set upon termination of script & print any error message
trap 'ipcrm -s $semid ; test -n "${error_message:-}" && echo "${0##*/}:$LINENO: ERROR - $error_message" 1>&2' EXIT

########################################
# test 1: setall semval
########################################
ipcmd semctl -s $semid setall $SEMVMX
semval=$(ipcmd semctl -s $semid getall | tr ' ' '\n' | uniq)
if [ "$semval" -ne $SEMVMX ]
then
  error_message="(getall/setall): semval == $semval (expected $SEMVMX)"
  exit 1
fi

# for remaining tests, use IPCMD_SEMID environment variable to verify it works
export IPCMD_SEMID=$semid

########################################
# test 2: setall semval...
########################################

# first argument form
ipcmd semctl setall 1
ipcmd semctl getall |
  tr ' ' '\n' |
    while read semval
    do
      if [ $semval -ne 1 ]
      then
        error_message="(getall/setall): semval == $semval (expected 1)"
        exit 1
      fi
    done

# second argument form
ipcmd semctl setall 0:$((SEMMSL-1))=2
ipcmd semctl getall |
  tr ' ' '\n' |
    while read semval
    do
      if [ $semval -ne 2 ]
      then
        error_message="(getall/setall): semval == $semval (expected 2)"
        exit 1
      fi
    done

# second argument form, sem_num_lbound only
# semvals == 0 1 ... SEMMSL-1
semvals=$(awk -v SEMMSL=$SEMMSL 'BEGIN{for(i=0;i<SEMMSL;i++)printf("%i=%i ",i,i)}')

ipcmd semctl setall $semvals
ipcmd semctl getall |
  tr ' ' '\n' |
    nl -v 0 |
      while read line semval
      do
        if [ $line -ne $semval ]
        then
          error_message="(getall/setall): semval == $semval (expected $line)"
          exit 1
        fi
      done

########################################
# test 3: setval/getval
########################################
# zero-out semaphore set 
ipcmd semctl setall 0

semnum=0
while [ $semnum -lt $SEMMSL ]
do
  ipcmd semctl setval $semnum $semnum
  semnum=$((semnum+1))
done

# again, assign semaphore i = i, for i = 0,...,SEMMSL-1
semnum=0
while [ $semnum -lt $SEMMSL ]
do
  semval=$(ipcmd semctl getval $semnum)
  if [ $semval -ne $semnum ]
  then
    error_message="(getval/setval): semval == $semval (expected $semnum)"
    exit 1
  fi
  semnum=$((semnum+1))
done

########################################
# test 4: getpid
########################################
# set the first semaphore to 1, then increment in a background command
ipcmd semctl setval 0 0
ipcmd semop 0=+1 &

wait

background_command_pid=$!
semctl_getpid=$(ipcmd semctl getpid 0)
if [ $semctl_getpid -ne $background_command_pid ]
then
  error_message="semctl getpid == $semctl_getpid (expected $background_command_pid)"
  exit 1
fi

########################################
# test 5: getncnt
########################################
# set the first semaphore to 0
ipcmd semctl setval 0 0

# three background processes "post"
ipcmd semop 0=-1 &
ipcmd semop 0=-1 &
ipcmd semop 0=-1 &

sleep 1 # give semop processes a chance to run...
semctl_getncnt=$(ipcmd semctl getncnt 0)
if [ $semctl_getncnt -ne 3 ]
then
  error_message="(semctl getncnt) == $semctl_getncnt (expected 3)"
  exit 1
fi

# clean up -- set the semaphore value to 0 & wait for background processes to
# complete
ipcmd semctl setval 0 3
wait

########################################
# test 6: getzcnt
########################################
# set the first semaphore to 1
ipcmd semctl setval 0 1

# two background processes wait for the semaphore value to become 0
ipcmd semop 0=0 &
ipcmd semop 0=0 &

sleep 1 # give semop processes a chance to run...
semctl_getzcnt=$(ipcmd semctl getzcnt 0)
if [ $semctl_getzcnt -ne 2 ]
then
  error_message="(semctl getzcnt) == $semctl_getzcnt (expected 2)"
  exit 1
fi

# clean up -- set the semaphore value to 0 & wait for background processes to
# complete
ipcmd semctl setval 0 0
wait

########################################
# test 7: ipcmd semop & IPC_NOWAIT
########################################

ipcmd semctl setall 1

set +o errexit # disable for this test -- we expect exit status 2
for cmd in 'ipcmd semop -n 0' "ipcmd semop -n 0:$((SEMMSL-1))=0" \
           "ipcmd semop 0:$((SEMMSL-1))=0n"
do
  eval $cmd
  exit_status=$?
  if [ $exit_status -ne 2 ]
  then
    error_message="($cmd) exit status == $exit_status (expected 2)"
    exit 1
  fi
done
set -o errexit

########################################
# test 8: ipcmd semop with argument
########################################

ipcmd semctl setall 2
output1=$(ipcmd semop -u -2 : echo hey)
for cmd in 'ipcmd semop -u -2 : echo success' \
           'ipcmd semop -u 0=-2 : echo success' \
           'ipcmd semop 0=-2u : echo success'
do
  output=$($cmd)
  if [ "$output" != success ]
  then
    error_message="($cmd) output == '$output' (expected 'success')"
    exit 1
  elif [ $(ipcmd semctl getval 0) -ne 2 ]
  then
    error_message="($cmd) semval == '$(ipcmd semctl getval 0)' (expected 2)"
    exit 1
  fi
done

