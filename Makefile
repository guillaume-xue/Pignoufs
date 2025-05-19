CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/opt/homebrew/Cellar/openssl@3/3.4.1/include -Iinclude
LDFLAGS = -L/opt/homebrew/Cellar/openssl@3/3.4.1/lib -lcrypto

SRC_DIR = src
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)

# Define executables and their dependencies
EXECUTABLES = $(BIN_DIR)/pignoufs_mmap_sha1 $(BIN_DIR)/pignoufs_corrupt $(BIN_DIR)/pignoufs
PIGNOUFS_MMAP_SHA1_DEPS = $(BIN_DIR)/mmap_sha1.o
PIGNOUFS_CORRUPT_DEPS = $(BIN_DIR)/corrupt.o
PIGNOUFS_DEPS = $(BIN_DIR)/main.o $(BIN_DIR)/commands.o $(BIN_DIR)/sha1.o

all: $(EXECUTABLES) links

$(BIN_DIR)/pignoufs_mmap_sha1: $(PIGNOUFS_MMAP_SHA1_DEPS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/pignoufs_corrupt: $(PIGNOUFS_CORRUPT_DEPS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/pignoufs: $(PIGNOUFS_DEPS)
	$(CC) $^ -o $@ $(LDFLAGS)

links: $(BIN_DIR)/pignoufs
	ln -sf $(BIN_DIR)/pignoufs mkfs
	ln -sf $(BIN_DIR)/pignoufs ls
	ln -sf $(BIN_DIR)/pignoufs df
	ln -sf $(BIN_DIR)/pignoufs cp
	ln -sf $(BIN_DIR)/pignoufs rm
	ln -sf $(BIN_DIR)/pignoufs lock
	ln -sf $(BIN_DIR)/pignoufs chmod
	ln -sf $(BIN_DIR)/pignoufs cat
	ln -sf $(BIN_DIR)/pignoufs input
	ln -sf $(BIN_DIR)/pignoufs add
	ln -sf $(BIN_DIR)/pignoufs addinput
	ln -sf $(BIN_DIR)/pignoufs fsck
	ln -sf $(BIN_DIR)/pignoufs find
	ln -sf $(BIN_DIR)/pignoufs mount
	ln -sf $(BIN_DIR)/pignoufs grep
	ln -sf $(BIN_DIR)/pignoufs mkdir
	ln -sf $(BIN_DIR)/pignoufs rmdir
	ln -sf $(BIN_DIR)/pignoufs tree

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/commands.o: $(SRC_DIR)/commands.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
	rm -f mkfs ls df cp rm lock chmod cat input add addinput fsck mount find grep mkdir rmdir tree

.PHONY: all clean links