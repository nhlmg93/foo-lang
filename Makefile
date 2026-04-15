.PHONY: all run test clean vendor

CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -I$(VENDOR_DIR)
TARGET = foo
SRC = main.c
VENDOR_DIR = vendor
ARENA_URL = https://raw.githubusercontent.com/nhlmg93/stb_headers/master/arena.h

all: $(TARGET)

vendor:
	@if [ ! -d $(VENDOR_DIR) ]; then \
		mkdir -p $(VENDOR_DIR); \
		curl -L -o $(VENDOR_DIR)/arena.h $(ARENA_URL); \
		echo "Downloaded arena.h to $(VENDOR_DIR)/"; \
	elif [ ! -f $(VENDOR_DIR)/arena.h ]; then \
		curl -L -o $(VENDOR_DIR)/arena.h $(ARENA_URL); \
		echo "Downloaded arena.h to $(VENDOR_DIR)/"; \
	fi

$(TARGET): vendor $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) -I$(VENDOR_DIR)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	@echo "Running tests..."
	./$(TARGET) test.foo


clean:
	rm -f $(TARGET)

clean-all: clean
	rm -rf $(VENDOR_DIR)
