/* 
 * File:   TCPHandler.h
 * Author: NareshRK
 *
 * Created on September 3, 2018, 10:03 AM
 */

#ifndef TCPHANDLER_H
#define	TCPHANDLER_H

#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
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

#include "spsc_atomic1.h"
#include "AllStructure.h"
#include "DealerStructure.h"
#include "nsecm_constants.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"
#include "AllMsgHandling.h"
#include "BookBuilder.h"



class TCPCONNECTION
{
  int32_t nSendBuff;
  int32_t nRecvBuff;
public:
  
  bool HandleTCPConnections(ConfigFile& oConfigStore,dealerToIdMap& DealToIdMap, dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap, TOKENSUBSCRIPTION& oTokenSub);
  int  CreateTCPConnection(const char* pcIpaddress, int32_t nPortNumber);
  int  CreateTCPClient(const char* pcIpaddress, int32_t nPortNumber, int RecvBuf);
  bool TCPProcessHandling(int iFDinterface, int iFDfo, int iFDcm, dealerToIdMap& DealToIdMap, dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap, TOKENSUBSCRIPTION& oTokenSub,ConfigFile &oConfigStore, connectionMap &connMap, ERRORCODEINFO [], ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain);
  
  int SendData(int iSockFD, char *cSendData, int iMsgLen, CONNINFO *);
  bool RecvData(char* pchBuffer, int32_t& iBytesRemaining, int32_t iSockId, CONNINFO* pConn);
  
  bool closeFD(int iSockFD);
  
};




//void StartTCP( int _nMode, const char* pcIpaddress_init, int nPortNumber_init,const char* pcIpaddress, int nPortNumber, int nSendBuff,int nRecvBuff,int iTCPCore,dealerInfoMap*  dealerinfoMap);


#endif	/* TCPHANDLER_H */

