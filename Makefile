CC=cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
#CFLAGS_DEBUG = -Wall -Wextra -Wpedantic -o0 -g

all: nft dft

nft: trre_nft.c
	$(CC) $(CFLAGS) trre_nft.c -o trre

dft: trre_dft.c
	$(CC) $(CFLAGS) trre_dft.c -o trre_dft

clean:
	rm -f trre
