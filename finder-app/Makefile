ifdef CROSS_COMPILE
CC=${CROSS_COMPILE}gcc
else
CC=gcc
endif

all:
	${CC} writer.c -o writer

clean:
	-rm writer 2>/dev/null
