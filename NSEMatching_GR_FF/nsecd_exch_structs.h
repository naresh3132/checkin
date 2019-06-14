#ifndef NSE_CD_NNF_ORD_H_
#define NSE_CD_NNF_ORD_H_

#include<string>
#include<cstring>
#include<stdint.h>
#include<byteswap.h>
//#include "common.h"
 enum class cd_contractfile
 {
    Token = 1,
    AssetToken = 2,
    MinimumLotQuantity = 31,
    BoardLotQuantity = 32
 }; 

namespace NSECD
{

#pragma pack(2)

static double swapDouble(double d)
{
  union _swapDouble
  {
    double dNum;
    unsigned long lVal;
  }swapNum;

  swapNum.dNum = d;
  swapNum.lVal = __bswap_64(swapNum.lVal);

  return swapNum.dNum;
}

typedef struct multiLegOrderInfo
{
  int16_t lotSize;
  int16_t  ratio;
  int16_t  multiplier;
  int32_t avlblOppQty;
  int32_t execQty;
  int32_t token;
  int32_t orderQty;
  int32_t orderPrice;
  int16_t buySell;
  int32_t orderRemainingQty;
  int16_t ordBookIndx;
  
  multiLegOrderInfo()
  {
     lotSize = 0;
     ratio = 0;
     multiplier = 0;
     avlblOppQty = 0;
     execQty = 0;
     token = 0;
     orderQty = 0;
     orderPrice = 0;
     buySell = 0;
     orderRemainingQty = 0;
     ordBookIndx = -1;
  }
}MultiLegOrderInfo;

struct INVITATION_MESSAGE
{
  int16_t TransactionCode;
  int16_t InvitationCnt;
  void swapBytes()
  {
    TransactionCode = __bswap_16(TransactionCode);
    InvitationCnt   = __bswap_16(InvitationCnt);
  }
};

struct TAP_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  uint32_t CheckSum[4];
  inline void swapBytes()
  {
    sLength     = __bswap_16(sLength);
    iSeqNo      = __bswap_32(iSeqNo);
  }
};

// This is our own custom defined header [NOT EXCHANGE DEFINED], to optimize receive side processing
struct CUSTOM_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  uint32_t CheckSum[4];
  int16_t sTransCode;
};


struct MESSAGE_HEADER
{
  int16_t TransactionCode;
  int32_t LogTime;
  char AlphaChar[2];
  int32_t TraderId;
  int16_t ErrorCode;
  int64_t Timestamp;
  int64_t TimeStamp1;
  char TimeStamp2[8];
  int16_t MessageLength;
  void swapBytes()
  {
    TransactionCode = __bswap_16(TransactionCode);
    LogTime         = __bswap_32(LogTime);
    TraderId        = __bswap_32(TraderId);
    ErrorCode       = __bswap_16(ErrorCode);
    MessageLength   = __bswap_16(MessageLength);
  }
};

struct ME_MESSAGE_HEADER
  {
    int16_t sLength;
    int32_t iSeqNo;
    uint32_t CheckSum[4];
    int16_t TransactionCode;
    int32_t LogTime;
    char AlphaChar[2];
    int32_t TraderId;
    int16_t ErrorCode;
    int64_t Timestamp;
    char TimeStamp1[8];
    char TimeStamp2[8];
    int16_t MessageLength;

   inline void swapBytes()
    {
      sLength     = __bswap_16(sLength);
      iSeqNo      = __bswap_32(iSeqNo);
      TransactionCode      = __bswap_16(TransactionCode);
      LogTime      = __bswap_32(LogTime);
      TraderId      = __bswap_32(TraderId);
      ErrorCode      = __bswap_16(ErrorCode);
      MessageLength      = __bswap_16(MessageLength);   
    }
  };

struct INNER_MESSAGE_HEADER
{
  int32_t iTraderId;
  int32_t LogTime;
  char AlphaChar[2];
  int16_t TransactionCode;
  int16_t ErrorCode;
  int64_t Timestamp;
  int64_t TimeStamp1;
  char TimeStamp2[8];
  int16_t MessageLength;
};

struct BCAST_HEADER
{
  char Reserved1[2];
  char Reserved2[2];
  int32_t LogTime;
  char AlphaChar[2];
  int16_t TransCode;
  int16_t ErrorCode;
  int32_t BCSeqNo;
  char Reserved3;
  char Reserved4[3];
  char TimeStamp2[8];
  char Filler2[8];
  int16_t MessageLength;
};

struct ST_BROKER_ELIGIBILITY_PER_MKT
{
  unsigned char Reserved1:4;
  unsigned char AuctionMarket:1;
  unsigned char Reserved2:1;
  unsigned char Reserved3:1;
  unsigned char Reserved4:1;
  char Reserved5;
};

struct ST_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct ST_EX_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct ST_PL_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct PORTFOLIO_DATA
{
  char Portfolio[10];
  int32_t Token;
  int32_t LastUpdtDtTime;
  char DeletFlag;
};

struct CONTRACT_DESC
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  uint32_t StrikePrice;
  char OptionType[2];
  int16_t CALevel;
};

struct ST_ORDER_FLAGS
{
  unsigned char AON:1;
  unsigned char IOC:1;
  unsigned char GTC:1;
  unsigned char Day:1;
  unsigned char MIT:1;
  unsigned char SL:1;
  unsigned char Market:1;
  unsigned char ATO:1;
  unsigned char Reserved:3;
  unsigned char Frozen:1;
  unsigned char Modified:1;
  unsigned char Traded:1;
  unsigned char MatchedInd:1;
  unsigned char MF:1;
};

struct ADDITIONAL_ORDER_FLAGS
{
  unsigned char Reserved1:1;
  unsigned char COL:1;
  unsigned char Reserved2:6;
};

struct CONTRACT_DESC_TR
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  uint32_t StrikePrice;
  char OptionType[2];
  inline CONTRACT_DESC_TR & operator = (const CONTRACT_DESC &rhs)
  {
    memcpy(this,&rhs, sizeof(CONTRACT_DESC_TR));
    return *this;
  }
};

struct MS_ERROR_RESPONSE
{
  MESSAGE_HEADER header;
  char Key[14];
  char ErrorMessage[128];
};

struct ST_STOCK_ELEGIBLE_INDICATORS
{
  unsigned char Reserved1:5;
  unsigned char BooksMerged:1;
  unsigned char MinFill:1;
  unsigned char AON:1;
  char Reserved2;
};

//NEW LOGIN CHANGES STARTS
struct MS_GR_REQUEST //gateway router Request
{
  ME_MESSAGE_HEADER  msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
};

struct MS_GR_RESPONSE
{
  ME_MESSAGE_HEADER  msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
  char cIPAddress[16];
  int32_t iPort;
  char cSessionKey[8];
};

struct MS_BOX_SIGN_ON_REQUEST_IN
{
  ME_MESSAGE_HEADER  msg_hdr;
  int16_t wBoxId;
  char cBrokerId[5];
  char cReserved[5]; 
  char cSessionKey[8];
};

struct MS_BOX_SIGN_ON_REQUEST_OUT
{
  ME_MESSAGE_HEADER  msg_hdr;
  int16_t wBoxId;
  char cReserved[5];
};

//NEW LOGIN CHANGES ENDS

struct MS_SIGNON_REQ  //SIGN_ON_REQUEST_IN(2300)
{
  ME_MESSAGE_HEADER  msg_hdr;
  int32_t UserId;
  char Reserved3[8];
  char Passwd[8];
  char Reserved4[8];
  char NewPassword[8];
  char TraderName[26];
  int32_t LastPasswdChangeDate;
  char BrokerId[5];
  char Reserved1;
  int16_t BranchId;
  int32_t VersionNumber;
  int32_t Batch2StartTime;
  char HostSwitchContext;
  char Color[50];
  char Reserved2;
  int16_t UserType;
  double SequenceNumber;
  char WsClassName[14];
  char BrokerStatus;
  char ShowIndex;
  ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
  int16_t MemberType;
  char ClearingStatus;
  char BrokerName[25];
  char Reserved5[16];
  char Reserved6[16];
  char Reserved7[16];

  void swapBytes()
  {
    UserId          = __bswap_32(UserId);
    VersionNumber   = __bswap_32(VersionNumber);
    Batch2StartTime = __bswap_32(Batch2StartTime);
    UserType        = __bswap_32(UserType);
  }
};

struct MS_SIGNON_RESP    // SIGN_ON_REQUEST_OUT(2301)
{
  //TAP_HEADER tap_hdr;
  ME_MESSAGE_HEADER msg_hdr;
  int32_t UserId;
  char Reserved5[8];
  char Passwd[8];
  char Reserved6[8];
  char NewPassword[8];
  char TraderName[26];
  int32_t LastPasswdChangeDate;
  char BrokerId[5];
  char Reserved1;
  int16_t BranchId; 
  int32_t VersionNumber;
  int32_t EndTime;
  char Reserved2;
  char Color[50];
  char Reserved3;
  int16_t UserType;
  double SequenceNumber;
  char Reserved4[14];
  char BrokerStatus;
  char ShowIndex;
  ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
  int16_t MemberType;
  char ClearingStatus;
  char BrokerName[25];
  char Reserved7[16];
  char Reserved8[16];
  char Reserved9[16];

};


struct MS_INDUSTRY_INDEX_DLOAD_RESP_ARRAY
{
   int16_t IndustryCode;
   char IndexName[21];
   int32_t IndexValue;
};        

struct MS_INDUSTRY_INDEX_DLOAD_RESP  //INDUSTRY_INDEX_DLOAD_OUT (1111).  474 bytes
{
  //TAP_HEADER tap_hdr;
  ME_MESSAGE_HEADER msg_hdr;
  short NumberOfRecords;
  MS_INDUSTRY_INDEX_DLOAD_RESP_ARRAY Industry_dload[16];
};


struct MS_SYSTEM_INFO_REQ  //SYSTEM_INFORMATION_IN(1600)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t LastUpdatePortfolioTime;
};


struct MS_SYSTEM_INFO_DATA  //SYSTEM_INFORMATION_OUT(1601)
{
  //TAP_HEADER tap_hdr;
  ME_MESSAGE_HEADER msg_hdr;
  ST_MARKET_STATUS st_market_status;
  ST_EX_MARKET_STATUS st_ex_market_status;
  ST_PL_MARKET_STATUS st_PL_market_status;
  char UpdatePortfolio;
  int32_t MarketIndex;
  int16_t DefSettPeriodNormal;
  int16_t DefSettPeriodSpot;
  int16_t DefSettPeriodAuction;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  int16_t WamingPercent;
  int16_t VolumeFreezePercent;
  int16_t SnapQuoteTime;
  char Reserved1[2];
  int32_t BoardLotQty;
  int32_t TickSize;
  int16_t MaximumGtcDays;
  ST_STOCK_ELEGIBLE_INDICATORS st_stock_eligible_indicators;
  int16_t DiscQtyPerAllwd;
  int32_t RiskFreeInterestRate;

};

struct MS_UPDATE_LOCAL_DATABASE //UPDATE_LOCALDB_IN(7300)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t LastUpdateSecTime;
  int32_t LastUpdateParticipantTime;
  int32_t LastUpdateInstTime;
  int32_t LastUpdateIndexTime;
  char ReqForOpenOrders;
  char Reserved1;
  ST_MARKET_STATUS st_market_status;
  ST_EX_MARKET_STATUS st_ex_market_status;
  ST_PL_MARKET_STATUS st_pl_market_status;
};

struct MS_PARTIAL_SYS_INFO //PARTIAL_SYSTEM_INFO(7321)
{
  ME_MESSAGE_HEADER msg_hdr;
  ST_MARKET_STATUS st_market_status;

};


struct UPDATE_LDB_HEADER //UPDATE_LOCALDB_HEADER(7307)  //UPDATE_LOCALDB_TRAILER(7308)
{
  ME_MESSAGE_HEADER msg_hdr;
  char Reserved1[2];
};

struct UPDATE_LDB_DATA //UPDATE_LOCALDB_DATA(7304)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  INNER_MESSAGE_HEADER inheader;
  char Data[436];
};

struct INDEX_DETAILS 
{
  char IndexName[16];
  int32_t Token;
  int32_t LastUpdateDateTime;
};
struct MS_DOWNLOAD_INDEX //BCAST_INDEX_MSTR_CHG(7325)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  INDEX_DETAILS inxdet[17];
};
struct BCAST_INDEX_MAP_DETAILS
{
  char BcastName[26];
  char ChangedName[10];
  char DeleteFlag;
  int32_t LastUpdateDateTime;
};

struct MS_DOWNLOAD_INDEX_MAP  //BCAST_INDEX_MAP_TABLE(7326)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  BCAST_INDEX_MAP_DETAILS sIndicesMap[10];
};

struct EXCH_PORTFOLIO_REQ //EXCH_PORTF_IN(1775)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t LastUpdateDtTime;
};

struct EXCH_PORTFOLIO_RESP //EXCH_PORTF_OUT(1776)
{
  ME_MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  char MoreRecs;
  char Filler;
  PORTFOLIO_DATA portfolio_data[15];
};

struct MS_MESSAGE_DOWNLOAD_REQ //DOWNLOAD_REQUEST(7000)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int64_t SeqNo; /*as per NNF it is double but as we send timestamp1 as int64 in resp converting Seqno to int64 for ease of coding*/
};

struct MS_MESSAGE_DOWNLOAD_HEADER  //TRAILER_RECORD(7011)
{
  ME_MESSAGE_HEADER msg_hdr;
};

struct MS_MESSAGE_DOWNLOAD_DATA //DOWNLOAD_DATA(7021)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  INNER_MESSAGE_HEADER inner_hdr;
  char Data[524];
};

struct MS_MESSAGE_DOWNLOAD_TRAILER  //TRAILER_RECORD(7031)
{
  ME_MESSAGE_HEADER msg_hdr;
};


struct SIGNOFF_OUT  //SIGN_OFF_REQUEST_OUT(2321)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t UserId;
  char Reserved[145];
};

/**************************************************************/

typedef struct _MS_OE_REQUEST   //BOARD_LOT_IN(2000)
{
  ME_MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  unsigned char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  unsigned char reserved2;
  int16_t ReasonCode;
  unsigned char reserved3[4];
  uint32_t TokenNo;
  CONTRACT_DESC contract_desc;
  char CounterPartyBrokerId[5];
  unsigned char reserved4;
  unsigned char reserved5[2];
  char CloseoutFlag;
  unsigned char reserved6;
  int16_t OrderType;
  double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  uint32_t DisclosedVolume;
  uint32_t DisclosedVolumeRemaining;
  uint32_t TotalVolumeRemaining;
  uint32_t Volume;
  uint32_t VolumeFilledToday;
  uint32_t Price;
  uint32_t TriggerPrice;
  uint32_t GoodTillDate;
  uint32_t EntryDateTime;
  uint32_t MinimumFill;
  uint32_t LastModified;
  ST_ORDER_FLAGS st_order_flags;
  int16_t BranchId;
  uint32_t TraderId;
  char BrokerId[5];
  char cOrdFiller[24];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  ADDITIONAL_ORDER_FLAGS additional_order_flags;
  char GiveUpFlag;
  uint32_t filler;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void swapBytes()
  {
    CompetitorPeriod        = __bswap_16(CompetitorPeriod);
    SolicitorPeriod         = __bswap_16(SolicitorPeriod);
    ReasonCode              = __bswap_16(ReasonCode);
    TokenNo                 = __bswap_32(TokenNo);
    OrderType               = __bswap_16(OrderType);
    OrderNumber             = swapDouble(OrderNumber);
    BookType                = __bswap_16(BookType);
    BuySellIndicator        = __bswap_16(BuySellIndicator);
    DisclosedVolume         = __bswap_32(DisclosedVolume);
    DisclosedVolumeRemaining= __bswap_32(DisclosedVolumeRemaining);
    TotalVolumeRemaining    = __bswap_32(TotalVolumeRemaining);
    Volume                  = __bswap_32(Volume);
    VolumeFilledToday       = __bswap_32(VolumeFilledToday);
    Price                   = __bswap_32(Price);
    TriggerPrice            = __bswap_32(TriggerPrice);
    GoodTillDate            = __bswap_32(GoodTillDate);
    EntryDateTime           = __bswap_32(EntryDateTime);
    MinimumFill             = __bswap_32(MinimumFill);
    LastModified            = __bswap_32(LastModified);
    BranchId                = __bswap_16(BranchId);
    TraderId                = __bswap_32(TraderId);
    ProClientIndicator      = __bswap_16(ProClientIndicator);
    SettlementPeriod        = __bswap_16(SettlementPeriod);
    NnfField                = swapDouble(NnfField);
    MktReplay               = swapDouble(MktReplay) ;
  }
}MS_OE_REQUEST;

typedef MS_OE_REQUEST  NSE_FO_NNF_NEW;      //BOARD_LOT_IN(2000)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_NEW_ACCEPT;  //ORDER_CONFIRMATION_OUT(2073)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_NEW_REJECT;  //ORDER_ERROR_OUT(2231)

typedef NSE_FO_NNF_NEW  NSE_FO_NNF_MOD;      //ORDER_MOD_IN(2040)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_MOD_ACCEPT;  //ORDER_MOD_CONFIRM_OUT(2074)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_MOD_REJECT;  //ORDER_MOD_REJ_OUT(2042)

typedef NSE_FO_NNF_NEW  NSE_FO_NNF_CXL;      //ORDER_CANCEL_IN(2070)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_CXL_ACCEPT;  //ORDER_CANCEL_CONFIRM_OUT(2075)
typedef NSE_FO_NNF_NEW  NSE_FO_NNF_CXL_REJECT;  //ORDER_CXL_REJ_OUT(2072)

typedef NSE_FO_NNF_NEW  NSE_FO_NNF_KILL_ALL;    //KILL_SWITCH_IN(2063)

typedef struct _MS_SPD_LEG_INFO2
{
  int32_t Token2;
  CONTRACT_DESC SecurityInformation2;
  char OpBrokerId2[5];
  char Fillerx2;
  int16_t OrderType2;
  int16_t BuySell2;
  int32_t DisclosedVol2;
  int32_t DisclosedVolRemaining2;
  int32_t TotalVolRemaining2;
  int32_t Volume2;
  int32_t VolumeFilledToday2;
  int32_t Price2;
  int32_t TriggerPrice2;
  int32_t MinFillAon2;
  ST_ORDER_FLAGS OrderFlags2;
  char OpenClose2;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags2;
  char GiveUpFlag2;
  char FillerY;
  inline void swapBytes()
  {
    Token2                          = __bswap_32(Token2);
    OrderType2                      = __bswap_16(OrderType2);
    BuySell2                        = __bswap_16(BuySell2);
    DisclosedVol2                   = __bswap_32(DisclosedVol2);
    DisclosedVolRemaining2          = __bswap_32(DisclosedVolRemaining2);
    TotalVolRemaining2              = __bswap_32(TotalVolRemaining2);
    Volume2                         = __bswap_32(Volume2);
    VolumeFilledToday2              = __bswap_32(VolumeFilledToday2);
    Price2                          = __bswap_32(Price2);
    TriggerPrice2                   = __bswap_32(TriggerPrice2);
    MinFillAon2                     = __bswap_32(MinFillAon2);
    
  }
}MS_SPD_LEG_INFO2;

typedef struct _MS_SPD_LEG_INFO3
{
  int32_t Token3;
  CONTRACT_DESC SecurityInformation3;
  char OpBrokerId3[5];
  char Fillerx3;
  int16_t OrderType3;
  int16_t BuySell3;
  int32_t DisclosedVol3;
  int32_t DisclosedVolRemaining3;
  int32_t TotalVolRemaining3;
  int32_t Volume3;
  int32_t VolumeFilledToday3;
  int32_t Price3;
  int32_t TriggerPrice3;
  int32_t MinFillAon3;
  ST_ORDER_FLAGS OrderFlags3;
  char OpenClose3;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags3;
  char GiveUpFlag3;
  char FillerZ;

  inline void swapBytes()
  {
    Token3                      = __bswap_32(Token3);
    OrderType3                  = __bswap_16(OrderType3);
    BuySell3                    = __bswap_16(BuySell3);
    DisclosedVol3               = __bswap_32(DisclosedVol3);
    DisclosedVolRemaining3      = __bswap_32(DisclosedVolRemaining3);
    TotalVolRemaining3          = __bswap_32(TotalVolRemaining3);
    Volume3                     = __bswap_32(Volume3);
    VolumeFilledToday3          = __bswap_32(VolumeFilledToday3);
    Price3                      = __bswap_32(Price3);
    TriggerPrice3               = __bswap_32(TriggerPrice3);
    MinFillAon3                 = __bswap_32(MinFillAon3);
    
  }
}MS_SPD_LEG_INFO3;


typedef struct _MS_SPD_OE_REQUEST
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char ParticipantType1;
  char Filler1;
  int16_t CompetitorPeriod1;
  int16_t SolicitorPeriod1;
  char ModCxlBy1;
  char Filler9;
  int16_t ReasonCode1;
  char StartAlpha[2];
  char EndAlpha[2];
  int32_t Token1;
  CONTRACT_DESC SecurityInformation1;
  char OpBrokerId1[5];
  char Fillerx1;
  char FillerOpions1[3];
  char Fillery1;
  int16_t OrderType1;
  double OrderNUmber1;
  char AccountNUmber1[10];
  int16_t BookType1;
  int16_t BuySell1;
  int32_t DisclosedVol1;
  int32_t DisclosedVolRemaining1;
  int32_t TotalVolRemaining1;
  int32_t Volume1;
  int32_t VolumeFilledToday1;
  int32_t Price1;
  int32_t TriggerPrice1;
  int32_t GoodTillDate1;
  int32_t EntryDateTime1;
  int32_t MinFillAon1;
  int32_t LastModified1;
  ST_ORDER_FLAGS OrderFlags1;
  int16_t BranchId1;
  uint32_t TraderId1;
  char BrokerId1[5];
  char cOrdFiller[24];
  char OpenClose1;
  char Settlor1[12];
  int16_t ProClient1;
  int16_t SettlementPeriod1;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  char GiveUpFlag1;
  unsigned char filler1to16[2];
  char filler17;
  char filler18;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
  int32_t PriceDiff;
  MS_SPD_LEG_INFO2 leg2;
  MS_SPD_LEG_INFO3 leg3;

  inline void swapBytes()
  {
    CompetitorPeriod1         = __bswap_16(CompetitorPeriod1);
    SolicitorPeriod1          = __bswap_16(SolicitorPeriod1);
    ReasonCode1               = __bswap_16(ReasonCode1);
    Token1                    = __bswap_32(Token1);
    OrderType1                = __bswap_16(OrderType1);
    OrderNUmber1              = swapDouble(OrderNUmber1);
    BookType1                 = __bswap_16(BookType1);
    BuySell1                  = __bswap_16(BuySell1);
    DisclosedVol1             = __bswap_32(DisclosedVol1);
    DisclosedVolRemaining1    = __bswap_32(DisclosedVolRemaining1);
    TotalVolRemaining1        = __bswap_32(TotalVolRemaining1);
    Volume1                   = __bswap_32(Volume1);
    VolumeFilledToday1        = __bswap_32(VolumeFilledToday1);
    Price1                    = __bswap_32(Price1);
    TriggerPrice1             = __bswap_32(TriggerPrice1);
    GoodTillDate1             = __bswap_32(GoodTillDate1);
    EntryDateTime1            = __bswap_32(EntryDateTime1);
    MinFillAon1               = __bswap_32(MinFillAon1);
    LastModified1             = __bswap_32(LastModified1);
    BranchId1                 = __bswap_16(BranchId1);
    TraderId1                 = __bswap_32(TraderId1);
    ProClient1                = __bswap_16(ProClient1);
    SettlementPeriod1         = __bswap_16(SettlementPeriod1);
    NnfField                  = swapDouble(NnfField);
    MktReplay                 = swapDouble(MktReplay);
    PriceDiff                 = __bswap_32(PriceDiff);
    leg2.swapBytes();
    leg3.swapBytes();
  }
} MS_SPD_OE_REQUEST;

typedef MS_SPD_OE_REQUEST    NSE_FO_NNF_SPD_NEW;        //SP_BOARD_LOT_IN(2100)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_NEW_ACCEPT;    //SP_ORDER_CONFIRMATION(2124)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_NEW_REJECT;    //SP_ORDER_ERROR(2154)

typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_MOD;        //SP_ORDER_MOD_IN(2118)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_MOD_ACCEPT;    //SP_ORDER_MOD_CON_OUT(2136)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_MOD_REJECT;    //SP_ORDER_MOD_REJ_OUT(2133)

typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_CXL;        //SP_ORDER_CANCEL_IN(2106)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_CXL_ACCEPT;    //SP_ORDER_CXL_CONFIRMATION(2130)
typedef NSE_FO_NNF_SPD_NEW    NSE_FO_NNF_SPD_CXL_REJECT;    //SP_ORDER_CXL_REJ_OUT(2127)

typedef MS_SPD_OE_REQUEST    NSE_FO_NNF_ML_NEW;            //TWOL_BOARD_LOT_IN(2102) or THRL_BOARD_LOT_IN(2104)
typedef NSE_FO_NNF_ML_NEW    NSE_FO_NNF_ML_NEW_ACCEPT;     //TWOL_ORDER_CONFIRMATION(2125) OR THRL_ORDER_CONFIRMATION(2126)
typedef NSE_FO_NNF_ML_NEW    NSE_FO_NNF_ML_NEW_REJECT;     //TWOL_ORDER_ERROR(2155) OR THRL_ORDER_ERROR(2156)

typedef NSE_FO_NNF_ML_NEW    NSE_FO_NNF_ML_CXL_ACCEPT;    //TWOL_ORDER_CXL_CONFIRMATION(2131) OR THRL_ORDER_CXL_CONFIRMATION(2132) OR ORDER_CANCEL_CONFIRM_OUT(2075)



struct MS_SL_TRIGGER
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  double ResponseOrderNumber;
  char BrokerId[5];
  char reserved1;
  int32_t TraderNumber;
  char AccountNumber[10];
  int16_t BuySellIndicator;
  int32_t OriginalVolume;
  int32_t DisclosedVolume;
  int32_t RemainingVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t Price;
  ST_ORDER_FLAGS OrderFlags;
  int32_t GoodTillDate;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  double CounterTraderOrderNumber;
  char CounterBrokerId[5];
  int32_t Token;
  CONTRACT_DESC contract_desc;
  char OpenClose;
  char OldOpenClose;
  char BookType;
  int32_t NewVolume;
  char OldAccountNumber[10];
  char Participant[12];
  char OldParticipant[12];
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags;
  char ReservedFiller;
  char GiveUpTrade;
  char PAN[10];
  char oldPAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
  

  inline void swapBytes()
  {
    ResponseOrderNumber           = swapDouble(ResponseOrderNumber);
    TraderNumber                  = __bswap_32(TraderNumber);
    BuySellIndicator              = __bswap_16(BuySellIndicator);
    OriginalVolume                = __bswap_32(OriginalVolume);
    DisclosedVolume               = __bswap_32(DisclosedVolume);
    RemainingVolume               = __bswap_32(RemainingVolume);
    DisclosedVolumeRemaining      = __bswap_32(DisclosedVolumeRemaining);
    Price                         = __bswap_32(Price);
    GoodTillDate                  = __bswap_32(GoodTillDate);
    FillNumber                    = __bswap_32(FillNumber);
    FillQuantity                  = __bswap_32(FillQuantity);
    FillPrice                     = __bswap_32(FillPrice);
    VolumeFilledToday             = __bswap_32(VolumeFilledToday);
    ActivityTime                  = __bswap_32(ActivityTime);
    CounterTraderOrderNumber      = swapDouble(CounterTraderOrderNumber);
    Token                         = __bswap_32(Token);
    NewVolume                     = __bswap_32(NewVolume);
  }

  inline void getData(MS_OE_REQUEST *request)
  {
    TraderNumber                = request->TraderId;
    Token                       = request->TokenNo;
    contract_desc               = request->contract_desc;//TODO:AB:byteswap for contract desc.
    ResponseOrderNumber         = request->OrderNumber;
    BookType                    = request->BookType;
    BuySellIndicator            = request->BuySellIndicator;
    DisclosedVolume             = request->DisclosedVolume;
    DisclosedVolumeRemaining    = request->DisclosedVolumeRemaining;
    RemainingVolume             = request->TotalVolumeRemaining;
    OriginalVolume              = request->Volume;
    VolumeFilledToday           = request->VolumeFilledToday;
    Price                       = request->Price;
    GoodTillDate                = request->GoodTillDate;
    OrderFlags                  = request->st_order_flags;
    AddtnlOrderFlags            = request->additional_order_flags;
    ActivityType[0]             = ' ';
    ActivityType[1]             = ' ';
    OpenClose                   = ' ';

    memcpy(BrokerId,request->BrokerId,5);
    memcpy(AccountNumber,request->AccountNumber,10);
    memcpy(Participant,request->Settlor,12);

  }

  inline void getData(MS_SPD_OE_REQUEST *request)
  {
    TraderNumber            = request->TraderId1;
    ResponseOrderNumber     = request->OrderNUmber1;
    GoodTillDate            = request->GoodTillDate1;
    ActivityType[0]         = ' ';
    ActivityType[1]         = ' ';
    OpenClose               = ' ';

    memcpy(BrokerId, request->BrokerId1 , sizeof(BrokerId));
    memcpy(AccountNumber,request->AccountNUmber1,sizeof(AccountNumber));
    memcpy(Participant,request->Settlor1,sizeof(Participant));

    memset(CounterBrokerId, ' ' ,sizeof(CounterBrokerId));
    memset(OldAccountNumber, ' ' ,sizeof(OldAccountNumber));
    memset(OldParticipant, ' ' ,sizeof(OldParticipant));
    
  }

};
typedef MS_SL_TRIGGER NSE_CM_MS_SL_TRIGGER;


typedef struct _MS_TRADE_CONFRIM
{
  MESSAGE_HEADER msg_hdr;
  double ResponseOrderNumber;
  char BrokerId[5];
  char reserved1;
  int32_t TraderNumber;
  char AccountNumber[10];
  int16_t BuySellIndicator;
  int32_t OriginalVolume;
  int32_t DisclosedVolume;
  int32_t RemainingVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t Price;
  ST_ORDER_FLAGS OrderFlags;
  int32_t GoodTillDate;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  double CounterTraderOrderNumber;
  char CounterBrokerId[5];
  int32_t Token;
  CONTRACT_DESC contract_desc;
  char OpenClose;
  char OldOpenClose;
  char BookType;
  int32_t NewVolume;
  char OldAccountNumber[10];
  char Participant[12];
  char OldParticipant[12];
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags;
  char ReservedFiller;
  char GiveUpTrade;
  char PAN[10];
  char oldPAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
  

  inline void swapBytes()
  {
    ResponseOrderNumber           = swapDouble(ResponseOrderNumber);
    TraderNumber                  = __bswap_32(TraderNumber);
    BuySellIndicator              = __bswap_16(BuySellIndicator);
    OriginalVolume                = __bswap_32(OriginalVolume);
    DisclosedVolume               = __bswap_32(DisclosedVolume);
    RemainingVolume               = __bswap_32(RemainingVolume);
    DisclosedVolumeRemaining      = __bswap_32(DisclosedVolumeRemaining);
    Price                         = __bswap_32(Price);
    GoodTillDate                  = __bswap_32(GoodTillDate);
    FillNumber                    = __bswap_32(FillNumber);
    FillQuantity                  = __bswap_32(FillQuantity);
    FillPrice                     = __bswap_32(FillPrice);
    VolumeFilledToday             = __bswap_32(VolumeFilledToday);
    ActivityTime                  = __bswap_32(ActivityTime);
    CounterTraderOrderNumber      = swapDouble(CounterTraderOrderNumber);
    Token                         = __bswap_32(Token);
    NewVolume                     = __bswap_32(NewVolume);
  }

  inline void getData(MS_OE_REQUEST *request)
  {
    TraderNumber                      = request->TraderId;
    Token                             = request->TokenNo;
    contract_desc                     = request->contract_desc;//TODO:AB:byteswap for contract desc.
    ResponseOrderNumber               = request->OrderNumber;
    BookType                          = request->BookType;
    BuySellIndicator                  = request->BuySellIndicator;
    DisclosedVolume                   = request->DisclosedVolume;
    DisclosedVolumeRemaining          = request->DisclosedVolumeRemaining;
    RemainingVolume                   = request->TotalVolumeRemaining;
    OriginalVolume                    = request->Volume;
    VolumeFilledToday                 = request->VolumeFilledToday;
    Price                             = request->Price;
    GoodTillDate                      = request->GoodTillDate;
    OrderFlags                        = request->st_order_flags;
    AddtnlOrderFlags                  = request->additional_order_flags;
    ActivityType[0]                   = ' ';
    ActivityType[1]                   = ' ';
    OpenClose                         = ' ';

    memcpy(BrokerId,request->BrokerId,5);
    memcpy(AccountNumber,request->AccountNumber,10);
    memcpy(Participant,request->Settlor,12);

  }

  inline void getData(MS_SPD_OE_REQUEST *request)
  {
    TraderNumber            = request->TraderId1;
    ResponseOrderNumber     = request->OrderNUmber1;
    GoodTillDate            = request->GoodTillDate1;
    ActivityType[0]         = ' ';
    ActivityType[1]         = ' ';
    OpenClose               = ' ';

    memcpy(BrokerId, request->BrokerId1 , sizeof(BrokerId));
    memcpy(AccountNumber,request->AccountNUmber1,sizeof(AccountNumber));
    memcpy(Participant,request->Settlor1,sizeof(Participant));

    memset(CounterBrokerId, ' ' ,sizeof(CounterBrokerId));
    memset(OldAccountNumber, ' ' ,sizeof(OldAccountNumber));
    memset(OldParticipant, ' ' ,sizeof(OldParticipant));

  }
} MS_TRADE_CONFIRM;

typedef MS_TRADE_CONFIRM NSE_FO_NNF_TRADE; //TRADE_CONFIRM(2222)

/**************************************************************/

struct PRICE_VOL_MOD //PRICE_VOL_MOD (2013)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t TokenNo;
  int32_t TraderId;
  double OrderNum;
  int16_t BuySell;
  int32_t Price;
  int32_t Volume;
  int32_t LastModified;
  char Reference[4];
};

struct MS_TRADE_INQ_DATA
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t TokenNo;
  CONTRACT_DESC contract_desc;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  char MktType;
  char BuyOpenClose;
  int32_t NewVolume;
  char BuyBrokerId[5];
  char SellBrokerId[5];
  int32_t TraderId;
  char ReqBy;
  char SellOpenClose;
  char BuyAccNum[10];
  char SellAccNum[10];
  char BuyParticipant[12];
  char SellParticipant[12];
  char ReservedFiller[2];
  char BuyGiveupFlag;
  char SellGiveupFlag;
};

// NEWLY ADDED ON 10 MARCH 2015.

//BOARD_LOT_IN_TR(20000)
struct MS_OE_REQUEST_TR
{
  TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t UserId;
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR contract_desc_tr;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t Volume;
  int32_t Price;
  int32_t GoodTillDate;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  int32_t filler;
  double NnfField;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[32];

  void swapBytes()
  {
    TransactionCode         = __bswap_16(TransactionCode);
    UserId                  = __bswap_32(UserId);
    ReasonCode              = __bswap_16(ReasonCode);
    TokenNo                 = __bswap_32(TokenNo);
    BookType                = __bswap_16(BookType);
    BuySellIndicator        = __bswap_16(BuySellIndicator);
    DisclosedVolume         = __bswap_32(DisclosedVolume);
    Volume                  = __bswap_32(Volume);
    Price                   = __bswap_32(Price);
    GoodTillDate            = __bswap_32(GoodTillDate);
    //TODO:AB:Order Flags
    BranchId                = __bswap_16(BranchId);
    TraderId                = __bswap_32(TraderId);
    ProClientIndicator      = __bswap_16(ProClientIndicator);
    //TODO:AB:Aditional Order Flags
    NnfField                = __bswap_64(NnfField);
  }
};


//ORDER_QUICK_CANCEL_IN_TR(20060)
//ORDER_CANCEL_IN_TR(20070)
//ORDER_MOD_IN_TR(20040)
struct MS_OM_REQUEST_TR
{
  TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t UserId;
  char ModCanBy;
  int32_t TokenNo;
  CONTRACT_DESC_TR contract_desc_tr;
  double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t TotalVolumeRemaining;
  int32_t Volume;
  int32_t VolumeFilledToday;
  int32_t Price;
  int32_t GoodTillDate;
  int32_t EntryDateTime;
  int32_t LastModified;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  int32_t filler;
  double NnfField;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[32];

  inline void swapBytes()
  {
    TransactionCode         = __bswap_16(TransactionCode);
    UserId                  = __bswap_32(UserId);
    TokenNo                 = __bswap_32(TokenNo);
    //TODO:AB:Contract Desc
    OrderNumber             = swapDouble(OrderNumber);
    BookType                = __bswap_16(BookType);
    BuySellIndicator        = __bswap_16(BuySellIndicator);
    DisclosedVolume         = __bswap_32(DisclosedVolume);
    DisclosedVolumeRemaining= __bswap_32(DisclosedVolumeRemaining);
    TotalVolumeRemaining    = __bswap_32(TotalVolumeRemaining);
    Volume                  = __bswap_32(Volume);
    VolumeFilledToday       = __bswap_32(VolumeFilledToday);
    Price                   = __bswap_32(Price);
    GoodTillDate            = __bswap_32(GoodTillDate);
    EntryDateTime           = __bswap_32(EntryDateTime);
    LastModified            = __bswap_32(LastModified);
    //TODO:AB:Order Flags
    BranchId                = __bswap_16(BranchId);
    TraderId                = __bswap_32(TraderId);
    ProClientIndicator      = __bswap_16(ProClientIndicator);
    //TODO:AB:Aditional Order Flags
    NnfField                = __bswap_64(NnfField);
  }
};

typedef struct _MS_OE_RESPONSE_SL //BOARD_LOT_IN(2000)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  unsigned char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  unsigned char reserved2;
  int16_t ReasonCode;
  unsigned char reserved3[4];
  uint32_t TokenNo;
  CONTRACT_DESC contract_desc;
  char CounterPartyBrokerId[5];
  unsigned char reserved4;
  unsigned char reserved5[2];
  char CloseoutFlag;
  unsigned char reserved6;
  int16_t OrderType;
  double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  uint32_t DisclosedVolume;
  uint32_t DisclosedVolumeRemaining;
  uint32_t TotalVolumeRemaining;
  uint32_t Volume;
  uint32_t VolumeFilledToday;
  uint32_t Price;
  uint32_t TriggerPrice;
  uint32_t GoodTillDate;
  uint32_t EntryDateTime;
  uint32_t MinimumFill;
  int32_t LastModified;
  ST_ORDER_FLAGS st_order_flags;
  int16_t BranchId;
  uint32_t TraderId;
  char BrokerId[5];
  char cOrdFiller[24];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  ADDITIONAL_ORDER_FLAGS additional_order_flags;
  char GiveUpFlag;
  int32_t filler;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(_MS_OE_REQUEST) - sizeof(TAP_HEADER));

    msg_hdr.TransactionCode         = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode               = __bswap_16(msg_hdr.ErrorCode);
    SwapDouble((char*) &OrderNumber);
    LastModified                    = __bswap_32(LastModified);
    EntryDateTime                   = __bswap_32(EntryDateTime);
    ReasonCode                      = __bswap_16(ReasonCode);
    TokenNo                         = __bswap_32(TokenNo);
    BookType                        = __bswap_16(BookType);
    Volume                          = __bswap_32(Volume);
    TraderId                        = __bswap_32(TraderId);
    SwapDouble((char*) &NnfField);
    
  }
} MS_OE_RESPONSE_SL;


/*Sneha*/
typedef struct _MS_OE_RESPONSE //BOARD_LOT_IN(2000)
{
  MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  unsigned char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  unsigned char reserved2;
  int16_t ReasonCode;
  unsigned char reserved3[4];
  uint32_t TokenNo;
  CONTRACT_DESC contract_desc;
  char CounterPartyBrokerId[5];
  unsigned char reserved4;
  unsigned char reserved5[2];
  char CloseoutFlag;
  unsigned char reserved6;
  int16_t OrderType;
  double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  uint32_t DisclosedVolume;
  uint32_t DisclosedVolumeRemaining;
  uint32_t TotalVolumeRemaining;
  uint32_t Volume;
  uint32_t VolumeFilledToday;
  uint32_t Price;
  uint32_t TriggerPrice;
  uint32_t GoodTillDate;
  uint32_t EntryDateTime;
  uint32_t MinimumFill;
  int32_t LastModified;
  ST_ORDER_FLAGS st_order_flags;
  int16_t BranchId;
  uint32_t TraderId;
  char BrokerId[5];
  char cOrdFiller[24];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  ADDITIONAL_ORDER_FLAGS additional_order_flags;
  char GiveUpFlag;
  int32_t filler;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(_MS_OE_REQUEST) - sizeof(TAP_HEADER));

    msg_hdr.TransactionCode     = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode           = __bswap_16(msg_hdr.ErrorCode);
    SwapDouble((char*) &OrderNumber);
    LastModified                = __bswap_32(LastModified);
    EntryDateTime               = __bswap_32(EntryDateTime);
    ReasonCode                  = __bswap_16(ReasonCode);
    TokenNo                     = __bswap_32(TokenNo);
    BookType                    = __bswap_16(BookType);
    Volume                      =  __bswap_32(Volume);
    TraderId                    = __bswap_32(TraderId);
    SwapDouble((char*) &NnfField);
  }
} MS_OE_RESPONSE;

//PRICE_CONFIRMATION_TR(20012)
//ORDER_ERROR_TR(20231)
//ORDER_CXL_CONFIRMATION_TR(20075)
//ORDER_MOD_CONFIRMATION_TR(20074)
//ORDER_CONFIRMATION_TR(20073)
//ORDER_CANCEL_REJECT_TR(20072)
//ORDER_MOD_REJECT_TR(20042)
struct MS_OE_RESPONSE_TR
{
  TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t LogTime;
  int32_t UserId;
  int16_t ErrorCode;
  int64_t Timestamp1;
  char Timestamp2;
  char ModCanBy;
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR contract_desc_tr;
  char CloseoutFlag;
  double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t TotalVolumeRemaining;
  int32_t Volume;
  int32_t VolumeFilledToday;
  int32_t Price;
  int32_t GoodTillDate;
  int32_t EntryDateTime;
  int32_t LastModified;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  int32_t filler;
  double NnfField;
  int64_t Timestamp;
  char Reserved[20];
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[32];

  inline void swapBytes()
  {
    TransactionCode                         = __bswap_16(TransactionCode);
    LogTime                                 = __bswap_32(LogTime);
    UserId                                  = __bswap_32(UserId);
    ErrorCode                               = __bswap_16(ErrorCode);
    Timestamp1                              = swapDouble(Timestamp1);
    TokenNo                                 = __bswap_32(TokenNo);
    ReasonCode                              = __bswap_16(ReasonCode);
    //TODO:AB:Contract Desc
    OrderNumber                             = swapDouble(OrderNumber);
    BookType                                = __bswap_16(BookType);
    BuySellIndicator                        = __bswap_16(BuySellIndicator);
    DisclosedVolume                         = __bswap_32(DisclosedVolume);
    DisclosedVolumeRemaining                = __bswap_32(DisclosedVolumeRemaining);
    TotalVolumeRemaining                    = __bswap_32(TotalVolumeRemaining);
    Volume                                  = __bswap_32(Volume);
    VolumeFilledToday                       = __bswap_32(VolumeFilledToday);
    Price                                   = __bswap_32(Price);
    GoodTillDate                            = __bswap_32(GoodTillDate);
    EntryDateTime                           = __bswap_32(EntryDateTime);
    LastModified                            = __bswap_32(LastModified);
    //TODO:AB:Order Flags
    BranchId                                = __bswap_16(BranchId);
    TraderId                                = __bswap_32(TraderId);
    ProClientIndicator                      = __bswap_16(ProClientIndicator);
    //TODO:AB:Aditional Order Flags
    NnfField                                = swapDouble(NnfField);
    Timestamp                               = swapDouble(Timestamp);
  }

  inline void getData(MS_OE_REQUEST_TR *request)
  {
    UserId                                  = request->UserId;
    Timestamp2                              = ' ';
    ModCanBy                                = ' ';
    TokenNo                                 = request->TokenNo;
    ReasonCode                              = request->ReasonCode;
    contract_desc_tr                        = request->contract_desc_tr;//TODO:AB:byteswap for contract desc.
    CloseoutFlag                            = ' ';
    memcpy(AccountNumber,request->AccountNumber,sizeof(AccountNumber));
    BookType                                = request->BookType;
    BuySellIndicator                        = request->BuySellIndicator;
    DisclosedVolume                         = request->DisclosedVolume;
    Volume                                  = request->Volume;
    Price                                   = request->Price;
    GoodTillDate                            = request->GoodTillDate;
    OrderFlags                              = request->OrderFlags;
    BranchId                                = request->BranchId;
    TraderId                                = request->TraderId;
    memcpy(BrokerId,request->BrokerId,sizeof(BrokerId));
    OpenClose                               = ' ';
    memcpy(Settlor,request->Settlor,sizeof(Settlor));
    ProClientIndicator                      = request->ProClientIndicator;
    AddtnlOrderFlags1                       = request->AddtnlOrderFlags1;
    filler                                  = request->filler;
    NnfField                                = request->NnfField;
    //Reset Other fields
    TotalVolumeRemaining                    = Volume;
    VolumeFilledToday                       = 0;
    DisclosedVolumeRemaining                = DisclosedVolume;
   
  }
  inline void getData(MS_OM_REQUEST_TR *request)
  {
    //LogTime       = From MKT-Engine
    //ErrorCode     = From MKT-Engine
    UserId                        = request->UserId;
    Timestamp2                    =  ' ';
    ModCanBy                      = ' ';
    TokenNo                       = request->TokenNo;
    contract_desc_tr              = request->contract_desc_tr;//TODO:AB:byteswap for contract desc.
    CloseoutFlag                  = ' ';
    memcpy(AccountNumber,request->AccountNumber,sizeof(AccountNumber));
    OrderNumber                   = request->OrderNumber;
    BookType                      = request->BookType;
    BuySellIndicator              = request->BuySellIndicator;
    DisclosedVolume               = request->DisclosedVolume;
    DisclosedVolumeRemaining      = request->DisclosedVolumeRemaining;
    TotalVolumeRemaining          = request->TotalVolumeRemaining;
    Volume                        = request->Volume;
    VolumeFilledToday             = request->VolumeFilledToday;
    Price                         = request->Price;
    GoodTillDate                  = request->GoodTillDate;
    EntryDateTime                 = request->EntryDateTime;
    LastModified                  = request->LastModified;
    OrderFlags                    = request->OrderFlags;
    BranchId                      = request->BranchId;
    TraderId                      = request->TraderId;
    memcpy(BrokerId,request->BrokerId,sizeof(BrokerId));
    OpenClose                     = ' ';
    memcpy(Settlor,request->Settlor,sizeof(Settlor));
    ProClientIndicator            = request->ProClientIndicator;
    AddtnlOrderFlags1             = request->AddtnlOrderFlags1;
    filler                        = request->filler;
    NnfField                      = request->NnfField;
  }
  inline void getData(MS_OE_RESPONSE_TR *request)
  {
    UserId                        = request->UserId;
    Timestamp2                    =  ' ';
    ModCanBy                      = ' ';
    TokenNo                       = request->TokenNo;
    contract_desc_tr              = request->contract_desc_tr;//TODO:AB:byteswap for contract desc.
    CloseoutFlag                  = ' ';
    memcpy(AccountNumber,request->AccountNumber,sizeof(AccountNumber));
    OrderNumber                   = request->OrderNumber;
    BookType                      = request->BookType;
    BuySellIndicator              = request->BuySellIndicator;
    DisclosedVolume               = request->DisclosedVolume;
    DisclosedVolumeRemaining      = request->DisclosedVolumeRemaining;
    TotalVolumeRemaining          = request->TotalVolumeRemaining;
    Volume                        = request->Volume;
    VolumeFilledToday             = request->VolumeFilledToday;
    Price                         = request->Price;
    GoodTillDate                  = request->GoodTillDate;
    EntryDateTime                 = request->EntryDateTime;
    LastModified                  = request->LastModified;
    OrderFlags                    = request->OrderFlags;
    BranchId                      = request->BranchId;
    TraderId                      = request->TraderId;
    memcpy(BrokerId,request->BrokerId,sizeof(BrokerId));
    OpenClose                     = ' ';
    memcpy(Settlor,request->Settlor,sizeof(Settlor));
    ProClientIndicator            = request->ProClientIndicator;
    AddtnlOrderFlags1             = request->AddtnlOrderFlags1;
    filler                        = request->filler;
    NnfField                      = request->NnfField;
  }
};

//TRADE_CONFIRMATION_TR(20222)
struct TRADE_CONFIRMATION_TR 
{
  TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t LogTime;
  int32_t TraderId;
  int64_t Timestamp;
  int64_t Timestamp1;
  char Timestamp2[8];
  double ResponseOrderNumber;
  char BrokerId[5];
  char Reserved;
  char AccountNumber[10];
  int16_t BuySellIndicator;
  int32_t OriginalVolume;
  int32_t DisclosedVolume;
  int32_t RemainingVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t Price;
  ST_ORDER_FLAGS OrderFlags;
  int32_t GoodTillDate;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  int32_t Token;
  CONTRACT_DESC_TR contract_desc_tr;
  char OpenClose;
  char BookType;
  char Participant[12];
  ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void swapBytes()
  {
    TransactionCode                 = __bswap_16(TransactionCode);
    LogTime                         = __bswap_32(LogTime);
    TraderId                        = __bswap_32(TraderId);
    Timestamp                       = __bswap_64(Timestamp);
    Timestamp1                      = __bswap_64(Timestamp1);
    Token                           = __bswap_32(Token);
    //TODO:AB:Contract Desc
    ResponseOrderNumber             = swapDouble(ResponseOrderNumber);
    BookType                        = __bswap_16(BookType);
    BuySellIndicator                = __bswap_16(BuySellIndicator);
    DisclosedVolume                 = __bswap_32(DisclosedVolume);
    DisclosedVolumeRemaining        = __bswap_32(DisclosedVolumeRemaining);
    RemainingVolume                 = __bswap_32(RemainingVolume);
    OriginalVolume                  = __bswap_32(OriginalVolume);
    VolumeFilledToday               = __bswap_32(VolumeFilledToday);
    Price                           = __bswap_32(Price);
    GoodTillDate                    = __bswap_32(GoodTillDate);
    ActivityTime                    = __bswap_32(ActivityTime);
    //TODO:AB:Order Flags
    //TODO:AB:Aditional Order Flags
    FillNumber                      = __bswap_32(FillNumber);
    FillQuantity                    = __bswap_32(FillQuantity);
    FillPrice                       = __bswap_32(FillPrice);
  }

  inline void getData(MS_OE_RESPONSE_TR *request)
  {
    TraderId                        = request->UserId;
    Token                           = request->TokenNo;
    contract_desc_tr                = request->contract_desc_tr;//TODO:AB:byteswap for contract desc.
    ResponseOrderNumber             = request->OrderNumber;
    BookType                        = request->BookType;
    BuySellIndicator                = request->BuySellIndicator;
    DisclosedVolume                 = request->DisclosedVolume;
    DisclosedVolumeRemaining        = request->DisclosedVolumeRemaining;
    RemainingVolume                 = request->TotalVolumeRemaining;
    OriginalVolume                  = request->Volume;
    VolumeFilledToday               = request->VolumeFilledToday;
    Price                           = request->Price;
    GoodTillDate                    = request->GoodTillDate;
    OrderFlags                      = request->OrderFlags;
    AddtnlOrderFlags1               = request->AddtnlOrderFlags1;
    ActivityType[0]                 = ' ';
    ActivityType[1]                 = ' ';
    OpenClose                       = ' ';

    memcpy(BrokerId,request->BrokerId,5);
    memcpy(AccountNumber,request->AccountNumber,10);
    memcpy(Participant,request->Settlor,12);

  }


  inline void getData(MS_OE_REQUEST *request)
  {
    TraderId                = request->TraderId;
    Token                   = request->TokenNo;
    memcpy(&contract_desc_tr,&request->contract_desc,sizeof(contract_desc_tr));//TODO:AB:field mapping is samed
    ResponseOrderNumber     = request->OrderNumber;
    BookType                = request->BookType;
    BuySellIndicator        = request->BuySellIndicator;
    DisclosedVolume         = request->DisclosedVolume;
    DisclosedVolumeRemaining= request->DisclosedVolumeRemaining;
    RemainingVolume         = request->TotalVolumeRemaining;
    OriginalVolume          = request->Volume;
    VolumeFilledToday = request->VolumeFilledToday;
    Price                   = request->Price;
    GoodTillDate            = request->GoodTillDate;
    OrderFlags              = request->st_order_flags;
    AddtnlOrderFlags1       = request->additional_order_flags;
    ActivityType[0]         = ' ';
    ActivityType[1]         = ' ';
    OpenClose               = ' ';

    memcpy(BrokerId,request->BrokerId,5);
    memcpy(AccountNumber,request->AccountNumber,10);
    memcpy(Participant,request->Settlor,12);

  }

  inline void getData(MS_SPD_OE_REQUEST *request)
  {
    TraderId                = request->TraderId1;
    //TODO:AB:byteswap for contract desc.
    ResponseOrderNumber     = request->OrderNUmber1;
    GoodTillDate            = request->GoodTillDate1;
    ActivityType[0]         = ' ';
    ActivityType[1]         = ' ';
    OpenClose               = ' ';

    memcpy(BrokerId, request->BrokerId1 , sizeof(BrokerId));
    memcpy(AccountNumber,request->AccountNUmber1,sizeof(AccountNumber));
    memcpy(Participant,request->Settlor1,sizeof(Participant));
  }

};


typedef struct _BCAST_MSG
{
  char cNetID[2];
  short wNoOfPackets;
  char  cPackets[512];

  inline void swapBytes()
  {
    wNoOfPackets = __bswap_16(wNoOfPackets);
  }
} BCAST_MSG;

typedef struct _BCAST_COMPRESSION
{
  short wCompressionLen;
  char cBroadcastData[512];

  inline void swapBytes()
  {
    wCompressionLen = __bswap_16(wCompressionLen);
  }
}BCAST_COMPRESSION;

typedef unsigned int long32_t;
typedef unsigned char byte;

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

  inline void swapBytes()
  {
    wReserved1                       = __bswap_16(wReserved1);
    wReserved2                       = __bswap_16(wReserved2);
    lLogTime                         = __bswap_32(lLogTime);
    wTransCode                       = __bswap_16(wTransCode);
    wErrorCode                       = __bswap_16(wErrorCode);
    lBCSeqNo                         = __bswap_32(lBCSeqNo);
    lReserved2                       = __bswap_32(lReserved2);
    wMsgLen                          = __bswap_16(wMsgLen);

  }
} NNF_BCAST_HEADER;

typedef struct _NNF_MBP_INFO
{
  long32_t lQty;
  long32_t lPrice;
  short wNoOfOrders;
  short wbbBuySellFlag;

  inline void swapBytes()
  {
    lQty = __bswap_32(lQty);
    lPrice = __bswap_32(lPrice);
    wNoOfOrders = __bswap_16(wNoOfOrders);
    wbbBuySellFlag = __bswap_16(wbbBuySellFlag);

  }
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

  inline void swapBytes()
  {
    lToken                      = __bswap_32(lToken);
    wBookType                   = __bswap_16(wBookType);
    wTradingStatus              = __bswap_16(wTradingStatus);
    lVolTraded                  = __bswap_32(lVolTraded);
    lLTP                        = __bswap_32(lLTP) ;
    lNetChangeClosing           = __bswap_32(lNetChangeClosing);
    lLTQ                        = __bswap_32(lLTQ);
    lLTT                        = __bswap_32(lLTT);
    lATP                        = __bswap_32(lATP);
    wAuctionNo                  = __bswap_16(wAuctionNo);
    wAuctionStatus              = __bswap_16(wAuctionStatus);
    wInitiatorType              = __bswap_16(wInitiatorType);
    lInitiatorPrice             = __bswap_32(lInitiatorPrice);
    lInitiatorQty               = __bswap_32(lInitiatorQty);
    lAutionPrice                = __bswap_32(lAutionPrice);
    lAuctionQty                 = __bswap_32(lAuctionQty);

    for (unsigned char index=0;index<10;++index)
    {
      MBPRecord[index].swapBytes();
    }

    wBbTotalBuyFlag             = __bswap_16(wBbTotalBuyFlag);
    wBbTotalSellFlag            = __bswap_16(wBbTotalSellFlag);
    dblTotalBuyQty              = swapDouble(dblTotalBuyQty);
    dblTotalSellQty             = swapDouble(dblTotalSellQty);
    //NNF_MBP_INDICATOR nnfMBPInd;
    lClosePrice                 = __bswap_32(lClosePrice);
    lOpenPrice                  = __bswap_32(lOpenPrice);
    lHighPrice                  = __bswap_32(lHighPrice);
    lLowPrice                   = __bswap_32(lLowPrice);
  }
} NNF_MBP_DATA;

typedef struct _NNF_MBP_PACKET
{
#define  maxRecord  2
  NNF_BCAST_HEADER bcastHeader;
  short wNoOfRecords;
  NNF_MBP_DATA PriceData[maxRecord];

  inline void swapBytes(bool swapHeader = true, bool swapData=true)
  {
    if(swapHeader)
    {
      bcastHeader.swapBytes();
    }
    for(unsigned char index =0 ; swapData && (index < wNoOfRecords) && (wNoOfRecords <= maxRecord) ; ++index)
    {
      PriceData[index].swapBytes();
    }
    
    wNoOfRecords = __bswap_16(wNoOfRecords);
  }

} NNF_MBP_PACKET;

/*========       MKT_MVMT_CM_OI_IN       ========*/
typedef struct OPEN_INTEREST
{
  long32_t  lTokenNo;
  long32_t  lCurrentOI;

  inline void swapBytes()
  {
    lTokenNo    = __bswap_32(lTokenNo);
    lCurrentOI  = __bswap_32(lCurrentOI);
  }

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
  OPEN_INTEREST openInterest[58];

  inline void swapBytes()
  {
    lLogTime          = __bswap_32(lLogTime);
    nTransactionCode  = __bswap_16(nTransactionCode);
    nNoOfRecords      = __bswap_16(nNoOfRecords);
    wMsgLen           = __bswap_16(wMsgLen);

    for(short index =0 ; (index < nNoOfRecords) && (index < 58) ; ++index )
    {
      openInterest[index].swapBytes();
    }
  }

}NNF_NNF_CM_ASSET_OI;

#pragma pack()
/*===============================================*/


typedef MS_OE_REQUEST_TR    NSE_FO_NNF_NEW_TR;      //BOARD_LOT_IN(20000)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_NEW_ACCEPT_TR;  //ORDER_CONFIRMATION_OUT(20073)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_NEW_REJECT_TR;  //ORDER_ERROR_OUT(20231)

typedef MS_OM_REQUEST_TR    NSE_FO_NNF_MOD_TR;      //ORDER_MOD_IN(20040)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_MOD_ACCEPT_TR;  //ORDER_MOD_CONFIRM_OUT(20074)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_MOD_REJECT_TR;  //ORDER_MOD_REJ_OUT(20042)

typedef MS_OM_REQUEST_TR    NSE_FO_NNF_CXL_TR;      //ORDER_CANCEL_IN(20070)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_CXL_ACCEPT_TR;  //ORDER_CANCEL_CONFIRM_OUT(20075)
typedef MS_OE_RESPONSE_TR   NSE_FO_NNF_CXL_REJECT_TR;  //ORDER_CXL_REJ_OUT(20072)


const int16_t sLen_Logon_Req          = sizeof(MS_SIGNON_REQ);
const int16_t sLen_SysInfo_Req        = sizeof(MS_SYSTEM_INFO_REQ);
const int16_t sLen_UpdLDB_Req         = sizeof(MS_UPDATE_LOCAL_DATABASE);
const int16_t sLen_UpdPortfolio_Req   = sizeof(EXCH_PORTFOLIO_REQ);
const int16_t sLen_MsgDownload_Req    = sizeof(MS_MESSAGE_DOWNLOAD_REQ);


const int16_t sLen_Add_Req      = sizeof(MS_OE_REQUEST_TR);
const int16_t sLen_Mod_Can_Req  = sizeof(MS_OM_REQUEST_TR);
const int16_t sLen_Tap_Hdr      = sizeof(TAP_HEADER);
const int16_t sLen_Cust_Hdr     = sizeof(CUSTOM_HEADER);

void printExchangeMsg(const char *msgBuffer, char printBuffer[], int bufferLen);
static void printDebugMsg(char sDebugString[1024]);

} //nsamespace NSEFO

#endif

