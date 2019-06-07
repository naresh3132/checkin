/* 
 * File:   main.cpp
 * Author: tarunc
 *
 * Created on April 24, 2017, 6:12 PM
 */

//#include <cstdlib>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <assert.h>
#include <errno.h>
#include "lzo1z.h"
#include "fcast_headers.h"
#include <string.h>
#include <string>
#include <boost/lockfree/spsc_queue.hpp>
#include <thread>
#include <fstream>
#include "ConfigReader.h"
using namespace std;


#define CONFIG_FILE_NAME		"config.ini"

#define PORT 31495
#define MULTICAST_IP "228.2.2.2"

string lszIpAddress = "10.250.34.221";
/*
 * 
*/
int nSockMulticast;
int nRecvCount;
struct sockaddr_in server_addr;
struct ip_mreq multicastAddr;
int nSockState;
int m_CoreID;
int64_t llOldTS = 0;
int64_t llNewTS = 0;
FILE* fptrDumpData = NULL;




struct FCastData
{
  timespec RecvTime;
  char RecvData[512];
};
FCastData objFCastRecvData;
FCastData objFCastDumpData;
boost::lockfree::spsc_queue<FCastData, boost::lockfree::capacity<10000> > FCastInQ;
        
int ConnectSocket()
{
    char strErrorMsg[256] = {0};
    nSockMulticast = socket(AF_INET,SOCK_DGRAM,0);

    if(nSockMulticast < 0 )
    {
        sprintf(strErrorMsg,"Error while creating socket : %d\n",errno);
        std::cout<<strErrorMsg << std::endl;
        nSockState = 0;
        return -1;
    }

    /*if(setsockopt(nSockMulticast,IPPROTO_IP,IP_TTL,(char*)&nTTL, sizeof(nTTL)) < 0)
    {
        sprintf(strErrorMsg,"Error while setting TTL. Error number returned : %d\n",errno);
        std::cout<<strErrorMsg << std::endl;
        //PostLog(strErrorMsg,&LogErrBuffer);
        nSockState = 0;
        return -1;
    }	
    else*/
    {
        int nReuseAddr = 1; 
         if(setsockopt(nSockMulticast,SOL_SOCKET,SO_REUSEADDR,(char*)&nReuseAddr, sizeof(nReuseAddr)) < 0)
         {
            sprintf(strErrorMsg,"Error while setting REUSEADDR. Error number returned : %d\n",errno);
            std::cout<<strErrorMsg << std::endl;
            //PostLog(strErrorMsg,&LogErrBuffer);
            nSockState = 0;
            return -1;
         }

         if(bind(nSockMulticast, (struct sockaddr*)&server_addr, sizeof(server_addr))< 0)
        {
            sprintf(strErrorMsg,"Error while binding socket : %d\n",errno);
            std::cout<<strErrorMsg << std::endl;
            //PostLog(strErrorMsg,&LogErrBuffer); 
            nSockState = 0;
            return -1;
        }
        else
        {

            if(setsockopt(nSockMulticast,IPPROTO_IP, IP_ADD_MEMBERSHIP,&multicastAddr, sizeof(multicastAddr)) < 0)
            {
                sprintf(strErrorMsg,"Binding on port : %d\n", server_addr.sin_port);
                std::cout<<strErrorMsg << std::endl;
                //PostLog(strErrorMsg,&LogErrBuffer);
            }	
            else
            {
                sprintf(strErrorMsg,"Membership added successfully.\n");
                std::cout<<strErrorMsg << std::endl;
                nSockState = 1;
                return 0;
            }
        }
    }
}

int ProcessLTP(ExchangeStructsFO::NNF_TICKER_TRADE_DATA *pTickerTrdData)
{
//    gettimeofday(&objFCastData.RecvTime, NULL);
//    llNewTS = (objFCastData.RecvTime.tv_sec * 1000000000) + objFCastData.RecvTime.tv_usec;
//    std::cout << "DataArrDiff " << llNewTS - llOldTS << std::endl;
//    llOldTS = llNewTS;
//    return(0);
    
    int nRet = 0;
    pTickerTrdData->wNoOfRecords = (short)__bswap_16((unsigned short)pTickerTrdData->wNoOfRecords);
    pTickerTrdData->bcastHeader.lLogTime = (long32_t)__bswap_32((long32_t)pTickerTrdData->bcastHeader.lLogTime);        
    short wPktCount = (short) pTickerTrdData->wNoOfRecords;
    nRet = wPktCount;
    
    std::cout << "In ProcessLTP FO: " <<  wPktCount << std::endl;
    for(int i=0; i < wPktCount; i++)
    {
        std::cout<< "ProcessLTP::"
                 << "Token=" << __bswap_32(pTickerTrdData->TickerIndexData[i].lToken) 
                 << "|LTP=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lFillPrice)
                 << "|DayHi=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lDayHiOI)
                 << "|DayLow=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lDayLowOI)
                 << "|FillVol=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lFillVol)
                 << "|Open Int=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lOpenInterest)
                 << std::endl;
    }

    return nRet;
}

int ProcessLTP(ExchangeStructsCM::NNF_TICKER_TRADE_DATA *pTickerTrdData)
{
    int nRet = 0;
    pTickerTrdData->wNoOfRecords = (short)__bswap_16((unsigned short)pTickerTrdData->wNoOfRecords);
    pTickerTrdData->bcastHeader.lLogTime = (long32_t)__bswap_32((long32_t)pTickerTrdData->bcastHeader.lLogTime);        
    short wPktCount = (short) pTickerTrdData->wNoOfRecords;
    nRet = wPktCount;

    for(int i=0; i < wPktCount; i++)
    {
        std::cout<< "ProcessLTP::"
                 << "Token=" << (short)__bswap_16(pTickerTrdData->TickerIndexData[i].wToken) 
                 << "|LTP=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lFillPrice)
                 << "|FillVol=" << (long32_t)__bswap_32((long32_t)pTickerTrdData->TickerIndexData[i].lFillVol)
                 << std::endl;
    }

    return nRet;
}

void ProcessRecvData(const char* pRecvBuff, int32_t length, bool isCM=false) {
  
    unsigned char strInBuff[2048], strOutBuff[2048];
    int nSize = 0;
    lzo_uint nInSize  = 0, nOutSize = 0 ;
    int nCompLen = 0;
    size_t szExchgPreface = 8;
    char strErrorMsg[512] = {0};
    int nErrInfo;
    
    if (pRecvBuff == NULL || length<=0 )
    {
        std::cout << "ProcessRecvData: Invalid Buffer params" << std::endl;
    }

    ExchangeStructsFO::BCAST_HEADER *pBcastHeader = NULL;
    ExchangeStructsFO::BCAST_COMPRESSION *pCompPkt = NULL;
    ExchangeStructsFO::NNF_BCAST_HEADER *pPktHeader = NULL;

    pBcastHeader = (ExchangeStructsFO::BCAST_HEADER*)pRecvBuff;

    pBcastHeader->wNoOfPackets = __bswap_16((unsigned short)pBcastHeader->wNoOfPackets);
    short wData = pBcastHeader->wNoOfPackets;
    std::cout<<"wData::"<<wData<<std::endl;
    nSize = 0;
    nCompLen = 0;

    int defcase = 0;
    for(int nPacketCount = 0; (nPacketCount < wData) && !defcase; ++nPacketCount)
    {
      
        pPktHeader = NULL;
        pCompPkt = (ExchangeStructsFO::BCAST_COMPRESSION*)(pBcastHeader->cPackets + nSize);

        if(pCompPkt->wCompressionLen == 0)
        {
            pPktHeader = (ExchangeStructsFO::NNF_BCAST_HEADER*)(pCompPkt->cBroadcastData + szExchgPreface);
            pPktHeader->wTransCode = __bswap_16((unsigned short)pPktHeader->wTransCode);
            std::cout << "TxnCode " << pPktHeader->wTransCode << std::endl;
            
            pPktHeader->wMsgLen = __bswap_16((unsigned short)pPktHeader->wMsgLen);

            if(pPktHeader->wMsgLen > 512)
            {
                sprintf(strErrorMsg,"FO:Not and Exchange packet\n");
                std::cout<<strErrorMsg<<std::endl;
                 break;
            }
            nSize += pPktHeader->wMsgLen;
            nCompLen = pPktHeader->wMsgLen + sizeof(short);
        }
        else
        {
            
            pCompPkt->wCompressionLen = __bswap_16((unsigned short)pCompPkt->wCompressionLen);
            nCompLen = pCompPkt->wCompressionLen;
            memset(strInBuff,0,2048);
            memset(strOutBuff,0,2048);
            nInSize = pCompPkt->wCompressionLen;
            nOutSize = 0;
            nErrInfo = 0;
            if(nInSize <= 0 || nInSize > 512)
            {
              sprintf(strErrorMsg,"FO:Invalid Compression length. Len:%d", nInSize);
              std::cout<<strErrorMsg<<std::endl;
              break;
            }

            memcpy(strInBuff,pCompPkt->cBroadcastData,pCompPkt->wCompressionLen);

            nErrInfo = lzo1z_decompress(strInBuff,
                    nInSize, strOutBuff, &nOutSize, NULL);

            if(nErrInfo != 0)
            {
                sprintf(strErrorMsg,"FO:Error while lzo1z_decompress data : %d returned. %d\n",nErrInfo,nInSize);
                std::cout<<strErrorMsg<<std::endl;
                break;
            }
            else
            {
                pPktHeader = (ExchangeStructsFO::NNF_BCAST_HEADER*)(strOutBuff + szExchgPreface);
                pPktHeader->wTransCode = __bswap_16((unsigned short)pPktHeader->wTransCode);
//                std::cout << "TxnCode " << pPktHeader->wTransCode << std::endl;
                
                pPktHeader->wMsgLen = __bswap_16((unsigned short)pPktHeader->wMsgLen);
                nSize += nCompLen + sizeof(short);
            }
        }

//            snprintf(strErrorMsg, 500, "FO:TRANSCODE %d MsgLen %d CompLength %d", pPktHeader->wTransCode, pPktHeader->wMsgLen, pCompPkt->wCompressionLen); 
//            std::cout<<strErrorMsg<<std::endl;

        
        if (pPktHeader->wTransCode==7202)
        {
            if (isCM)
            {
                ExchangeStructsCM::NNF_TICKER_TRADE_DATA *pTickerTrdPkt = (ExchangeStructsCM::NNF_TICKER_TRADE_DATA*)(pPktHeader);
                ProcessLTP(pTickerTrdPkt);                
            }
            else
            {
                ExchangeStructsFO::NNF_TICKER_TRADE_DATA *pTickerTrdPkt = (ExchangeStructsFO::NNF_TICKER_TRADE_DATA*)(pPktHeader);
                ProcessLTP(pTickerTrdPkt);
            }
        }
        
    }
}


int32_t DumpToFile()
{
  std::cout << "Started thread DumpToFile" << std::endl;
  while(1)
  {
    if(FCastInQ.pop(objFCastDumpData) == false)
    {
      continue;
    }
    else
    {
      //std::cout << "Got Some Data, Pushing now.." << std::endl;
      fwrite((void*)&objFCastDumpData, 1, sizeof(FCastData), fptrDumpData);
      
    }
  }
  
  std::cout << "Leaving thread DumpToFile" << std::endl;
  
}

struct Depth
{
  int32_t BidPrice;
  int32_t BidQty;
  int32_t AskPrice;
  int32_t AskQty;
  
  Depth()
  {
    BidPrice = 0;
    BidQty   = 0;
    AskPrice = 0;
    AskQty   = 0;
  }
};

struct FFOrderBookTouchLine
{
  int32_t Token;
  int32_t LTP;
  Depth   oBestBidAsk; 
  
  FFOrderBookTouchLine()
  {
    Token = 0;
    LTP   = 0;
  }
};
void ProcessDepth1(const char* pRecvBuff, int32_t length, bool isCM=false) 
{
  FFOrderBookTouchLine *pTL;
  pTL = (FFOrderBookTouchLine*)pRecvBuff;
  
  std::cout<<pTL->oBestBidAsk.AskPrice<<"("<<pTL->oBestBidAsk.AskQty<<")"<<pTL->oBestBidAsk.BidPrice<<"("<<pTL->oBestBidAsk.BidQty<<")"<<std::endl;
  
}

int32_t main(int argc, char** argv)
{
    
//  if(argc < 2)
//  {
//    std::cout<<"USAGE::<BINARY> <MODE> for mode 1, <BINARY> <MODE> <FILE> for Mode 2"<<std::endl;
//    exit(1);  
//  }
  ConfigReader lszConfigReader(CONFIG_FILE_NAME);
  lszConfigReader.dump(); 
  
  std::string lszIpAddress = lszConfigReader.getProperty("LOCAL_IP_ADDRESS");
  std::string lszMulticast_IpAddress = lszConfigReader.getProperty("MODE2_MULTICAST_IP");
  int lnMulticast_PortNumber = atoi(lszConfigReader.getProperty("MODE2_MULTICAST_PORT").c_str());
  std::string lszMode1_Multicast_IpAddress = lszConfigReader.getProperty("MODE1_MULTICAST_IP");
  int lnMode1_Multicast_PortNumber = atoi(lszConfigReader.getProperty("MODE1_MULTICAST_PORT").c_str());
  int iDelayMode2 = atoi(lszConfigReader.getProperty("DELAY_MODE2").c_str());
  int Mode = atoi(lszConfigReader.getProperty("MODE").c_str());
  std::string FCastFile = lszConfigReader.getProperty("FCastFile");
  
  
  // User Params to be set
  cpu_set_t set;
  CPU_ZERO(&set);
  
  int32_t nCoreID = 6;
  CPU_SET(nCoreID, &set);   
  if(sched_setaffinity(0,sizeof(cpu_set_t), &set) < 0)
  {
    std::cout << "sched_setaffinity failed..." << std::endl;
  }
  else
  {
    std::cout << "sched_setaffinity success...Pinned to Core " << nCoreID << std::endl;
  }   
  char strErrorMsg[256] = {0};
//  if(stoi(argv[1])==1)
  if(Mode==1)
  {
    fptrDumpData = fopen("FCastDump.dat", "wb");
    std::thread thrdFCastDumper(DumpToFile);

    char strServerIP[16];// = "10.250.34.221";
    char strMCastIP[16];// = "228.1.1.1";

//    strcpy(strServerIP, "10.250.34.221");
//    strcpy(strMCastIP, "234.1.2.5");
    
    strcpy(strServerIP, lszIpAddress.c_str());
    strcpy(strMCastIP, lszMode1_Multicast_IpAddress.c_str());

    memset(&server_addr,'0',sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(lnMode1_Multicast_PortNumber);

    multicastAddr.imr_multiaddr.s_addr = inet_addr(strMCastIP);
    multicastAddr.imr_interface.s_addr = inet_addr(strServerIP);

    if (ConnectSocket() < 0)
    {
        nSockState = 0;
    }

    while(1)
    {
        nRecvCount = recvfrom(nSockMulticast, (void*)objFCastRecvData.RecvData, 512, 0,NULL, NULL);
        if(nRecvCount > 0)
        {
          std::cout<< "|RecvBytes " << nRecvCount <<"|"<<sizeof(objFCastRecvData)<< std::endl;  
//          ProcessDepth1(objFCastRecvData.RecvData, 512, false);
//          ProcessRecvData(objFCastRecvData.RecvData, 512, false);
          clock_gettime(CLOCK_REALTIME, &objFCastRecvData.RecvTime);
//          llNewTS = (objFCastData.RecvTime.tv_sec * 1000000000) + objFCastData.RecvTime.tv_usec;
//          std::cout << "DataArrDiff " << llNewTS - llOldTS << std::endl;
//          llOldTS = llNewTS;
          FCastInQ.push(objFCastRecvData);
        }
        else
        {
            sprintf(strErrorMsg,"Error returned while receiving data. Error code returned : %d\n",errno);
            std::cout<< strErrorMsg << std::endl;
        }        
    }

    thrdFCastDumper.join();
    fclose(fptrDumpData);
  }

  else if(Mode==2)
  {

     
    struct in_addr localInterface;
    struct sockaddr_in groupSock;
    struct ip_mreq mreq;
     
    std::cout<<"Mode 2"<<std::endl;
    int size_FCastData = sizeof(FCastData);
    char Buffer[size_FCastData];
    ExchangeStructsFO::BCAST_HEADER *pBcastHeader = NULL;
    
    ifstream FFFile(FCastFile, ios::in | ios::binary);
    std::cout<<"File name::"<<FCastFile<<std::endl;
    assert(FCastFile.c_str());
    struct hostent *hostp;
    char *hostaddrp;
    int fd,n;
    struct sockaddr_in myaddr;
   
    socklen_t addrlen = sizeof(myaddr);            
    int recvlen;

    if((fd =socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0)
    {
      perror("cannot create socket");
      return 0;
    }
    cout<<"file descriptor:: "<<fd<<endl;

    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = inet_addr(lszMulticast_IpAddress.c_str());
    myaddr.sin_port = htons(lnMulticast_PortNumber);

    
    localInterface.s_addr = inet_addr(lszIpAddress.c_str());
    if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface)) < 0)
    {
      printf("Setting local interface error");
      exit(1);
    }
    else
    {  
      printf("Setting the local interface...OK\n");    
    }
    
    
   
    u_char ttl = 64;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
      printf("Setting local interface error\n");
      exit(1);
    }
    else
    {
      printf("Setting the local interface...OK");
    }
    
    
    
    if (FFFile.is_open()) 
    {
      while(1)
      {
        FFFile.read(Buffer, size_FCastData);
        FCastData *fcast = (FCastData*)Buffer;
        usleep(iDelayMode2);
        if (!FFFile) 
        {
            std::cout << "ERROR : Only " << FFFile.gcount() << "bytes were able to be read out of "<< size_FCastData<< std::endl;
            break;
        }
        n = sendto(fd, &fcast->RecvData, 512, 0 , (struct sockaddr *)&myaddr, addrlen);
        if(n > 0)
          {
//            ProcessRecvData(fcast->RecvData, 512, false);
            std::cout<< "|RecvBytes " << n <<"|"<<sizeof(objFCastRecvData)<< std::endl;  
 
          }
          else
          {
              sprintf(strErrorMsg,"Error returned while Sending data. Error code returned : %d\n",errno);
              std::cout<< strErrorMsg << std::endl;
          }
      }

      FFFile.close();
    }
  }
  return 0;
}

