/* 
 * File:   fcast_headers.h
 * Author: NareshRK
 *
 * Created on October 8, 2018, 1:57 PM
 */

#ifndef FCAST_HEADERS_H
#define	FCAST_HEADERS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <string.h>
#pragma pack(2)
typedef unsigned int long32_t;    

namespace ExchangeStructsFO
{
  typedef struct _STD_HEADER
  {
    int nMsgCode;  
    short wMsgLen;
    long lSeconds;
    long lMicroSec;  
    long lSeqNo;
    long lExchangeTimestamp;
    char strExchgSeg[16]; 
    long lToken;
    long lEntrySeconds;
    long lEntryMicroSec;
    long lExchgSeqNo;
  } STD_HEADER;


  typedef struct _LTP_DATA_PKT
  {
      STD_HEADER header;
      char strSymbol[30];
      short wMarketStatus;
      long lLTP;
      long lLTQ;
      long lOpenInterest;
      long lDayHiOI;
      long lDayLowOI;
  } LTP_DATA_PKT;


  typedef struct _ORDER_BOOK
  {
      long lQty;
      long lPrice;
      int wNumOrders;
      short wBuySellFlag;
  } ORDER_BOOK;

  typedef struct _BID_ASK_PKT
  {
      STD_HEADER header;
      char strSymbol[30];
      double dblTBQ;
      double dblTSQ;
      int wNoOfRecords;
      ORDER_BOOK Records[10];
      double dblVolTradedToday;
      long lATP;
      long lLTT;
      long lNumOfTrades;
      long lTotalTradedVal;
      long lOpenPrice;
      long lClosePrice;
      long lLTP;
  } BID_ASK_PKT;

  typedef struct _INDEX_DATA
  {
      STD_HEADER header;
      char strIndexName[22];
      long lIndexValue;
      long lHighIndexValue;
      long lLowIndexValue;
      long lOpeningIndex;
      long lCloseingIndex;
      long lPercentChange;
      long lYearlyHigh;
      long lYearlyLow;
  } INDEX_DATA;

  typedef struct _LOG_HEADER
  {
      short wMsgCode;
      short wMsgLen;
  } LOG_HEADER;

  typedef struct _INIT_LOGGER
  {
    LOG_HEADER header;  
    char strPathFileName[488];
    char strClientID[20];
    _INIT_LOGGER()
    {
      header.wMsgCode = 100;
      header.wMsgLen = 512;
      memset(strPathFileName,'\0',488);
      memset(strClientID,'\0',20);
    }
  } INIT_LOGGER;

  typedef struct _LOG_DATA
  {
    LOG_HEADER header;
    char strLogData[2044];
    _LOG_DATA()
    {
      header.wMsgCode = 101;
      header.wMsgLen = 2048;
      memset(strLogData,'\0',2044);
    }
  } LOG_DATA;
 
  typedef struct _BCAST_HEADER
  {
      char cNetID[2];
      short wNoOfPackets;
      char  cPackets[512];
  } BCAST_HEADER; 

  typedef struct _BCAST_COMPRESSION
  {
    short wCompressionLen;
    char cBroadcastData[512];
  }BCAST_COMPRESSION;

  typedef struct _NNF_BCAST_HEADER
  {
    short wReserved1;
    short wReserved2;
    long32_t lLogTime;
    char strAlphaChar[2];
    short wTransCode;
    short wErrorCode;
    long32_t lBCSeqNo;
    long32_t lReserved2;
    char strTimestamp[8];
    char strFiller[8];
    short wMsgLen;
  } NNF_BCAST_HEADER;

  typedef struct _NNF_MESSAGE_HEADER
  {
      short wTransCode;
      long32_t lLogTime;
      char strAlphaChar[2];
      long32_t lTradeID;
      short wErrorCode;
      char strTimestamp[8];
      char strTimestamp1[8];
      char strTimestamp2[8];
      short wMsgLen;
  } NNF_MESSAGE_HEADER;

  typedef struct _NNF_MBP_INFO
  {
      long32_t lQty;
      long32_t lPrice;
      short wNoOfOrders;
      short wbbBuySellFlag;
  } NNF_MBP_INFO;

  typedef struct _NNF_MBP_INDICATOR
  {
      unsigned char Reserved:4;
      unsigned char Sell:1;
      unsigned char Buy:1;
      unsigned char LastTradeLess:1;
      unsigned char LastTradeMore:1;
      unsigned char Reserved2;
  } NNF_MBP_INDICATOR;
/*===========7201================*/

  typedef struct _NNF_MARKETWISE_INFO
  {
      NNF_MBP_INDICATOR nnfMBPInd;
      long32_t lBuyVolume;
      long32_t lBuyPrice;
      long32_t lSellVolume;
      long32_t lSellPrice;
      long32_t lLTP;
      long32_t lLTT;
  } NNF_MARKETWISE_INFO;

  typedef struct _NNF_MARKETWATCH_BROADCAST
  {
      long wToken;
      NNF_MARKETWISE_INFO nnfMarketInfo[3];
      long lOpenIntrest;
  } NNF_MARKETWATCH_BROADCAST;
  
  typedef struct __NNF_BCAST_INQUIRY_RESPONSE
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MARKETWATCH_BROADCAST InquiryRecord[5];      
  } NNF_BCAST_INQUIRY_RESPONSE;
/*===========7201================*/

/*===========7202================*/

  typedef struct _NNF_TICKER_INDEX_INFO
  {
      long32_t lToken;
      short wMarketType;
      long32_t lFillPrice;
      long32_t lFillVol;
      long32_t lOpenInterest;
      long32_t lDayHiOI;
      long32_t lDayLowOI;
  } NNF_TICKER_INDEX_INFO;

  typedef struct _NNF_TICKER_TRADE_DATA
  {
      //NNF_MESSAGE_HEADER bcastHeader;
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_TICKER_INDEX_INFO TickerIndexData[17];
  } NNF_TICKER_TRADE_DATA;

/*===========7202================*/

/*============7207==============*/

  typedef struct _NNF_INDEX_DATA
  {
      char strIndexName[21];
      long32_t lIndexValue;
      long32_t lHighIndexValue;
      long32_t lLowIndexValue;
      long32_t lOpeningIndex;
      long32_t lCloseingIndex;
      long32_t lPercentChange;
      long32_t lYearlyHigh;
      long32_t lYearlyLow;
      long32_t lNoOfUpMoves;
      long32_t lNoOfDownMoves;
      double dMarketCapitalisation;
      char cNetChangeIndicator;
      char cFiller;
  } NNF_INDEX_DATA;

  typedef struct _NNF_BCAST_INDICES
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_INDEX_DATA IndexData[6];
  } NNF_BCAST_INDICES;
  /*============7207==============*/


/*===========7208==============*/

  typedef struct _NNF_MBP_DATA
  {
      long32_t lToken;
      short wBookType;
      short wTradingStatus;
      long32_t lVolTraded;
      long32_t lLTP;
      char cNetChangeInd;
      long32_t lNetChangeClosing;
      long32_t lLTQ;
      long32_t lLTT;
      long32_t lATP;
      short wAuctionNo;
      short wAuctionStatus;
      short wInitiatorType;
      long32_t lInitiatorPrice;
      long32_t lInitiatorQty;
      long32_t lAutionPrice;
      long32_t lAuctionQty;
      NNF_MBP_INFO MBPRecord[10];
      short wBbTotalBuyFlag;
      short wBbTotalSellFlag;
      double dblTotalBuyQty;
      double dblTotalSellQty;
      NNF_MBP_INDICATOR nnfMBPInd;
          long32_t lClosePrice;
      long32_t lOpenPrice;
      long32_t lHighPrice;
      long32_t lLowPrice;
  } NNF_MBP_DATA;

  typedef struct _NNF_MBP_PACKET
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MBP_DATA PriceData[2];

  } NNF_MBP_PACKET;

/*====================7208======================*/

/*=====================7211======================*/
  typedef struct _MBP_SPD_INFO
  {
    short wNoOfOrders;
    long32_t lVolume;
    long32_t lPrice;  
  } MBP_SPD_INFO;

  typedef struct _TOTAL_SPD_ORD_VOL
  {
    double dblBuy;
    double dblSell;
  } TOTAL_SPD_ORD_VOL;

  typedef struct _NNF_MS_SPD_MKT_INFO
  {
    NNF_BCAST_HEADER bcastHeader;
    long32_t lToken1;
    long32_t lToken2;
    short wMBPBuy;
    short wMBPSell;
    long32_t lLastActiveTime;
    long32_t lTradedVolume;
    double dblTotalTradedValue;
    MBP_SPD_INFO MBPBuys[5];
    MBP_SPD_INFO MBPSells[5];
    TOTAL_SPD_ORD_VOL TotOrdVol;
    long32_t lOpenPriceDiff;
    long32_t lDayHiPriceDiff;
    long32_t lDayLowPriceDiff;
    long32_t lLastTradedPriceDiff;
    long32_t lLastUpdateTime;    
  } NNF_MS_SPD_MKT_INFO;
  /*===============================================*/
  
  /*==================== 7130 =====================*/
  /*========       MKT_MVMT_CM_OI_IN       ========*/
  typedef struct _NNF_OPEN_INTEREST
  {
    long32_t  lTokenNo;
    long32_t  lCurrentOI;
    
  }NNF_OPEN_INTEREST;
  
  typedef struct _NNF_CM_ASSET_OI
  {
    char        Reserved1[2];
    char        Reserved2[2];
    long32_t    lLogTime;
    char        cMarketType[2];
    short       nTransactionCode;
    short       nNoOfRecords;
    char        cReserved3[8];
    char        cTimeStamp[8];
    char        cReserved4[8];
    short       wMsgLen;
    NNF_OPEN_INTEREST openInterest[58];
    
  }NNF_NNF_CM_ASSET_OI;
  /*===============================================*/
  
  /*==================== 6501 =====================*/
  typedef struct _NNF_ST_BCAST_DESTINATION
  {
    unsigned char bReserved : 4;
    unsigned char bJrnlRequred : 1;
    unsigned char bTandem : 1;
    unsigned char bControlWorkstation : 1;
    unsigned char bTraderWorkstation : 1;
    char          cReserved; 
    
  }NNF_ST_BCAST_DESTINATION;
  
  typedef struct _NNF_GEN_BCAST_MESSAGE
  {
    NNF_MESSAGE_HEADER  bcastHeader;
    short       nBrancNumber;
    char        cBrokerNumber[5];
    char        cActionCode[3];
    NNF_ST_BCAST_DESTINATION destination;
    char        cReserved[26];
    short       nBroadcastMsgLen;
    char        cBroadcastMessage[239];
    
  }NNF_GEN_BCAST_MESSAGE;
  
  /*===============================================*/

  /*==================== 7305-Start ===============*/
  typedef struct _NNF_SEC_INFO
  {
      char        cInstrumentName[6];
      char        cSymbol[10];
      char        cSeries[2];
      long32_t    lExpiryDate;
      long32_t    lStrikePrice;
      char        cOptionType[2];
      short       iCALevel;
  }NNF_SEC_INFO;

  typedef struct _ST_SEC_ELIGIBILITY_PER_MKT
  {
      unsigned char   bResevered :7;
      unsigned char   bEligibility:1;
      short           iStatus;
  }ST_SEC_ELIGIBILITY;

  typedef struct _ST_ELIGIBILITY_INDICATOR
  {
      unsigned char   bResevered :5;
      unsigned char   bMinimumFill:1;
      unsigned char   bAON:1;
      unsigned char   bPartMarketIndex:1;
      char            cReserved;
  }ST_ELIGIBILITY_INDICATOR;

  typedef struct _NNF_MS_SECURITY_UPDATE_INFO
  {
      NNF_MESSAGE_HEADER  bcastHeader;
      long32_t            lToken;
      NNF_SEC_INFO        securityInfo;
      short               iPermitedToTrade;
      double              dIssueCapital;
      long32_t            lWarningQuantity;
      long32_t            lFreezeQuantity;
      char                cCreditRating[12];
      ST_SEC_ELIGIBILITY  stPerMktEligility[4];
      short               iIssueRate;
      long32_t            lIssueStartDate;
      long32_t            lInterestPaymentDate;
      long32_t            lIssueMaturityDate;
      long32_t            lMarginePercentage;
      long32_t            lMinLotQuantity;
      long32_t            lBoardLotQuantity;
      long32_t            lTockSize;
      char                cName[25];
      char                cFiller;
      long32_t            lListingDate;
      long32_t            lExpulsionDate;
      long32_t            lReAdmissionDate;
      long32_t            lRecordDate;
      long32_t            lLowPriceRange;
      long32_t            lHighPriceRange;
      long32_t            lExpiryDate;
      long32_t            lNoDeliveryStartDate;
      long32_t            lNoDeliveryEndDate;
      ST_ELIGIBILITY_INDICATOR stEligilityIndi;
      long32_t            lBookClosureStartDate;
      long32_t            lBookClosureEndDate;
      long32_t            lExerciseStartDate;
      long32_t            lExerciseEndDate;
      long32_t            lOldToken;
      char                cAssetInstrument[6];
      char                cAssetName[10];
      long32_t            lAssetToken;
      long32_t            lIntrinsicValue;
      long32_t            lExtrinsicValue;
      char                stPurpose[2];
      long32_t            lLocalUpdateDateTime;
      char                cDeleteFlag;
      char                cRemark[25];
      long32_t            lBasePrice;
  }NNF_MS_SECURITY_UPDATE_INFO;
/*====================  7305-End  ===============*/
  
/*==================== 7220-Start ===============*/
  typedef struct _TRADE_EXECUTION_RANGE_DETAILS
  {
    long32_t lToken;
    long32_t lHighExecBand;
    long32_t lLowExecBand;
  }TRADE_EXECUTION_RANGE_DETAILS;

  typedef struct _TRADE_EXECUTION_RANGE_DATA
  {
    long32_t lMsgCount;
    TRADE_EXECUTION_RANGE_DETAILS stTradeExecuationRangeDetails[25];
  }TRADE_EXECUTION_RANGE_DATA;
  
  typedef struct _NNF_MS_BCAST_TRADE_EXECUTION_RANGE
  {
    NNF_MESSAGE_HEADER  bcastHeader;
    TRADE_EXECUTION_RANGE_DATA stTradeExecutionRangeData;
  }NNF_MS_BCAST_TRADE_EXECUTION_RANGE;
/*==================== 7220-End  ===============*/

  typedef struct _OUTAGE_DATA
  {
    int16_t sStreamNo;
    int16_t sStatus;
    char chReserved[200];
  }OUTAGE_DATA;  
  
  
  typedef struct _NNF_MS_BCAST_CONT_MESSAGE
  {
    NNF_BCAST_HEADER  bcastHeader;
    OUTAGE_DATA stOutageData;
  }NNF_MS_BCAST_CONT_MESSAGE;
  
}  // namespace ExchangeStructsFO

namespace ExchangeStructsCM
{
  typedef struct _STD_HEADER
  {
    int nMsgCode;
    short wMsgLen;
    long lSeconds;
    long lMicroSec;
    long lSeqNo;
    long lExchgTimestamp;
    char strExchgSeg[16];
    long lToken;
    long lEntrySeconds;
    long lEntryMicroSec;
    long lExchgSeqNo;

    _STD_HEADER()
    {
      nMsgCode = 0;
      wMsgLen = 0;
      lSeconds = 0;
      lMicroSec = 0;
      lSeqNo = 0;
      lExchgTimestamp = 0;
      sprintf(strExchgSeg,"nse_cm");
      lToken = 0;
      lEntryMicroSec = 0;
      lEntrySeconds = 0;
      lExchgSeqNo = 0;
    }
  }STD_HEADER;

  typedef struct _LTP_DATA_PKT
  {
    STD_HEADER header;
    char strSymbol[30];
    short wMarketStatus;
    long lLTP;
    long lLTQ;
    long lOpenInterest;
    long lDayHiOI;
    long lDayLowOI;
    _LTP_DATA_PKT()
    {
      memset(strSymbol,0,30);
      wMarketStatus = 0;
      lLTP = 0;
      lLTQ = 0;
      lOpenInterest = 0;
      lDayHiOI = 0;
      lDayLowOI = 0;
    }
  } LTP_DATA_PKT;

  typedef struct _INDEX_DATA
  {
    STD_HEADER header;
    char strIndexName[22];
    long lIndexValue;
    long lHighIndexValue;
    long lLowIndexValue;
    long lOpeningIndex;
    long lCloseingIndex;
    long lPercentChange;
    long lYearlyHigh;
    long lYearlyLow;
    _INDEX_DATA()
    {
      memset(strIndexName,0,22);
      lIndexValue = 0;
      lHighIndexValue = 0;
      lLowIndexValue = 0;
      lOpeningIndex = 0;
      lCloseingIndex = 0;
      lPercentChange = 0;
      lYearlyHigh = 0;
      lYearlyLow = 0;
    }
  } INDEX_DATA;

  typedef struct _ORDER_BOOK
  {
    long lQty;
    long lPrice;
    int wNumOrders;
    short wBuySellFlag;
  } ORDER_BOOK;

  typedef struct _BID_ASK_PKT
  {
    STD_HEADER header;
    char strSymbol[30];
    double dblTBQ;
    double dblTSQ;
    int wNoOfRecords;
    ORDER_BOOK Records[10];
    double dblVolTradedToday;
    long lATP;
    long lLTT;
    long lNumOfTrades;
    long lTotalTradedVal;
    long lOpenPrice;
    long lClosePrice;
    long lLTP;
  } BID_ASK_PKT;

  typedef struct _LOG_HEADER
  {
    short wMsgCode;
    short wMsgLen;
  } LOG_HEADER;

  typedef struct _INIT_LOGGER
  {
    LOG_HEADER header;
    char strPathFileName[488];
    char strClientID[20];
    _INIT_LOGGER()
    {
      header.wMsgCode = 100;
      header.wMsgLen = 512;
      memset(strPathFileName,'\0',488);
      memset(strClientID,'\0',20);
    }
  } INIT_LOGGER;

  typedef struct _LOG_DATA
  {
    LOG_HEADER header;
    char strLogData[2044];
    _LOG_DATA()
    {
      header.wMsgCode = 101;
      header.wMsgLen = 2048;
      memset(strLogData,'\0',2044);
    }
  } LOG_DATA;

  typedef struct _BCAST_HEADER
  {
    char cNetID[2];
    short wNoOfPackets;
    char  cPackets[512];
  } BCAST_HEADER;

  typedef struct _BCAST_COMPRESSION
  {
    short wCompressionLen;
    char cBroadcastData[512];
  }BCAST_COMPRESSION;

  typedef struct _NNF_BCAST_HEADER
  {
    long32_t lReserved1;
    long32_t lLogTime;
    char strAlphaChar[2];
    short wTransCode;
    short wErrorCode;
    long32_t lBCSeqNo;
    long32_t lReserved2;
    char strTimestamp[8];
    char strFiller[8];
    short wMsgLen;
  } NNF_BCAST_HEADER;

  typedef struct _NNF_MESSAGE_HEADER
  {
      //long32_t lReserved;
    short wTransCode;
    long32_t lLogTime;
    char strAlphaChar[2];
    long32_t lTraderID;
    //short wTransCode;
    short wErrorCode;
    //long lReserved2;
    char strTimestamp[8];
    char strTimestamp1[8];
    char strTimestamp2[8];
    short wMsgLen;
  } NNF_MESSAGE_HEADER;

  typedef struct _NNF_MBP_INFO
  {
      long32_t lQty;
      long32_t lPrice;
      short wNoOfOrders;
      short wbbBuySellFlag;
  } NNF_MBP_INFO;

  typedef struct _NNF_MBP_INDICATOR
  {
      unsigned char Reserved:4;
      unsigned char Sell:1;
      unsigned char Buy:1;
      unsigned char LastTradeLess:1;
      unsigned char LastTradeMore:1;
      unsigned char Reserved2;
  } NNF_MBP_INDICATOR;


  /*===========7200================*/
  /*typedef struct _NNF_MBO_MBP_TERMS
  {
      unsigned char Reserved1:6;
      unsigned int nAon;
      unsigned int nMF
  } NNF_MBP_MBO_TERMS;*/
  /*===========7200================*/

  /*===========7201================*/

    typedef struct _NNF_MARKETWISE_INFO
    {
        NNF_MBP_INDICATOR nnfMBPInd;
        long32_t lBuyVolume;
        long32_t lBuyPrice;
        long32_t lSellVolume;
        long32_t lSellPrice;
        long32_t lLTP;
        long32_t lLTT;
    } NNF_MARKETWISE_INFO;

    typedef struct _NNF_MARKETWATCH_BROADCAST
    {
        short wToken;
        NNF_MARKETWISE_INFO nnfMarketInfo[3];
    } NNF_MARKETWATCH_BROADCAST;

    typedef struct __NNF_BCAST_INQUIRY_RESPONSE
    {
        NNF_BCAST_HEADER bcastHeader;
        short wNoOfRecords;
        NNF_MARKETWATCH_BROADCAST InquiryRecord[5];
    } NNF_BCAST_INQUIRY_RESPONSE;
  /*===========7201================*/

  /*===========7202================*/

  typedef struct _NNF_TICKER_INDEX_INFO
  {
      short wToken;
      short wMarketType;
      long32_t lFillPrice;
      long32_t lFillVol;
      long32_t lMarketIndexVol;
  } NNF_TICKER_INDEX_INFO;


  typedef struct _NNF_TICKER_TRADE_DATA
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_TICKER_INDEX_INFO TickerIndexData[28];
  } NNF_TICKER_TRADE_DATA;

  /*===========7202================*/

  /*============7207==============*/

  typedef struct _NNF_INDEX_DATA
  {
      char strIndexName[21];
      long32_t lIndexValue;
      long32_t lHighIndexValue;
      long32_t lLowIndexValue;
      long32_t lOpeningIndex;
      long32_t lClosingIndex;
      long32_t lPercentChange;
      long32_t lYearlyHigh;
      long32_t lYearlyLow;
      long32_t lNoOfUpMoves;
      long32_t lNoOfDownMoves;
      double dMarketCapitalisation;
      char cNetChangeIndicator;
      char cFiller;
  } NNF_INDEX_DATA;



  typedef struct _NNF_BCAST_INDICES
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_INDEX_DATA IndexData[6];
  } NNF_BCAST_INDICES;
  /*============7207==============*/


  /*===========7208==============*/

  typedef struct _NNF_MBP_DATA
  {
      short wToken;
      short wBookType;
      short wTradingStatus;
      long32_t lVolTraded;
      long32_t lLTP;
      char cNetChangeInd;
      long32_t lNetChangeClosing;
      long32_t lLTQ;
      long32_t lLTT;
      long32_t lATP;
      short wAuctionNo;
      short wAuctionStatus;
      short wInitiatorType;
      long32_t lInitiatorPrice;
      long32_t lInitiatorQty;
      long32_t lAutionPrice;
      long32_t lAuctionQty;
      NNF_MBP_INFO MBPRecord[10];
      short wBbTotalBuyFlag;
      short wBbTotalSellFlag;
      double dblTotalBuyQty;
      double dblTotalSellQty;
      NNF_MBP_INDICATOR nnfMBPInd;
      long32_t lClosePrice;
      long32_t lOpenPrice;
      long32_t lHighPrice;
      long32_t lLowPrice;
  } NNF_MBP_DATA;

  typedef struct _NNF_MBP_PACKET
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      NNF_MBP_DATA PriceData[2];

  } NNF_MBP_PACKET;

  /*====================7208======================*/

  /*====================7216======================*/

  typedef struct _INDICES_VIX
  {
      char strIndexname[21];
      long32_t lIndexVal;
      long32_t lHighIndexVal;
      long32_t lLowIndexVal;
      long32_t lOpenIndexVal;
      long32_t lCloseIndexVal;
      long32_t lPercentChange;
      long32_t lYearlyHigh;
      long32_t lYearlyLow;
      long32_t lNoOfUpMoves;
      long32_t lNoOfDownMoves;
      double dblMktCap;
      char cNetChgInd;
      char cFiller;

  } INDICES_VIX;

  typedef struct _BCAST_INDICES_VIX
  {
      NNF_BCAST_HEADER bcastHeader;
      short wNoOfRecords;
      INDICES_VIX Indices[6];
  } BCAT_INDICES_VIX;
/*====================7216======================*/
/*====================7215======================*/

}  // namespace ExchangeStructsCM;;;

#pragma pack()

#ifdef	__cplusplus
}
#endif

#endif	/* FCAST_HEADERS_H */

