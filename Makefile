CC=gcc
CFLAGS=-Wall -Wextra -pedantic -Werror
OFLAGS=-O3
LFLAGS=-lncursesw -ltcmalloc

ste: ste.c
	$(CC) $(CFLAGS) $(OFLAGS) $(LFLAGS) -o ste $^
