CC?=
PREFIX?=/usr/local
CPPFLAGS=-I./libpidutil -I$(PREFIX)/include
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CFLAGS+=$(CPPFLAGS)
LDFLAGS=-L$(PREFIX)/lib -L./libpidutil
LDLIBS=-lpidutil -lpthread

SUBDIRS = libpidutil
PROGS = fsipd logfile_test
OBJ = logfile.o fsipd.o

.PHONY: $(SUBDIRS) get-deps

all: get-deps $(SUBDIRS) fsipd

fsipd: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LDLIBS) -o fsipd

get-deps:
	git submodule update --init

$(SUBDIRS):
	$(MAKE) -C $@ all

test: logfile.h logfile.c logfile_test.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) logfile.c logfile_test.c -o logfile_test

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
	$(MAKE) -C libpidutil clean
