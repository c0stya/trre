CC = c99
CFLAGS = -Wall -Wextra -Wpedantic -o0 -g

all: trre

trre: trre.c
	$(CC) $(CFLAGS) trre.c -o trre

clean:
	rm -f trre
