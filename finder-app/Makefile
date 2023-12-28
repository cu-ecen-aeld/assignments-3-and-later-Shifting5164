CC:=gcc
ifdef CROSS_COMPILE
CC:=aarch64-linux-gnu-gcc
endif


all:
	${CC} writer.c -o writer

clean:
	rm writer
