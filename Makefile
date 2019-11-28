CC = g++
CCFLAGS	= -Wall

.PHONY: all clean compile compress

all: fs

clean:
	rm *.o FileSystem

compile: FileSystem.cc
	$(CC) $(CCFLAGS) -c FileSystem.cc -o FileSystem.o

fs: FileSystem.o
	$(CC) $(CCFLAGS) -o fs FileSystem.o
	
compress:
	zip fs-sim.zip FileSystem.cc FileSystem.h create_fs Makefile README.md