# Cross-compilation variables
CC ?= gcc          # default compiler, can be overridden for cross-compiling
CFLAGS ?= -Wall -Wextra -g

# Source and target
SRC := aesdsocket.c
TARGET := aesdsocket

# Default target
all: $(TARGET)

# Compile target in current directory
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Clean target
clean:
	rm -f $(TARGET)

.PHONY: all clean default
default: all