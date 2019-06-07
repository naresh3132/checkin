/* 
 * File:   DealerMessage.h
 * Author: NareshRK
 *
 * Created on September 5, 2018, 10:30 AM
 */

#ifndef DEALERSTRUCTURE_H
#define	DEALERSTRUCTURE_H

const int DEALER_NOT_FOUND              = -4001;
const int DEALER_ALREADY_LOGGED_IN      = -4002;
const int TOKEN_NOT_FOUND               = -4003;
const int PRICE_RANGE_CHECK_FAIL        = -4004;

const int ORDER_NOT_FOUND               = -4010;
const int ADD_ORDER_PENDING             = -4011;
const int MOD_ORDER_PENDING             = -4012;
const int CAN_ORDER_PENDING             = -4013;
const int ORDER_FILLED                  = -4014;
const int MOD_REJECTED                  = -4015;
const int CAN_REJECTED                  = -4016;
const int PENDING                       = -4020;
//const int PARTIAL_FILLED                = -4017;

enum class Actionable
{
  FALSE = 0,
  TRUE  = 1
};


namespace DEALER
{
  #pragma pack(1)

  struct MSG_HEADER
  {
    int32_t iMsgLen;
    int16_t iSeg;
    int16_t iTransCode;
    int32_t iDealerID;
    int16_t iErrorCode;
    int32_t iSeqNo;
    int64_t llTimeStamp;
    
    MSG_HEADER()
    {
      iMsgLen         = 0;
      iSeg            = 0;
      iTransCode      = 0;
      iDealerID       = 0;
      iErrorCode      = 0;
      iSeqNo          = 0;
      llTimeStamp     = 0;
    }
  };
  
  struct LOG_IN_REQ
  {
    MSG_HEADER header;
    char cDealerName[32+1];
  };
  
  struct LOG_IN_RES
  {
    MSG_HEADER header;
  };
  
  struct LOG_OUT_REQ
  {
    MSG_HEADER header;
  };
  
  struct LOG_OUT_RES
  {
    MSG_HEADER header;
  };
  
  struct DOWNLOAD_START_NOTIFICATION
  {
    MSG_HEADER header;
  };
  
  
  struct CONTRACT_DOWNLOAD_START
  {
    MSG_HEADER header;
  };
  
  struct CONTRACT_DOWNLOAD_END
  {
    MSG_HEADER header;
  };
  
  struct ERROR_DOWNLOAD_START
  {
    MSG_HEADER header;
  };
  
  struct ERROR_DOWNLOAD
  {
    MSG_HEADER header;
    int16_t iErrorCode;
    char    cErrString[128];
    
    ERROR_DOWNLOAD()
    {
      iErrorCode = 0;
    }
  };
  struct ERROR_DOWNLOAD_END
  {
    MSG_HEADER header;
  };
  
  
  
  struct DOWNLOAD_END_NOTIFICATION
  {
    MSG_HEADER header;
  };
  
  
  
  struct CONTRACTDATA
  {
   int16_t iSeg;
   int32_t AssetToken;
   int32_t Token;
   char Symbol[10 + 1];
   char InstumentName[6 + 1];
   char OptionType[2 + 1];
   char TradingSymbols [25 + 1];
   char cSeries [ 2 + 1];
   uint16_t iTickSize;
   int32_t ExpiryDate;
   int32_t MinimumLotQuantity;
   int32_t HighDPR;
   int32_t LowDPR;
   int32_t StrikePrice;
  };
  
  struct CONTRACT_DOWNLOAD
  {
    MSG_HEADER header;
    CONTRACTDATA ConrctData;
  };
  
  struct GENERIC_ORD_MSG
  {
    MSG_HEADER header;
    int64_t iExchOrdID;
    int32_t iIntOrdID;
    int32_t iTokenNo;
    int32_t iDisclosedVolume;
    int32_t iPrice;
    int32_t iTTQ;
    int32_t iQty;
    int16_t BuySellIndicator;
    int16_t iOrdStatus;
    int16_t isActionable;
    
    GENERIC_ORD_MSG()
    {
      iExchOrdID        = 0;
      iIntOrdID         = 0;
      iTokenNo          = 0;
      iDisclosedVolume  = 0;
      iPrice            = 0;
      iTTQ              = 0;
      iQty              = 0;
      BuySellIndicator  = 0;
      iOrdStatus        = 0;
      isActionable      = 0;
    }
  };
  
  struct TRD_MSG
  {
    MSG_HEADER header;
    int64_t iTimeStamp;
    int64_t iExchOrdID;
    int32_t iTradeId;
    int32_t iIntOrdID;
    int32_t iTTQ;
    int32_t iTokenNo;
    int32_t iTradePrice;
    int32_t iTradeQty;
    int32_t iQtyRemaining;
    int16_t BuySellIndicator;
    int16_t iOrdStatus;
    int16_t isActionable;
    
    TRD_MSG()
    {
      iTimeStamp        = 0;
      iExchOrdID        = 0;
      iTradeId          = 0;
      iIntOrdID         = 0;
      iTTQ              = 0;
      iTokenNo          = 0;
      iTradePrice       = 0;
      iTradeQty         = 0;
      iQtyRemaining     = 0;
      BuySellIndicator  = 0;
      iOrdStatus        = 0;
      isActionable      = 0;
    }
  };
  
  struct ORDER_DOWNLOAD_REQ
  {
    MSG_HEADER header;
  };
  
  struct ORDER_DOWNLOAD_START
  {
    MSG_HEADER header;
  };
  
  struct ORDER_DOWNLOAD
  {
    GENERIC_ORD_MSG GenOrd;
  };
  
  struct ORDER_DOWNLOAD_END
  {
    MSG_HEADER header;
  };
  
  struct TRADE_DOWNLOAD_REQ
  {
    MSG_HEADER header;
  };
  
  struct TRADE_DOWNLOAD_START
  {
    MSG_HEADER header;
  };
  
  struct TRADE_DOWNLOAD
  {
    TRD_MSG TradeStore;
  };
  
  struct TRADE_DOWNLOAD_END
  {
    MSG_HEADER header;
  };
  
  struct TOKEN_DEPTH_REQUEST
  {
    MSG_HEADER header;
    int32_t    iToken;
  };
  
  struct Depth
  {
    int32_t BidPrice;
    int32_t BidQty;
    int32_t AskPrice;
    int32_t AskQty;
  };
  
  struct FFOrderBook
  {
    int32_t Token;
    int32_t LTP;
    Depth   oDep[5]; 

  };
  
  struct TOKEN_DEPTH_RESPONSE
  {
    MSG_HEADER header;
    FFOrderBook oDepthInfo;
  };
  
  
  #pragma pack()
}
#endif	/* DEALERSTRUCTURE_H */

