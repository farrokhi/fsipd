PREFIX?=/usr/local
INC=-I$(PREFIX)/include
LIB=-L$(PREFIX)/lib -lpidutil
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CC?=cc

PROGS = fsipd logfile_test
CFILES = fsipd.c logfile.c

all: fsipd

fsipd: $(CFILES)
	$(CC) $(CFLAGS) $(INC) $(LIB) $(CFILES) -o fsipd

test: logfile.c logfile_test.c
	$(CC) $(CFLAGS) $(INC) $(LIB) logfile.c logfile_test.c -o logfile_test

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
