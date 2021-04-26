CC?=
PREFIX?=/usr/local
BINDIR=$(PREFIX)/bin
CPPFLAGS=-I./libpidutil -I$(PREFIX)/include
CFLAGS=-Wall -Werror -Wextra -g -std=c17 -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CFLAGS+=$(CPPFLAGS)
LDFLAGS=-L$(PREFIX)/lib -L./libpidutil
LDLIBS=-lpidutil -lpthread

TARGET=fsipd

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

install:
	install -D $(TARGET) $(BINDIR)/$(TARGET)

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
	$(MAKE) -C libpidutil clean
