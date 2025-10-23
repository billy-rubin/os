TARGET = thread-example-2
SRCS = 1-2/f.c

CC=gcc
RM=rm
CFLAGS= -g -Wall -fsanitize=address
LIBS=-lpthread
INCLUDE_DIR="."

all: ${TARGET}

${TARGET}: ${SRCS}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS} ${LIBS} -o ${TARGET}

clean:
	${RM} -f *.o ${TARGET}
