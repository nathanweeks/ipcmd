/*-
 * Copyright (c) 2011 Nathan Weeks
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define _XOPEN_SOURCE 600
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <unistd.h>

//**************************************
// utility functions
//**************************************
static void print_usage_and_exit(const char *usage)
{
    fprintf(stderr, "usage: %s\n", usage);
    exit(EXIT_FAILURE);
}

static int get_mode_arg(const char *mode_arg, const char *ipcmd_command) {
    char *endptr;
    errno = 0;
    int mode = (int)strtoul(mode_arg, &endptr, 8);
    if (errno != 0) {
        perror("ipcmd: invalid -m MODE"); // TODO: include command
        exit(EXIT_FAILURE);
    } else if (endptr == mode_arg || *endptr != '\0') {
        // no octal number, or entire argument wasn't an octal number
        fprintf(stderr, "ipcmd %s: invalid -m mode\n", ipcmd_command);
        exit(EXIT_FAILURE);
    } else if ((mode | 0666) != 0666) { // TODO: refine error message
        // Accept only read/write (alter for semaphores) permissions.  While
        // Linux & Solaris implementations seem to accept and ignore an
        // execute bit, if the user specified it, it was probably
        // unintentional, so flag this as an error.
        fprintf(stderr, "ipcmd %s: invalid mode\n", ipcmd_command);
        exit(EXIT_FAILURE);
    }
    return mode;
}

// get hex key_t option argument
static key_t get_key_t_arg(const char *key_t_arg, const char *ipcmd_command) {
    char *endptr;
    errno = 0;
    uintmax_t key = strtoumax(key_t_arg, &endptr, 16);
    if (errno != 0) {
        perror("ipcmd: invalid -Q key"); // TODO: include command
        exit(EXIT_FAILURE);
    } else if (endptr == key_t_arg || *endptr != '\0') {
    // argument not entirely a hexidecimal number
        fprintf(stderr, "ipcmd %s: invalid -Q key", ipcmd_command);
        exit(EXIT_FAILURE);
    } 
    return (key_t) key;
}

static short int get_short_arg (
    const char *short_arg,
    const char *ipcmd_command // whence this function was called
) {
    char *endptr;
    errno = 0;
    long arg = strtol(short_arg, &endptr, 10);
    // if error converting to long, or argument not a digit, or argument not
    // entirely a digit
    if (errno != 0) {
        perror("ipcmd: invalid integer argument");
        exit(EXIT_FAILURE);
    } else if (endptr == short_arg || *endptr != '\0') {
        fprintf(stderr, "ipcmd %s: invalid short integer argument\n",
                        ipcmd_command);
        exit(EXIT_FAILURE);
    } else if (arg < SHRT_MIN || arg > SHRT_MAX) {
        fprintf(stderr, "ipcmd %s: integer argument (%lu) out of valid range "
                        "[%hi,%hi]\n",ipcmd_command, arg,
                        SHRT_MIN, SHRT_MAX);
        exit(EXIT_FAILURE);
    }
    return (short)arg;
}

static unsigned short int get_unsigned_short_arg(
    const char *unsigned_short_arg,
    const char *ipcmd_command // whence this function was called
) {
    char *endptr;
    errno = 0;
    long arg = strtol(unsigned_short_arg, &endptr, 10);
    if (errno != 0 || endptr == unsigned_short_arg || *endptr != '\0') {
        perror("ipcmd: invalid integer argument");
        exit(EXIT_FAILURE);
    } else if (endptr == unsigned_short_arg || *endptr != '\0') {
        fprintf(stderr, "ipcmd %s: invalid unsigned short integer argument\n",
                        ipcmd_command);
        exit(EXIT_FAILURE);
    } else if (arg < 0 || arg > USHRT_MAX) {
        fprintf(stderr, "ipcmd %s: integer argument (%li) out of valid range "
                        "[0,%hu]\n",ipcmd_command, arg,USHRT_MAX);
        exit(EXIT_FAILURE);
    }
    return (unsigned short) arg;
}

// RETURN VALUE
//     An 'int' representation of the string referenced by optarg.
//     If the value is outside the range of an int, the program will exit.
static int get_int_arg(
    const char *int_arg,
    const char *ipcmd_command // whence this function was called
) {
    char *endptr;
    errno = 0;
    long arg = strtol(int_arg, &endptr, 10);
    if (errno != 0) {
        perror("ipcmd: invalid integer argument");
        exit(EXIT_FAILURE);
    } else if (endptr == int_arg || *endptr != '\0') {
        fprintf(stderr, "ipcmd %s: invalid integer argument\n", ipcmd_command);
        exit(EXIT_FAILURE);
    } 
    return (int)arg;
}

// RETURN VALUE
//     A 'long' representation of the string referenced by optarg.
static long get_long_arg(
    const char *short_arg,
    const char *ipcmd_command // whence this function was called
) {
    char *endptr;
    errno = 0;
    long arg = strtol(short_arg, &endptr, 10);
    if (errno != 0) {
        perror("ipcmd: invalid long integer argument"); // TODO: include command
        exit(EXIT_FAILURE);
    } else if (endptr == short_arg || *endptr != '\0') {
        fprintf(stderr, "ipcmd %s: invalid long integer argument\n", 
                ipcmd_command);
        exit(EXIT_FAILURE);
    }
    return arg;
}

static void ipcmd_ftok(int argc, char *argv[]) {
    const char *usage = "ipcmd ftok [path [id]]";
    key_t key;
    const char *path = "."; // path defaults to current directory
    int id = 1;       // id defaults to 1

    if (argc == 2)
        path = argv[1];
    else if (argc == 3) {
        path = argv[1];
        // Behavior of ftok() is undefined when id == 0; disallow
        // that.  While ftok() will accept an int > 255, it considers
        // only the low-order 8 bits; if the user entered such a
        // value, it was likely in error.
        errno = 0;
        id = (int)strtol(argv[2], NULL, 10);
        if (errno != 0) {
            perror("ipcmd ftok: invalid id");
            exit(EXIT_FAILURE);
        } else if (id < 1 || id > 255) {
            fprintf(stderr, "ipcmd ftok: id must be an integer between "
                            "1 and 255\n");
            exit(EXIT_FAILURE);
        }
    } else if (argc > 3)
       print_usage_and_exit(usage); 

    if ((key = ftok(path, id)) == (key_t)-1) {
        perror("ipcmd semget: ftok");
        exit(EXIT_FAILURE);
    }

    printf("0x%x\n", key);
}

// NOTE: may not implement msgctl, as most of its functionality overlaps with
// that of ipcs. The only extra functionality that msgctl provides is to
// adjust certain queue attributes.
static void ipcmd_msgget(int argc, char *argv[]) {
    const char *usage = "ipcmd msgget [-Q msgkey [-e]] [-m mode]";
    const int default_mode = 0600; // read & write permission for owner
    // default: create message queue, error if already exists, mode 600
    int msgflg = IPC_CREAT | IPC_EXCL | default_mode;
    key_t key = IPC_PRIVATE; // default if "-Q msgkey" is not specified
    int msqid;
    int c;

    while ((c = getopt(argc, argv, "ehm:Q:")) != -1)
    {
        switch (c)
        {
            case 'e':
                msgflg ^= IPC_EXCL; // remove IPC_EXCL from msgflg
                break;
            case 'm':
                msgflg ^= default_mode; // clear default_mode bits
                msgflg |= get_mode_arg(optarg, "msgget"); // user-supplied mode
                break;
            case 'Q':
                key = get_key_t_arg(optarg, "msgget");
                break;
            default: // unknown or missing argument
                print_usage_and_exit(usage);
        }
    }

    // detect invalid option combinations (-e and not -Q)
    if ((!(msgflg & IPC_EXCL) && key == IPC_PRIVATE))
        print_usage_and_exit(usage);
        
    if ((msqid = msgget(key, msgflg)) == -1) {
        fprintf(stderr, "ipcmd msgget (msgget()): ");
        switch (errno) {
            case EACCES:
                fprintf(stderr, "A message queue identifier exists for the "
                    "argument key, but operation permission as specified by "
                    "the low-order 9 bits of msgflg would not be granted.\n");
                break;
            case EEXIST:
                fprintf(stderr, "A message queue identifier exists for the "
                    "argument key but ((msgflg & IPC_CREAT) && (msgflg & "
                    "IPC_EXCL)) is non-zero.\n");
                break;
            case ENOENT:
                fprintf(stderr, "A message queue identifier does not exist "
                    "for the argument key and (msgflg & IPC_CREAT) is 0.\n");
                break;
            case ENOSPC:
                fprintf(stderr, "A message queue identifier is to be created "
                    "but the system-imposed limit on the maximum number of "
                    "allowed message queue identifiers system-wide would be "
                    "exceeded.\n");
                break;
            default:
                fprintf(stderr, "%s\n", strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    printf("%i\n", msqid);
}

const char *ipcmd_msgsnd_strerror(int errnum) {
    switch(errnum) {
        case EACCES:
            return "Operation permission is denied to the calling process.";
        case EIDRM:
            return "The message queue identifier msqid is removed from the "
                   "system.";
        case EINTR:
            return "The msgsnd() function was interrupted by a signal.";
        case EINVAL:
            return "The value of msqid is not a valid message queue "
                   "identifier, or the value of mtype is less than 1; or the "
                   "value of msgsz is less than 0 or greater than the "
                   "system-imposed limit.";
        default:
            return strerror(errno);
    }
}

const char *ipcmd_msgctl_strerror(int errnum) {
    switch(errnum) {
        case EACCES:
            return "The argument cmd is IPC_STAT and the calling process does "
                   "not have read permission";
        case EINVAL:
            return "The value of msqid is not a valid message queue "
                   "identifier; or the value of cmd is not a valid command.";
        // case EPERM: not applicable, since ipcmd does not call msgctl() with
        //       either IPC_SET or IPC_RMID. Update this function if this ever
        //       changes.
        default:
            return strerror(errno);
    }
}

// FIXME: It probably doesn't make sense to allow both "-n" and more than one 
// message argument, as it would be impossible to know which messages were sent.
static void ipcmd_msgsnd(int argc, char *argv[]) {
    const char *usage = "msgsnd [-q msqid] [-t mtype] [-n] [message...]";
    struct msg {long mtype; char mtext[];}; 
    struct msg *msgp;
    long mtype = 1;
    int msqid = 0;
    int msgflg = 0;
    int c;
    struct msqid_ds buf;
    size_t msgsz;

    while ((c = getopt(argc, argv, "nq:t:")) != -1)
    {
        switch (c)
        {
            case 'n':
                msgflg |= IPC_NOWAIT;
                break;
            case 'q':
                msqid = get_int_arg(optarg, "msgsnd");
                break;
            case 't':
                mtype = get_long_arg(optarg, "msgsnd");
                break;
            default:  // unknown option
                print_usage_and_exit(usage);
        }
    }

    if (!msqid) { // -q option not used
        if (!getenv("IPCMD_MSQID")) { //IPCMD_MSQID environment variable not set
            fprintf(stderr, "ipcmd msgsnd: must either specify [-q msqid] or "
                            "set IPCMD_MSQID environment variable\n");
            exit(1);
        } else
            msqid = get_int_arg(getenv("IPCMD_MSQID"), "msgget");
    }

    // BUG (maybe): it's possible the user has write permission, but not read
    // permission, on the message queue.
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        fprintf(stderr, "ipcmd msgsnd (msgctl()): %s\n",
                ipcmd_msgctl_strerror(errno));
        exit(EXIT_FAILURE);
    }

    // FIXME: maximum number of bytes allowed on a queue is not necessarily
    // equal to the max number of bytes allowed in a message (they are equal in
    // Solaris, though).
    if ((msgp = (struct msg *)malloc(sizeof(struct msg) + buf.msg_qbytes+1)) ==
        NULL) {
        perror("ipcmd msgsnd: malloc");
        exit(EXIT_FAILURE);
    }

    msgp->mtype = mtype; // any user-specified applies to all messages

    if (optind < argc) {   // message arguments specified
        do {
            if ((msgsz = strlen(argv[optind])) > (size_t)(buf.msg_qbytes)) {
                fprintf(stderr,"ipcmd msgsnd: message argument length > "
                               "msg_qbytes\n");
                exit(EXIT_FAILURE);
            }

            strcpy(msgp->mtext, argv[optind]);

            if (msgsnd(msqid, (void *)msgp, msgsz, msgflg) == -1) {
                if (errno == EAGAIN) // message could not be sent and "-n" used
                    exit(2); 
                else {
                    fprintf(stderr, "ipcmd msgsnd (msgsnd()): %s\n", 
                        ipcmd_msgsnd_strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            optind++;
        } while (optind < argc);
    } else { // read message from stdin
        msgsz = fread(msgp->mtext, (size_t)1, buf.msg_qbytes+1, stdin);

        if (ferror(stdin)) {
            perror("ipcmd msgsnd: fread");
            exit(EXIT_FAILURE);
        }
        
        // if 1 more byte was read than the queue can hold
        if (msgsz == (size_t)buf.msg_qbytes+1) {
            fprintf(stderr,"ipcmd msgsnd: message length > msg_qbytes\n");
            exit(EXIT_FAILURE);
        }

        if (msgsnd(msqid, (void *)msgp, msgsz, msgflg) == -1) {
            if (errno == EAGAIN)
                exit(2); // message could not be sent and "-n" specified
            else {
                fprintf(stderr, "ipcmd msgsnd (msgsnd()): %s\n", 
                    ipcmd_msgsnd_strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void ipcmd_msgrcv(int argc, char *argv[]) {
    const char *usage = "ipcmd msgrcv [q msqid] [-t msgtyp] [-n] [-v]";
    struct msg {long mtype; char mtext[];}; 
    struct msg *msgp;
    long msgtyp = 0; // 0: default is to receive a message of any type
    int msqid = 0;
    int msgflg = 0;
    int c;
    struct msqid_ds buf;
    size_t msgsz;
    ssize_t bytes_received;
    int verbose = 0; // if 1, print type of received message to stderr

    while ((c = getopt(argc, argv, "nq:t:v")) != -1)
    {
        switch (c)
        {
            case 'n':
                msgflg |= IPC_NOWAIT;
                break;
            case 'q':
                msqid = get_int_arg(optarg, "msgrcv");
                break;
            case 't':
                msgtyp = get_long_arg(optarg, "msgrcv");
                break;
            case 'v':
                verbose = 1;
                break;
            default:  // unknown option
                print_usage_and_exit(usage);
        }
    }

    if (!msqid) { // -q option not used
        if (!getenv("IPCMD_MSQID")) { //IPCMD_MSQID environment variable not set
            fprintf(stderr, "ipcmd msgrcv: must either specify [-q msqid] or "
                            "set IPCMD_MSQID environment variable\n");
            exit(1);
        } else
            msqid = get_int_arg(getenv("IPCMD_MSQID"), "msgget");
    }
        
    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        fprintf(stderr, "ipcmd msgrcv (msgctl()): %s\n",
                ipcmd_msgctl_strerror(errno));
        exit(EXIT_FAILURE);
    }

    if ((msgp = (struct msg *)malloc(sizeof(struct msg) + buf.msg_qbytes)) ==
        NULL) {
        perror("ipcmd msgrcv: malloc");
        exit(EXIT_FAILURE);
    }

    msgsz = buf.msg_qbytes;

    if ((bytes_received = msgrcv(msqid, (void *)msgp, msgsz, msgtyp, msgflg))
         == (ssize_t)-1) {
        if (errno == ENOMSG) // "-n" option specified and no message of desired
            exit(2);         // type in queue
        else {
            fprintf(stderr, "ipcmd msgrcv (msgrcv()): ");
            switch(errno) {
                case E2BIG:
                    fprintf(stderr, "The value of mtext is greater than msgsz "
                        "and (msgflg & MSG_NOERROR) is 0.\n");
                    break;
                case EACCES:
                    fprintf(stderr, "Operation permission is denied to the "
                        "calling process\n");

                    break;
                case EIDRM:
                    fprintf(stderr, "The message queue identifier msqid is "
                        "removed from the system.\n");
                    break;
                case EINTR:
                    fprintf(stderr, "The msgrcv() function was interrupted by "
                        "a signal.\n");
                    break;
                case EINVAL:
                    fprintf(stderr, "msqid is not a valid message queue "
                        "identifier.\n");
                    break;
                case ENOMSG:
                    fprintf(stderr, "The queue does not contain a message of "
                        "the desired type and (msgflg & IPC_NOWAIT) is "
                        "non-zero.\n");
                    break;
                default:
                    fprintf(stderr, "%s\n", strerror(errno)); 
            }
            exit(EXIT_FAILURE);
        }
    }

    if (verbose)
        fprintf(stderr, "%li\n", msgp->mtype);

    if (fwrite(msgp->mtext, (size_t)1, (size_t)bytes_received, stdout) 
        < (size_t)bytes_received) {
        perror("ipcmd msgsnd: fwrite");
        exit(EXIT_FAILURE);
    }
}

// TODO: restrict mode argument to bits 666 (i.e., no "execute" bit)
// * NOTE: semget() allows specifying nsems without IPC_CREAT, (i.e., we could 
//   use '-N nsems' without '-c', so ipcmd semget could verifies that the
//   semaphore set contains at least nsems semaphores, but would specifying
//   the two together likely be a mistake? Revisit this and decide.
// * Does it make sense to accept "-s semid", or could we just use
// IPCMD_SEMID=SEMID ipcmd...
static void ipcmd_semget(int argc, char *argv[]) {
    const char *usage = 
    "ipcmd semget [-S semkey [-e]] [-m mode] [-N nsems]\n"
    "  -S       : create semaphore set associated with semkey\n"
    "  -e       : no error if the semaphore set already exists\n"
    "  -m mode  : read/alter permissions (octal value; default: 600)\n"
    "  -N nsems : create a semaphores set with nsems semaphores (default 1)";

    const int default_mode = 0600; // read & alter permission for owner
    // default: create semaphore set, error if already exists, mode 600
    int semflg = IPC_CREAT | IPC_EXCL | default_mode;
    key_t key = IPC_PRIVATE; // default if "-S semkey" is not specified
    int nsems = 1;  // create 1 semaphore by default if "-c" (IPC_CREAT) is 
                    // specified; this parameter is ignored otherwise
    int semid;
    int c;

    while ((c = getopt(argc, argv, "em:N:S:")) != -1)
    {
        switch (c)
        {
            case 'e':
                semflg ^= IPC_EXCL; // remove IPC_EXCL from semflg
                break;
            case 'm':
                semflg ^= default_mode; // clear default_mode bits
                semflg |= get_mode_arg(optarg, "semget"); // user-supplied mode
                break;
            case 'N':
                nsems = get_int_arg(optarg, "semget");
                break;
            case 'S':
                key = get_key_t_arg(optarg, "semget");
                break;
            default: // unknown or missing argument
                print_usage_and_exit(usage);
        }
    }

    if (optind != argc) // arguments specified
        print_usage_and_exit(usage);

    // detect invalid option combinations (-e and not -S)
    if ((!(semflg & IPC_EXCL) && key == IPC_PRIVATE))
        print_usage_and_exit(usage);
        
    if ((semid = semget(key, nsems, semflg)) == -1) {
        fprintf(stderr, "ipcmd semget (semget()): ");
        switch (errno) {
            case EACCES:
                fprintf(stderr, "A semaphore identifier exists for key, but "
                    "operation permission as specified by the low-order 9 bits "
                    "of semflg would not be granted.\n");
                break;
            case EEXIST:
                fprintf(stderr, "A semaphore identifier exists for the "
                    "argument key but ((semflg &IPC_CREAT) && "
                    "(semflg &IPC_EXCL)) is non-zero.\n");
                exit(2);
                break;
            case EINVAL:
                fprintf(stderr, "The value of nsems is either less than or "
                    "equal to 0 or greater than the system-imposed limit, or a "
                    "semaphore identifier exists for the argument key, but the "
                    "number of semaphores in the set associated with it is "
                    "less than nsems and nsems is not equal to 0.\n");
                break;
            case ENOENT:
                fprintf(stderr, "A semaphore identifier does not exist for the "
                                "argument key and (semflg &IPC_CREAT) is equal "
                                "to 0.\n");
                break;
            case ENOSPC:
                fprintf(stderr, "A semaphore identifier is to be created but "
                    "the system-imposed limit on the maximum number of allowed "
                    "semaphores system-wide would be exceeded.\n");
                break;
            default:
                fprintf(stderr, "%s\n", strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    printf("%i\n", semid);
}

// unsigned short interval due to SEMMSL <= USHRT_MAX in all known
// implementations (Mac OS X 10.5/10.6 claims 87381 as the default SEMMSL, but
// it doesn't appear to support this in practice)
static void get_interval(
    const char *interval_arg,    // lower_bound[:upper_bound]=...
    const char delimiter,        // character after interval; "=" or ":"
    unsigned short *lower_bound, 
    unsigned short *upper_bound  // == lower_bound if no upper_bound specified
) {
    char *beginptr, *endptr;
    errno = 0;
    long val = strtol(interval_arg, &endptr, 10);

    // if error converting to long, or argument not a digit, or argument not
    // entirely a digit
    if (errno != 0) {
        perror("ipcmd: strtol");
        exit(EXIT_FAILURE);
    } else if (endptr == interval_arg) {
        // no integer at beginning of string
        fprintf(stderr, "ipcmd: invalid argument: %s\n", interval_arg);
        exit(EXIT_FAILURE);
    } else if (val < 0 || val > USHRT_MAX) {
        fprintf(stderr, "ipcmd: argument (%li) out of valid range\n", val);
        exit(EXIT_FAILURE);
    }

    *lower_bound = (unsigned short) val;

    if (*endptr == ':') {
        beginptr = endptr+1; // next character
        errno = 0;
        val = strtol(beginptr, &endptr, 10);
        if (errno != 0) {
            perror("strtol");
            exit(EXIT_FAILURE);
        } else if (endptr == beginptr) {
            // no integer after comma
            fprintf(stderr, "ipcmd: invalid argument: %s\n", interval_arg);
            exit(EXIT_FAILURE);
        } else if (val < 0 || val > USHRT_MAX) {
            fprintf(stderr, "ipcmd: argument (%li) out of valid range\n", val);
            exit(EXIT_FAILURE);
        }

        *upper_bound = (unsigned short) val;

        if (*upper_bound < *lower_bound) {
            fprintf(stderr, "ipcmd: invalid argument (%s); upper bound of "
                  "interval must be >= lower bound\n", interval_arg);
            exit(EXIT_FAILURE);
        }
    } else
        *upper_bound = *lower_bound; // no upper_bound specified

    // kludge: the function that gets the value will pick up at the delimiter;
    // ensure it exists immediately after the interval to verify the entire
    // argument is of the right form
    if (*endptr != delimiter) {
        fprintf(stderr, "ipcmd: invalid argument (%s)\n", interval_arg);
        exit(EXIT_FAILURE);
    }
}

static unsigned short get_interval_semval(
    const char *arg // sem_num_lbound[,sem_num_ubound]=semval
)
{
    long val;
    char *beginptr, *endptr;

    // this check shouldn't be needed, as it was done in get_interval()
    if ((beginptr = strchr(arg, '=')) == NULL) {
        fprintf(stderr, "ipcmd: invalid argument (%s)\n", arg);
        exit(EXIT_FAILURE);
    }

    beginptr++; // next character

    errno = 0;
    val = strtol(beginptr, &endptr, 10);
    // if error converting to long, or argument not a digit, or argument not
    // entirely a digit
    if (errno != 0) {
        perror("strtol");
        exit(EXIT_FAILURE);
    } else if (endptr == beginptr) {
        // no integer at beginning of field
        fprintf(stderr, "ipcmd semctl setall: invalid argument: %s\n", arg);
        exit(EXIT_FAILURE);
    } else if (val < 0 || val > USHRT_MAX) {
        fprintf(stderr, 
                "ipcmd semctl setall: semval (%li) out of valid range\n", val);
        exit(EXIT_FAILURE);
    } else if (*endptr != '\0') {
        // extra characters after semval
        fprintf(stderr, "ipcmd semop: invalid argument: %s\n", arg);
        exit(EXIT_FAILURE);
    }

    return (unsigned short) val;
}

const char *ipcmd_semctl_strerror(int errnum) {
    switch(errnum) {
        case EACCES:
            return "Operation permission is denied to the calling process.";
        case EINVAL:
            return "The value of semid is not a valid semaphore identifier, "
                   "or the value of semnum is less than 0 or greater than or "
                   "equal to sem_nsems, or the value of cmd is not a valid "
                   "command.";
        case EPERM:
            return "The argument cmd is equal to IPC_RMID or IPC_SET and the "
                   "effective user ID of the calling process is not equal to "
                   "that of a process with appropriate privileges and it is "
                   "not equal to the value of sem_perm.cuid or sem_perm.uid in "
                   "the data structure associated with semid.";
        case ERANGE:
            return "The argument cmd is equal to SETVAL or SETALL and the "
                   "value to which semval is to be set is greater than the "
                   "system-imposed maximum.";
        default:
            return strerror(errnum);
    }
}
// TODO: This would be more elegant if we used long options as follows:
//     [--chown owner] [--chgrp group] [--chmod mode]
//     --getval semnum
//     --setval semnum value
//     --getpid semnum
//     --getncnt semnum
//     --getzcnt semnum
//     --getall
//     --setall arg [arg...]
static void ipcmd_semctl(int argc, char *argv[]) {
    const char *usage = 
    "ipcmd semctl [-s semid] <subcommand> <args>\n"
    "Where <subcommand> <args> is one of the following:\n"
    "  getval  SEMNUM\n"
    "  setval  SEMNUM SEMVAL\n"
    "  getpid  SEMNUM\n"
    "  getncnt SEMNUM\n"
    "  getzcnt SEMNUM\n"
    "  getall\n"
    "  setall  [SEMNUM_LBOUND[,SEMNUM_UBOUND]=]SEMVAL...";
    int semid = 0;
    int semnum = -1; // semnum argument for some commands
    int cmd = IPC_STAT; // command that won't be supported, so it's safe to use
                        // as an "unset" value
    unsigned short sem_nsems;
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short  *array;
    } arg;
    struct semid_ds seminfo;
    int c;

    while ((c = getopt(argc, argv, "s:")) != -1)
    {
        switch (c)
        {
            case 's':
                semid = get_int_arg(optarg, "semctl");
                break;
            default: // unknown or missing argument
                print_usage_and_exit(usage);
        }
    }

    if (optind == argc) // no subcommand specified
        print_usage_and_exit(usage);

    if (!semid) { // -s option not used
        if (!getenv("IPCMD_SEMID")) { //IPCMD_SEMID environment variable not set
            fprintf(stderr, "ipcmd semctl: must either specify [-s semid] or "
                            "set IPCMD_SEMID environment variable\n");
            exit(1);
        } else
            semid = get_int_arg(getenv("IPCMD_SEMID"), "semctl");
    }

    if (strncmp(argv[optind], "getval", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = GETVAL;
    else if (strncmp(argv[optind], "setval", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = SETVAL;
    else if (strncmp(argv[optind], "getpid", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = GETPID;
    else if (strncmp(argv[optind], "getncnt", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = GETNCNT;
    else if (strncmp(argv[optind], "getzcnt", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = GETZCNT;
    else if (strncmp(argv[optind], "getall", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = GETALL;
    else if (strncmp(argv[optind], "setall", (size_t)_POSIX_ARG_MAX) == 0)
        cmd = SETALL;
    else
        print_usage_and_exit(usage);

    optind++;

    switch(cmd) {
        case GETVAL:
        case GETPID:
        case GETNCNT:
        case GETZCNT:
            if (optind+1 != argc) // if not exactly one more argument (SEMNUM)
                print_usage_and_exit(usage);
            semnum = get_int_arg(argv[optind++], "semctl");
            int result;
            if ((result = semctl(semid, semnum, cmd, arg)) == -1) {
                fprintf(stderr, "ipcmd semctl getzcnt (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }
            printf("%i\n", result);
            break;
        // 2010-10-10: For OS X 10.5/10.6, SETVAL works with values up to 32767;
        // however, it doesn't report an error for values >= 32767.
        case SETVAL:
            if (optind == argc) // if missing SEMNUM operand
                print_usage_and_exit(usage);
            semnum = get_int_arg(argv[optind++], "semctl");
            if (optind == argc) // if missing VALUE operand
                print_usage_and_exit(usage);
            arg.val = get_int_arg(argv[optind++], "semctl setval VALUE");
            if (optind != argc) // if arguments after VALUE
                print_usage_and_exit(usage);
            if (semctl(semid, semnum, cmd, arg) == -1) {
                fprintf(stderr, "ipcmd semctl setval (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;
        case SETALL:
            if (optind == argc) // missing SEMVAL operands
                print_usage_and_exit(usage);

            // need to know sem_nsems
            arg.buf = &seminfo;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                fprintf(stderr, "ipcmd semctl setall (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }

            // NOTE: The (short) cast is used because Linux 2.4+ defines
            // sem_nsems as an unsigned long int, while everyone else (and
            // SUSv4) defines it as an unsigned short. This should never
            // be > USHRT_MAX anyway, as the sem_num member of struct sembuf
            // is unsigned short, and it wouldn't make sense to have more
            // semaphores than could be operated on.
            sem_nsems = (short)arg.buf->sem_nsems;

            if ((arg.array = 
                (unsigned short *)malloc(sem_nsems*sizeof(unsigned short)))
                == NULL) {
                perror("ipcmd semctl setall: malloc");
                exit(EXIT_FAILURE);
            }

            // one non-interval SEMVAL operand
            if (optind+1 == argc && !strchr(argv[optind], '=')) {
                unsigned short semval = 
                               get_unsigned_short_arg(argv[optind], "semctl");
                for (unsigned short i = 0; i < sem_nsems; i++)
                    arg.array[i]=semval;
            } else { 
                unsigned short sem_num_lbound, sem_num_ubound;
                // Verify that there exists a semval for each semaphore in the
                // set. While this isn't explicitly required by SETALL, it's
                // likely that the user made a mistake if these aren't equal,
                // and will likely cause data-corruption! 

                int semval_count = 0; // number of specified semvals
                int sem_num_sum = 0;  // sum of specified sem_nums
                for (int i = optind; i < argc; i++) {
                    get_interval(argv[i],'=',&sem_num_lbound,&sem_num_ubound);
                    semval_count += (sem_num_ubound - sem_num_lbound + 1);
                    // sem_num_sum == sem_num_lbound + ... + sem_num_ubound
                    sem_num_sum += ((int)sem_num_ubound+1)*sem_num_ubound/2 -
                                   ((int)sem_num_lbound)*(sem_num_lbound-1)/2;
                }
                if (semval_count != (int)sem_nsems || 
                    // 0+1+...+N-1 == N*(N-1)/2-1
                    sem_num_sum != (int)sem_nsems*(sem_nsems-1)/2) {
                    fprintf(stderr, "ipcmd semctl setall: invalid number of "
                                    "semval arguments specified\n");
                    exit(EXIT_FAILURE);
                }

                // now actually set arg.array
                for (int i = optind; i < argc; i++) {
                    get_interval(argv[i],'=',&sem_num_lbound,&sem_num_ubound); 
                    for (unsigned short sem_num = sem_num_lbound; 
                         sem_num <= sem_num_ubound; sem_num++)
                        arg.array[sem_num]=get_interval_semval(argv[i]);
                }
            }

            if (semctl(semid, 0, SETALL, arg) == -1) {
                fprintf(stderr, "ipcmd semctl setall (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }

            break;
        case GETALL:
            if (optind != argc) // if extra arguments after "getall"
                print_usage_and_exit(usage);

            // get number of semaphores in set
            arg.buf = &seminfo;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                fprintf(stderr, "ipcmd semctl getall (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }

            // Allocate array for semaphores. Again, the (short) cast is
            // needed because for Linux 2.4+ defines sem_nsems as an unsigned
            // long
            sem_nsems = (unsigned short)arg.buf->sem_nsems;
            if ((arg.array = 
                (unsigned short *)malloc(sem_nsems*sizeof(unsigned short)))
                == NULL) {
                    perror("ipcmd semctl getall: malloc");
                    exit(EXIT_FAILURE);
            }

            if (semctl(semid, 0, GETALL, arg) == -1) {
                fprintf(stderr, "ipcmd semctl getall (semctl()): %s\n",
                        ipcmd_semctl_strerror(errno));
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < (int)sem_nsems; i++) {
                if (i < sem_nsems - 1)
                    printf("%hu ", arg.array[i]);
                else // last semaphore to print
                    printf("%hu\n", arg.array[i]);
            }
            break;
        default:
            break; // will never get here
    }
}

static void get_interval_sem_op (
    const char *arg, // sem_num_lbound[,sem_num_ubound]=sem_op[un]
    short *sem_op,
    short *sem_flg   // SEM_UNDO and/or IPC_NOWAIT
)
{
    long val;
    char *beginptr, *endptr;

    // this check shouldn't be needed, as it was done in get_interval()
    if ((beginptr = strchr(arg, '=')) == NULL) {
        fprintf(stderr, "ipcmd: invalid argument (%s)\n", arg);
        exit(EXIT_FAILURE);
    }

    beginptr++; // next character

    errno = 0;
    val = strtol(beginptr, &endptr, 10);
    // if error converting to long, or argument not a digit, or argument not
    // entirely a digit
    if (errno != 0) {
        perror("strtol");
        exit(EXIT_FAILURE);
    } else if (endptr == beginptr) {
        // no integer at beginning of field
        fprintf(stderr, "ipcmd semop: invalid argument: %s\n", arg);
        exit(EXIT_FAILURE);
    } else if (val < SHRT_MIN || val > SHRT_MAX) {
        fprintf(stderr, "ipcmd semop: sem_op (%li) out of valid range "
                        "[%hi,%hi]\n", val, SHRT_MIN, SHRT_MAX);
        exit(EXIT_FAILURE);
    }

    *sem_op = (short) val;

    // ensure any remaining string contains only "n" or "u"
    if (strcspn(endptr, "nu")) {
        fprintf(stderr, "ipcmd semop: invalid argument: %s\n", arg);
        exit(EXIT_FAILURE);
    }

    *sem_flg = 0;
    if (strchr(endptr, 'n') != NULL)
        *sem_flg |= IPC_NOWAIT;
    if (strchr(endptr, 'u') != NULL)
        *sem_flg |= SEM_UNDO;
}

// Sum of cardinalities of all integer intervals in arguments. This is useful
// for determining the size of various arrays before allocating memory for
// them.
static size_t get_interval_count(
    int interval_argc,
    char *interval_argv[]
) {
    size_t count = 0;

    unsigned short lower_bound;
    unsigned short upper_bound;

    for (int i = 0; i < interval_argc; i++) {
        get_interval(interval_argv[i], '=', &lower_bound, &upper_bound);
        count += (size_t)(upper_bound - lower_bound + 1);
    }

    return count;
}


// set semaphore-operation array from interval arguments
//
// RETURN VALUE
//     nsops - total number of semaphore operations (for semop(..., nsops))
static size_t set_interval_sops (
    int argc,               // number of args in argv[]
    char *argv[],           // semaphore interval argument array
    struct sembuf *sops,
    short sem_flg_additions // add these flags to sem_flg for each operation
)
{
    size_t nsops = 0;

    unsigned short sem_num_lbound; // lower bound of sem_num interval
    unsigned short sem_num_ubound; // upper bound of sem_num interval
    short sem_op;
    short sem_flg;

    for (int i = 0; i < argc; i++) {
        get_interval(argv[i], '=', &sem_num_lbound, &sem_num_ubound);
        get_interval_sem_op(argv[i], &sem_op, &sem_flg);
        sem_flg |= sem_flg_additions;
        for (int sem = (int)sem_num_lbound;
             sem <= (int)sem_num_ubound; 
             sem++, nsops++)
        {
            sops[nsops].sem_num = (unsigned short)sem;
            sops[nsops].sem_op = sem_op;
            sops[nsops].sem_flg = sem_flg;
        }
    }

    return nsops;
}

static void ipcmd_semop(int argc, char *argv[]) {
    const char *usage = 
    "ipcmd semop [-s semid] [-n] [-u] <ARGS>\n"
    "Where ARGS is one of the following forms:\n"
    "  sem_op [: COMMAND [<COMMAND_ARGS>]]\n"
    "or\n"
    "  sem_num[:sem_num]=[+|-]sem_op[n][u]... [: COMMAND [<COMMAND_ARGS>]]\n"
    "Options:\n"
    "  -s semid : semaphore identifier of an existing semaphore set\n"
    "  -n       : (IPC_NOWAIT) all operations are non-blocking\n"
    "  -u       : (SEM_UNDO) undo all nonzero operations upon exit";
    int semid = 0;
    short int sem_flg = 0;
    size_t nsops;
    struct sembuf *sops;
    int command_arg = 0; // index into argv[] of optional command argument
    int c;

    while ((c = getopt(argc, argv, "ns:u0123456789")) != -1)
    {
        switch (c)
        {
            case 'n':
                sem_flg |= IPC_NOWAIT;
                break;
            case 's':
                semid = get_int_arg(optarg, "semop");
                break;
            case 'u':
                sem_flg |= SEM_UNDO;
                break;
            // Test if a negative integer so we can handle this case:
            // $ ipcmd semop -1
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                // if there is only one digit (e.g., "-1"), getopt() will have 
                // incremented optind, so back up so it still indexes the
                // sem_op argument in argv[]. If there was more than one
                // digit, getopt() would have treated it as a string containing
                // multiple options, and would not have incremented optind.
                optind = (optind == argc || strcmp(argv[optind],":") == 0) ?
                         optind-1 : optind;
                goto process_arguments;
        }
    }

process_arguments:

    if (optind == argc) // if no operands specified
        print_usage_and_exit(usage);

    if (!semid) { // -s option not used
        if (!getenv("IPCMD_SEMID")) { //IPCMD_SEMID environment variable not set
            fprintf(stderr, "ipcmd semop: must either specify [-s semid] or "
                            "set IPCMD_SEMID environment variable\n");
            exit(1);
        } else
            semid = get_int_arg(getenv("IPCMD_SEMID"), "semctl");
    }

    // if first operand has a "=", assume semaphore interval arguments
    // (sem_num[:sem_num]=[+|-]sem_op[n][u]...)
    if (strchr(argv[optind], '=')) {
        // determine start of user-specified command argument, if any
        for (int opt = optind+1; opt < argc; opt++)
            if (strncmp(argv[opt], ":", strlen(":")+1) == 0) {
                command_arg = opt+1;
                // verify command argument after ":" exists
                if (command_arg == argc)
                    print_usage_and_exit(usage);
                break;
            }
        
        int interval_argc = (command_arg>0? command_arg-1-optind : argc-optind);
        // get number of sops
        nsops = get_interval_count(interval_argc, &argv[optind]);
        if ((sops=(struct sembuf *)malloc(nsops*sizeof(struct sembuf)))==NULL){
            perror("ipcmd semop: malloc");
            exit(EXIT_FAILURE);
        }
        // second pass through semaphore arguments sets sembuf array
        set_interval_sops(interval_argc, &argv[optind], sops, sem_flg);
    } else { // assume a single sem_op applied to all semaphores in the set

        // verify there is either a command argument...
        if ((optind+2 < argc) && strcmp(argv[optind+1], ":") == 0)
            command_arg = optind+2;
        else if (optind+1 != argc) // or exactly one (sem_op) argument
            print_usage_and_exit(usage);

        short sem_op = get_short_arg(argv[optind], "semop");

        union semun {
            int val;
            struct semid_ds *buf;
            unsigned short  *array;
        } arg;
        struct semid_ds seminfo;
        arg.buf = &seminfo;

        if (semctl(semid, 0, IPC_STAT, arg) == -1) {
            fprintf(stderr, "ipcmd semop (semctl()): %s\n",
                ipcmd_semctl_strerror(errno));
            exit(EXIT_FAILURE);
        } 

        nsops = (size_t)arg.buf->sem_nsems;
        
        if ((sops=(struct sembuf *)malloc(nsops*sizeof(struct sembuf)))==NULL){
            perror("ipcmd semop: malloc");
            exit(EXIT_FAILURE);
        }
        // NOTE: POSIX.1-2008 lists incorrect type for sem_num member of
        // sembuf in the description of semop() (listed as "short", should be
        // "unsigned short") see: http://austingroupbugs.net/view.php?id=329
        for (unsigned short sem_num=0; sem_num < nsops; sem_num++) {
           sops[sem_num].sem_num = sem_num;
           sops[sem_num].sem_op = sem_op;
           sops[sem_num].sem_flg = sem_flg;
        }
    }

    if (semop(semid, sops, nsops) == -1) {
        if (errno == EAGAIN) // process would have be suspended had IPC_NOWAIT
            exit(2);         // (-n) not been specified
        else {
            fprintf(stderr, "ipcmd semop (semop()): ");
            switch (errno) {
                case E2BIG:
                    fprintf(stderr, "The value of nsops is greater than the "
                        "system-imposed maximum.\n");
                    break;
                case EACCES:
                    fprintf(stderr, "Operation permission is denied to the "
                        "calling process.\n");
                    break;
                case EFBIG:
                    fprintf(stderr, "The value of sem_num is less than 0 or "
                        "greater than or equal to the number of semaphores in "
                        "the set associated with semid.\n");
                    break;
                case EIDRM:
                    fprintf(stderr, "The semaphore identifier semid is "
                        "removed from the system.\n");
                    break;
                case EINTR:
                    fprintf(stderr, "The semop() function was interrupted by "
                        "a signal.\n");
                    break;
                case EINVAL:
                    fprintf(stderr, "The value of semid is not a valid "
                        "semaphore identifier, or the number of individual "
                        "semaphores for which the calling process requests a "
                        "SEM_UNDO would exceed the system-imposed limit.\n");
                    break;
                case ENOSPC:
                    fprintf(stderr, "The limit on the number of individual "
                        "processes requesting a SEM_UNDO would be exceeded.\n");
                    break;

                case ERANGE:
                    fprintf(stderr, "An operation would cause a semval to "
                        "overflow the system-imposed limit, or an operation "
                        "would cause a semadj value to overflow the "
                        "system-imposed limit.\n");
                    break;
                default:
                    fprintf(stderr, "%s\n", strerror(errno));
            }
            exit(EXIT_FAILURE);
        }
    }
    if (command_arg)
        if (execvp(argv[command_arg], &argv[command_arg]) == -1) {
            perror("ipcmd semop: execvp");
            exit(EXIT_FAILURE);
        }
}

int main(int argc, char *argv[]) {
    const char *usage = 
        "ipcmd <command> [options] [args]\n\n"
        "Where <command> is one of the following:\n"
        "    ftok      generate an IPC key\n"
        "    msgget    create a message queue\n"
        "    msgrcv    receive a message\n"
        "    msgsnd    send a message\n"
        "    semctl    initialization/query semaphores\n"
        "    semget    create a semaphore set\n"
        "    semop     semaphore operations"
                        ;
    if (argc < 2)
        print_usage_and_exit(usage);

    argc--; argv++; // consume "ipcmd" from argv, leaving <command> ...

    if (strncmp(argv[0], "ftok", (size_t)_POSIX_ARG_MAX) == 0)
        ipcmd_ftok(argc, argv);
    else if (strncmp(argv[0], "msgget", strlen("msgget")+1) == 0)
        ipcmd_msgget(argc, argv);
    else if (strncmp(argv[0], "msgrcv", strlen("msgrcv")+1) == 0)
        ipcmd_msgrcv(argc, argv);
    else if (strncmp(argv[0], "msgsnd", strlen("msgsnd")+1) == 0)
        ipcmd_msgsnd(argc, argv);
    else if (strncmp(argv[0], "semctl", strlen("semctl")+1) == 0)
        ipcmd_semctl(argc, argv);
    else if (strncmp(argv[0], "semget", strlen("semget")+1) == 0)
        ipcmd_semget(argc, argv);
    else if (strncmp(argv[0], "semop", strlen("semop")+1) == 0)
        ipcmd_semop(argc, argv);
    else 
        print_usage_and_exit(usage);

    return 0;
}
