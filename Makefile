.PHONY: all run clean clean-all vendor

CC = gcc
CFLAGS = -std=c23 -Wall -Wextra
TARGET = foo
SRC = main.c
INCLUDED_SRC = lib/arena.c lib/ast.c lib/lexer.c lib/parser.c \
	lib/interpreter/evaluator.c lib/compiler/bytecode.c \
	lib/compiler/compiler.c lib/compiler/vm.c
VENDOR_DIR = vendor
STB_DS_HEADER = $(VENDOR_DIR)/stb_ds.h
STB_DS_URL = https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h

all: $(TARGET)

vendor: $(STB_DS_HEADER)

$(STB_DS_HEADER):
	mkdir -p $(VENDOR_DIR)
	curl -fL -o $(STB_DS_HEADER) $(STB_DS_URL)

$(TARGET): $(SRC) $(INCLUDED_SRC) $(STB_DS_HEADER)
	$(CC) $(CFLAGS) -I$(VENDOR_DIR) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

clean-all: clean
	rm -rf $(VENDOR_DIR)
