CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
DEBUG    = -std=c11 -O0 -g -fsanitize=address,undefined -Wall -Wextra
BIN      = build/logparse
SRC      = src/main.c src/table.c src/normalize.c

all: $(BIN)

$(BIN): $(SRC) src/table.h src/normalize.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC)

debug: $(SRC)
	@mkdir -p build
	$(CC) $(DEBUG) -o build/logparse-debug $(SRC)

test: $(BIN)
	sh tests/run.sh

clean:
	rm -rf build

.PHONY: all debug test clean
