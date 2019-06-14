#ifndef _FCAST_HEADERS_
#define	_FCAST_HEADERS_

#include <stdio.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#pragma pack(2)

namespace NSE_FCAST_FO {

    ///FO
    typedef struct _BCAST_HEADER {
        char cNetID[2];
        short wNoOfPackets;
        char cPackets[512];
    }BCAST_HEADER;

    typedef struct _BCAST_COMPRESSION {
        short wCompressionLen;
        char cBroadcastData[512];
    }BCAST_COMPRESSION;
    
    typedef struct _NNF_MBP_INFO {
        int32_t lQty;
        int32_t lPrice;
        short wNoOfOrders;
        short wbbBuySellFlag;
    }NNF_MBP_INFO;

    typedef struct _NNF_MBP_INDICATOR {
        unsigned char Reserved : 4;
        unsigned char Sell : 1;
        unsigned char Buy : 1;
        unsigned char LastTradeLess : 1;
        unsigned char LastTradeMore : 1;
        unsigned char Reserved2;
    }NNF_MBP_INDICATOR;
    

    typedef struct _NNF_BCAST_HEADER {
        short wReserved1;
        short wReserved2;
        int32_t lLogTime;
        char strAlphaChar[2];
        short wTransCode;
        short wErrorCode;
        int32_t lBCSeqNo;
        int32_t lReserved2;
        char strTimestamp[8];
        char strFiller[8];
        short wMsgLen;
    }NNF_BCAST_HEADER;

    /*===========7208==============*/
    typedef struct _NNF_MBP_DATA {
        int32_t lToken;
        short wBookType;
        short wTradingStatus;
        int32_t lVolTraded;
        int32_t lLTP;
        char cNetChangeInd;
        int32_t lNetChangeClosing;
        int32_t lLTQ;
        int32_t lLTT;
        int32_t lATP;
        short wAuctionNo;
        short wAuctionStatus;
        short wInitiatorType;
        int32_t lInitiatorPrice;
        int32_t lInitiatorQty;
        int32_t lAutionPrice;
        int32_t lAuctionQty;
        NNF_MBP_INFO MBPRecord[10];
        short wBbTotalBuyFlag;
        short wBbTotalSellFlag;
        double dblTotalBuyQty;
        double dblTotalSellQty;
        NNF_MBP_INDICATOR nnfMBPInd;
        int32_t lClosePrice;
        int32_t lOpenPrice;
        int32_t lHighPrice;
        int32_t lLowPrice;
    }NNF_MBP_DATA;
    
    typedef struct _NNF_MBP_PACKET
    {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MBP_DATA PriceData[2];

    } NNF_MBP_PACKET;


    typedef struct _GENERIC_BCAST_MESSAGE{
        char cNetID[2];
        short wNoOfPackets;
        short wCompressionLen;
        char compressedData[510];
    }GENERIC_BCAST_MESSAGE;
  typedef struct _NNF_TICKER_INDEX_INFO
  {
      int32_t lToken;
      short wMarketType;
      int32_t lFillPrice;
      int32_t lFillVol;
      int32_t lOpenInterest;
      int32_t lDayHiOI;
      int32_t lDayLowOI;
  } NNF_TICKER_INDEX_INFO;
  typedef struct _NNF_TICKER_TRADE_DATA
  {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_TICKER_INDEX_INFO TickerIndexData[17];
  } NNF_TICKER_TRADE_DATA;
  
  
  typedef struct _MS_BCAST_CONT_MESSAGE
  {
    NNF_BCAST_HEADER bcastHeader;
    int16_t StreamNumber;
    int16_t Status;
    char Reserved[200];
  }MS_BCAST_CONT_MESSAGE;
}

///CM
namespace NSE_FCAST_CM {

    typedef struct _BCAST_HEADER {
        char cNetID[2];
        short wNoOfPackets;
        char cPackets[512];
    } BCAST_HEADER;

    typedef struct _BCAST_COMPRESSION {
        short wCompressionLen;
        char cBroadcastData[512];
    } BCAST_COMPRESSION;

    typedef struct _NNF_BCAST_HEADER {
        int32_t lReserved1;
        int32_t lLogTime;
        char strAlphaChar[2];
        short wTransCode;
        short wErrorCode;
        int32_t lBCSeqNo;
        int32_t lReserved2;
        char strTimestamp[8];
        char strFiller[8];
        short wMsgLen;
    } NNF_BCAST_HEADER;
    
    typedef struct _NNF_MBP_INFO {
        int32_t lQty;
        int32_t lPrice;
        short wNoOfOrders;
        short wbbBuySellFlag;
    } NNF_MBP_INFO;

    typedef struct _NNF_MBP_INDICATOR {
        unsigned char Reserved : 4;
        unsigned char Sell : 1;
        unsigned char Buy : 1;
        unsigned char LastTradeLess : 1;
        unsigned char LastTradeMore : 1;
        unsigned char Reserved2;
    } NNF_MBP_INDICATOR;
    

    typedef struct _NNF_MBP_DATA {
        short wToken;
        short wBookType;
        short wTradingStatus;
        int32_t lVolTraded;
        int32_t lLTP;
        char cNetChangeInd;
        int32_t lNetChangeClosing;
        int32_t lLTQ;
        int32_t lLTT;
        int32_t lATP;
        short wAuctionNo;
        short wAuctionStatus;
        short wInitiatorType;
        int32_t lInitiatorPrice;
        int32_t lInitiatorQty;
        int32_t lAutionPrice;
        int32_t lAuctionQty;
        NNF_MBP_INFO MBPRecord[10];
        short wBbTotalBuyFlag;
        short wBbTotalSellFlag;
        double dblTotalBuyQty;
        double dblTotalSellQty;
        NNF_MBP_INDICATOR nnfMBPInd;
        int32_t lClosePrice;
        int32_t lOpenPrice;
        int32_t lHighPrice;
        int32_t lLowPrice;
    } NNF_MBP_DATA;
    
    typedef struct _NNF_MBP_PACKET
    {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MBP_DATA PriceData[2];

    } NNF_MBP_PACKET;
 
    
  typedef struct _GENERIC_BCAST_MESSAGE{
      char cNetID[2];
      short wNoOfPackets;
      short wCompressionLen;
      char compressedData[510];
  }GENERIC_BCAST_MESSAGE;    
 
  typedef struct _NNF_TICKER_INDEX_INFO
  {
      short wToken;
      short wMarketType;
      int32_t lFillPrice;
      int32_t lFillVol;
      int32_t lMarketIndexVol;
  } NNF_TICKER_INDEX_INFO;
  typedef struct _NNF_TICKER_TRADE_DATA
  {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_TICKER_INDEX_INFO TickerIndexData[28];
  } NNF_TICKER_TRADE_DATA;
  
  typedef struct _MS_BCAST_CONT_MESSAGE
  {
    NNF_BCAST_HEADER bcastHeader;
    int16_t StreamNumber;
    int16_t Status;
    char Reserved[200];
  }MS_BCAST_CONT_MESSAGE;
}

namespace NSE_FCAST_CD {

    ///FO
    typedef struct _BCAST_HEADER {
        char cNetID[2];
        short wNoOfPackets;
        char cPackets[512];
    }BCAST_HEADER;

    typedef struct _BCAST_COMPRESSION {
        short wCompressionLen;
        char cBroadcastData[512];
    }BCAST_COMPRESSION;
    
    typedef struct _NNF_MBP_INFO {
        int32_t lQty;
        int32_t lPrice;
        short wNoOfOrders;
        short wbbBuySellFlag;
    }NNF_MBP_INFO;

    typedef struct _NNF_MBP_INDICATOR {
        unsigned char Reserved : 4;
        unsigned char Sell : 1;
        unsigned char Buy : 1;
        unsigned char LastTradeLess : 1;
        unsigned char LastTradeMore : 1;
        unsigned char Reserved2;
    }NNF_MBP_INDICATOR;
    

    typedef struct _NNF_BCAST_HEADER {
        short wReserved1;
        short wReserved2;
        int32_t lLogTime;
        char strAlphaChar[2];
        short wTransCode;
        short wErrorCode;
        int32_t lBCSeqNo;
        int32_t lReserved2;
        char strTimestamp[8];
        char strFiller[8];
        short wMsgLen;
    }NNF_BCAST_HEADER;

    /*===========7208==============*/
    typedef struct _NNF_MBP_DATA {
        int32_t lToken;
        short wBookType;
        short wTradingStatus;
        int32_t lVolTraded;
        int32_t lLTP;
        char cNetChangeInd;
        int32_t lNetChangeClosing;
        int32_t lLTQ;
        int32_t lLTT;
        int32_t lATP;
        short wAuctionNo;
        short wAuctionStatus;
        short wInitiatorType;
        int32_t lInitiatorPrice;
        int32_t lInitiatorQty;
        int32_t lAutionPrice;
        int32_t lAuctionQty;
        NNF_MBP_INFO MBPRecord[10];
        short wBbTotalBuyFlag;
        short wBbTotalSellFlag;
        double dblTotalBuyQty;
        double dblTotalSellQty;
        NNF_MBP_INDICATOR nnfMBPInd;
        int32_t lClosePrice;
        int32_t lOpenPrice;
        int32_t lHighPrice;
        int32_t lLowPrice;
    }NNF_MBP_DATA;
    
    typedef struct _NNF_MBP_PACKET
    {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MBP_DATA PriceData[2];

    } NNF_MBP_PACKET;


    typedef struct _GENERIC_BCAST_MESSAGE{
        char cNetID[2];
        short wNoOfPackets;
        short wCompressionLen;
        char compressedData[510];
    }GENERIC_BCAST_MESSAGE;
  typedef struct _NNF_TICKER_INDEX_INFO
  {
      int32_t lToken;
      short wMarketType;
      int32_t lFillPrice;
      int32_t lFillVol;
      int32_t lOpenInterest;
      int32_t lDayHiOI;
      int32_t lDayLowOI;
  } NNF_TICKER_INDEX_INFO;
  typedef struct _NNF_TICKER_TRADE_DATA
  {
      char systemHeader[8];
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_TICKER_INDEX_INFO TickerIndexData[17];
  } NNF_TICKER_TRADE_DATA;
  
  
  typedef struct _MS_BCAST_CONT_MESSAGE
  {
    NNF_BCAST_HEADER bcastHeader;
    int16_t StreamNumber;
    int16_t Status;
    char Reserved[200];
  }MS_BCAST_CONT_MESSAGE;
}

#endif

#pragma pack()