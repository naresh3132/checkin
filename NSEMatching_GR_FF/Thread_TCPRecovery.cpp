/* 
 * File:   Thread_TCPRecovery.cpp
 * Author: Sneha
 *
 * Created on August 2, 2016
 */
#include "Thread_TCP.h"
#include "Thread_TCPRecovery.h"
#include "All_Structures.h"
#include "BrodcastStruct.h"
#include <fstream>
#define PORT "8888"
#define STDIN 0
#define BUFFERSIZE 2048

char logBufTCPRcv[400];

int gMaxWriteRetry;

void StartTCPRecovery(const char* pcIpaddress, int nPortNumber, int iTCPRcvryCore, short wStreamID, int iMaxWriteAttempt)
{
    snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|TCP Recovery started");
    Logger::getLogger().log(DEBUG, logBufTCPRcv); 
    TaskSetCPU(iTCPRcvryCore);
     
    gMaxWriteRetry = iMaxWriteAttempt;
    CreateTCPSocket(pcIpaddress, nPortNumber, wStreamID);
}        

int CreateTCPSocket(const char* pcIpaddress, int nPortNumber, short StreamID)
{
    int status, adrlen, new_sd, maxfd, sock;
    CLIENT_MSG client_message;
    struct addrinfo hints;
    struct addrinfo *servinfo;  //will point to the results
    bool bExit = false;
    
    struct timespec req = {0};
    req.tv_sec = 0;
    req.tv_nsec = 5000;


    //store the connecting address and size
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size;

    fd_set read_flags,write_flags, tempset; // the flag sets to be used
    struct timeval waitd = {10, 0};          // the max wait time for an event
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
      snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|getaddrinfo error %s", gai_strerror(status));
      Logger::getLogger().log(DEBUG, logBufTCPRcv); 
      //std::cout<<"TCPRcvry:getaddrinfo error:"<<gai_strerror(status)<<std::endl;
      //exit(1);
      return 0;
    }

    //make socket
    sock =  socket(AF_INET, SOCK_STREAM, 0);    //socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock < 0) {
       snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|server socket failure %d",errno);
       Logger::getLogger().log(DEBUG, logBufTCPRcv); 
        //std::cout<<"TCPRcvry:server socket failure:"<<errno<<std::endl;
        //exit(1);
        return 0;
    }
    
    sockaddr_in	lcListenerAddr;
    lcListenerAddr.sin_family = AF_INET;
    lcListenerAddr.sin_port = htons((short)nPortNumber);
    inet_aton(pcIpaddress, &(lcListenerAddr.sin_addr));    
    
    if(bind(sock, (sockaddr*)&lcListenerAddr, sizeof(sockaddr_in)) < 0)
    {
      snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|Bind error %d|%s|%d|%s",errno,pcIpaddress,nPortNumber,lcPortNumber);
      Logger::getLogger().log(DEBUG, logBufTCPRcv); 
      //std::cout<<"TCPRcvry:Bind error "<<errno<<":"<<strerror(errno)<<":"<<pcIpaddress<<":"<<nPortNumber<<":"<<lcPortNumber<<std::endl;
      //exit(1);
      return 0;
    }      
    
    //allow reuse of port
    int yes=1;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
      snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|setsockopt error %d", errno);
      Logger::getLogger().log(DEBUG, logBufTCPRcv); 
      //std::cout<<"TCPRcvry:setsockopt"<<std::endl;
        //exit(1);
        return 0;
    }    

    freeaddrinfo(servinfo);

    //listen
    if(listen(sock, 10) < 0) {
        snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|Listen error %d",errno);
         Logger::getLogger().log(DEBUG, logBufTCPRcv); 
        //std::cout<<"TCPRcvry:Listen error "<<errno<<std::endl;
        //exit(1);
        return 0;
    }
    their_addr_size = sizeof(their_addr);

    int numSent;
    int numRead;
    int lnRecvBufSize  = 1024 * 20;
    int sendbuff =  1024 * 20;
    char buf[BUFFERSIZE] = {0};   
    FD_ZERO(&tempset);
    FD_SET(sock, &tempset);
    FD_ZERO(&write_flags);
    FD_SET(STDIN_FILENO, &write_flags);
    
    maxfd = sock;
    
    while(1) 
    {
        bExit = false;
        sel = select(maxfd+1, &tempset, &write_flags, (fd_set*)0, &waitd);
          
        //if an error with select
        if(sel <= 0)
        {
            continue;
        }
        else
        {
            //accept
            new_sd = accept(sock, (struct sockaddr*)&their_addr, &their_addr_size);
            if( new_sd < 0) {
                snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|Accept error %d", errno);
                Logger::getLogger().log(DEBUG, logBufTCPRcv); 
            }
            else
            {
                if(0 != setsockopt(new_sd, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))        
                {
                    //Logger::getLogger().log(ERROR, "[SYS] setsockopt failed for setting RecvBufSize");
                  snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|setsockopt failed for setting RecvBufSize");
                    Logger::getLogger().log(DEBUG, logBufTCPRcv);  
                  //std::cout<<"[SYS]TCPRcvry:setsockopt failed for setting RecvBufSize\n";
                }        
                set_nonblock(new_sd);
                snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|Connected for TCP Rcvry", new_sd);
                Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                //std::cout<<"FD "<<new_sd<<"|Connected for TCP Rcvry"<<std::endl;
                
                do
                {
                  numRead = recv(new_sd , &client_message.msgBuffer, 2048, 0);
                }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
            
                if(numRead == 0)
                {
                  shutdown(new_sd, SHUT_RDWR);
                  close(new_sd);
                 
                  snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|Zero bytes read",new_sd );
                  Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                  //std::cout<<"FD "<<new_sd<<"|TCPRcvry|Zero bytes read."<<std::endl;
                  continue;
                }
                
                if (numRead > 0)
                {
                    BROADCAST_DATA BroadcastData;
                    RECOVERY_REQ* rcvryReq = (RECOVERY_REQ*)(client_message.msgBuffer);
                    snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|TCPRcvry req|%c|%d|%d|%d",new_sd,rcvryReq->cMsgType,rcvryReq->wStreamID,rcvryReq->nBegSeqNo,rcvryReq->nEndSeqNo);
                      Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                    //std::cout<<"FD "<<new_sd<<"|TCPRcvry req|"<<rcvryReq->cMsgType<<"|"<<rcvryReq->wStreamID<<"|"<<rcvryReq->nBegSeqNo<<"|"<<rcvryReq->nEndSeqNo<<std::endl;
                    if (rcvryReq->cMsgType == RECOVERY_REQUEST &&
                        rcvryReq->wStreamID == StreamID)
                    {
                        std::ifstream readFile;
                        readFile.open("TCPRecovery", std::ios::in);
                        
                        /*First send recovery success msg*/
                        RECOVERY_RESP rcvryResp = {0};
                        rcvryResp.cMsgType = RECOVERY_RESPONSE;
                        rcvryResp.cReqStatus = REQUEST_SUCCESS;
                        rcvryResp.header.nSeqNo = 1;
                        rcvryResp.header.wMsgLen = sizeof (RECOVERY_RESP);
                        rcvryResp.header.wStremID = 1;
                        
                        //int i1 = write( new_sd , (char *)&rcvryResp , sizeof(RECOVERY_RESP));
                        int i1 = SendPacket(new_sd, (char *)&rcvryResp , sizeof(RECOVERY_RESP));
                        
                        if (readFile.is_open())
                        {
                            while (bExit == false && readFile.read(buf, sizeof(BroadcastData.stBcastMsg)))
                            {
                                MSG_HEADER *pmsg_hdr = (MSG_HEADER*)buf;
                                snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|SeqNo in file %d",new_sd,pmsg_hdr->nSeqNo);
                                 Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                                //std::cout<<"FD "<<new_sd<<"|SeqNo in file "<<pmsg_hdr->nSeqNo<<std::endl;
                                if (pmsg_hdr->nSeqNo >= rcvryReq->nBegSeqNo &&
                                    pmsg_hdr->nSeqNo <= rcvryReq->nEndSeqNo)
                                {
                                    switch(pmsg_hdr->cMsgType)
                                    {
                                      case NEW_ORDER:
                                      case ORDER_MODIFICATION:
                                      case ORDER_CANCELLATION:
                                      case SPREAD_NEW_ORDER:
                                      case SPREAD_ORDER_CANCELLATION:
                                      case SPREAD_ORDER_MODIFICATION:
                                      {
                                          //GENERIC_ORD_MSG* order = (GENERIC_ORD_MSG*)buf;
                                          GENERIC_ORD_MSG order = {0};
                                          memcpy (&order, buf, sizeof (GENERIC_ORD_MSG));
                                          snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|Sending TCPRcvry|Order# %ld|SeqNo %d",new_sd,int(order.dblOrdID),order.header.nSeqNo);
                                           Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                                          //std::cout<<"FD "<<new_sd<<"|Sending TCPRcvry|Order#"<<order.dblOrdID<<"|SeqNo "<<order.header.nSeqNo<<std::endl;
                                          //int i =  write (new_sd, (char *)&order , sizeof(GENERIC_ORD_MSG));
                                          int i =  SendPacket (new_sd, (char *)&order , sizeof(GENERIC_ORD_MSG));
                                      }
                                      break;
                                      case TRADE_MESSAGE:
                                      case SPREAD_TRADE_MESSAGE:
                                      {
                                          //TRD_MSG trade = (TRD_MSG)buf;
                                          TRD_MSG trade = {0};
                                          memcpy (&trade, buf, sizeof (TRD_MSG));
                                          snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|Sending TCPRcvry|Trade# %ld %ld|SeqNo %d",new_sd,int(trade.dblBuyOrdID),int(trade.dblSellOrdID),trade.header.nSeqNo);
                                            Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                                          //std::cout<<"FD "<<new_sd<<"|Sending TCPRcvry|Trade#"<<trade.dblBuyOrdID<<" "<<trade.dblSellOrdID<<"|SeqNo "<<trade.header.nSeqNo<<std::endl;
                                          //int i =  write (new_sd, (char *)&trade , sizeof(TRD_MSG));
                                           int i =  SendPacket (new_sd, (char *)&trade , sizeof(TRD_MSG));
                                     }
                                      break;
                                    }
                                }
                                else if (pmsg_hdr->nSeqNo > rcvryReq->nEndSeqNo)
                                {
                                    bExit = true;
                                }
                                memset (buf, 0, BUFFERSIZE);
                            }
                        }
                        else
                        {
                          snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|Couldn't open file TCPRecovery..!!");
                           Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                            //std::cout<<"Couldn't open file TCPRecovery..!!"<<std::endl;
                        }
                    }
                    else
                    {
                        RECOVERY_RESP rcvryResp;
                        rcvryResp.cMsgType = RECOVERY_RESPONSE;
                        rcvryResp.cReqStatus = REQUEST_ERROR;
                        rcvryResp.header.nSeqNo = 1;
                        rcvryResp.header.wMsgLen = sizeof (RECOVERY_RESP);
                        rcvryResp.header.wStremID = 1;
                        
                        snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|FD %d|Incorrect StreamID. Rejecting request",new_sd);
                         Logger::getLogger().log(DEBUG, logBufTCPRcv); 
                        //std::cout<<"FD "<<new_sd<<"|Incorrect StreamID. Rejecting request."<<std::endl;
                        
                       // int i1 = write( new_sd , (char *)&rcvryResp , sizeof(RECOVERY_RESP));
                        int i1 = SendPacket (new_sd , (char *)&rcvryResp , sizeof(RECOVERY_RESP));
                    }
                    close (new_sd);
                }
            }
        }
    }
    snprintf(logBufTCPRcv,400,"Thread_TCPRcvry|Exiting normally");
     Logger::getLogger().log(DEBUG, logBufTCPRcv); 
   //std::cout<<"\n\nExiting normally\n";
   return 0;
}


int SendPacket(int FD, char* msg, int msgLen)
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
         snprintf(logBufTCPRcv,500,"Thread_TCPRcvry|FD %d|SendPacket|Partial data sent|%d|%d|%d",FD, bytesSent, msgLen, errno);
         Logger::getLogger().log(DEBUG, logBufTCPRcv);
     }
     if (bytesSent == -1)
    {
          if(errno == EAGAIN || errno == EWOULDBLOCK)
          {
            usleep(10000);
            if (gMaxWriteRetry <= iWriteAttempt)
            {
               snprintf (logBufTCPRcv, 500, "Thread_TCPRcvry|FD %d|Disconnecting slow client|Error %d", FD, errno);
               Logger::getLogger().log(DEBUG, logBufTCPRcv);
               bExit = true;  
               retVal = -1;
               shutdown(FD, SHUT_RDWR);
               close(FD);
            }
             iWriteAttempt++;
          }
          else
          {
              snprintf(logBufTCPRcv, 500, "Thread_TCPRcvry|FD %d|Issue received|Drop connection| last error code|%d", FD, errno);
              Logger::getLogger().log(DEBUG, logBufTCPRcv);
              bExit = true;  
              retVal = -1;
              shutdown(FD, SHUT_RDWR);
              close(FD);
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


