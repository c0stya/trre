CC = c99
CFLAGS = -Wall -Wextra -Wpedantic -o0 -g

all: trre

trre: trre_dft.c
	$(CC) $(CFLAGS) trre_dft.c -o trre

clean:
	rm -f trre
