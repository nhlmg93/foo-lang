.PHONY: all run test clean vendor

CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -I$(VENDOR_DIR)
TARGET = foo
SRC = main.c
VENDOR_DIR = vendor
STB_DS_URL = https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h

all: $(TARGET)

vendor:
	@if [ ! -f $(VENDOR_DIR)/stb_ds.h ]; then \
		mkdir -p $(VENDOR_DIR); \
		curl -L -o $(VENDOR_DIR)/stb_ds.h $(STB_DS_URL); \
		echo "Downloaded stb_ds.h to $(VENDOR_DIR)/"; \
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
