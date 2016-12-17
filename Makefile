# the compiler: gcc for C program
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall

#Name of C File
TARGET = client

#Name of executable you want to create
FINALBUILD = chat

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(FINALBUILD) $(TARGET).c

clean:
	$(RM) $(FINALBUILD)
