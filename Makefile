PREFIX?=/usr/local
INC=-I$(PREFIX)/include
LIB=-L$(PREFIX)/lib -lpidutil
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CC?=cc

PROGS = fsipd

all: $(PROGS)

fsipd: fsipd.c
	$(CC) $(CFLAGS) $(INC) $(LIB) fsipd.c -o fsipd

clean:
	rm -f *.o *.a a.out core temp.* $(LIBPIDFILE) $(PROGS)
	rm -fr *.dSYM
