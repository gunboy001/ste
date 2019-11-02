CC=gcc
CFLAGS=-Wall -Wextra -pedantic -Werror -Warray-bounds
OFLAGS=-O3 -ltcmalloc
LFLAGS=-lncursesw
DFLAGS=-g -O0 -v -da -Q

ste: ste.c
	$(CC) $(CFLAGS) $(OFLAGS) $(LFLAGS) -o ste $^
	
dbg: ste.c
	$(CC) $(CFLAGS) $(DFLAGS) $(LFLAGS) -o dbg $^

clean:
	rm ste ste.c.* dbg
