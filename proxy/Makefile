TARGET = proxy
SRCS = main.c threadpool/threadpool.c logger/logger.c cache/cache.c

CC=gcc
RM=rm
CFLAGS= -g -Wall -fsanitize=address -fsanitize=leak -Wextra -Werror
LIBS=-lpthread
INCLUDE_DIR="."

all: ${TARGET}

${TARGET}: ${SRCS}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS} ${LIBS} -o ${TARGET}

clean:
	${RM} -f *.o ${TARGET}
