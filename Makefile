PREFIX?=/usr/local
INC=-I$(PREFIX)/include
LIB=-L$(PREFIX)/lib -lutil
CFLAGS=-g -Wall -Wextra -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CC?=cc

SOURCES=$(wildcard src/**/*.c src/*.c)

all: fsipd

fsipd: $(SOURCES) Makefile 
	$(CC) $(CFLAGS) $(INC) $(LIB) src/fsipd.c -o bin/fsipd

clean:
	rm -f bin/* 

