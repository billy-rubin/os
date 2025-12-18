CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread

BIN = lab23_spin
SRC = main.c list.c thread_funcs.c

.PHONY: all all clean

all: $(BIN)

$(BIN): $(SRC)
	@echo "[SPIN] Building $(BIN)..."
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	@echo "[SPIN] Cleaning..."
	rm -f $(BIN)