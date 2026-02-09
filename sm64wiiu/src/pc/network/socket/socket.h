#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include "pc/network/network.h"

typedef int SOCKET;
extern char gGetHostName[];

SOCKET socket_initialize(void);
void socket_shutdown(SOCKET socketHandle);

#endif
