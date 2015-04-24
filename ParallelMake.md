#summary Using ipcmd to serialize commands executed during a parallel make.

## Introduction ##

**ipcmd** can be used to serialize invocations of any command during a parallel _make_. This page describes how to take advantage of this capability, using static library creation as an example.

## make ##

On Unix systems, [make](http://en.wikipedia.org/wiki/Make_%28software%29) is the most commonly used utility to automate the process of building software from source code written in statically-typed programming languages. Most such software consists of multiple source code files, many of which can be compiled independently, so to decrease build time on multiprocessor systems, many _make_ implementations support a _-j max\_jobs_ option to enable the simultaneous processing of up to _max\_jobs_ rules in a makefile.

## ar ##

A static library is created by compiling one or more source code files and placing the resulting object (.o) files into an archive library file with the [ar](http://en.wikipedia.org/wiki/Ar_%28Unix%29) utility.

One way to accomplish this in a makefile is to use the special _make_ syntax for creating archive libraries; e.g., the following rule specifies that the archive library libexample.a is created from _file1.o_, _file2.o_, and _file3.o_:
```
libexample.a: libexample.a(file1.o) libexample.a(file2.o) libexample.a(file3.o)
```

## Problem ##

The GNU _make_ manual contains [the following caveat](http://www.gnu.org/software/make/manual/make.html#Archive-Pitfalls):

> It is important to be careful when using parallel execution (the -j switch...) and archives. If multiple ar commands run at the same time on the same archive file, they will not know about each other and can corrupt the file.

> Possibly a future version of make will provide a mechanism to circumvent this problem by serializing all recipes that operate on the same archive file. But for the time being, you must either write your makefiles to avoid this problem in some other way, or not use -j.

If not overridden in the makefile, the following POSIX-specified default rule will be used to compile C source code files and add the resulting object files to an archive library:

```
.c.a:
	$(CC) -c $(CFLAGS) $<
	$(AR) $(ARFLAGS) $@ $*.o
	rm -f $*.o
```

When executed with parallel _make_, this rule contains a race condition: it allows concurrent instances of the _ar_ execution line to update the same archive whose name is contained in the internal macro _$@_.

## Solution ##

To prevent archive library corruption when using parallel _make_, one can write the makefile to create all object files before creating an archive library and adding the object files with a single _ar_ command. In this case, the previously-mentioned default rule for creating archive libraries from C source code should not be relied on.

While straightforward for small makefiles, the previously-described method may be time-consuming to implement in large, existing projects. To achieve a reduction in build time without spending the manpower to refactor makefiles, **ipcmd** may be used with a binary semaphore (which assumes values in the set {0,1}) to serialize the _ar_ invocations.

POSIX specifies that the _make_ **AR** macro is defined as "AR = ar". This macro can be redefined in the makefile or as a command-line argument to _make_ to serialize invocations of _ar_ thus:

```
AR = ipcmd semop -u -1 : ar
```

This command results in the following:
  1. **ipcmd** calls _semop()_ to decrement the semaphore. After the _semop()_ completes, the semaphore value is 0, and any other _semop()_ attempting to decrement the semaphore will block.
  1. **ipcmd** execs the command after the ":" (in this case, _ar_).
  1. The _-u_ option caused the _SEM\_UNDO_ flag to be added to the _semop()_, so after the _ar_ command exits, the kernel undoes the semaphore operation (i.e., increments the semaphore value), allowing another _semop()_ call to complete.

The following example illustrates how **ipcmd** may be used in this manner:
```
$ export IPCMD_SEMID=$(ipcmd semget)    # create a semaphore
$ ipcmd semctl setall 1                 # initialize the semaphore to 1
$ make -j 8 AR='ipcmd semop -u -1 : ar' # parallel make with 8 jobs
...
$ ipcrm -s $IPCMD_SEMID                 # remove the semaphore
```

## Distributing ipcmd ##
Since **ipcmd** is highly-portable to systems supporting SysV IPC and has an unrestrictive BSD license, it can be distributed with software that benefits from parallel _make_, but needs invocations of certain commands within the makefile(s) serialized.

A "wrapper" makefile can be used to build **ipcmd** and invoke the original makefile; e.g., rename the original the original makefile to _Makefile.main_ and create the following makefile named _Makefile_:
```
IPCMDSRC = ipcmd-0.1.0
IPCMD = $(IPCMDSRC)/bin/ipcmd

all: $(IPCMD)
	export IPCMD_SEMID=$$($$PWD/$(IPCMD) semget); \
	trap 'ipcrm -s $$IPCMD_SEMID' EXIT; \
	$$PWD/$(IPCMD) semctl setall 1; \
	make -f Makefile.main AR="$$PWD/$(IPCMD) semop -u -1 : ar"

$(IPCMD):
	cd $(IPCMDSRC); make
```

## Example: MPICH2 ##

The MPICH2 FAQ contains [the following entry](http://wiki.mcs.anl.gov/mpich2/index.php/Frequently_Asked_Questions#Q:_The_build_fails_when_I_use_parallel_make.):

> Q: The build fails when I use parallel make.

> A: Parallel make (often invoked with make -j4) will cause several job steps in the build process to update the same library file (libmpich.a) concurrently. Unfortunately, neither the ar nor the ranlib programs correctly handle this case, and the result is a corrupted library. For now, the solution is to not use a parallel make when building MPICH2.

**Solution:** Use a semaphore to serialize invocations of _ar_ and _ranlib_:

```
$ export IPCMD_SEMID=$(ipcmd semget) # create a semaphore
$ ipcmd semctl setall 1              # initialize the semaphore to 1
$ ./configure AR='ipcmd semop -u -1 : ar' RANLIB='ipcmd semop -u -1 : ranlib' # serialize invocations of ar and ranlib
...
$ make -j 8                          # parallel make with 8 jobs
...
$ ipcrm -s $IPCMD_SEMID              # remove semaphore
```

On a machine with two quad-core Intel Xeon E5504 2.0 GHz CPUs, running RHEL 5.4, _make -j 8_ resulting in the the following reduction in build time for mpich2-1.4 when configured with:

```
$ ./configure --disable-f77 --disable-fc AR='ipcmd semop -u -1 : ar' RANLIB='ipcmd semop -u -1 : ranlib'
```

![http://ipcmd.googlecode.com/files/mpich2_build_times.png](http://ipcmd.googlecode.com/files/mpich2_build_times.png)

