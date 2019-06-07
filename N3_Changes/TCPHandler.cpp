#include <unordered_map>

#include "TCPHandler.h"
//#include "main.cpp"


char logBufTCP[400];

int iMaxWriteAttempt_Global = 300; //provide it in config

extern CONNINFO *gpConnFO;
extern CONNINFO *gpConnCM;

bool TCPCONNECTION::closeFD(int iSockFD)
{
  close(iSockFD);
  return true;
}


bool TCPCONNECTION::RecvData(char* pchBuffer, int32_t& iBytesRemaining, int32_t iSockId, CONNINFO* pConn)
{
    int32_t iBytesRcvd = 0;

    iBytesRcvd = recv(iSockId , pchBuffer+iBytesRemaining , 3000 , 0);
    
    if( (iBytesRcvd < 0) && ((errno == EAGAIN) ||(errno == EWOULDBLOCK)) )
    { 
        return(false);
    }
    else if(iBytesRcvd > 0) 
    { 
//      std::cout<<"iSockId:"<<iSockId<<"|ByteRecieved::"<<iBytesRcvd<<std::endl;
      iBytesRemaining += iBytesRcvd;
      return(true);
    }
    else
    {
      snprintf(logBufTCP,500,"TCP|ME Disconnection Received");
      Logger::getLogger().log(DEBUG, logBufTCP);
      pConn->status = DISCONNECTED;
      exit(1);
      return(false);      
    }
}


int TCPCONNECTION::SendData(int FD, char *msg, int msgLen, CONNINFO* pConnInfo)
{
  int bytesSent = 0;
  int retVal = 0;
  int dataSent = 0;
  bool bExit= false;
  int iWriteAttempt = 0;  
   
  while (bExit == false)
  {
    bytesSent = send(FD,(char*) (msg + dataSent), (msgLen - dataSent), MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytesSent != msgLen)
    {
      snprintf(logBufTCP,500,"TCP Send|FD %d|SendToClient|Partial data sent|%d|%d|%d",FD, bytesSent, msgLen, errno);
      Logger::getLogger().log(DEBUG, logBufTCP);
    }
    if (bytesSent == -1)
    {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
        usleep(10000);
        if (iMaxWriteAttempt_Global <= iWriteAttempt)
        {
         snprintf (logBufTCP, 500, "TCP Send|FD %d|Disconnecting slow client|Error %d", FD, errno);
         Logger::getLogger().log(DEBUG, logBufTCP);
//         Logger::getLogger().log(DEBUG, logBuf);
         pConnInfo->status = DISCONNECTED;
         bExit = true;  
         retVal = -1;
        }
        iWriteAttempt++;
      }
      else
      {  
        snprintf(logBufTCP, 500, "TCP Send|FD %d|Issue received|Drop connection| last error code|%d", FD, errno);
        Logger::getLogger().log(DEBUG, logBufTCP);
//        Logger::getLogger().log(DEBUG, logBufTCP);
        pConnInfo->status = DISCONNECTED;
        bExit = true;  
        retVal = -1;
      }	
   }
   else
   {
      iWriteAttempt = 0;
      dataSent += bytesSent;
      if (dataSent == msgLen) 
      {
         bExit = true;
      } 
   }
  } 
    
   
  return bytesSent;
}

int TCPCONNECTION::CreateTCPConnection(const char* pcIpaddress, int32_t nPortNumber)
{
  
    struct addrinfo hints;
    struct addrinfo *servinfo; 
    int status, maxfd, sock;

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;     
    
    char lcPortNumber[16 + 1] = {0};
    memset(&lcPortNumber, 0, sizeof(lcPortNumber));
    
    snprintf(lcPortNumber, sizeof(lcPortNumber), "%d", nPortNumber);
        
    if ((status = getaddrinfo(pcIpaddress, lcPortNumber, &hints, &servinfo)) != 0) 
    {
      snprintf(logBufTCP,400,"TCP|getaddrinfo error: %s", gai_strerror(status));
      Logger::getLogger().log(DEBUG, logBufTCP);
      
      sleep(5);
      exit(1);
    }

    
    sock =  socket(AF_INET, SOCK_STREAM, 0);    
    if (sock < 0) {
        
      snprintf(logBufTCP,400,"TCP| Server socket failure: %d", errno);
      Logger::getLogger().log(DEBUG, logBufTCP);
      sleep(5);
      exit(1);
    }
    snprintf(logBufTCP,400,"TCP|UI Server socket:: %d", sock);
    Logger::getLogger().log(DEBUG, logBufTCP);
    sockaddr_in	lcServerAddr;
    lcServerAddr.sin_family = AF_INET;
    lcServerAddr.sin_port = htons((short)nPortNumber);
    inet_aton(pcIpaddress, &(lcServerAddr.sin_addr));    
    
    if(bind(sock, (sockaddr*)&lcServerAddr, sizeof(sockaddr_in)) < 0)
    {
      snprintf(logBufTCP,400,"TCP| Bind error %d %s %s %d %s", errno , strerror(errno), pcIpaddress, nPortNumber, lcPortNumber);
      Logger::getLogger().log(DEBUG, logBufTCP);
      exit(1);
    }      
    
    int yes=1;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {

      snprintf(logBufTCP,400,"TCP| setsockopt error %d ", errno);
      Logger::getLogger().log(DEBUG, logBufTCP);
      sleep(5);
      exit(1);
    }    

    freeaddrinfo(servinfo);
    
    if(listen(sock, 10) < 0)
    {
      snprintf(logBufTCP,400,"TCP| Listen error %d ", errno);
      Logger::getLogger().log(DEBUG, logBufTCP);
      sleep(5);
      exit(1);
    }
    
    
    snprintf(logBufTCP,400,"TCP|UI SERVER|IP %s|Port %d|Ready to accept multiple connections..!!",pcIpaddress, nPortNumber);
    Logger::getLogger().log(DEBUG, logBufTCP);
    
    return sock;

}

int TCPCONNECTION::CreateTCPClient(const char* pcIpaddress, int32_t iPortNumber, int iRecvBuf)
{
  
  struct sockaddr_in server;
  int lnRecvBufSize = iRecvBuf;
  int iSockId = socket(AF_INET , SOCK_STREAM , 0);
  if(iSockId == -1)
  {
    sprintf(logBufTCP, "TCP|Client| Could not create socket|Error %d|%s|", errno, strerror(errno));
    Logger::getLogger().log(DEBUG, logBufTCP);
    return -1;
  }
	
  sprintf(logBufTCP,  "TCP|Client|Socket Created %d|IP %s|Port %d",iSockId,pcIpaddress,iPortNumber); 
  Logger::getLogger().log(DEBUG, logBufTCP);
	
  
  if(0 != setsockopt(iSockId, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))	
  {
    sprintf(logBufTCP, "TCP|Client| setsockopt failed for setting RecvBufSize"); 
    Logger::getLogger().log(DEBUG, logBufTCP);
    return -1;
  }	 
  
  sprintf(logBufTCP, "TCP|Client| Attempting Connection on | %s | %d |ConnectToExch", pcIpaddress, iPortNumber); 
  Logger::getLogger().log(DEBUG, logBufTCP);
  
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(pcIpaddress);
  server.sin_port = htons(iPortNumber);
	
  if (connect(iSockId , (struct sockaddr *)&server , sizeof(server)) < 0)
  {
    sprintf(logBufTCP, "TCP|Client| connect() failed:[%d][%s]", errno, strerror(errno)); 
    Logger::getLogger().log(DEBUG, logBufTCP);
    return -1;
  }
   
  int nReuseAddr = 1;
  if(setsockopt(iSockId, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuseAddr, sizeof(nReuseAddr)) < 0)
  {
    sprintf(logBufTCP, "TCP|Client| Error while setting REUSEADDR.");
    Logger::getLogger().log(DEBUG, logBufTCP);
    close(iSockId);
    return -1;
  }
	
  fcntl(iSockId, F_SETFL, O_NONBLOCK);    //We are using Non Blocking Sockets
  char flag = 0;
  setsockopt(iSockId, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof flag);

  sprintf(logBufTCP, "TCP|Client| Connected to Exch |%s:%d|SocketId %d|ConnectToExch", pcIpaddress, iPortNumber, iSockId);
  Logger::getLogger().log(DEBUG, logBufTCP);
  
  return iSockId;
}

bool TCPCONNECTION::TCPProcessHandling(int iFDui, int iFDfo, int iFDcm, dealerToIdMap &DealToIdMap ,dealerInfoMap &DealInfoMap, ContractInfoMap &ContInfoMap, TOKENSUBSCRIPTION &oTokenSub,ConfigFile &oConfigStore, connectionMap& connMap, ERRORCODEINFO loErrInfo[], ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain)
{
  
  struct sockaddr_in  their_addr;
  socklen_t their_addr_size;
  int16_t *ipLength = new int16_t;
  int16_t iLength;
  int iRetVal;
  fd_set read_flags,write_flags, tempset;
  struct timeval waitd = {0, 0};
  int sel,new_sd;
  int maxfd = iFDui;
  char tempBuf[3000] = {0};
//  connectionMap connMap;
  connectionItr connItr;    
  std::pair<connectionItr, bool> connInsertRet;
  
  DATA_RECEIVED SendData;
  DATA_RECEIVED Recv_Data;
  
  int numSent;
  int numRead;
  int lnRecvBufSize  = oConfigStore.iRecvBuff;
  int lnsendbufSize  = oConfigStore.iSendBuff;
  
  their_addr_size = sizeof(their_addr);
  
  FD_ZERO(&read_flags);
  FD_ZERO(&write_flags);
  FD_SET(iFDui, &read_flags);
  
  
  while(1)
  {
    memcpy(&tempset,&read_flags,sizeof(tempset));
    sel = select(maxfd+1, &tempset, &write_flags, (fd_set*)0, &waitd);
    if (sel <= 0)
    {
      if(sel == -1)
      {
        perror("select()");
      }
    }
    else
    {
      
      if(FD_ISSET(iFDui, &tempset))
      {
        new_sd = accept(iFDui, (struct sockaddr*)&their_addr, &their_addr_size);
        if( new_sd < 0) 
        {
           snprintf(logBufTCP,400,"TCP_UI|Accept error %d", errno);
           Logger::getLogger().log(DEBUG, logBufTCP);
//           Logger::getLogger().log(DEBUG, logBufTCP); 
        }
        else
        {
          if(0 != setsockopt(new_sd, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))        
          {
            snprintf(logBufTCP,400,"TCP_UI|setsockopt failed for setting RecvBufSize");
            Logger::getLogger().log(DEBUG, logBufTCP);
//              Logger::getLogger().log(DEBUG, logBufTCP); 

          }   
          else
          {
            snprintf(logBufTCP,400,"TCP_UI|Receive Buffer size: %d", lnRecvBufSize);
            Logger::getLogger().log(DEBUG, logBufTCP);
//              Logger::getLogger().log(DEBUG, logBufTCP); 
          }
          if(0 != setsockopt(new_sd, SOL_SOCKET, SO_SNDBUF, &lnsendbufSize, sizeof(lnsendbufSize)))     
          {
            snprintf(logBufTCP,400,"TCP_UI|Setsockopt failed for setting SendBufSize");
            Logger::getLogger().log(DEBUG, logBufTCP);
//              Logger::getLogger().log(DEBUG, logBufTCP); 
          }  
          else
          {
            snprintf(logBufTCP,400,"TCP_UI|Send Buffer size: %d", lnsendbufSize);
            Logger::getLogger().log(DEBUG, logBufTCP);
//              Logger::getLogger().log(DEBUG, logBufTCP); 
          }
          
          int flag = 1;
          if(0 > setsockopt(new_sd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag))) 
          {
            snprintf(logBufTCP,400,"TCP_UI|Setsockopt failed for TCP_NODELAY errno %d|%s", errno, strerror(errno));
            Logger::getLogger().log(DEBUG, logBufTCP);
//              Logger::getLogger().log(DEBUG, logBufTCP); 
          }

          FD_SET(new_sd, &read_flags);
          FD_SET(new_sd, &write_flags);

          CONNINFO* connObj = new (CONNINFO);
          memcpy(connObj->IP, inet_ntoa(their_addr.sin_addr), sizeof(connObj->IP));
          connInsertRet = connMap.insert(std::pair<int, CONNINFO*>(new_sd, connObj));
          if (connInsertRet.second == false)
          {
            shutdown(new_sd, SHUT_RDWR);
            close(new_sd);

            snprintf(logBufTCP,400,"TCP_UI|FD %d|Failed to insert in map", new_sd);
            Logger::getLogger().log(DEBUG, logBufTCP);
//            Logger::getLogger().log(DEBUG, logBufTCP); 

            FD_CLR(new_sd, &read_flags);
            FD_CLR(new_sd, &write_flags);
            delete (connObj);
          }
          else
          {
            snprintf(logBufTCP,400,"TCP_UI|FD %d|%s|Successful Connection!", new_sd,connObj->IP);
            Logger::getLogger().log(DEBUG, logBufTCP);
//            Logger::getLogger().log(DEBUG, logBufTCP); 
            maxfd = (maxfd < new_sd)? new_sd:maxfd;
          }
          FD_CLR(iFDui, &tempset);
        }  
      }
      
      for(int fdSocket=1; fdSocket< maxfd+1 ; fdSocket++)
      {
        //socket ready for reading
        if(FD_ISSET(fdSocket, &tempset))
        {
          connItr = connMap.find(fdSocket);
          if (connItr == connMap.end())
          {
              continue;
          }
          CONNINFO* pConnInfo = connItr->second;

          do
          {
            numRead = recv(fdSocket ,(char*) ((pConnInfo->msgBuffer)+(pConnInfo->msgBufSize)), (3000 - (pConnInfo->msgBufSize)), 0);

          }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
          if(numRead == 0)
          {
            auto it = DealInfoMap.find(pConnInfo->dealerID);
            if(it != DealInfoMap.end())
            {
              DEALERINFO* pDealInfo = it->second;
              pDealInfo->iStatus = LOGGED_OFF;
            }
            
            shutdown(fdSocket, SHUT_RDWR);
            close(fdSocket);

            snprintf(logBufTCP,400,"TCP_UI|FD %d|Dealer %d|Disconnection Received",fdSocket,pConnInfo->dealerID); 
            Logger::getLogger().log(DEBUG, logBufTCP);
            FD_CLR(fdSocket, &read_flags);
            FD_CLR(fdSocket, &write_flags);
            if (fdSocket == maxfd)
            {
              while (FD_ISSET(maxfd, &read_flags) == false)
                maxfd -= 1;                    
            }
            CanOnLogOut(pConnInfo->dealerID, iFDfo, iFDcm,oConfigStore);
            delete (pConnInfo);
            connMap.erase(fdSocket);
            
          }

          if(numRead > 0)
          {   
            SendData.MyFd = fdSocket;
            SendData.ptrConnInfo = pConnInfo;
            pConnInfo->msgBufSize = (pConnInfo->msgBufSize) + numRead;

            int packets = 0;
            short exitloop = 0; 
            int tapheaderlength  = 0, dataprocessed = 0;

            while(exitloop == 0)
            {
               DEALER::MSG_HEADER *tapHdr=(DEALER::MSG_HEADER *)pConnInfo->msgBuffer; 

               tapheaderlength = tapHdr->iMsgLen;

//               std::cout << " tapheaderlength = tapHdr->iMsgLen  " <<tapheaderlength << " tapHdr->iTransCode " << tapHdr->iTransCode  << " msgBuff size "<<(pConnInfo->msgBufSize)<< std::endl;

               if(pConnInfo->msgBufSize < tapheaderlength  || tapheaderlength == 0 )
               {
                  exitloop = 1;                                  
               }
               else
               {
                  memcpy(SendData.msgBuffer, pConnInfo->msgBuffer, tapheaderlength); 
                  packets++;
                  int dequeFailureAttempts = 0;

                  if(ProcessRecvUIMessage( SendData, DealToIdMap ,DealInfoMap, ContInfoMap, oTokenSub, oConfigStore, iFDcm, iFDfo, loErrInfo, std::ref(Inqptr_MainToFF), std::ref(Inqptr_FFToMain)) == -1)
                  {
                    if(pConnInfo->status == DISCONNECTED)
                    {
                      shutdown(fdSocket, SHUT_RDWR);
                      close(fdSocket);
                        
                      FD_CLR(fdSocket, &read_flags);
                      FD_CLR(fdSocket, &write_flags);
                      if (fdSocket == maxfd)
                      {
                        while (FD_ISSET(maxfd, &read_flags) == false)
                          maxfd -= 1;                    
                      }
                      delete (pConnInfo);
                      connMap.erase(fdSocket);

                      snprintf(logBufTCP,400,"TCP_UI|FD %d|Disconnected due to Error",fdSocket); 
                      Logger::getLogger().log(DEBUG, logBufTCP);
                      
                      break; 
                    }
                    else if(pConnInfo->status == DISCONNECT_CLIENT)
                    {
                      auto it = DealInfoMap.find(pConnInfo->dealerID);
                      if(it != DealInfoMap.end())
                      {
                        DEALERINFO* pDealInfo = it->second;
                        pDealInfo->iStatus = LOGGED_OFF;
                      }
                      
                      int FD = SendData.MyFd;
                      
                      shutdown(FD, SHUT_RDWR);
                      close(FD);

                      
                      FD_CLR(FD, &read_flags);
                      FD_CLR(FD, &write_flags);
                      if (FD == maxfd)
                      {
                        while (FD_ISSET(maxfd, &read_flags) == false)
                          maxfd -= 1;                    
                      }
                      
                      snprintf(logBufTCP,400,"TCP_UI|FD %d|Dealer %d|Disconnect Client",FD,pConnInfo->dealerID); 
                      Logger::getLogger().log(DEBUG, logBufTCP);
                      
                      delete (pConnInfo);
                      connMap.erase(FD);
                      
                      break;
                    }
                  }
                  pConnInfo->msgBufSize= (pConnInfo->msgBufSize) - tapheaderlength;
                  memset(tempBuf, 0, sizeof(tempBuf));
                  memcpy(tempBuf, ((pConnInfo->msgBuffer) + tapheaderlength), (pConnInfo->msgBufSize)); 
                  memcpy(pConnInfo->msgBuffer,tempBuf, (pConnInfo->msgBufSize));
                }    
              } 
               

          }

        } 
      }
      
      
    }
   
    
    if(Inqptr_FFToMain->dequeue(SendData))  //Handle Token Depth response to client
    {
      SendTokenDepth(&SendData);
    } 
    
    
    
    if(oConfigStore.FOSeg == true)        //Handle response from FO simulator
    {
      if(!RecvData(gpConnFO->msgBuffer, gpConnFO->msgBufSize, iFDfo, gpConnFO))
      {
        if(gpConnFO->msgBufSize <= 0)
        {
          continue;
        }
      }

      ipLength = (int16_t*)gpConnFO->msgBuffer;
      iLength = __bswap_16(*ipLength);
      
      if(gpConnFO->msgBufSize < iLength)
      {
        continue;
      }
      iRetVal = ProcRcvdMsgFO(gpConnFO->msgBuffer, gpConnFO->msgBufSize, iFDfo, gpConnFO, connMap);
      
    }
    
  }
  
}

