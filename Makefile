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
TEST_UNIT_PROGS = tests/unit/test_chomp

.PHONY: $(SUBDIRS) get-deps test check test-unit test-integration clean-tests

all: get-deps $(SUBDIRS) fsipd

fsipd: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LDLIBS) -o fsipd

get-deps:
	git submodule update --init

$(SUBDIRS):
	$(MAKE) -C $@ all

logfile_test: logfile.h logfile.c logfile_test.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) logfile.c logfile_test.c -o logfile_test

tests/unit/test_chomp: tests/unit/test_chomp.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/unit/test_chomp.c -o tests/unit/test_chomp

test-unit: $(TEST_UNIT_PROGS) logfile_test
	@echo "Running unit tests..."
	@for test in $(TEST_UNIT_PROGS) ./logfile_test; do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

test-integration: fsipd
	@echo "Running integration tests..."
	@./tests/integration/test_daemon.sh

test: test-unit test-integration

check: test

install:
	install -D $(TARGET) $(BINDIR)/$(TARGET)

clean-tests:
	rm -f $(TEST_UNIT_PROGS)
	rm -f /tmp/fsipd_test_*

clean: clean-tests
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM
	$(MAKE) -C libpidutil clean
