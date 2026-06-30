CC = gcc
CFLAGS = -Wall -g
LDLIBS = -lpthread

all: myshell

myshell: myshell.c csapp.c myshell.h csapp.h
	$(CC) $(CFLAGS) -o myshell myshell.c csapp.c $(LDLIBS)

clean:
	rm -f myshell *.o *~

.PHONY: all clean
