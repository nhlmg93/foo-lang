CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = foo
TEST_TARGET = test_lexer

.PHONY: all clean test

all: $(TARGET)

$(TARGET): main.c token.c arena.c lexer.c
	$(CC) $(CFLAGS) -o $@ main.c

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): test/test_lexer.c
	$(CC) $(CFLAGS) -o $@ test/test_lexer.c

clean:
	rm -f $(TARGET) $(TEST_TARGET)
