TARGET       = mythread_create
SRCS         = test.c my_thread.c
OBJS         = $(SRCS:.c=.o)

CC           = gcc
RM           = rm -f
CFLAGS       = -g -Wall -Wextra -std=gnu11 -fsanitize=address
LDFLAGS      = -fsanitize=address
LIBS         = -lpthread
INCLUDE_DIR  = .

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c my_thread.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
