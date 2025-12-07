# Cross-compilation variables
#Toolchain Configuration
# CC is the compiler variable used by make. 
# CROSS_COMPILE is a user-supplied prefix. For example:
# If CROSS_COMPILE is not set, CC = gcc (native compiler).
# If you run make CROSS_COMPILE=aarch64-none-linux-gnu-, then CC = aarch64-none-linux-gnu-gcc (cross-compiler).
# Use Buildroot's CC if provided, else default to gcc

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

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

.PHONY: all clean