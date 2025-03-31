CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/opt/homebrew/Cellar/openssl@3/3.4.1/include
LDFLAGS = -L/opt/homebrew/Cellar/openssl@3/3.4.1/lib -lcrypto

SRC_DIR = src
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)
EXECUTABLES = $(BIN_DIR)/pignoufs_mmap_sha1 $(BIN_DIR)/pignoufs_corrupt

all: $(EXECUTABLES)

$(BIN_DIR)/pignoufs_mmap_sha1: $(BIN_DIR)/mmap_sha1.o
	$(CC) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/pignoufs_corrupt: $(BIN_DIR)/corrupt.o
	$(CC) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
