CC=gcc
CFLAGS=-I.
DEPS = myfsck.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

myfsck: myfsck.o readwrite.o 
	gcc -o myfsck myfsck.o readwrite.o -I.

readwrite.o:
	gcc -c readwrite.c 	

myfsck.o:
	gcc -c myfsck.c   

clean:
	rm -f myfsck myfsck.o readwrite.o
