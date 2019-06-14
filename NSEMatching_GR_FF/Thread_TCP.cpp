/* 
 * File:   Thread_TCP.cpp
 * Author: muditsharma
 *
 * Created on March 1, 2016, 5:00 PM
 */

#include "Thread_TCP.h"
#include "All_Structures.h"
#include "spsc_atomic1.h"
#include "netinet/tcp.h"
//#include "CommonFunctions.h" 
//#include "tcpserver.h"
#include "perf.h"  

#define PORT "8888"
#define STDIN 0
#define BUFFERSIZE "2048"

//bool TimerStarted = INIT_PERF_TIMER(7);
struct sockaddr name;

char logBufTCP[400];
void set_nonblock(int socket) {
    int flags;
    flags = fcntl(socket,F_GETFL,0);
    assert(flags != -1);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int _nSegMode_TCP;

static inline int64_t getCurrentTimeInNano() {
    TS_VAR(currTime);
    GET_CURR_TIME(currTime);
    return TIMESTAMP(currTime);
}
void StartTCP(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer, int _nMode, const char* pcIpaddress_GR, int nPortNumber_GR, const char* pcIpaddress, int nPortNumber, int nSendBuff,int nRecvBuff,int iTCPCore, dealerInfoMap*  dealerinfoMap)
{
    snprintf(logBufTCP,400,"Thread_TCP|TCP started");
     Logger::getLogger().log(DEBUG, logBufTCP);
    //std::cout<< "TCP started" << std::endl;        
    _nSegMode_TCP = _nMode;

    snprintf(logBufTCP,400,"Thread_TCP|Pinning TCP to %d",iTCPCore);
     Logger::getLogger().log(DEBUG, logBufTCP);
    //std::cout<<logBufTCP;
     TaskSetCPU(iTCPCore);
    CreateNonBlockSocket(Inqptr_TCPServerToMe,Inqptr_MeToTCPServer, pcIpaddress_GR, nPortNumber_GR,pcIpaddress, nPortNumber, nSendBuff,nRecvBuff,dealerinfoMap);
}        


int CreateNonBlockSocket_Initial(const char* pcIpaddress, int nPortNumber)
{
  
    struct addrinfo hints;
    struct addrinfo *servinfo; 
    
    int sel;                      // holds return value for select();
 
    int status, maxfd, sock;
    
 
    //socket infoS
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; //tcp
    hints.ai_flags = AI_PASSIVE;     //use local-host address
    
    char lcPortNumber[16 + 1] = {0};
    memset(&lcPortNumber, 0, sizeof(lcPortNumber));
    
    snprintf(lcPortNumber, sizeof(lcPortNumber), "%d", nPortNumber);
    
    //get server info, put into servinfo
    if ((status = getaddrinfo(pcIpaddress, lcPortNumber, &hints, &servinfo)) != 0) 
    {
      snprintf(logBufTCP,400,"Thread_TCP|getaddrinfo error: %s", gai_strerror(status));
      Logger::getLogger().log(DEBUG, logBufTCP); 
      sleep(5);
      exit(1);
    }

    //make socket
    sock =  socket(AF_INET, SOCK_STREAM, 0);    //socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock < 0) {
        
      snprintf(logBufTCP,400,"Thread_TCP| Server socket failure: %d", errno);
      Logger::getLogger().log(DEBUG, logBufTCP); 
      sleep(5);
      exit(1);
    }
    
    sockaddr_in	lcListenerAddr;
    lcListenerAddr.sin_family = AF_INET;
    lcListenerAddr.sin_port = htons((short)nPortNumber);
    inet_aton(pcIpaddress, &(lcListenerAddr.sin_addr));    
    
    if(bind(sock, (sockaddr*)&lcListenerAddr, sizeof(sockaddr_in)) < 0)
    {
      
       snprintf(logBufTCP,400,"Thread_TCP| Bind error %d %s %s %d %s", errno , strerror(errno), pcIpaddress, nPortNumber, lcPortNumber);
       Logger::getLogger().log(DEBUG, logBufTCP); 
      exit(1);
    }      
    
    //allow reuse of port
    int yes=1;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        
       snprintf(logBufTCP,400,"Thread_TCP| setsockopt error %d ", errno);
       Logger::getLogger().log(DEBUG, logBufTCP); 
         sleep(5);
        exit(1);
    }    


    freeaddrinfo(servinfo);

    //listen
    if(listen(sock, 10) < 0) {
        
        snprintf(logBufTCP,400,"Thread_TCP| Listen error %d ", errno);
       Logger::getLogger().log(DEBUG, logBufTCP); 
        sleep(5);
        exit(1);
    }
    maxfd = sock;
    
    snprintf(logBufTCP,400,"Thread_TCP|GR|Ready to accept multiple connections..!!");
    Logger::getLogger().log(DEBUG, logBufTCP); 
    
   
    
    /*Initial Login Process Ends*/
    return sock;

}

int SendToClient_GR(int FD, char* msg, int msgLen)
{
    int bytesSent = 0;
    int retVal = 0;

    
        //i = write( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR));
       // i = send(FD, msg, msgLen, MSG_NOSIGNAL);
           int dataSent = 0;
           bool bExit= false;
           int iWriteAttempt = 0;   
           while (bExit == false)
           {
                   //bytesSent = write (FD, (char*)(msg + dataSent), (msgLen-dataSent));
                   bytesSent = send(FD,(char*) (msg + dataSent), (msgLen - dataSent), MSG_DONTWAIT | MSG_NOSIGNAL);
                   if (bytesSent != msgLen)
                   {
                       snprintf(logBufTCP,500,"Thread_ME|FD %d|SendToClient|Partial data sent|%d|%d|%d",FD, bytesSent, msgLen, errno);
                       Logger::getLogger().log(DEBUG, logBufTCP);
                        //std::cout<<"SendToClient|Partial data sent|"<<bytesSent<<"|"<<msgLen<<"|"<<errno<<std::endl;
                   }
                   if (bytesSent == -1)
                  {
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                      usleep(10000);
                      if (3 <= iWriteAttempt)
                      {
                         snprintf (logBufTCP, 500, "Thread_ME|FD %d|Disconnecting slow client|Error %d", FD, errno);
                         Logger::getLogger().log(DEBUG, logBufTCP);
                         
                         bExit = true;  
                         retVal = -1;
                      }
                       iWriteAttempt++;
                    }
                     else
	{
          snprintf(logBufTCP, 500, "Thread_ME|FD %d|Issue received|Drop connection| last error code|%d", FD, errno);
          Logger::getLogger().log(DEBUG, logBufTCP);
	     //std::cout <<"FD "<<FD<<"|Issue received|Drop connection| last error code| " << errno << std::endl;
                          
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


int CreateNonBlockSocket(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer, const char* pcIpaddress_Init, int nPortNumber_Init, const char* pcIpaddress, int nPortNumber, int nSendBuff,int nRecvBuff ,dealerInfoMap*  dealerinfoMap)
{
  //Initial login Process 27nov2017
  struct sockaddr_in  GR_addr;
  socklen_t GR_addr_size;
  fd_set read_flags_GR,write_flags_GR, tempset_GR;
  struct timeval waitd_GR = {0, 0};
  int sel_GR;
  DATA_RECEIVED RcvData_GR;
  DATA_RECEIVED SendData_GR;
  int maxfd_GR,new_sd_GR;
  int SockId_GR = CreateNonBlockSocket_Initial(pcIpaddress_Init, nPortNumber_Init);
  bool mainConnFlag=false;
  char msgBuffer[3000];
  int msgBufSize=0;
  int ConnCheck=0;
  //Ends
//  std::cout<<"sock::"<<SockId_GR<<std::endl;
  /*Initial Login Process starts*/
  
  FD_ZERO(&read_flags_GR);
  FD_ZERO(&write_flags_GR);
  FD_SET(SockId_GR, &read_flags_GR);
  
  GR_addr_size = sizeof(GR_addr);
  maxfd_GR = SockId_GR;
    
    /*Initial Login Process ends*/
  
//    std::cout<<"pcIpaddress_Init::"<<pcIpaddress_Init<<"|nPortNumber_Init::"<<nPortNumber_Init<<std::endl;
    int status, adrlen, new_sd, maxfd, sock;
    CLIENT_MSG client_message;
    DATA_RECEIVED SendData;
    DATA_RECEIVED RcvData;
    TCP_BUFFER    tcp_buff;
    struct addrinfo hints;
    struct addrinfo *servinfo;  //will point to the results
    connectionMap connMap;
    connectionItr connItr;    
    std::pair<connectionItr, bool> connInsertRet;
  
   
   struct timespec req = {0};
   req.tv_sec = 0;
   req.tv_nsec = 5000;


    //store the connecting address and size
    struct sockaddr_in  their_addr;
    socklen_t their_addr_size;

    fd_set read_flags,write_flags, tempset; // the flag sets to be used
    struct timeval waitd = {0, 0};          // the max wait time for an event
    int sel;                      // holds return value for select();


    //socket infoS
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; //tcp
    hints.ai_flags = AI_PASSIVE;     //use local-host address
    
    char lcPortNumber[16 + 1] = {0};
    memset(&lcPortNumber, 0, sizeof(lcPortNumber));
    
    snprintf(lcPortNumber, sizeof(lcPortNumber), "%d", nPortNumber);
    
    //get server info, put into servinfo
    if ((status = getaddrinfo(pcIpaddress, lcPortNumber, &hints, &servinfo)) != 0) 
    {
      //fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      snprintf(logBufTCP,400,"Thread_TCP|getaddrinfo error: %s", gai_strerror(status));
       Logger::getLogger().log(DEBUG, logBufTCP); 
      sleep(5);
      exit(1);
    }

    //make socket
    sock =  socket(AF_INET, SOCK_STREAM, 0);    //socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock < 0) {
        //printf("\nserver socket failure %m", errno);
        snprintf(logBufTCP,400,"Thread_TCP| Server socket failure: %d", errno);
       Logger::getLogger().log(DEBUG, logBufTCP); 
        sleep(5);
        exit(1);
    }
    
    sockaddr_in	lcListenerAddr;
    lcListenerAddr.sin_family = AF_INET;
    lcListenerAddr.sin_port = htons((short)nPortNumber);
    inet_aton(pcIpaddress, &(lcListenerAddr.sin_addr));    
    
    if(bind(sock, (sockaddr*)&lcListenerAddr, sizeof(sockaddr_in)) < 0)
    {
      //printf("\nBind error %d %s %s %d %s", errno , strerror(errno), pcIpaddress, nPortNumber, lcPortNumber);
       snprintf(logBufTCP,400,"Thread_TCP| Bind error %d %s %s %d %s", errno , strerror(errno), pcIpaddress, nPortNumber, lcPortNumber);
       Logger::getLogger().log(DEBUG, logBufTCP); 
      exit(1);
    }      
    
    //allow reuse of port
    int yes=1;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        //perror("setsockopt");
       snprintf(logBufTCP,400,"Thread_TCP| setsockopt error %d ", errno);
       Logger::getLogger().log(DEBUG, logBufTCP); 
         sleep(5);
        exit(1);
    }    

    //unlink and bind
    //unlink(pcIpaddress);
//    if(bind (sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
//        printf("\nBind error %d %s %s %d %s", errno , strerror(errno), pcIpaddress, nPortNumber, lcPortNumber);
//        exit(1);
//    }

    freeaddrinfo(servinfo);

    //listen
    if(listen(sock, 10) < 0) {
        //printf("\nListen error %m", errno);
        snprintf(logBufTCP,400,"Thread_TCP| Listen error %d ", errno);
       Logger::getLogger().log(DEBUG, logBufTCP); 
        sleep(5);
        exit(1);
    }
    their_addr_size = sizeof(their_addr);
    
    


    int numSent;
    int numRead;
    int lnRecvBufSize  = 1024 * 20;
    int sendbuff =  1024 * 1024 * 10;
    char tempBuf[3000] = {0};
    if (nSendBuff != 0 )
    {
        sendbuff = nSendBuff;
    }
    if (nRecvBuff != 0 )
    {
        lnRecvBufSize = nRecvBuff;
    }
    char lcBuffer[8096];    
    tcp_buff.buffsize = 0;
    
    /*Sneha - multiple connection changes:14/07/16 - S*/
    FD_ZERO(&read_flags);
    FD_ZERO(&write_flags);
    FD_SET(sock, &read_flags);
    
    maxfd = sock;
    
    snprintf(logBufTCP,400,"Thread_TCP|Ready to accept multiple connections..!!");
      Logger::getLogger().log(DEBUG, logBufTCP); 
    //std::cout<<"Ready to accept multiple connections..!!"<<std::endl;
   
    while(1) 
    {
        memcpy(&tempset,&read_flags,sizeof(tempset));
        
        sel = select(maxfd+1, &tempset, &write_flags, (fd_set*)0, &waitd);
        
        if(sel <= 0)
        {
//          std::cout<<"sel::"<<sel<<std::endl;
           if (Inqptr_MeToTCPServer->dequeue(RcvData))
           {
             if (DISCONNECT_CLIENT == RcvData.Transcode && 0 == (RcvData.ptrConnInfo)->recordCnt)
             {
                  CONNINFO* pConnInfo = (RcvData.ptrConnInfo);
                   int FD = RcvData.MyFd;
                 
                   shutdown(FD, SHUT_RDWR);
                   close(FD);
                   
                   snprintf(logBufTCP,400,"Thread_TCP|FD %d|Staus = DISCONNECTED_CLIENT received and recordCnt = 0", FD);
                   Logger::getLogger().log(DEBUG, logBufTCP); 
                 
                   FD_CLR(FD, &read_flags);
                   FD_CLR(FD, &write_flags);
                   if (FD == maxfd)
                   {
                      while (FD_ISSET(maxfd, &read_flags) == false)
                        maxfd -= 1;                    
                   }
                 
                   if (pConnInfo->dealerID != 0)
                   {
                       dealerInfoItr dealerItr = dealerinfoMap->find(pConnInfo->dealerID);
                       if (dealerItr != dealerinfoMap->end())
                       {
                         (dealerItr->second)->status = LOGGED_OFF;
                         if ((dealerItr->second)->COL == 1)
                         {
                             CUSTOM_HEADER COLHdr;
                             COLHdr.sTransCode = COL;
                             COLHdr.iSeqNo = pConnInfo->dealerID;
                             COLHdr.swapBytes();    
                             memcpy(SendData.msgBuffer, &COLHdr, sizeof(CUSTOM_HEADER)); 
                             SendData.MyFd = -1;
                             SendData.ptrConnInfo = NULL; 
                             Inqptr_TCPServerToMe->enqueue(SendData);
                         }
                       }
                       else
                       {
                         snprintf(logBufTCP,400,"Thread_TCP|FD %d|Unable to find dealer to log off %d",FD,pConnInfo->dealerID);
                         Logger::getLogger().log(DEBUG, logBufTCP); 
                        }
                   }             
                   delete (pConnInfo);
                   connMap.erase(FD);
             }
           }
        }
        else
        {
         
          if(FD_ISSET(sock, &tempset))
          {
            //accept
            new_sd = accept(sock, (struct sockaddr*)&their_addr, &their_addr_size);
            if( new_sd < 0) 
            {
               snprintf(logBufTCP,400,"Thread_TCP|Accept error %d", errno);
               Logger::getLogger().log(DEBUG, logBufTCP); 
            }
            else
            {
              if(0 != setsockopt(new_sd, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))        
              {
                  snprintf(logBufTCP,400,"Thread_TCP|[SYS] setsockopt failed for setting RecvBufSize");
                  Logger::getLogger().log(DEBUG, logBufTCP); 

              }   
              else
              {
                  snprintf(logBufTCP,400,"Thread_TCP|Receive Buffer size: %d", lnRecvBufSize);
                  Logger::getLogger().log(DEBUG, logBufTCP); 
              }
              if(0 != setsockopt(new_sd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)))     
              {
                  snprintf(logBufTCP,400,"Thread_TCP|Setsockopt failed for setting SendBufSize");
                  Logger::getLogger().log(DEBUG, logBufTCP); 
              }  
              else
              {
                  snprintf(logBufTCP,400,"Thread_TCP|Send Buffer size: %d", sendbuff);
                  Logger::getLogger().log(DEBUG, logBufTCP); 
              }
              set_nonblock(new_sd);

              int flag = 1;
              if(0 > setsockopt(new_sd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag))) 
              {
                  snprintf(logBufTCP,400,"Thread_TCP|Setsockopt failed for TCP_NODELAY errno %d|%s", errno, strerror(errno));
                  Logger::getLogger().log(DEBUG, logBufTCP); 
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

                snprintf(logBufTCP,400,"Thread_TCP|FD %d|Failed to insert in map", new_sd);
                Logger::getLogger().log(DEBUG, logBufTCP); 

                FD_CLR(new_sd, &read_flags);
                FD_CLR(new_sd, &write_flags);
                delete (connObj);
              }
              else
              {
                snprintf(logBufTCP,400,"Thread_TCP|FD %d|%s|Successful Connection!", new_sd,connObj->IP);
                Logger::getLogger().log(DEBUG, logBufTCP); 
                maxfd = (maxfd < new_sd)? new_sd:maxfd;
              }
              FD_CLR(sock, &tempset);
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
              if ( pConnInfo->status == DISCONNECTED && pConnInfo->recordCnt == 0)
              {
                shutdown(fdSocket, SHUT_RDWR);
                close(fdSocket);

                snprintf(logBufTCP,400,"Thread_TCP|FD %d|Staus = DISCONNECTED and recordCnt = 0", fdSocket);
                Logger::getLogger().log(DEBUG, logBufTCP); 
                //std::cout<<"FD "<<fdSocket<<"|Staus = DISCONNECTED and recordCnt = 0."<<std::endl;
                FD_CLR(fdSocket, &read_flags);
                FD_CLR(fdSocket, &write_flags);
                if (fdSocket == maxfd)
                {
                   while (FD_ISSET(maxfd, &read_flags) == false)
                     maxfd -= 1;                    
                }
                if (pConnInfo->dealerID != 0)
                {
                    dealerInfoItr dealerItr = dealerinfoMap->find(pConnInfo->dealerID);
                    if (dealerItr != dealerinfoMap->end())
                    {
                      (dealerItr->second)->status = LOGGED_OFF;
                      if ((dealerItr->second)->COL == 1)
                      {
                        CUSTOM_HEADER COLHdr;
                        COLHdr.sTransCode = COL;
                        COLHdr.iSeqNo = pConnInfo->dealerID;
                        COLHdr.swapBytes();    
                        memcpy(SendData.msgBuffer, &COLHdr, sizeof(CUSTOM_HEADER)); 
                        SendData.MyFd = -1;
                        SendData.ptrConnInfo = NULL; 
                        Inqptr_TCPServerToMe->enqueue(SendData);
                      }
                    }
                    else
                    {
                      snprintf(logBufTCP,400,"Thread_TCP|FD %d|Unable to find dealer to log off %d",fdSocket,pConnInfo->dealerID);
                      Logger::getLogger().log(DEBUG, logBufTCP); 
                       //std::cout<<"FD "<<fdSocket<<"|Unable to find dealer to log off|"<<pConnInfo->dealerID<<std::endl;
                    }
                }             
                delete (pConnInfo);
                connMap.erase(fdSocket);
                continue;
              }
              do
              {
                numRead = recv(fdSocket ,(char*) ((pConnInfo->msgBuffer)+(pConnInfo->msgBufSize)), (3000 - (pConnInfo->msgBufSize)), 0);

              }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));

              if(numRead == 0)
              {
                 if (pConnInfo->recordCnt == 0)
                 {
                    shutdown(fdSocket, SHUT_RDWR);
                    close(fdSocket);

                    snprintf(logBufTCP,400,"Thread_TCP|FD %d|Disconnection Received",fdSocket); 
                    Logger::getLogger().log(DEBUG, logBufTCP); 
                    //std::cout<<"TCP_Thread:FD "<<fdSocket<<"|Disconnection Received."<<std::endl;
                    FD_CLR(fdSocket, &read_flags);
                    FD_CLR(fdSocket, &write_flags);
                    if (fdSocket == maxfd)
                    {
                      while (FD_ISSET(maxfd, &read_flags) == false)
                        maxfd -= 1;                    
                    }
                    if (pConnInfo->dealerID != 0)
                    {
                      dealerInfoItr dealerItr = dealerinfoMap->find(pConnInfo->dealerID);
                      if (dealerItr != dealerinfoMap->end())
                      {
                        (dealerItr->second)->status = LOGGED_OFF;
                        if ((dealerItr->second)->COL == 1)
                        {
                          CUSTOM_HEADER COLHdr;
                          COLHdr.sTransCode = COL;
                          COLHdr.iSeqNo = pConnInfo->dealerID;
                          COLHdr.swapBytes();    
                           memcpy(SendData.msgBuffer, &COLHdr, sizeof(CUSTOM_HEADER)); 
                          SendData.MyFd = -1;
                          SendData.ptrConnInfo = NULL; 
                          Inqptr_TCPServerToMe->enqueue(SendData);
                        }
                      }
                      else
                      {
                        snprintf(logBufTCP,400,"Thread_TCP|FD %d|Unable to find dealer to log off %d",fdSocket,pConnInfo->dealerID);
                        Logger::getLogger().log(DEBUG, logBufTCP); 
                      }
                    }
                    delete (pConnInfo);
                    connMap.erase(fdSocket);
                 }
                 continue;
              }

               /*Sneha - multiple connection changes:14/07/16 - E*/ 
              if(numRead > 0)
              {   
                SendData.MyFd = fdSocket;
                SendData.ptrConnInfo = pConnInfo;
                pConnInfo->msgBufSize = (pConnInfo->msgBufSize) + numRead;

                int packets = 0;
                switch (_nSegMode_TCP) 
                {
                  case SEG_NSECM:
                  {
                    short exitloop = 0; 
                    int tapheaderlength  = 0, dataprocessed = 0;

                    while(exitloop == 0)
                    {
                       NSECM::ME_MESSAGE_HEADER *tapHdr=(NSECM::ME_MESSAGE_HEADER *)pConnInfo->msgBuffer; 

                       tapheaderlength = __bswap_16(tapHdr->sLength);

//                       std::cout << " tapheaderlength = tapHdr->sLength  " <<tapheaderlength << " tapHdr->TransactionCode " << __bswap_16(tapHdr->TransactionCode)  << " msgBuff size "<<(pConnInfo->msgBufSize)<< std::endl;

                       if(pConnInfo->msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                       {
                          exitloop = 1;                                  
                       }
                       else
                       {
                          memcpy(SendData.msgBuffer, pConnInfo->msgBuffer, tapheaderlength); 
                          pConnInfo->recordCnt++;
                          packets++;
                          int dequeFailureAttempts = 0;
                          GET_PERF_TIME(SendData.recvTimeStamp);
                          while (Inqptr_TCPServerToMe->enqueue(SendData) == false)
                          {
                              if (++dequeFailureAttempts%1000==0)
                              {
                                  snprintf(logBufTCP, 400, "Thread_TCP|FD %d|Unable to enqueue the Message of TAP seqNo %d|deque attempts", fdSocket, __bswap_32(tapHdr->iSeqNo));
                                  Logger::getLogger().log(DEBUG, logBufTCP);                                        
                                  usleep(10);
                              }
                          }                                  
                          pConnInfo->msgBufSize= (pConnInfo->msgBufSize) - tapheaderlength;
                          memset(tempBuf, 0, sizeof(tempBuf));
                          memcpy(tempBuf, ((pConnInfo->msgBuffer) + tapheaderlength), (pConnInfo->msgBufSize)); 
                          memcpy(pConnInfo->msgBuffer,tempBuf, (pConnInfo->msgBufSize));
                        }    
                    } 
                  }
                  break;
                  case SEG_NSEFO:
                  {
                    short exitloop = 0; 
                    int tapheaderlength  = 0;
                    while(exitloop == 0)
                    {
                      NSEFO::ME_MESSAGE_HEADER *tapHdr=(NSEFO::ME_MESSAGE_HEADER *)pConnInfo->msgBuffer;
                      tapheaderlength = __bswap_16(tapHdr->sLength);
                      //std::cout << " tapheaderlength = tapHdr->sLength  " <<  __bswap_16(tapHdr->sLength) << " tapHdr->TransactionCode " << __bswap_16(tapHdr->TransactionCode)  <<" msgbuf size "<<pConnInfo->msgBufSize<< std::endl;

                      if(pConnInfo->msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                      {
                         exitloop = 1;                                  
                      }
                      else
                      {
                         memcpy(SendData.msgBuffer, pConnInfo->msgBuffer, tapheaderlength); 

                         pConnInfo->recordCnt++;
                          packets++;
                         int dequeFailureAttempts = 0;
                         GET_PERF_TIME(SendData.recvTimeStamp);
                         while (Inqptr_TCPServerToMe->enqueue(SendData) == false)
                         {
                             if (++dequeFailureAttempts%1000==0)
                             {
                                 snprintf(logBufTCP, 400, "Thread_TCP|FD %d|Unable to enqueue the Message of TAP seqNo %d|deque attempts", fdSocket, __bswap_32(tapHdr->iSeqNo));
                                 Logger::getLogger().log(DEBUG, logBufTCP);                                        
                                 usleep(10);
                             }
                         }
                         pConnInfo->msgBufSize = pConnInfo->msgBufSize - tapheaderlength;
                         memset(tempBuf, 0, sizeof(tempBuf));
                         memcpy(tempBuf, ((pConnInfo->msgBuffer) + tapheaderlength), (pConnInfo->msgBufSize)); 
                         memcpy(pConnInfo->msgBuffer,tempBuf, (pConnInfo->msgBufSize));
                      }    
                    } 
                  }
                  break;
                  case SEG_NSECD:
                  {
                    short exitloop = 0; 
                    int tapheaderlength  = 0;
                    while(exitloop == 0)
                    {
                      NSECD::ME_MESSAGE_HEADER *tapHdr=(NSECD::ME_MESSAGE_HEADER *)pConnInfo->msgBuffer;
                      tapheaderlength = __bswap_16(tapHdr->sLength);
                      //std::cout << " tapheaderlength = tapHdr->sLength  " <<  __bswap_16(tapHdr->sLength) << " tapHdr->TransactionCode " << __bswap_16(tapHdr->TransactionCode)  <<" msgbuf size "<<pConnInfo->msgBufSize<< std::endl;

                      if(pConnInfo->msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                      {
                         exitloop = 1;                                  
                      }
                      else
                      {
                         memcpy(SendData.msgBuffer, pConnInfo->msgBuffer, tapheaderlength); 

                         pConnInfo->recordCnt++;
                          packets++;
                         int dequeFailureAttempts = 0;
                         GET_PERF_TIME(SendData.recvTimeStamp);
                         while (Inqptr_TCPServerToMe->enqueue(SendData) == false)
                         {
                             if (++dequeFailureAttempts%1000==0)
                             {
                                 snprintf(logBufTCP, 400, "Thread_TCP|FD %d|Unable to enqueue the Message of TAP seqNo %d|deque attempts", fdSocket, __bswap_32(tapHdr->iSeqNo));
                                 Logger::getLogger().log(DEBUG, logBufTCP);                                        
                                 usleep(10);
                             }
                         }
                         pConnInfo->msgBufSize = pConnInfo->msgBufSize - tapheaderlength;
                         memset(tempBuf, 0, sizeof(tempBuf));
                         memcpy(tempBuf, ((pConnInfo->msgBuffer) + tapheaderlength), (pConnInfo->msgBufSize)); 
                         memcpy(pConnInfo->msgBuffer,tempBuf, (pConnInfo->msgBufSize));
                      }    
                    } 
                  }
                  break;
                  default:
                    break;
                }
              }

            } //if(FD_ISSET(new_sd, &read_flags)) 
          } //end for    
        }
        /*==========================================*/
        /*Initial Login Process starts*/

        memcpy(&tempset_GR,&read_flags_GR,sizeof(tempset_GR));
        sel_GR = select(maxfd_GR+1, &tempset_GR, &write_flags_GR, (fd_set*)0, &waitd_GR);
        if(sel_GR <= 0)
        { 
          if(sel_GR < 0)
          {
            Logger::getLogger().log(DEBUG, "DISCONNECTION RECIEVED");
          }
        } 
        else
        {
            
            if(FD_ISSET(SockId_GR, &tempset_GR))
            {   
                //accept
                new_sd_GR = accept(SockId_GR, (struct sockaddr*)&GR_addr, &GR_addr_size);
                if( new_sd_GR < 0) 
                {
                   snprintf(logBufTCP,400,"Thread_TCP Init|Accept error %d", errno);
                   Logger::getLogger().log(DEBUG, logBufTCP); 
                    //printf("\nAccept error %m", errno);
                }
                else
                {
                    if(0 != setsockopt(new_sd_GR, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))        
                    {
                        //Logger::getLogger().log(ERROR, "[SYS] setsockopt failed for setting RecvBufSize"); 
                        snprintf(logBufTCP,400,"Thread_TCP Init|[SYS] setsockopt failed for setting RecvBufSize");
                          Logger::getLogger().log(DEBUG, logBufTCP); 
                        //std::cout<<"[SYS] setsockopt failed for setting RecvBufSize\n";
                    }   
                    else
                    {
                        snprintf(logBufTCP,400,"Thread_TCP Init|Receive Buffer size: %d", lnRecvBufSize);
                        Logger::getLogger().log(DEBUG, logBufTCP); 
                        //std::cout<<"Receive Buffer size:"<<lnRecvBufSize<<std::endl;
                    }
                    if(0 != setsockopt(new_sd_GR, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)))     
                    {
                        //Logger::getLogger().log(ERROR, "[SYS] setsockopt failed for setting RecvBufSize"); 
                        snprintf(logBufTCP,400,"Thread_TCP Init|Setsockopt failed for setting SendBufSize");
                        Logger::getLogger().log(DEBUG, logBufTCP); 
                        //std::cout<<"[SYS] setsockopt failed for setting SendBufSize\n";
                    }  
                    else
                    {
                        snprintf(logBufTCP,400,"Thread_TCP Init|Send Buffer size: %d", sendbuff);
                        Logger::getLogger().log(DEBUG, logBufTCP); 
                        //std::cout<<"Send Buffer size:"<<sendbuff<<std::endl;
                    }
                    set_nonblock(new_sd_GR);
                    
                     int flag = 1;
                     if(0 > setsockopt(new_sd_GR, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag))) 
                     {
                         snprintf(logBufTCP,400,"Thread_TCP Init|Setsockopt failed for TCP_NODELAY errno %d|%s", errno, strerror(errno));
                         Logger::getLogger().log(DEBUG, logBufTCP); 
                     }
                         
                    //struct sockaddr_in* client_addr = &(their_addr);
                    FD_SET(new_sd_GR, &read_flags_GR);
                    FD_SET(new_sd_GR, &write_flags_GR);
                    
                    
                    char IP[16];
                    memcpy(IP, inet_ntoa(GR_addr.sin_addr), sizeof(IP));
                    
                    snprintf(logBufTCP,400,"Thread_TCP Init|FD %d|%s|Successful Connection!", new_sd_GR,IP);
                    Logger::getLogger().log(DEBUG, logBufTCP); 
                  
                    maxfd_GR = (maxfd_GR < new_sd_GR)? new_sd_GR:maxfd_GR;
                   
                    FD_CLR(SockId_GR, &tempset_GR);
                }
            }  
            
            for(int fdSocket=1; fdSocket< maxfd_GR+1 ; fdSocket++)
            {
              //socket ready for reading
              if(FD_ISSET(fdSocket, &tempset_GR))
              {
               
                do
                {
                  numRead = recv(fdSocket ,(char*) ((msgBuffer)+(msgBufSize)), (3000 - (msgBufSize)), 0);
                }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
//                std::cout<<"Byte received::"<<numRead<<std::endl; 
                if(numRead == 0)
                {
                   
                  FD_CLR(fdSocket, &read_flags_GR);
                  FD_CLR(fdSocket, &write_flags_GR);
                  if (fdSocket == maxfd_GR)
                  {
                     while (FD_ISSET(maxfd_GR, &read_flags_GR) == false)
                       maxfd_GR -= 1;                    
                  }
                  
                 continue;
                }
                if(numRead > 0)
                {    
                  msgBufSize = msgBufSize + numRead;
                    
                     
                    switch (_nSegMode_TCP)
                    {
                     case SEG_NSECM:
                     {
                       int tapheaderlength  = 0;

                       NSECM::ME_MESSAGE_HEADER *tapHdr=(NSECM::ME_MESSAGE_HEADER *)msgBuffer; 

                       tapheaderlength = __bswap_16(tapHdr->sLength);

                       if(msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                       {
                          snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSEFO |Bytes Expected %d|Bytes Received  %d",fdSocket,tapheaderlength, msgBufSize);
                          Logger::getLogger().log(DEBUG, logBufTCP);
                       }
                       else
                       {

                         if(__bswap_16(tapHdr->TransactionCode)==GR_REQUEST)
                         {  
                           NSECM::MS_GR_REQUEST *pGRrequest = (NSECM::MS_GR_REQUEST *)msgBuffer;
                           NSECM::MS_GR_RESPONSE GRresponse;

                           snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSEFO %d|BoxId: %d",fdSocket,__bswap_16(pGRrequest->msg_hdr.TransactionCode),__bswap_16(pGRrequest->wBoxId));
                              Logger::getLogger().log(DEBUG, logBufTCP);
                              GRresponse.msg_hdr.TransactionCode = GR_RESPONSE;
                              GRresponse.msg_hdr.ErrorCode = 0;
                              GRresponse.msg_hdr.sLength = sizeof(NSEFO::MS_GR_RESPONSE);
                              GRresponse.wBoxId  = pGRrequest->wBoxId;

                              memset(GRresponse.cIPAddress, 0, sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cIPAddress,pcIpaddress,sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cSessionKey,"1234567",sizeof(GRresponse.cSessionKey));
                              GRresponse.iPort = __bswap_32(nPortNumber);
                              int bytesWritten = 0;

                              GRresponse.msg_hdr.swapBytes();  
                              
                              bytesWritten = SendToClient_GR( fdSocket , (char *)&GRresponse , sizeof(NSEFO::MS_GR_RESPONSE));

//                               bytesWritten = send(fdSocket,(char *)&GRresponse, sizeof(NSECM::MS_GR_RESPONSE), MSG_DONTWAIT | MSG_NOSIGNAL);
                              GRresponse.msg_hdr.swapBytes(); 

                              if (GRresponse.msg_hdr.ErrorCode != 0)
                              {
                                FD_CLR(fdSocket, &read_flags_GR);
                                FD_CLR(fdSocket, &write_flags_GR);
                                if (fdSocket == maxfd_GR)
                                {
                                   while (FD_ISSET(maxfd_GR, &read_flags_GR) == false)
                                     maxfd_GR -= 1;  
                                }
                                shutdown(fdSocket, SHUT_RDWR);
                                close(fdSocket);
                                snprintf(logBufTCP, 500, "Thread_TCP|FD %d|ERROR %d|DISCONNECTED",fdSocket,GRresponse.msg_hdr.ErrorCode);
                                Logger::getLogger().log(DEBUG, logBufTCP);
                                
                              }
                              snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_RESPONSE NSEFO %d|Error Code %d|Bytes Sent  %d|Message Size:: %d|cIPAddress %s|iPort %i|Session key %s",fdSocket,GRresponse.msg_hdr.TransactionCode,GRresponse.msg_hdr.ErrorCode, bytesWritten,GRresponse.msg_hdr.sLength,GRresponse.cIPAddress,__bswap_32(GRresponse.iPort),GRresponse.cSessionKey);
                              Logger::getLogger().log(DEBUG, logBufTCP);


                              msgBufSize = msgBufSize - tapheaderlength;
                              memset(tempBuf, 0, sizeof(tempBuf));
                              memcpy(tempBuf, ((msgBuffer) + tapheaderlength), (msgBufSize)); 
                              memcpy(msgBuffer,tempBuf, (msgBufSize));
                            }
                            else
                            {
                              Logger::getLogger().log(DEBUG, "Invalid Transcode");
                            }
                         
                       }    


                     }
                     break;
                     case SEG_NSEFO:
                     {


                       int tapheaderlength  = 0;

                          NSEFO::ME_MESSAGE_HEADER *tapHdr=(NSEFO::ME_MESSAGE_HEADER *)msgBuffer;
                          tapheaderlength = __bswap_16(tapHdr->sLength);

//                           std::cout<<"transcode::"<<__bswap_16(tapHdr->TransactionCode)<<"|Length:"<<tapheaderlength<<"|Size"<<sizeof(NSEFO::MS_GR_REQUEST)<<std::endl;
                          if(msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                          {
                             snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSEFO |Bytes Expected %d|Bytes Received  %d",fdSocket,tapheaderlength, msgBufSize);
                             Logger::getLogger().log(DEBUG, logBufTCP);
                          }
                          else
                          {

                            if(__bswap_16(tapHdr->TransactionCode)==GR_REQUEST)
                            {

                              NSEFO::MS_GR_REQUEST *pGRrequest = (NSEFO::MS_GR_REQUEST *)msgBuffer;
                              NSEFO::MS_GR_RESPONSE GRresponse;

                              snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSEFO %d|BoxId: %d",fdSocket,__bswap_16(pGRrequest->msg_hdr.TransactionCode),__bswap_16(pGRrequest->wBoxId));
                              Logger::getLogger().log(DEBUG, logBufTCP);
                              GRresponse.msg_hdr.TransactionCode = GR_RESPONSE;
                              GRresponse.msg_hdr.ErrorCode = 0;
                              GRresponse.msg_hdr.sLength = sizeof(NSEFO::MS_GR_RESPONSE);
                              GRresponse.wBoxId  = pGRrequest->wBoxId;

                              memset(GRresponse.cIPAddress, 0, sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cIPAddress,pcIpaddress,sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cSessionKey,"1234567",sizeof(GRresponse.cSessionKey));
                              GRresponse.iPort = __bswap_32(nPortNumber);
                              int bytesWritten = 0;

                              GRresponse.msg_hdr.swapBytes();  
                              
                              
                              bytesWritten = SendToClient_GR( fdSocket , (char *)&GRresponse , sizeof(NSEFO::MS_GR_RESPONSE));

//                               bytesWritten = send(fdSocket,(char *)&GRresponse, sizeof(NSECM::MS_GR_RESPONSE), MSG_DONTWAIT | MSG_NOSIGNAL);
                              GRresponse.msg_hdr.swapBytes(); 
                              
                              
                              if (GRresponse.msg_hdr.ErrorCode != 0)
                              {
                                FD_CLR(fdSocket, &read_flags_GR);
                                FD_CLR(fdSocket, &write_flags_GR);
                                if (fdSocket == maxfd_GR)
                                {
                                   while (FD_ISSET(maxfd_GR, &read_flags_GR) == false)
                                     maxfd_GR -= 1;  
                                }
                                shutdown(fdSocket, SHUT_RDWR);
                                close(fdSocket);
                                snprintf(logBufTCP, 500, "Thread_TCP|FD %d|ERROR %d|DISCONNECTED",fdSocket,GRresponse.msg_hdr.ErrorCode);
                                Logger::getLogger().log(DEBUG, logBufTCP);
                                
                              }
                              snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_RESPONSE NSEFO %d|Error Code %d|Bytes Sent  %d|Message Size:: %d|cIPAddress %s|iPort %i|Session key %s",fdSocket,GRresponse.msg_hdr.TransactionCode,GRresponse.msg_hdr.ErrorCode, bytesWritten,GRresponse.msg_hdr.sLength,GRresponse.cIPAddress,__bswap_32(GRresponse.iPort),GRresponse.cSessionKey);
                              Logger::getLogger().log(DEBUG, logBufTCP);
                              


                              msgBufSize = msgBufSize - tapheaderlength;
                              memset(tempBuf, 0, sizeof(tempBuf));
                              memcpy(tempBuf, ((msgBuffer) + tapheaderlength), (msgBufSize)); 
                              memcpy(msgBuffer,tempBuf, (msgBufSize));
                            }
                            else
                            {
                              Logger::getLogger().log(DEBUG, "Invalid Transcode");
                            }
                          }    
                     }
                     break;
                     case SEG_NSECD:
                     {


                       int tapheaderlength  = 0;

                          NSEFO::ME_MESSAGE_HEADER *tapHdr=(NSEFO::ME_MESSAGE_HEADER *)msgBuffer;
                          tapheaderlength = __bswap_16(tapHdr->sLength);

//                           std::cout<<"transcode::"<<__bswap_16(tapHdr->TransactionCode)<<"|Length:"<<tapheaderlength<<"|Size"<<sizeof(NSEFO::MS_GR_REQUEST)<<std::endl;
                          if(msgBufSize < tapheaderlength  || tapheaderlength == 0 )
                          {
                             snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSECD |Bytes Expected %d|Bytes Received  %d",fdSocket,tapheaderlength, msgBufSize);
                             Logger::getLogger().log(DEBUG, logBufTCP);
                          }
                          else
                          {

                            if(__bswap_16(tapHdr->TransactionCode)==GR_REQUEST)
                            {

                              NSECD::MS_GR_REQUEST *pGRrequest = (NSECD::MS_GR_REQUEST *)msgBuffer;
                              NSECD::MS_GR_RESPONSE GRresponse;

                              snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_REQUEST NSECD %d|BoxId: %d",fdSocket,__bswap_16(pGRrequest->msg_hdr.TransactionCode),__bswap_16(pGRrequest->wBoxId));
                              Logger::getLogger().log(DEBUG, logBufTCP);
                              GRresponse.msg_hdr.TransactionCode = GR_RESPONSE;
                              GRresponse.msg_hdr.ErrorCode = 0;
                              GRresponse.msg_hdr.sLength = sizeof(NSECD::MS_GR_RESPONSE);
                              GRresponse.wBoxId  = pGRrequest->wBoxId;

                              memset(GRresponse.cIPAddress, 0, sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cIPAddress,pcIpaddress,sizeof(GRresponse.cIPAddress));
                              memcpy(GRresponse.cSessionKey,"1234567",sizeof(GRresponse.cSessionKey));
                              GRresponse.iPort = __bswap_32(nPortNumber);
                              int bytesWritten = 0;

                              GRresponse.msg_hdr.swapBytes();  
                              
                              
                              bytesWritten = SendToClient_GR( fdSocket , (char *)&GRresponse , sizeof(NSECD::MS_GR_RESPONSE));

//                               bytesWritten = send(fdSocket,(char *)&GRresponse, sizeof(NSECM::MS_GR_RESPONSE), MSG_DONTWAIT | MSG_NOSIGNAL);
                              GRresponse.msg_hdr.swapBytes(); 
                              
                              
                              if (GRresponse.msg_hdr.ErrorCode != 0)
                              {
                                FD_CLR(fdSocket, &read_flags_GR);
                                FD_CLR(fdSocket, &write_flags_GR);
                                if (fdSocket == maxfd_GR)
                                {
                                   while (FD_ISSET(maxfd_GR, &read_flags_GR) == false)
                                     maxfd_GR -= 1;  
                                }
                                shutdown(fdSocket, SHUT_RDWR);
                                close(fdSocket);
                                snprintf(logBufTCP, 500, "Thread_TCP|FD %d|ERROR %d|DISCONNECTED",fdSocket,GRresponse.msg_hdr.ErrorCode);
                                Logger::getLogger().log(DEBUG, logBufTCP);
                                
                              }
                              snprintf(logBufTCP, 500, "Thread_TCP|FD %d|GR_RESPONSE NSEFO %d|Error Code %d|Bytes Sent  %d|Message Size:: %d|cIPAddress %s|iPort %i|Session key %s",fdSocket,GRresponse.msg_hdr.TransactionCode,GRresponse.msg_hdr.ErrorCode, bytesWritten,GRresponse.msg_hdr.sLength,GRresponse.cIPAddress,__bswap_32(GRresponse.iPort),GRresponse.cSessionKey);
                              Logger::getLogger().log(DEBUG, logBufTCP);
                              


                              msgBufSize = msgBufSize - tapheaderlength;
                              memset(tempBuf, 0, sizeof(tempBuf));
                              memcpy(tempBuf, ((msgBuffer) + tapheaderlength), (msgBufSize)); 
                              memcpy(msgBuffer,tempBuf, (msgBufSize));
                            }
                            else
                            {
                              Logger::getLogger().log(DEBUG, "Invalid Transcode");
                            }
                          }    
                     }
                     break;
                     default:
                       break;
                   }
                }

       } //if(FD_ISSET(new_sd, &read_flags)) 
      } //end for    
    
     /*Initial Login Process Ends*/   
        }
   } //end while
   snprintf(logBufTCP,400,"Thread_TCP|Exiting normally");
   Logger::getLogger().log(DEBUG, logBufTCP); 
   //std::cout<<"\n\nExiting normally\n";
   return 0;
}   

