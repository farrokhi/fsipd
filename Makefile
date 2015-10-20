PREFIX=/usr/local
INC=-I$(PREFIX)/include
LIB=-L$(PREFIX)/lib -L. -lpidfile
CFLAGS=-Wall -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CC?=cc
AR?=AR
RANLIB?=ranlib

PROGS = fsipd
LIBPIDFILE = libpidfile.a
OBJS = pidfile.o

all: $(LIBPIDFILE) $(PROGS)

$(LIBPIDFILE) : $(OBJS)
	$(AR) rv $(LIBPIDFILE) $?
	$(RANLIB) $(LIBPIDFILE)

fsipd: fsipd.c $(LIBPIDFILE)
	$(CC) $(CFLAGS) $(INC) $(LIB) fsipd.c -o fsipd

clean:
	rm -f *.o a.out core temp.* $(LIBPIDFILE) $(PROGS)
	rm -fr *.dSYM