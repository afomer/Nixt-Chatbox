#
# Makefile for Unix-Chatbox 
#
CC = gcc
COPT = -O3
CFLAGS = -g -Wall
LDFLAGS = -pthread

all: client server

util.o: libs/util.c libs/util.h
	$(CC) $(CFLAGS) -c util.c

client.o: client.c libs/util.h
	$(CC) $(CFLAGS) -c client.c 

server.o: server.c libs/util.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c server.c 

client: client.o libs/util.o

server: server.o libs/util.o

clean:
	rm -f *~ *.o client core *.tar *.zip *.gzip *.bzip *.gz
