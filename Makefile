.POSIX:

########################################
# POSIX
########################################
CC = c99
CFLAGS = -O1
#DEBUG = -O0 -g

########################################
# gcc
########################################
#CC = gcc
#CFLAGS = -O1 -std=c99 -Wall -Wextra
#DEBUG = -O0 -g 

########################################
# PathScale
########################################
#CC = pathcc
#CFLAGS = -O1 -std=c99 -fullwarn 
#DEBUG = -O0 -g2

bin/ipcmd: src/ipcmd.c
	$(CC) $(CFLAGS) $(DEBUG) -o $@ $?

check:
	PATH=bin:$$PATH sh test/semaphores.sh
	PATH=bin:$$PATH sh test/message_queues.sh

clean:
	rm -f bin/ipcmd
