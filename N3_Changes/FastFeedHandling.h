/* 
 * File:   FastFeedHandling.h
 * Author: NareshRK
 *
 * Created on October 8, 2018, 12:37 PM
 */

#ifndef FASTFEEDHANDLING_H
#define	FASTFEEDHANDLING_H
#include "FileAttributeAssignment.h"
#include "AllStructure.h"
#include "fcast_headers.h"
#include "spsc_atomic1.h"
#include "lzo1z.h"
#include "unordered_map"

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

struct FFOrderBook
{
  int32_t Token;
  int32_t LTP;
  Depth   oDep[5]; 
  
  FFOrderBook()
  {
    Token = 0;
    LTP   = 0;
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

typedef std::unordered_map<int,int > TokenIndexMap;
typedef TokenIndexMap::iterator IterTokenIndexMap;

class FastFeedHandler
{
  int iSockFd;
  char    *m_pcRecvData;
  int32_t m_iByteRemaining;
  int32_t m_iRecvDataSize;
  
public:
  
  FastFeedHandler()
  {
    
  }
  
  FastFeedHandler(int32_t );
  
  void ReceiveFFData();
  
  void ProcessFFData(TokenIndexMap& mpTokenIndex, FFOrderBook *oFFStore, int iServSockFd, struct sockaddr_in* groupSock);
  
  int CreateFFReciever( const char* pcIpaddress, int32_t iPortNumber, const char* pcIfaceIp );
  
  int creatFFeedServer(const char* lszIpAddress, const char* lszFcast_IpAddress, int32_t lnFcast_PortNumber, int32_t iTTLOpt, int32_t iTTLVal, struct sockaddr_in& groupSock);
  
  void HandleFFConnection(ConfigFile& oConfigdata, TOKENSUBSCRIPTION &oTokenSub, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain);
  
  int ProcessDepth(ExchangeStructsFO::NNF_MBP_PACKET *, TokenIndexMap& mpTokenIndex, FFOrderBook oFFStore[], int iServSockFd, struct sockaddr_in* groupSock);
};

#endif	/* BROADCASTHANDLING_H */

