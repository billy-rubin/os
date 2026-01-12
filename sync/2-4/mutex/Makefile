CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread

BIN = mutex
SRC = main.c mutex.c

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	@echo "[MUTEX] Building $(BIN)..."
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	@echo "[MUTEX] Cleaning..."
	rm -f $(BIN)