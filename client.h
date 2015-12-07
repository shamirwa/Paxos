#include "message.h"
#include <sys/time.h>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <fstream>
#include <map>

#define XINU 0
#define VM 1
#define paramDebug 0
#define errorDebug 0
#define CLIENT_UPDATE_MSG 1

using namespace std;

void convertNTOHByteOrder(Client_Update* message);

void convertHTONByteOrder(Client_Update* message);

bool verifyFileName(string fileName, int *count);

bool verifyPortNumber(long portNumber);

void printUsage();

void error(const char msg[]);

