CC=cc
#CC = c99
#CFLAGS = -Wall -Wextra -Wpedantic -o0 -g
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -o2

all: nft dft

dft: trre_dft.c
	$(CC) $(CFLAGS) trre_dft.c -o trre_dft

nft: trre_nft.c
	$(CC) $(CFLAGS) trre_nft.c -o trre_nft


clean:
	rm -f trre
