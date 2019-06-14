/* 
 * File:   Thread_Broadcast.cpp
 * Author: muditsharma
 *
 * Created on March 1, 2016, 5:00 PM
 */

#include "Thread_Broadcast.h"
#include "BrodcastStruct.h"
#include "Thread_ME.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "fcast_headers.h"
#include "lzo1z.h"


char logBufBrdcst[200];
extern ORDER_BOOK_ME  ME_OrderBook;
int MaxTokenCount=0;
int _nMode=0;
uint64_t lastFFeedBroadCastTimeinMillies = 0;
NSE_FCAST_FO::GENERIC_BCAST_MESSAGE foFastFeedPacket;
NSE_FCAST_CM::GENERIC_BCAST_MESSAGE cmFastFeedPacket;
NSE_FCAST_CD::GENERIC_BCAST_MESSAGE cdFastFeedPacket;

NSE_FCAST_FO::NNF_MBP_PACKET foFFInternalPacket;
NSE_FCAST_CM::NNF_MBP_PACKET cmFFInternalPacket;
NSE_FCAST_CD::NNF_MBP_PACKET cdFFInternalPacket;

NSE_FCAST_FO::NNF_TICKER_TRADE_DATA foLTPInternalPacket;
NSE_FCAST_CM::NNF_TICKER_TRADE_DATA cmLTPInternalPacket;
NSE_FCAST_CD::NNF_TICKER_TRADE_DATA cdLTPInternalPacket;

NSE_FCAST_FO::MS_BCAST_CONT_MESSAGE foOutagePacket;
NSE_FCAST_CM::MS_BCAST_CONT_MESSAGE cmOutagePacket;
NSE_FCAST_CD::MS_BCAST_CONT_MESSAGE cdOutagePacket;

int32_t globalSeqNo = 0;
char workMem[LZO1Z_999_MEM_COMPRESS];

int creatFFeedSocket(const char* lszIpAddress, const char* lszFcast_IpAddress, int lnFcast_PortNumber, int32_t iTTLOpt, int32_t iTTLVal, struct sockaddr_in& groupSock){
    int ret;      
//    struct sockaddr_in groupSock;
    struct in_addr localInterface;
    ret = socket(AF_INET, SOCK_DGRAM, 0);
    if(ret < 0)
    {
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Opening datagram socket error");
        Logger::getLogger().log(DEBUG, logBufBrdcst);        
        exit(1);
    }
    else
    {
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Opening the datagram socket...OK");
        Logger::getLogger().log(DEBUG, logBufBrdcst);        
    }
 
    /* Initialize the group sockaddr structure with a */
    /* group address of 225.1.1.1 and port 5555. */
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr(lszFcast_IpAddress);
    groupSock.sin_port = htons(lnFcast_PortNumber);

    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */
    localInterface.s_addr = inet_addr(lszIpAddress);
    if(setsockopt(ret, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface)) < 0)
    {
       snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting local interface error");
       Logger::getLogger().log(DEBUG, logBufBrdcst);
       //printf("Thread_Boardcast:Setting local interface error");
     }
    else
    {  
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting the local interface %s...OK", lszIpAddress);
        Logger::getLogger().log(DEBUG, logBufBrdcst);
        //printf("Broadcast: Setting the local interface...OK\n");    
    }
    
    snprintf(logBufBrdcst, 200,"Thread_Boardcast|TTL Option %d|TTL Value %d", iTTLOpt, iTTLVal);
    Logger::getLogger().log(DEBUG, logBufBrdcst); 
    if (iTTLOpt == 1)
    {
        u_char ttl = iTTLVal;
        if (setsockopt(ret, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
        {
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL error");
          Logger::getLogger().log(DEBUG, logBufBrdcst);
        }
        else
        {
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL...OK");
          Logger::getLogger().log(DEBUG, logBufBrdcst);
        }
    }
    else if (iTTLOpt == 2)
    {
          int32_t TTL = iTTLVal;
          if(setsockopt(ret, IPPROTO_IP, IP_TTL, (char*)&TTL, sizeof(TTL)) < 0)
          {
              snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL error");
              Logger::getLogger().log(DEBUG, logBufBrdcst);
          }
          else
          {
            snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL...OK");
            Logger::getLogger().log(DEBUG, logBufBrdcst);
          }
    }
    return ret;
}

static inline int32_t getCurrentTimeMillis()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec)* 1000 + tv.tv_usec/1000);
}

static inline int64_t getCurrentTimeInNano()
{
    TS_VAR(currTime);
    GET_CURR_TIME(currTime);
    return TIMESTAMP(currTime);
}

void CheckAndSendFastFeedLTPData(int sockFd, struct sockaddr_in* groupSock)
{
    int32_t currentTimeInMillis = getCurrentTimeMillis();
    int32_t Token;
    int32_t LTP;
    unsigned char strInBuff[2048], strOutBuff[2048];
    lzo_uint nInSize = 0, nOutSize = 0;
    int64_t t0 = getCurrentTimeInNano();
    short currIndex = 0;
    short sent = 0;
    for(int i=0;i<MaxTokenCount;i++)
    {
      sent = 0;
      Token = ME_OrderBook.OrderBook[i].Token;
      LTP = ME_OrderBook.OrderBook[i].LTP;
      if (Token>0)
      {
        if (_nMode == SEG_NSECM)
        {
            if (Token>32767)
            {
                snprintf(logBufBrdcst, 200,"Thread_Boardcast|Skipping for token %d", Token);
                Logger::getLogger().log(DEBUG, logBufBrdcst);
            }
            else
            {
            cmLTPInternalPacket.TickerIndexData[currIndex].wToken = __bswap_16((short)Token);
            cmLTPInternalPacket.TickerIndexData[currIndex].lFillPrice = __bswap_32(LTP);                    
            cmLTPInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
            cmLTPInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
            currIndex++;
            }
            if (currIndex==28 || (i==MaxTokenCount-1))
            {
                cmLTPInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                memcpy(strInBuff, &cmLTPInternalPacket, sizeof(cmLTPInternalPacket));
                nInSize = sizeof(cmLTPInternalPacket);
                int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                if (compRetVal != 0)
                { 
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    return;
                }
                cmFastFeedPacket.wCompressionLen = nOutSize;
                cmFastFeedPacket.wCompressionLen = __bswap_16(cmFastFeedPacket.wCompressionLen);
                memcpy(&cmFastFeedPacket.compressedData, strOutBuff, nOutSize);
                if(sendto(sockFd,&cmFastFeedPacket, sizeof(cmFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                {
                    perror("Sending datagram message error");
                    std::cout << "Sending datagram message error" << std::endl;
                }    
                sent = currIndex;
                currIndex=0;
            }
        }
        else if (_nMode == SEG_NSEFO)
        {
            foLTPInternalPacket.TickerIndexData[currIndex].lToken = __bswap_32(Token);
            foLTPInternalPacket.TickerIndexData[currIndex].lFillPrice = __bswap_32(LTP);                    
            foLTPInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
            foLTPInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
            currIndex++;
            if (currIndex==17 || (i==MaxTokenCount-1))
            {
                foLTPInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                memcpy(strInBuff, &foLTPInternalPacket, sizeof(foLTPInternalPacket));
                nInSize = sizeof(foLTPInternalPacket);
                int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                if (compRetVal != 0)
                {
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    return;
                }
                foFastFeedPacket.wCompressionLen = nOutSize;
                foFastFeedPacket.wCompressionLen = __bswap_16(foFastFeedPacket.wCompressionLen);
                memcpy(&foFastFeedPacket.compressedData, strOutBuff, nOutSize);
                if(sendto(sockFd,&foFastFeedPacket, sizeof(foFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                {
                    perror("Sending datagram message error");
                    std::cout << "Sending datagram message error" << std::endl;
                }    
                sent = currIndex;
                currIndex=0;
            }
        }
        else if (_nMode == SEG_NSECD)
        {
            cdLTPInternalPacket.TickerIndexData[currIndex].lToken = __bswap_32(Token);
            cdLTPInternalPacket.TickerIndexData[currIndex].lFillPrice = __bswap_32(LTP);                    
            cdLTPInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
            cdLTPInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
            currIndex++;
            if (currIndex==17 || (i==MaxTokenCount-1))
            {
                cdLTPInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                memcpy(strInBuff, &cdLTPInternalPacket, sizeof(cdLTPInternalPacket));
                nInSize = sizeof(cdLTPInternalPacket);
                int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                if (compRetVal != 0)
                {
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    return;
                }
                cdFastFeedPacket.wCompressionLen = nOutSize;
                cdFastFeedPacket.wCompressionLen = __bswap_16(cdFastFeedPacket.wCompressionLen);
                memcpy(&cdFastFeedPacket.compressedData, strOutBuff, nOutSize);
                if(sendto(sockFd,&cdFastFeedPacket, sizeof(cdFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                {
                    perror("Sending datagram message error");
                    std::cout << "Sending datagram message error" << std::endl;
                }    
                sent = currIndex;
                currIndex=0;
            }
        }
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed LTP|%d|Segment %d|Token %d|LTP %d |Comp Size %d| Sent %d", globalSeqNo, _nMode, Token, LTP, nOutSize, sent);
        Logger::getLogger().log(DEBUG, logBufBrdcst);        
      }
    }
    int64_t t1 = getCurrentTimeInNano();
    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed LTP processing Time: %ld", t1-t0);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
}
void CheckAndSendFastFeedData(int sockFd, struct sockaddr_in* groupSock)
{
    int32_t currentTimeInMillis = getCurrentTimeMillis();
    int32_t Token;
    int32_t LTP;
    unsigned char strInBuff[2048], strOutBuff[2048];
    lzo_uint nInSize = 0, nOutSize = 0;
    
    int32_t bestBid[5]    = {0};
    int32_t bestBidQty[5] = {0};
    int32_t bestAsk[5]    = {0};
    int32_t bestAskQty[5] = {0};
    const int iMarketDepthSize = 5;
    if (currentTimeInMillis - lastFFeedBroadCastTimeinMillies > 250)
    {
      int64_t t0 = getCurrentTimeInNano();
      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Timer hit...Token Count %d",MaxTokenCount);
      Logger::getLogger().log(DEBUG, logBufBrdcst);

      short currIndex = 0;
      short sent = 0;
      int liMktDptIdx = 0;
      int liOrdBkIdx  = 0;
      for(int i=0;i<MaxTokenCount;i++)
      {
        for(int it = 0 ; it < iMarketDepthSize ; it++)
        {
          bestAsk[it]     = 0;
          bestAskQty[it]  = 0;
        }

        for(int it = 0 ; it < iMarketDepthSize ; it++)
        {
          bestBid[it]     = 0;
          bestBidQty[it]  = 0;
        }

        sent = 0;

        Token = ME_OrderBook.OrderBook[i].Token;

        LTP = ME_OrderBook.OrderBook[i].LTP;
        bestBid[0]    = ME_OrderBook.OrderBook[i].Buy[0].lPrice;
        bestBidQty[0] = ME_OrderBook.OrderBook[i].Buy[0].lQty;

        bestAsk[0]    = ME_OrderBook.OrderBook[i].Sell[0].lPrice;
        bestAskQty[0] = ME_OrderBook.OrderBook[i].Sell[0].lQty;

        if (bestAsk[0] == 2147483647 || bestAsk[0] < 0)
        {
          bestAsk[0] = 0;
        }

        for (int liOrdBkIdx = 1; liOrdBkIdx < (ME_OrderBook.OrderBook[i].SellRecords); liOrdBkIdx++)
        {
          if(liMktDptIdx == 5)
          {
            break;
          }
//          std::cout<<"Ask Price::"<<ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lPrice<<"|Qty::"<<ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lQty<<std::endl;
          if(2147483647 == ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lPrice )
          {
            break;
          }
          else if(bestAsk[liMktDptIdx] == ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lPrice)
          {
            bestAskQty[liMktDptIdx] = bestAskQty[liMktDptIdx] + ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lQty;
          }
          else
          {
            liMktDptIdx++;
            bestAsk[liMktDptIdx] = ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lPrice;
            bestAskQty[liMktDptIdx] = ME_OrderBook.OrderBook[i].Sell[liOrdBkIdx].lQty;
          }
          
        }

        liMktDptIdx = 0;
        liOrdBkIdx = 1;
        
        for (int liOrdBkIdx = 1; liOrdBkIdx < (ME_OrderBook.OrderBook[i].BuyRecords); liOrdBkIdx++)
        {
          if(liMktDptIdx == 5)
          {
            break;
          }
//          std::cout<<"Bid Price::"<<ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lPrice<<"|Qty::"<<ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lQty<<"|OrderNo::"<<(int64_t)ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].OrderNo<<"|Index::"<<liOrdBkIdx<<std::endl;
          if(0 == ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lPrice)
          {
            break;
          }
          if(bestBid[liMktDptIdx] == ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lPrice)
          {
            bestBidQty[liMktDptIdx] = bestBidQty[liMktDptIdx] + ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lQty;
          }
          else
          {
            liMktDptIdx++;
            bestBid[liMktDptIdx]    = ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lPrice;
            bestBidQty[liMktDptIdx] = ME_OrderBook.OrderBook[i].Buy[liOrdBkIdx].lQty;
          }
          
        }


        if (Token > 0)
        {
          if (_nMode == SEG_NSECM)
          {
            if (Token > 32767)
            {
              snprintf(logBufBrdcst, 200,"Thread_Boardcast|Skipping for token %d", Token);
              Logger::getLogger().log(DEBUG, logBufBrdcst);
            }
            else
            {
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[0].lPrice = __bswap_32(bestBid[0]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[1].lPrice = __bswap_32(bestBid[1]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[2].lPrice = __bswap_32(bestBid[2]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[3].lPrice = __bswap_32(bestBid[3]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[4].lPrice = __bswap_32(bestBid[4]);

              cmFFInternalPacket.PriceData[currIndex].MBPRecord[0].lQty = __bswap_32(bestBidQty[0]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[1].lQty = __bswap_32(bestBidQty[1]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[2].lQty = __bswap_32(bestBidQty[2]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[3].lQty = __bswap_32(bestBidQty[3]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[4].lQty = __bswap_32(bestBidQty[4]);

              cmFFInternalPacket.PriceData[currIndex].MBPRecord[5].lPrice = __bswap_32(bestAsk[0]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[6].lPrice = __bswap_32(bestAsk[1]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[7].lPrice = __bswap_32(bestAsk[2]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[8].lPrice = __bswap_32(bestAsk[3]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[9].lPrice = __bswap_32(bestAsk[4]);

              cmFFInternalPacket.PriceData[currIndex].MBPRecord[5].lQty = __bswap_32(bestAskQty[0]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[6].lQty = __bswap_32(bestAskQty[1]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[7].lQty = __bswap_32(bestAskQty[2]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[8].lQty = __bswap_32(bestAskQty[3]);
              cmFFInternalPacket.PriceData[currIndex].MBPRecord[9].lQty = __bswap_32(bestAskQty[4]);


              cmFFInternalPacket.PriceData[currIndex].wToken = __bswap_16((short)Token);
              cmFFInternalPacket.PriceData[currIndex].lLTP = __bswap_32(LTP);                    
              cmFFInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
              cmFFInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
              currIndex++;
            }

            if (currIndex==2 || (i==MaxTokenCount-1))
            {
                cmFFInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                memcpy(strInBuff, &cmFFInternalPacket, sizeof(cmFFInternalPacket));
                nInSize = sizeof(cmFFInternalPacket);


                int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                if (compRetVal != 0)
                {
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    return;
                }

                cmFastFeedPacket.wCompressionLen = nOutSize;
                cmFastFeedPacket.wCompressionLen = __bswap_16(cmFastFeedPacket.wCompressionLen);
                memcpy(&cmFastFeedPacket.compressedData, strOutBuff, nOutSize);

                if(sendto(sockFd,&cmFastFeedPacket, sizeof(cmFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                {
                    perror("Sending datagram message error");
                    std::cout << "Sending datagram message error" << std::endl;
                }    

                sent = currIndex;
                currIndex=0;
            }
          }
          else if (_nMode == SEG_NSEFO)
          {
              foFFInternalPacket.PriceData[currIndex].MBPRecord[0].lPrice = __bswap_32(bestBid[0]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[1].lPrice = __bswap_32(bestBid[1]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[2].lPrice = __bswap_32(bestBid[2]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[3].lPrice = __bswap_32(bestBid[3]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[4].lPrice = __bswap_32(bestBid[4]);

              foFFInternalPacket.PriceData[currIndex].MBPRecord[0].lQty = __bswap_32(bestBidQty[0]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[1].lQty = __bswap_32(bestBidQty[1]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[2].lQty = __bswap_32(bestBidQty[2]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[3].lQty = __bswap_32(bestBidQty[3]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[4].lQty = __bswap_32(bestBidQty[4]);

              foFFInternalPacket.PriceData[currIndex].MBPRecord[5].lPrice = __bswap_32(bestAsk[0]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[6].lPrice = __bswap_32(bestAsk[1]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[7].lPrice = __bswap_32(bestAsk[2]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[8].lPrice = __bswap_32(bestAsk[3]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[9].lPrice = __bswap_32(bestAsk[4]);

              foFFInternalPacket.PriceData[currIndex].MBPRecord[5].lQty = __bswap_32(bestAskQty[0]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[6].lQty = __bswap_32(bestAskQty[1]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[7].lQty = __bswap_32(bestAskQty[2]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[8].lQty = __bswap_32(bestAskQty[3]);
              foFFInternalPacket.PriceData[currIndex].MBPRecord[9].lQty = __bswap_32(bestAskQty[4]);

//              std::cout<<"Token::"<<Token<<"|BestBid::"<<bestBid[0]<<"("<<bestBidQty[0] <<")"<<"|"<<bestBid[1]<<"("<<bestBidQty[1] <<")"<<"|"<<bestBid[2]<<"("<< bestBidQty[2]<<")"<<"|"<<bestBid[3]<<"("<< bestBidQty[3]<<")"<<"|"<<bestBid[4]<<"("<< bestBidQty[4]<<")"<<std::endl;
//              std::cout<<"Token::"<<Token<<"|BestAsk::"<<bestAsk[0]<<"("<< bestAskQty[0]<<")"<<"|"<<bestAsk[1]<<"("<< bestAskQty[1]<<")"<<"|"<<bestAsk[2]<<"("<< bestAskQty[2]<<")"<<"|"<<bestAsk[3]<<"("<< bestAskQty[3]<<")"<<"|"<<bestAsk[4]<<"("<< bestAskQty[4]<<")"<<std::endl;

              foFFInternalPacket.PriceData[currIndex].lToken = __bswap_32(Token);
              foFFInternalPacket.PriceData[currIndex].lLTP = __bswap_32(LTP);
              foFFInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
              foFFInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
              currIndex++;

              if (currIndex==2 || (i==MaxTokenCount-1))
              {
                  foFFInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                  memcpy(strInBuff, &foFFInternalPacket, sizeof(foFFInternalPacket));
                  nInSize = sizeof(foFFInternalPacket);


                  int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                  if (compRetVal != 0)
                  {
                      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                      Logger::getLogger().log(DEBUG, logBufBrdcst);
                      return;
                  }

                  foFastFeedPacket.wCompressionLen = nOutSize;
                  foFastFeedPacket.wCompressionLen = __bswap_16(foFastFeedPacket.wCompressionLen);
                  memcpy(&foFastFeedPacket.compressedData, strOutBuff, nOutSize);

                  if(sendto(sockFd,&foFastFeedPacket, sizeof(foFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                  {
                      perror("Sending datagram message error");
                      std::cout << "Sending datagram message error" << std::endl;
                  }    

                  currIndex=0;
              }
          }
          
          else if (_nMode == SEG_NSECD)
          {
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[0].lPrice = __bswap_32(bestBid[0]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[1].lPrice = __bswap_32(bestBid[1]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[2].lPrice = __bswap_32(bestBid[2]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[3].lPrice = __bswap_32(bestBid[3]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[4].lPrice = __bswap_32(bestBid[4]);

              cdFFInternalPacket.PriceData[currIndex].MBPRecord[0].lQty = __bswap_32(bestBidQty[0]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[1].lQty = __bswap_32(bestBidQty[1]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[2].lQty = __bswap_32(bestBidQty[2]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[3].lQty = __bswap_32(bestBidQty[3]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[4].lQty = __bswap_32(bestBidQty[4]);

              cdFFInternalPacket.PriceData[currIndex].MBPRecord[5].lPrice = __bswap_32(bestAsk[0]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[6].lPrice = __bswap_32(bestAsk[1]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[7].lPrice = __bswap_32(bestAsk[2]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[8].lPrice = __bswap_32(bestAsk[3]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[9].lPrice = __bswap_32(bestAsk[4]);

              cdFFInternalPacket.PriceData[currIndex].MBPRecord[5].lQty = __bswap_32(bestAskQty[0]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[6].lQty = __bswap_32(bestAskQty[1]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[7].lQty = __bswap_32(bestAskQty[2]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[8].lQty = __bswap_32(bestAskQty[3]);
              cdFFInternalPacket.PriceData[currIndex].MBPRecord[9].lQty = __bswap_32(bestAskQty[4]);

//              std::cout<<"Token::"<<Token<<"|BestBid::"<<bestBid[0]<<"("<<bestBidQty[0] <<")"<<"|"<<bestBid[1]<<"("<<bestBidQty[1] <<")"<<"|"<<bestBid[2]<<"("<< bestBidQty[2]<<")"<<"|"<<bestBid[3]<<"("<< bestBidQty[3]<<")"<<"|"<<bestBid[4]<<"("<< bestBidQty[4]<<")"<<std::endl;
//              std::cout<<"Token::"<<Token<<"|BestAsk::"<<bestAsk[0]<<"("<< bestAskQty[0]<<")"<<"|"<<bestAsk[1]<<"("<< bestAskQty[1]<<")"<<"|"<<bestAsk[2]<<"("<< bestAskQty[2]<<")"<<"|"<<bestAsk[3]<<"("<< bestAskQty[3]<<")"<<"|"<<bestAsk[4]<<"("<< bestAskQty[4]<<")"<<std::endl;

              cdFFInternalPacket.PriceData[currIndex].lToken = __bswap_32(Token);
              cdFFInternalPacket.PriceData[currIndex].lLTP = __bswap_32(LTP);
              cdFFInternalPacket.bcastHeader.lLogTime = __bswap_32(currentTimeInMillis);
              cdFFInternalPacket.bcastHeader.lBCSeqNo = __bswap_32(++globalSeqNo%2000000000);
              currIndex++;

              if (currIndex==2 || (i==MaxTokenCount-1))
              {
                  cdFFInternalPacket.wNoOfRecords = __bswap_16(currIndex);
                  memcpy(strInBuff, &cdFFInternalPacket, sizeof(cdFFInternalPacket));
                  nInSize = sizeof(cdFFInternalPacket);


                  int compRetVal = lzo1z_999_compress(strInBuff, nInSize, strOutBuff, &nOutSize, (void*) workMem);
                  if (compRetVal != 0)
                  {
                      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Compression failed %d");
                      Logger::getLogger().log(DEBUG, logBufBrdcst);
                      return;
                  }

                  cdFastFeedPacket.wCompressionLen = nOutSize;
                  cdFastFeedPacket.wCompressionLen = __bswap_16(cdFastFeedPacket.wCompressionLen);
                  memcpy(&cdFastFeedPacket.compressedData, strOutBuff, nOutSize);

                  if(sendto(sockFd,&cdFastFeedPacket, sizeof(cdFastFeedPacket), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
                  {
                      perror("Sending datagram message error");
                      std::cout << "Sending datagram message error" << std::endl;
                  }    

                  currIndex=0;
              }
          }
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed|%d|Segment %d|Token %d|BidPrice(Qty) %d(%d)|%d(%d)|%d(%d)|%d(%d)|%d(%d)|LTP %d |compSize %d|sent %d", globalSeqNo, _nMode, Token, 
            bestBid[0],bestBidQty[0], bestBid[1],bestBidQty[1], bestBid[2],bestBidQty[2], bestBid[3],bestBidQty[3], bestBid[4],bestBidQty[4], LTP, nOutSize, sent);
          Logger::getLogger().log(DEBUG, logBufBrdcst);        
          
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed|%d|Segment %d|Token %d|AskPrice(Qty) %d(%d)|%d(%d)|%d(%d)|%d(%d)|%d(%d)|LTP %d |compSize %d|sent %d", globalSeqNo, _nMode, Token,
            bestAsk[0], bestAskQty[0], bestAsk[1], bestAskQty[1], bestAsk[2], bestAskQty[2], bestAsk[3], bestAskQty[3], bestAsk[4], bestAskQty[4], LTP, nOutSize, sent);
          Logger::getLogger().log(DEBUG, logBufBrdcst); 
        }
      }
      CheckAndSendFastFeedLTPData(sockFd, groupSock);
      lastFFeedBroadCastTimeinMillies = currentTimeInMillis;
      int64_t t1 = getCurrentTimeInNano();
      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed processing Time: %ld", t1-t0);
      Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
}

void StartBroadcast(ProducerConsumerQueue<BROADCAST_DATA>* qptr, const char* lszIpAddress, const char* lszBroadcast_IpAddress, int lnBroadcast_PortNumber, int iBrdcstCore, int32_t iTTLOpt, int32_t iTTLVal, short StreamID, bool startFF, const char* lszFcast_IpAddress, int lnFcast_PortNumber, int pTokenCount, int pMode)
{
    snprintf(logBufBrdcst, 200, "Thread_Boardcast|Broadcast started");
    Logger::getLogger().log(DEBUG, logBufBrdcst);	
    BROADCAST_DATA BroadcastData;
    struct in_addr localInterface;
    struct sockaddr_in groupSock;
    struct sockaddr_in fcgroupSock;
    int sd;      
    bool isFileOpen = false;
    bool isTCPFileOpen = false;
    char buf[2000] = {0};
    
    TaskSetCPU(iBrdcstCore);
    
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0)
    {
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Opening datagram socket error");
        Logger::getLogger().log(DEBUG, logBufBrdcst);
        exit(1);
    }
    else
    {
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Opening the datagram socket...OK");
        Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
 
    /* Initialize the group sockaddr structure with a */
    /* group address of 225.1.1.1 and port 5555. */
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr(lszBroadcast_IpAddress);
    groupSock.sin_port = htons(lnBroadcast_PortNumber);


    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */
    localInterface.s_addr = inet_addr(lszIpAddress);
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface)) < 0)
    {
       snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting local interface error");
       Logger::getLogger().log(DEBUG, logBufBrdcst);
       //printf("Thread_Boardcast:Setting local interface error");
     }
    else
    {  
        snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting the local interface...OK");
        Logger::getLogger().log(DEBUG, logBufBrdcst);
        //printf("Broadcast: Setting the local interface...OK\n");    
    }
    
    snprintf(logBufBrdcst, 200,"Thread_Boardcast|TTL Option %d|TTL Value %d", iTTLOpt, iTTLVal);
    Logger::getLogger().log(DEBUG, logBufBrdcst); 
    if (iTTLOpt == 1)
    {
        u_char ttl = iTTLVal;
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
        {
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL error");
          Logger::getLogger().log(DEBUG, logBufBrdcst);
        }
        else
        {
          snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL...OK");
          Logger::getLogger().log(DEBUG, logBufBrdcst);
        }
    }
    else if (iTTLOpt == 2)
    {
          int32_t TTL = iTTLVal;
          if(setsockopt(sd, IPPROTO_IP, IP_TTL, (char*)&TTL, sizeof(TTL)) < 0)
          {
              snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL error");
              Logger::getLogger().log(DEBUG, logBufBrdcst);
          }
          else
          {
            snprintf(logBufBrdcst, 200,"Thread_Boardcast|Setting TTL...OK");
            Logger::getLogger().log(DEBUG, logBufBrdcst);
          }
    }
     
    std::ofstream brdcstFile;
    brdcstFile.open ("BroadcastLog", std::ios_base::out);
    
    if (brdcstFile.is_open())
    {
        isFileOpen = true;
    }
    else
    {
      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Could not open file BroadcastLog");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
      //std::cout<<"Could not open file BroadcastLog"<<std::endl;
      isFileOpen = false;
      brdcstFile.close();
    }
    
    std::ofstream tcpRcvryFile;
    tcpRcvryFile.open ("TCPRecovery", std::ios_base::out);
    
    if (tcpRcvryFile.is_open())
    {
        isTCPFileOpen = true;
    }
    else
    {
      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Could not open file TCPRecovery");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
      isTCPFileOpen = false;
      tcpRcvryFile.close();
    }
    
    int FFSockId = 0;
    if (startFF)
    {
        FFSockId = creatFFeedSocket(lszIpAddress, lszFcast_IpAddress, lnFcast_PortNumber, iTTLOpt, iTTLVal, fcgroupSock);
        MaxTokenCount = pTokenCount;
        _nMode = pMode;
        
        cmFastFeedPacket.wNoOfPackets = __bswap_16((short)1);
        foFastFeedPacket.wNoOfPackets = __bswap_16((short)1);
        cdFastFeedPacket.wNoOfPackets = __bswap_16((short)1);
        
        cmFFInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7208);
        cmFFInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_CM::NNF_MBP_PACKET));
        cmFFInternalPacket.bcastHeader.lReserved1 = 999999;
        cmFFInternalPacket.bcastHeader.lReserved2 = 999999;
        cmFFInternalPacket.bcastHeader.strAlphaChar[0] = 'C';
        cmFFInternalPacket.bcastHeader.strAlphaChar[1] = 'M';
        strncpy(cmFFInternalPacket.bcastHeader.strTimestamp, "test123", 8);
        
        foFFInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7208);
        foFFInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_FO::NNF_MBP_PACKET));
        foFFInternalPacket.bcastHeader.wReserved1 = 999;
        foFFInternalPacket.bcastHeader.wReserved2 = 999;
        foFFInternalPacket.bcastHeader.lReserved2 = 999999;
        foFFInternalPacket.bcastHeader.strAlphaChar[0] = 'F';
        foFFInternalPacket.bcastHeader.strAlphaChar[1] = 'O';
        strncpy(foFFInternalPacket.bcastHeader.strTimestamp, "test123", 8);        
        
        
        cdFFInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7208);
        cdFFInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_CD::NNF_MBP_PACKET));
        cdFFInternalPacket.bcastHeader.wReserved1 = 999;
        cdFFInternalPacket.bcastHeader.wReserved2 = 999;
        cdFFInternalPacket.bcastHeader.lReserved2 = 999999;
        cdFFInternalPacket.bcastHeader.strAlphaChar[0] = 'C';
        cdFFInternalPacket.bcastHeader.strAlphaChar[1] = 'D';
        strncpy(cdFFInternalPacket.bcastHeader.strTimestamp, "test123", 8);        
        
        
        cmLTPInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7202);
        cmLTPInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_CM::NNF_TICKER_TRADE_DATA));
        foLTPInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7202);
        foFFInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_FO::NNF_TICKER_TRADE_DATA));
        
        cdLTPInternalPacket.bcastHeader.wTransCode = __bswap_16((short)7202);
        cdFFInternalPacket.bcastHeader.wMsgLen = __bswap_16((short)sizeof(NSE_FCAST_CD::NNF_TICKER_TRADE_DATA));
        
      snprintf(logBufBrdcst, 200,"Thread_Boardcast|Fast Feed Thread %d|Intf %s| FC IP %s| PC Port%d", FFSockId, lszIpAddress, lszFcast_IpAddress, lnFcast_PortNumber);
      Logger::getLogger().log(DEBUG, logBufBrdcst);
        
    }
    
//    int noopCount = 0;
    int noopCount = -1;
    while(1)
    {
        if(qptr->dequeue(BroadcastData))
        {
            noopCount=0;
            if(BroadcastData.wPacketType == 1)
            {
                GENERIC_ORD_MSG BroadcastDataPrint;
                BroadcastData.stBcastMsg.stGegenricOrdMsg.header.wStremID = StreamID;
                BroadcastData.stBcastMsg.stGegenricOrdMsg.header.wMsgLen = sizeof(GENERIC_ORD_MSG);
                if(sendto(sd,&BroadcastData.stBcastMsg.stGegenricOrdMsg, sizeof(BroadcastData.stBcastMsg.stGegenricOrdMsg), 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
                {
                    perror("Sending datagram message error");
                }
                else
                {
                    BroadcastDataPrint =(GENERIC_ORD_MSG)BroadcastData.stBcastMsg.stGegenricOrdMsg;
                          
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Seq# %d|Msg Type %c|Token %ld|Buy / Sell %c|Order Number %f|Price %d| Qty %d",
                                   BroadcastDataPrint.header.nSeqNo, BroadcastDataPrint.cMsgType, BroadcastDataPrint.nToken, BroadcastDataPrint.cOrdType,
                                   BroadcastDataPrint.dblOrdID, BroadcastDataPrint.nPrice, BroadcastDataPrint.nQty);
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    /*std::cout <<" Seq# " << BroadcastDataPrint.header.nSeqNo
                            << " Msg Type  " <<  BroadcastDataPrint.cMsgType
                            << " Token " << BroadcastDataPrint.nToken
                            << " Buy / Sell " << BroadcastDataPrint.cOrdType                            
                            << " Order Number " << BroadcastDataPrint.dblOrdID
                            << " Price " << BroadcastDataPrint.nPrice
                            << " Qty " << BroadcastDataPrint.nQty << std::endl;*/
                
                    if (isFileOpen)
                    {
                        memset(buf, 0, sizeof(buf));
                        sprintf(buf, "%d|%d|%d|%c|%lu|%f|%d|%c|%d|%d;",
                            BroadcastDataPrint.header.wMsgLen, BroadcastDataPrint.header.wStremID, BroadcastDataPrint.header.nSeqNo,
                            BroadcastDataPrint.cMsgType, BroadcastDataPrint.lTimeStamp,
                            BroadcastDataPrint.dblOrdID, BroadcastDataPrint.nToken,
                            BroadcastDataPrint.cOrdType, BroadcastDataPrint.nPrice, BroadcastDataPrint.nQty); 
                        brdcstFile<<buf;
                        brdcstFile.flush();
                    }
                    
                    //printf("Sending datagram message...OK\n");              
                }    
            }   
            else
            {
                TRD_MSG BroadcastDataPrint;
                BroadcastData.stBcastMsg.stTrdMsg.header.wMsgLen = sizeof (TRD_MSG);
                BroadcastData.stBcastMsg.stTrdMsg.header.wStremID = StreamID;
                if(sendto(sd,&BroadcastData.stBcastMsg.stTrdMsg, sizeof(BroadcastData.stBcastMsg.stTrdMsg), 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
                {
                    perror("Sending datagram message error");
                }
                else
                {
                    BroadcastDataPrint =(TRD_MSG)BroadcastData.stBcastMsg.stTrdMsg;
                            
                    snprintf(logBufBrdcst, 200,"Thread_Boardcast|Seq# %d|Msg Type %c|Token %ld|Buy Order Number  %f|Sell Order Number %f|Price %d| Qty %d",
                                   BroadcastDataPrint.header.nSeqNo, BroadcastDataPrint.cMsgType, BroadcastDataPrint.nToken, BroadcastDataPrint.dblBuyOrdID,
                                   BroadcastDataPrint.dblSellOrdID, BroadcastDataPrint.nTradePrice, BroadcastDataPrint.nTradeQty);
                    Logger::getLogger().log(DEBUG, logBufBrdcst);
                    /*std::cout <<" Seq# " << BroadcastDataPrint.header.nSeqNo
                            << " Msg Type  " <<  BroadcastDataPrint.cMsgType
                            << " Token " << BroadcastDataPrint.nToken
                            << " Buy Order Number " << BroadcastDataPrint.dblBuyOrdID
                            << " Sell Order Number " << BroadcastDataPrint.dblSellOrdID                         
                            << " Price " << BroadcastDataPrint.nTradePrice
                            << " Qty " << BroadcastDataPrint.nTradeQty << std::endl;*/
                    
                    if (isFileOpen)
                    {
                        memset(buf, 0, sizeof(buf));
                        sprintf(buf, "%d|%d|%d|%c|%lu|%f|%f|%d|%d|%d;",
                            BroadcastDataPrint.header.wMsgLen,BroadcastDataPrint.header.wStremID,BroadcastDataPrint.header.nSeqNo,
                            BroadcastDataPrint.cMsgType, BroadcastDataPrint.lTimestamp,
                            BroadcastDataPrint.dblBuyOrdID, BroadcastDataPrint.dblSellOrdID,
                            BroadcastDataPrint.nToken, BroadcastDataPrint.nTradePrice, BroadcastDataPrint.nTradeQty); 
                        brdcstFile<<buf;
                        brdcstFile.flush();
                    }
                    
                    //printf("Sending datagram message...OK\n");              
                }              
            }    
            /*Sneha*/
            if (isTCPFileOpen)
            {
                memset(buf, 0, sizeof(buf));   
                memcpy(buf,(void*)&BroadcastData.stBcastMsg, sizeof(BroadcastData.stBcastMsg));
                tcpRcvryFile.write(buf, sizeof(BroadcastData.stBcastMsg));
                tcpRcvryFile.flush();
            }
            if (FFSockId > 0)
            {
                CheckAndSendFastFeedData(FFSockId, &fcgroupSock);
            }
        }    
        else
        {
            //if (startFF && ++noopCount==100000 && FFSockId > 0)
            if (startFF && noopCount>=0 && ++noopCount==100000 && FFSockId > 0)
            {
                CheckAndSendFastFeedData(FFSockId, &fcgroupSock);
                noopCount=0;
            }
        }
    } 
    if (isFileOpen)
    {
        brdcstFile.close();
    }
} 

