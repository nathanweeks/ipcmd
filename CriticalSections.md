Semaphores can be used to ensure only one process is allowed to execute a [critical section](http://en.wikipedia.org/wiki/Critical_section) at a time. The following partial skeleton shell script illustrates this:

```
# Create a semaphore set with 1 semaphore (the default if -N is not specified)
export IPCMD_SEMID=$(ipcmd semget)

# Initialize the semaphore to 1 (allow only 1 process in the critical section
# at a time)
ipcmd semctl setall 1

# spawn 4 child processes...
for child in 1 2 3 4
do
  (
    # ... some code here ...
    ipcmd semop -1
    # ... inside critical section ...
    ipcmd semop +1
    # ... some code here ...
  ) &
done
```

If a script contains more than one independent critical section, multiple semaphores can be used; e.g., in the following example, two semaphores are used to guard against concurrent access to CRITICAL\_SECTION1 and CRITICAL\_SECTION2. Note that CRITICAL\_SECTION1 occurs twice; if a process enters either section, all other processes are excluded from entering both sections. This example also includes semaphore removal.

```
# Create a semaphore set with 2 semaphores
export IPCMD_SEMID=$(ipcmd semget -N 2)

# clean up: remove the semaphore when the script terminates, killing any
# background processes in case this script terminates unexpectedly. If
# executing these commands interactively, manually execute
# 'ipcrm -s $IPCMD_SEMID' when finished instead of using the following
# trap command.
trap 'kill 0; ipcrm -s $IPCMD_SEMID' EXIT

# Initialize both semaphores to 1 (allow only 1 process each critical section
# at a time)
ipcmd semctl setall 1

# call the critical section protected by semaphore 0 CRITICAL_SECTION1,
# and the critical section protected by semaphore 1 CRITICAL_SECTION2
readonly CRITICAL_SECTION1=0 CRITICAL_SECTION2=1

# spawn 4 child processes...
for child in 1 2 3 4
do
  (
    ipcmd semop $CRITICAL_SECTION1=-1
    # ... inside the first instance of critical section 1 ...
    ipcmd semop $CRITICAL_SECTION1=+1
 
    ipcmd semop $CRITICAL_SECTION2=-1
    # ... inside critical section 2 ...
    ipcmd semop $CRITICAL_SECTION2=+1

    ipcmd semop $CRITICAL_SECTION1=-1
    # ... inside the second instance of critical section 1 ...
    ipcmd semop $CRITICAL_SECTION1=+1
  ) &
done

# wait for background processes to terminate
# before exiting & removing the semaphore set
wait
```