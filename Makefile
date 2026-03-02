CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = foo

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c token.c arena.c lexer.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET)
