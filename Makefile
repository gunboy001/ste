CC=gcc
CFLAGS=-Wall -Wextra -pedantic -Werror
OFLAGS=-O3
LFLAGS=-lncurses

ste: ste.c
	$(CC) $(CFLAGS) $(OFLAGS) $(LFLAGS) -o ste $^