CC = gcc
CFLAGS = -Wall -Iinclude

src = $(wildcard src/*.c)
obj = $(src:.c=.o)

all: imap-client

imap-client: $(obj)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY:
clean:
	rm src/*.o



