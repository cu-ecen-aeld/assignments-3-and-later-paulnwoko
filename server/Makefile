# Cross-compilation variables
CC ?= gcc          # default compiler, can be overridden for cross-compiling
CFLAGS ?= -Wall -Wextra -g

# Source and target
SRC := aesdsocket.c
TARGET := aesdsocket
OUTDIR := server
OUT := $(TARGET)

# Default target
all: $(OUT)

# Compile
$(OUT): $(SRC) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^

# Ensure output directory exists
$(OUTDIR):
	mkdir -p $(OUTDIR)

# Clean
clean:
	rm -f $(OUT)

# Phony targets
.PHONY: all clean default

# Default is same as all
default: all