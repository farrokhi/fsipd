CC?=cc
PREFIX?=/usr/local
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CPPFLAGS=-I./libpidutil -I$(PREFIX)/include
LDFLAGS=-L$(PREFIX)/lib -L./libpidutil -lpidutil -lpthread
LDLIB=-lpidutil -lpthread

SUBDIRS = libpidutil
PROGS = fsipd logfile_test
CFILES = fsipd.c logfile.c

all: $(SUBDIRS) fsipd

fsipd: $(CFILES)

.PHONY: $(SUBDIRS)

$(SUBDIRS):
	git submodule update --init $@
	$(MAKE) -C $@ all

test: logfile.c logfile_test.c
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) logfile.c logfile_test.c -o logfile_test

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
	$(MAKE) -C libpidutil clean
