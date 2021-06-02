#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include "linkedList.h"
#include "protocol.h"

#define BUFFER_SIZE 1024
#define SA struct sockaddr

#define USAGE_MSG "./bin/zbid_server [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME\n\n-h                  Displays this help menu, and returns EXIT_SUCCESS.\n-j N                Number of job threads. If option not specified, default to 2.\n-t M                M seconds between time ticks. If option not specified, default is to wait on\ninput from stdin to indicate a tick.\nPORT_NUMBER         Port number to listen on.\nAUCTION_FILENAME    File to read auction item information from at the start of the server."

void run_server(int server_port);

#endif
