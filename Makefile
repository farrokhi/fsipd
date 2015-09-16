PREFIX=/usr/local
INC=-I$(PREFIX)/include
LIB=-L$(PREFIX)/lib  -lutil
FLAGS=-Wall -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CC?=cc

all: fsipd

fsipd: fsipd.c Makefile 
	$(CC) $(FLAGS) $(INC) $(LIB) fsipd.c -o fsipd

clean:
	rm -f fsipd

