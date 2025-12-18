CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread

BIN = lab23_rwlock
SRC = main.c list.c thread_funcs.c

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	@echo "[RWLOCK] Building $(BIN)..."
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	@echo "[RWLOCK] Cleaning..."
	rm -f $(BIN)