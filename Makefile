CC=gcc
CFLAGS=-Wall -Wextra -pedantic -Werror -Warray-bounds
OFLAGS=-O3 
LFLAGS=-lncursesw
DFLAGS=-g -O0 -v -da -Q

OBJ=ste.o fbuffer.o
DEPS=fbuffer.h

ste: $(OBJ)
	$(CC) $(CFLAGS) $(OFLAGS) $(LFLAGS) -o $@ $^

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(OFLAGS) $(LFLAGS) -c -o $@ $<

	

clean:
	rm ste $(OBJ)
