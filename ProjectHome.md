ipcmd exposes the XSI (SysV) [semaphore](http://beej.us/guide/bgipc/output/html/multipage/semaphores.html) and [message queue](http://beej.us/guide/bgipc/output/html/multipage/mq.html) APIs through a simple command-line interface, enabling scriptable inter-process communication and synchronization.

ipcmd can be used to:
  * [Serialize the execution of certain commands during a parallel make](http://code.google.com/p/ipcmd/wiki/ParallelMake) (e.g., the "ar" command when creating static libraries)
  * [Limit the number of concurrently executing processes](http://code.google.com/p/ipcmd/wiki/LimitConcurrentProcesses)
  * Add process synchronization to scripts (e.g., [barriers](http://code.google.com/p/ipcmd/source/browse/trunk/examples/barrier.sh)) and prevent concurrent access to [critical sections](http://code.google.com/p/ipcmd/wiki/CriticalSections)
  * Prototype or debug applications that use SysV semaphores or message queues

ipcmd is implemented as a single executable, generally requires no system configuration, and is portable to any system that supports SysV IPC (specifically tested on RHEL 5.4, FreeBSD 8.2, Mac OS X 10.5, Solaris 10, and Cygwin).

See this [presentation](http://code.google.com/p/ipcmd/downloads/detail?name=ipcmd_presentation.pdf) for a high-level overview of ipcmd. The [manual](http://code.google.com/p/ipcmd/downloads/detail?name=ipcmd-0.1.0_manual.pdf) contains detailed information on usage, as well as some examples.

### Publications: ###
Weeks, N. T., Kraeva, M. and Luecke, G. R. (2013), <a href='http://onlinelibrary.wiley.com/doi/10.1002/cpe.3001/abstract'>ipcmd: a command-line interface to System V semaphores and message queues</a>. Concurrency Computat.: Pract. Exper.. doi: 10.1002/cpe.3001