/*
 * Socket Interface
 * Copyright (c) 2009, Calvin Grunewald <cgrunewa@umd.edu>
 * $Revision: 1.00 $
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#include <geekos/defs.h>
#include <geekos/ktypes.h>

#define SOCK_DGRAM 0
#define SOCK_STREAM 1

#define AF_INET 2

extern uchar_t INADDR_ANY[4];
extern uchar_t INADDR_BROADCAST[4];

int Socket(uchar_t type, int flags);
int Connect(ulong_t id, ushort_t port, uchar_t ipAddress[4]);
int Accept(ulong_t id, ushort_t * clientPort, uchar_t clientIpAddress[4]);
int Listen(ulong_t id, ulong_t backlog);
int Bind(ulong_t id, ushort_t port, uchar_t ipAddress[4]);
int Receive(ulong_t id, void *buffer, ulong_t bufferSize);
int Send(ulong_t id, void *buffer, ulong_t bufferSize);
int Send_To(ulong_t id, uchar_t * buffer, ulong_t bufferSize, ushort_t port,
            uchar_t ipAddress[4]);
int Receive_From(ulong_t id, uchar_t * buffer, ulong_t bufferSize,
                 ushort_t * port, uchar_t ipAddress[4]);
int Close_Socket(ulong_t id);

#endif /* SOCKET_H_ */
