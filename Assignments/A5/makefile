LIBNAME = libmsocket.a
LIBDIR = /usr/lib
INCLUDEDIR = /usr/include

install: library
	mv $(LIBNAME) $(LIBDIR)
	cp graph.h $(INCLUDEDIR)
	rm -f mysocket.o

library: mysocket.o
	ar rcs $(LIBNAME) mysocket.o

mysocket.o: mysocket.h mysocket.c
	gcc -c mysocket.c -lpthread
