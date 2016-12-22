#
# Makefile for Unix-Chatbox 
#
CC = gcc
COPT = -O3
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: client server

util.o: util.c util.h
	$(CC) $(CFLAGS) -c util.c

client.o: client.c util.h
	$(CC) $(CFLAGS) -c client.c

server.o: server.c util.h
	$(CC) $(CFLAGS) -c server.c

client: client.o util.o

server: server.o util.o

clean:
	rm -f *~ *.o client core *.tar *.zip *.gzip *.bzip *.gz
