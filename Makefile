# This is a sample Makefile which compiles source files named:
# - tcpechotimeserv.c
# - tcpechotimecliv.c
# - time_cli.c
# - echo_cli.c
# and creating executables: "server", "client", "time_cli"
# and "echo_cli", respectively.
#
# It uses various standard libraries, and the copy of Stevens'
# library "libunp.a" in ~cse533/Stevens/unpv13e_solaris2.10 .
#
# It also picks up the thread-safe version of "readline.c"
# from Stevens' directory "threads" and uses it when building
# the executable "server".
#
# It is set up, for illustrative purposes, to enable you to use
# the Stevens code in the ~cse533/Stevens/unpv13e_solaris2.10/lib
# subdirectory (where, for example, the file "unp.h" is located)
# without your needing to maintain your own, local copies of that
# code, and without your needing to include such code in the
# submissions of your assignments.
#
# Modify it as needed, and include it with your submission.

CC = gcc

LIBS = -lm -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib


all: bin/fclient bin/fserver

bin/fclient: bin/fclient.o bin/list_recv.o
	${CC} ${FLAGS} -o bin/fclient bin/fclient.o bin/list_recv.o ${LIBS}
bin/fclient.o: fclient.c
	${CC} ${CFLAGS} -c -o bin/fclient.o fclient.c
bin/list_recv.o: list_recv.c
	${CC} ${CFLAGS} -c -o bin/list_recv.o list_recv.c

bin/fserver: bin/fserver.o bin/list_send.o bin/rtt.o
	${CC} ${FLAGS} -o bin/fserver bin/fserver.o bin/list_send.o bin/rtt.o ${LIBS}
bin/fserver.o: fserver.c
	${CC} ${CFLAGS} -c -o bin/fserver.o fserver.c
bin/list_send.o: list_send.c
	${CC} ${CFLAGS} -c -o bin/list_send.o list_send.c
bin/rtt.o: rtt.c
	${CC} ${CFLAGS} -c -o bin/rtt.o rtt.c

clean:
	rm bin/fclient bin/fserver bin/fclient.o bin/fserver.o bin/list_recv.o bin/list_send.o bin/rtt.o

