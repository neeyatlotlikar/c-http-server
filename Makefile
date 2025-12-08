CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS :=

SRC_DIR := src
BUILD_DIR := build

SRCS := $(SRC_DIR)/server_select.c
OBJS := $(BUILD_DIR)/server_select.o
BIN  := $(BUILD_DIR)/server

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

all: $(BUILD_DIR) $(BIN)

$(BUILD_DIR)/server_select.o: $(SRC_DIR)/server_select.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

