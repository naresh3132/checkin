/* 
 * File:   Thread_TCP.h
 * Author: muditsharma
 *
 * Created on March 1, 2016, 6:04 PM
 */

#include "CommonFunctions.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"
#include "nsecd_exch_structs.h"
#include "nsecm_constants.h"
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
#include<netinet/tcp.h>
#include <sys/select.h>

void set_nonblock(int socket);
void *get_in_addr(struct sockaddr *sa);


void StartTCP(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer, int _nMode, const char* pcIpaddress_init, int nPortNumber_init,const char* pcIpaddress, int nPortNumber, int nSendBuff,int nRecvBuff,int iTCPCore,dealerInfoMap*  dealerinfoMap);

int CreateSocket(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer);
int CreateNonBlockSocket(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer, const char* pcIpaddress_Init, int nPortNumber_Init, const char* pcIpaddress, int nPortNumber, int nSendBuff,int nRecvBuff ,dealerInfoMap*  dealerinfoMap);






