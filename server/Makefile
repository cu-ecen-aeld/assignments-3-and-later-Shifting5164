ifdef CROSS_COMPILE
CC=${CROSS_COMPILE}gcc
else
CC=gcc
endif

all:
	${CC}  -Wno-int-to-pointer-cast -pthread aesdsocket.c -o aesdsocket 

clean:
	-rm aesdsocket 2>/dev/null
