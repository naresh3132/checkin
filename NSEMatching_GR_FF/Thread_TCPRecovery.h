/* 
 * File:   Thread_TCPRecovery.h
 * Author: sneha
 *
 * Created on August 2, 2016, 2:48 PM
 */

#ifndef THREAD_TCPRECOVERY_H
#define	THREAD_TCPRECOVERY_H

#include "CommonFunctions.h"
#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netdb.h>

void StartTCPRecovery(const char* pcIpaddress, int nPortNumber, int iTCPRcvryCore, short streamID, int iMaxWriteAttempt);

int CreateTCPSocket(const char* pcIpaddress, int nPortNumber, short streamID);

int SendPacket(int FD, char* msg, int msgLen);

#endif	/* THREAD_TCPRECOVERY_H */

