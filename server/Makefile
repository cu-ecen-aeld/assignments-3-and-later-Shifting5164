CC?=${CROSS_COMPILE}gcc
LDFLAGS?=-lrt -pthread
CFLAGS?=-Wno-int-to-pointer-cast
DEBUG_OPS?=-o0 -ggdb
TARGET?=aesdsocket

all:
	${CC} ${CFLAGS} ${TARGET}.c -o ${TARGET} ${LDFLAGS} 

debug: 
	${CC} ${CFLAGS} ${DEBUG_OPS} ${TARGET}.c -o ${TARGET} ${LDFLAGS}

run: kill clean debug
	./${TARGET}

kill:
	-killall aesdsocket

clean:
	-rm -- ${TARGET} 2>/dev/null

strace: kill clean debug 
	strace ./${TARGET}

valgrind: kill clean debug 
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite --verbose --log-file=valgrind-out.txt ./${TARGET}

test: kill clean debug
	cppcheck ./${TARGET}.c
	clang-check-12 ./${TARGET}.c
