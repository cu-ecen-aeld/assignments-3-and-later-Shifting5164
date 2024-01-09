ifdef CROSS_COMPILE
CC=${CROSS_COMPILE}gcc
else
CC=gcc
endif
LIBS=-lrt -pthread
DEBUG_OPS=-o0 -ggdb


all:
	${CC} -Wno-int-to-pointer-cast aesdsocket.c -o aesdsocket ${LIBS} 

debug: 
	${CC} -Wno-int-to-pointer-cast ${DEBUG_OPS} aesdsocket.c -o aesdsocket ${LIBS}

clean:
	-rm aesdsocket 2>/dev/null

strace: clean all 
	strace ./aesdsocket

valgrind: clean debug 
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite --verbose --log-file=valgrind-out.txt ./aesdsocket
