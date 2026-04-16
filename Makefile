# Little Grandmaster Makefile
CC = gcc
CFLAGS = -O3 -march=native -Wall -std=c11 -Iinclude
LDFLAGS = -lm

SRC = src/main.c \
      src/bitboard.c \
      src/board.c \
      src/moves.c \
      src/search.c \
      src/uci.c \
      src/eval.c \
      src/hash.c

OBJ = $(SRC:.c=.o)
BIN = LittleGrandmaster

.PHONY: all clean profile bench

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN) *.png *.epd *.log

bench: $(BIN)
	./$(BIN) bench

profile: CFLAGS += -pg -g
profile: clean $(BIN)
