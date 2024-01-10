CC?=${CROSS_COMPILE}gcc
LDFLAGS?=-lrt -pthread
CFLAGS?=-Wno-int-to-pointer-cast
DEBUG_OPS?=-o0 -ggdb
TARGET?=aesdsocket

all:
	${CC} ${CFLAGS} ${TARGET}.c -o ${TARGET} ${LDFLAGS} 

debug: 
	${CC} ${CFLAGS} ${DEBUG_OPS} ${TARGET}.c -o ${TARGET} ${LDFLAGS}

run: clean debug
	-killall aesdsocket
	./${TARGET}

clean:
	-rm -- ${TARGET} 2>/dev/null

strace: clean debug 
	strace ./${TARGET}

valgrind: clean debug 
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite --verbose --log-file=valgrind-out.txt ./${TARGET}

test: clean debug
	cppcheck ./${TARGET}.c
	clang-check-12 ./${TARGET}.c
