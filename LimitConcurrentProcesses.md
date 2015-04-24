### Limiting Concurrent Child Processes ###

ksh93t has a **JOBMAX** environment variable that limits the number of background jobs that run at the same time; when this limit is reached, the shell will wait for a background job to complete before starting another background job. This feature can be emulated in other shells using ipcmd, with the additional feature of being able to reserve more than one processor slot for a given job.

```
# create a semaphore
export IPCMD_SEMID=$(ipcmd semget)

# clean up: remove the semaphore when the script terminates, killing any
# background processes in case this script terminates unexpectedly. If
# executing these commands interactively, manually execute 'ipcrm -s
# $IPCMD_SEMID' when finished instead of using the following trap command.
trap 'kill 0; ipcrm -s $IPCMD_SEMID' EXIT 

# assume there are 8 available processors
ipcmd semctl setall 8

# Example 1: execute each script in a directory, reserving one processor for
#            each script.
for script in scripts/*
do
  # decrement all (1) semaphores
  ipcmd semop -1
  # execute script asynchronously; increment semaphore after script exits
  ( $script; ipcmd semop +1 ) &
end do

# Example 2: execute a couple arbitrary commands using a syntactic shorthand.
#            The -u option sets the SEM_UNDO flag, which will cause the 
#            semaphore operation to be undone when the process exits.
ipcmd semop -u -2 : mycommand_two_threads &   # reserve two processors
ipcmd semop -u -3 : mycommand_three_threads & # reserve three processors

# wait for all child processes to finish
wait
```

### Limiting Concurrent Processes System-wide ###

The previous example creates a semaphore that can be easily accessed by child processes, but that is difficult for unrelated processes to locate. It may be desirable to create a semaphore that is easily accessible to all processes on the system that wish to voluntarily limit their CPU usage in this manner.

To accomplish this, create a semaphore using the **-S** _semkey_ option before any process accesses it (this could be conveniently done at system startup). **ipcmd ftok** can be used to generate this _semkey_; the _path_ argument to **ftok** should be one accessible to all processes that wish to access the semaphore (such as the **/** directory):

```
# Create a semaphore and initialize it to 8.
ipcmd semctl -s $(ipcmd semget -S $(ipcmd ftok /)) setall 8
```

Next, each process on the system that wants to access this semaphore uses the same **-S** _semkey_ option to **ipcmd semget**:

```
# Retrieve the semid of the existing semaphore set. By default, "ipcmd semget -S SEMKEY"
# errors if a semid associated with SEMKEY already already exists; the "-e" option
# suppresses this error.
export IPCMD_SEMID=$(ipcmd semget -e -S $(ipcmd ftok /))

for script in scripts/*
do
#... same code as previous example ...
```


No process should remove the semaphore (e.g., with **iprm -s $IPCMD\_SEMID**), since the semaphore should persist beyond the lifetime of any process.