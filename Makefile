CC?=cc
PREFIX?=/usr/local
INC=-I./libpidutil -I$(PREFIX)/include
LDFLAGS=-L./libpidutil -L$(PREFIX)/lib -lpidutil -lpthread
CFLAGS=-Wall -Wextra -g -O2 -static -pipe -funroll-loops -ffast-math -fno-strict-aliasing

SUBDIRS = libpidutil
PROGS = fsipd logfile_test
CFILES = fsipd.c logfile.c

all: $(SUBDIRS) fsipd

fsipd: $(CFILES)
	$(CC) $(CFLAGS) $(INC) $(CFILES) $(LDFLAGS) -o fsipd

.PHONY: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all

test: logfile.c logfile_test.c
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) logfile.c logfile_test.c -o logfile_test

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
	$(MAKE) -C libpidutil clean
