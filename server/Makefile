# Use ?= so the environment can override these
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Wextra
LDFLAGS ?= -lpthread -lrt

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket *.o
