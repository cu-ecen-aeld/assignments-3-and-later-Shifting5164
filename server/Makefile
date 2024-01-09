ifdef CROSS_COMPILE
CC=${CROSS_COMPILE}gcc
else
CC=gcc
endif

all:
	${CC}  -Wno-int-to-pointer-cast -pthread aesdsocket.c -o aesdsocket 

clean:
	-rm aesdsocket 2>/dev/null

strace:
	strace ./aesdsocket

valgrind:
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite --verbose ./aesdsocket
