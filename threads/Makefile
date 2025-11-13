TARGET = thread-example-5
SRCS = 1-5/b.c

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
