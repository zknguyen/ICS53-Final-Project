CC=gcc
CFLAGS=-Iinclude -Wall -Werror -g -Wno-unused 

SSRC=$(shell find src -name '*.c')
DEPS=$(shell find include -name '*.h')

LIBS=-lpthread

all: server

setup:
	mkdir -p bin

server: setup $(DEPS)
	$(CC) $(CFLAGS)  $(SSRC) lib/protocol.o -o bin/zbid_server $(LIBS)
	cp lib/zbid_client bin
	cp lib/auctionroom bin
	
.PHONY: clean

clean:
	rm -rf bin 
