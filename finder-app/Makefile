# Makefile for building "writer" application


# Variables

#Toolchain Configuration
# CC is the compiler variable used by make. 
# CROSS_COMPILE is a user-supplied prefix. For example:
# If CROSS_COMPILE is not set, CC = gcc (native compiler).
# If you run make CROSS_COMPILE=aarch64-none-linux-gnu-, then CC = aarch64-none-linux-gnu-gcc (cross-compiler).
#CC := $(CROSS_COMPILE)gcc
# Use Buildroot's CC if provided, else default to gcc
CC ?= $(CROSS_COMPILE)gcc

# Compiler Flags
# -Wall: Enables all common warnings.
# -Werror: Treats warnings as errors.
# -g: Includes debug symbols.
# -I.: Adds the current directory to the list of directories to search for header files.
# Add -I<path> if your headers are elsewhere
# CFLAGS := -Wall -Werror -g
#CFLAGS := -Wall -g -I.
CFLAGS += -Wall -g -I.
LDFLAGS +=

# Target and Object Definitions
TARGET := writer
OBJS := writer.o

# Default target
all: $(TARGET)

# Link the final binary
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean target
clean:
	rm -f $(TARGET) *.o

# Phony targets
.PHONY: all clean
