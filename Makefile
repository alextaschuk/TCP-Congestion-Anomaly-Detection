
# Compiler & flags
CC := gcc
CFLAGS := -Wall -Wextra -Iinclude -g -MMD -MP

# Directories
SRC_DIR := src
BUILD_DIR := build

# Binaries (in repo root)
CLIENT_BIN := client_app
SERVER_BIN := server_app

# Source files
COMMON_SRCS := \
	$(SRC_DIR)/utcp/api.c \
	$(SRC_DIR)/utcp/api/conn.c \
	$(SRC_DIR)/utcp/api/data.c \
	$(SRC_DIR)/utcp/api/globals.c \
	$(SRC_DIR)/utils/err.c \
	$(SRC_DIR)/utils/printable.c

CLIENT_SRCS := $(SRC_DIR)/utcp/client.c $(COMMON_SRCS)
SERVER_SRCS := $(SRC_DIR)/utcp/server.c $(COMMON_SRCS)

CLIENT_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

DEPS := $(CLIENT_OBJS:.o=.d) $(SERVER_OBJS:.o=.d)

# Default target
all: $(CLIENT_BIN) $(SERVER_BIN)

# Link client
$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Link server
$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Compile source files to objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(CLIENT_BIN) $(SERVER_BIN)

# Compile commands for clangd or other tools
.PHONY: compile_commands
compile_commands: all
	@mkdir -p $(BUILD_DIR)
	@bear -- $(MAKE)
