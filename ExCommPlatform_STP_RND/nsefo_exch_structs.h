#ifndef NSE_FO_NNF_ORD_H_
#define NSE_FO_NNF_ORD_H_

#include<string>
#include<stdint.h>
#include<byteswap.h>
#include "nsefo_constants.h"
#include "common_structs.h"
#include <memory.h>

#pragma pack(2)

struct NSEFO_Order_Details
{
  int32_t iInterOrderNo;
  int16_t iInstanceId;
  int16_t sOrderType;
};

struct NSEFO_Token_Details
{
  int32_t iTokenNo;
  int32_t iTokenIndexValue;
};

struct NSEFO_FullMsgDnld_Details
{
  uint64_t NnfField;
  char chSettlor[12];
};

struct NSEFO_DirectConnectIPPort
{
  int32_t iRecNo;
  char chIP[16];
  int32_t iPort;
  
  NSEFO_DirectConnectIPPort()
  {
    iRecNo = 0;
    memset(chIP, 0, 16);
    iPort = 0;
  }
};

struct NSEFO_INVITATION_MESSAGE
{
  int16_t TransactionCode;
  int16_t InvitationCnt;
};

struct NSEFO_TAP_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  //int16_t sResrvSeqNo;
  //char chCheckSum[16];
  uint32_t CheckSum[4];
  //int16_t sMsgCnt;
};

// This is our own custom defined header [NOT EXCHANGE DEFINED], to optimize receive side processing

struct NSEFO_CUSTOM_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  //int16_t sResrvSeqNo;
  //char chCheckSum[16];
  uint32_t CheckSum[4];
  //int16_t sMsgCnt;
  int16_t sTransCode;
};

struct NSEFO_MESSAGE_HEADER
{
  int16_t TransactionCode;
  int32_t LogTime;
  char AlphaChar[2];
  int32_t TraderId;
  int16_t ErrorCode;
  char Timestamp[8];
  //char TimeStamp1[8];
  int64_t Timestamp1; //It contains Jiffy(int64) but NNF Doc defines it as char[8], so we have changed to to int64, to avoid type casting	
  char TimeStamp2[8];
  int16_t MessageLength;
};

//struct NSEFO_INNER_MESSAGE_HEADER
//{
//  int32_t iTraderId;
//  int32_t LogTime;
//  char AlphaChar[2];
//  int16_t TransactionCode;
//  int16_t ErrorCode;
//  char Timestamp[8];
//  //char TimeStamp1[8];
//  int64_t Timestamp1; //It contains Jiffy(int64) but NNF Doc defines it as double, so we have changed to to int64, to avoid type casting	
//  char TimeStamp2[8];
//  int16_t MessageLength;
//};

struct NSEFO_INNER_MESSAGE_HEADER
{
  int16_t TransactionCode;
  int32_t LogTime;
  char AlphaChar[2];
  int32_t iTraderId;  
  int16_t ErrorCode;
  char Timestamp[8];
  //char TimeStamp1[8];
  int64_t Timestamp1; //It contains Jiffy(int64) but NNF Doc defines it as double, so we have changed to to int64, to avoid type casting	
  char TimeStamp2[8];
  int16_t MessageLength;
};

struct NSEFO_BCAST_HEADER
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

struct NSEFO_ST_BROKER_ELIGIBILITY_PER_MKT
{
  unsigned char Reserved1 : 4;
  unsigned char AuctionMarket : 1;
  unsigned char Reserved2 : 1;
  unsigned char Reserved3 : 1;
  unsigned char Reserved4 : 1;
  char Reserved5;
};

struct NSEFO_ST_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct NSEFO_ST_EX_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct NSEFO_ST_PL_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

struct NSEFO_PORTFOLIO_DATA
{
  char Portfolio[10];
  int32_t Token;
  int32_t LastUpdtDtTime;
  char DeletFlag;
};

struct NSEFO_CONTRACT_DESC
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  int32_t StrikePrice;
  char OptionType[2];
  int16_t CALevel;

  NSEFO_CONTRACT_DESC()
  {
    memset(this, ' ', sizeof(NSEFO_CONTRACT_DESC));
    CALevel = 0;
  }
};

struct NSEFO_ST_ORDER_FLAGS
{
  unsigned char AON : 1;
  unsigned char IOC : 1;
  unsigned char GTC : 1;
  unsigned char Day : 1;
  unsigned char MIT : 1;
  unsigned char SL : 1;
  unsigned char Market : 1;
  unsigned char ATO : 1;
  unsigned char Reserved : 3;
  unsigned char Frozen : 1;
  unsigned char Modified : 1;
  unsigned char Traded : 1;
  unsigned char MatchedInd : 1;
  unsigned char MF : 1;
};

struct NSEFO_ADDITIONAL_ORDER_FLAGS
{
  unsigned char Reserved1 : 1;
  unsigned char COL : 1;
  unsigned char Reserved2 : 6;
};

struct NSEFO_CONTRACT_DESC_TR
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  int32_t StrikePrice;
  char OptionType[2];

  NSEFO_CONTRACT_DESC_TR()
  {
    memset(this, ' ', sizeof(NSEFO_CONTRACT_DESC_TR));
  }
};

struct NSEFO_MS_ERROR_RESPONSE
{
  NSEFO_MESSAGE_HEADER header;
  char Key[14];
  char ErrorMessage[128];
};

struct NSEFO_ST_STOCK_ELEGIBLE_INDICATORS
{
  unsigned char Reserved1 : 5;
  unsigned char BooksMerged : 1;
  unsigned char MinFill : 1;
  unsigned char AON : 1;
  char Reserved2;
};


struct NSEFO_LB_QUERY_REQ
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;  
  char BrokerId[5];
  char Filler;
  int16_t Reserved;
};

struct NSEFO_LB_QUERY_RESP
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;  
  char IPAddress[16];
  char Port[6];
  char SessionKey[32];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_LB_QUERY_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
  
};


struct NSEFO_MS_HEART_BEAT_REQ
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
};

//NEW LOGIN CHANGES STARTS
struct NSEFO_MS_GR_REQUEST //gateway router Request
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
};

struct NSEFO_MS_GR_RESPONSE
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
  char cIPAddress[16];
  int32_t iPort;
  char cSessionKey[8];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_GR_RESPONSE));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

struct NSEFO_MS_BOX_SIGN_ON_REQUEST_IN
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerId[5];
  char cReserved[5]; 
  char cSessionKey[8];
};

struct NSEFO_MS_BOX_SIGN_ON_REQUEST_OUT
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cReserved[5];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_BOX_SIGN_ON_REQUEST_OUT));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

//NEW LOGIN CHANGES ENDS


struct NSEFO_MS_SIGNON_REQ //SIGN_ON_REQUEST_IN(2300)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
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
  NSEFO_ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
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

struct NSEFO_MS_SIGNON_RESP // SIGN_ON_REQUEST_OUT(2301)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
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
  NSEFO_ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
  int16_t MemberType;
  char ClearingStatus;
  char BrokerName[25];
  char Reserved7[16];
  char Reserved8[16];
  char Reserved9[16];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_SIGNON_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

struct NSEFO_MS_SYSTEM_INFO_REQ //SYSTEM_INFORMATION_IN(1600)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t LastUpdatePortfolioTime;
};

struct NSEFO_MS_SYSTEM_INFO_DATA //SYSTEM_INFORMATION_OUT(1601)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  NSEFO_ST_MARKET_STATUS st_market_status;
  NSEFO_ST_EX_MARKET_STATUS st_ex_market_status;
  NSEFO_ST_PL_MARKET_STATUS st_PL_market_status;
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
  NSEFO_ST_STOCK_ELEGIBLE_INDICATORS st_stock_eligible_indicators;
  int16_t DiscQtyPerAllwd;
  int32_t RiskFreeInterestRate;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_SYSTEM_INFO_DATA));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    DiscQtyPerAllwd = __bswap_16(DiscQtyPerAllwd);
  }
};

struct NSEFO_MS_UPDATE_LOCAL_DATABASE //UPDATE_LOCALDB_IN(7300)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t LastUpdateSecTime;
  int32_t LastUpdateParticipantTime;
  int32_t LastUpdateInstTime;
  int32_t LastUpdateIndexTime;
  char ReqForOpenOrders;
  char Reserved1;
  NSEFO_ST_MARKET_STATUS st_market_status;
  NSEFO_ST_EX_MARKET_STATUS st_ex_market_status;
  NSEFO_ST_PL_MARKET_STATUS st_pl_market_status;
};

struct NSEFO_MS_PARTIAL_SYS_INFO //PARTIAL_SYSTEM_INFO(7321)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  NSEFO_ST_MARKET_STATUS st_market_status;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr, pchData, sizeof(NSEFO_MS_PARTIAL_SYS_INFO));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);

    st_market_status.Normal = __bswap_16(st_market_status.Normal);
    st_market_status.Oddlot = __bswap_16(st_market_status.Oddlot);
    st_market_status.Spot = __bswap_16(st_market_status.Spot);
    st_market_status.Auction = __bswap_16(st_market_status.Auction);
  }
};

struct NSEFO_UPDATE_LDB_HEADER //UPDATE_LOCALDB_HEADER(7307)  //UPDATE_LOCALDB_TRAILER(7308)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  char Reserved1[2];
};

struct NSEFO_UPDATE_LDB_DATA //UPDATE_LOCALDB_DATA(7304)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  NSEFO_INNER_MESSAGE_HEADER inheader;
  char Data[436];
};

struct NSEFO_INDEX_DETAILS
{
  char IndexName[16];
  int32_t Token;
  int32_t LastUpdateDateTime;
};

struct NSEFO_MS_DOWNLOAD_INDEX //BCAST_INDEX_MSTR_CHG(7325)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  NSEFO_INDEX_DETAILS inxdet[17];
};

struct NSEFO_BCAST_INDEX_MAP_DETAILS
{
  char BcastName[26];
  char ChangedName[10];
  char DeleteFlag;
  int32_t LastUpdateDateTime;
};

struct NSEFO_MS_DOWNLOAD_INDEX_MAP //BCAST_INDEX_MAP_TABLE(7326)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  NSEFO_BCAST_INDEX_MAP_DETAILS sIndicesMap[10];
};

struct NSEFO_EXCH_PORTFOLIO_REQ //EXCH_PORTF_IN(1775)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t LastUpdateDtTime;
};

struct NSEFO_EXCH_PORTFOLIO_RESP //EXCH_PORTF_OUT(1776)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  char MoreRecs;
  char Filler;
  NSEFO_PORTFOLIO_DATA portfolio_data[15];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_EXCH_PORTFOLIO_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

struct NSEFO_MS_MESSAGE_DOWNLOAD_REQ //DOWNLOAD_REQUEST(7000)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  double SeqNo;
};

struct NSEFO_MS_MESSAGE_DOWNLOAD_HEADER //TRAILER_RECORD(7011)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_MESSAGE_DOWNLOAD_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

struct NSEFO_MS_MESSAGE_DOWNLOAD_DATA //DOWNLOAD_DATA(7021)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  NSEFO_INNER_MESSAGE_HEADER inner_hdr;
  char Data[436];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_MESSAGE_DOWNLOAD_DATA));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    inner_hdr.TransactionCode = __bswap_16(inner_hdr.TransactionCode);
  }
};

struct NSEFO_MS_MESSAGE_DOWNLOAD_TRAILER //TRAILER_RECORD(7031)
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_MESSAGE_DOWNLOAD_TRAILER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};

struct NSEFO_SIGNOFF_OUT //SIGN_OFF_REQUEST_OUT(2321)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t UserId;
  char Reserved[145];
};

struct NSEFO_INSTANCE_ORDER_ID
{
  uint32_t TransactionId : 24;    //Remaining bits will be for Order No
  uint8_t InstanceId : 8;         //Supporting 256 Instances
};

/**************************************************************/

typedef struct NSEFO_MS_OE_REQUEST //BOARD_LOT_IN(2000)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  unsigned char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  unsigned char reserved2;
  int16_t ReasonCode;
  unsigned char reserved3[4];
  uint32_t TokenNo;
  NSEFO_CONTRACT_DESC contract_desc;
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
  NSEFO_ST_ORDER_FLAGS st_order_flags;
  int16_t BranchId;
  uint32_t TraderId;
  char BrokerId[5];
  char cOrdFiller[24];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  NSEFO_ADDITIONAL_ORDER_FLAGS additional_order_flags;
  char GiveUpFlag;
  //In NNF Doc it is defined in this way, we are changing it to int32_t, to be in sync with Trimmed Structs	
  //	unsigned char filler1to16[2];
  //	char filler17;
  //	char filler18;
  int32_t filler;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_OE_REQUEST) - sizeof(NSEFO_TAP_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    //double dOrrNo         = OrderNumber;
    //int64_t llExOrderNo   = OrderNumber;
    SwapDouble((char*) &OrderNumber);
    LastModified = __bswap_32(LastModified);
    EntryDateTime = __bswap_32(EntryDateTime);
    ReasonCode = __bswap_16(ReasonCode);
    TokenNo = __bswap_32(TokenNo);
    BookType = __bswap_16(BookType);
    Volume = __bswap_32(Volume);
    TraderId = __bswap_32(TraderId);
    //NnfField              = __bswap_64(NnfField);	
    SwapDouble((char*) &NnfField);
    //MktReplay					= __bswap_64(MktReplay);	
  }
  
  void Default_Init(int32_t _UserID, std::string _TMID, int16_t _BranchId, int16_t _STPC)
  {
    
    msg_hdr.TraderId = __bswap_32(_UserID);
    TraderId = __bswap_32(_UserID);
    //st_order_flags.STPC = _STPC;
    BranchId = __bswap_16(_BranchId);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
  }    

  NSEFO_MS_OE_REQUEST()
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_OE_REQUEST));
    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST));
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED as mentioned in NNF Doc
    msg_hdr.LogTime = 0;
    //--msg_hdr.TraderId = __bswap_32(_UserID);
    msg_hdr.ErrorCode = 0;
    memset(msg_hdr.Timestamp, 0, 8);
    msg_hdr.Timestamp1 = 0;

    memset(msg_hdr.TimeStamp2, 0, 8);
    msg_hdr.MessageLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST) - sizeof(NSEFO_TAP_HEADER));

    //msg_hdr.TransactionCode = __bswap_16(NSEFO_ADD_REQ);
    msg_hdr.TransactionCode = NSEFO_ADD_REQ;
    //--msg_hdr.TraderId = __bswap_32(_UserID);

    ParticipantType = 'S';
    //reserved1;
    CompetitorPeriod = 0;
    SolicitorPeriod = 0;
    //ModifiedCancelledBy;
    //reserved2;
    ReasonCode = 0;
    //reserved3[4];
    //TokenNo;
    //contract_desc;
    //CounterPartyBrokerId[5];
    //reserved4;
    //reserved5[2];
    CloseoutFlag = 'O';
    //reserved6;
    OrderType = 0;
    OrderNumber = 0;
    //AccountNumber[10];
    //BookType;
    //BuySellIndicator;
    //DisclosedVolume;
    //DisclosedVolumeRemaining;
    //TotalVolumeRemaining;
    //Volume;
    //VolumeFilledToday;
    //Price;
    //TriggerPrice;
    GoodTillDate = 0;
    EntryDateTime = 0;
    MinimumFill = 0;
    //LastModified;
    st_order_flags.AON = 0;
    st_order_flags.ATO = 0;
    st_order_flags.Day = 1;
    st_order_flags.Frozen = 0;
    st_order_flags.GTC = 0;
    st_order_flags.IOC = 0;
    st_order_flags.MF = 0;
    st_order_flags.MIT = 0;
    st_order_flags.Market = 0;
    st_order_flags.MatchedInd = 0;
    st_order_flags.Modified = 0;
    //st_order_flags.Reserved
    st_order_flags.SL = 0;
    st_order_flags.Traded = 0;
    //--BranchId = __bswap_16(_BranchId);
    //--TraderId = __bswap_32(_UserID);
    //--memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    //cOrdFiller[24];
    OpenClose = 'O';
    //--memcpy(Settlor, _TMID.c_str(), _TMID.length());
    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	
    SettlementPeriod = 0;
    //--additional_order_flags.COL = _COL;
    additional_order_flags.COL = 1; //Need not Twiddle this
    //additional_order_flags.Reserved1;
    //additional_order_flags.Reserved2;
    GiveUpFlag = 'P';
    //filler;
    //NnfField;
    MktReplay = 0;
  }

  NSEFO_MS_OE_REQUEST(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL, int16_t _STPC)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_OE_REQUEST));
    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST));
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED as mentioned in NNF Doc
    msg_hdr.LogTime = 0;
    msg_hdr.TraderId = __bswap_32(_UserID);
    msg_hdr.ErrorCode = 0;
    memset(msg_hdr.Timestamp, 0, 8);
    msg_hdr.Timestamp1 = 0;

    memset(msg_hdr.TimeStamp2, 0, 8);
    msg_hdr.MessageLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST) - sizeof(NSEFO_TAP_HEADER));

    //msg_hdr.TransactionCode = __bswap_16(NSEFO_ADD_REQ);
    msg_hdr.TransactionCode = NSEFO_ADD_REQ;
    msg_hdr.TraderId = __bswap_32(_UserID);

    ParticipantType = 'S';
    //reserved1;
    CompetitorPeriod = 0;
    SolicitorPeriod = 0;
    //ModifiedCancelledBy;
    //reserved2;
    ReasonCode = 0;
    //reserved3[4];
    //TokenNo;
    //contract_desc;
    //CounterPartyBrokerId[5];
    //reserved4;
    //reserved5[2];
    CloseoutFlag = 'O';
    //reserved6;
    OrderType = 0;
    OrderNumber = 0;
    //AccountNumber[10];
    //BookType;
    //BuySellIndicator;
    //DisclosedVolume;
    //DisclosedVolumeRemaining;
    //TotalVolumeRemaining;
    //Volume;
    //VolumeFilledToday;
    //Price;
    //TriggerPrice;
    GoodTillDate = 0;
    EntryDateTime = 0;
    MinimumFill = 0;
    //LastModified;
    st_order_flags.AON = 0;
    st_order_flags.ATO = 0;
    st_order_flags.Day = 1;
    st_order_flags.Frozen = 0;
    st_order_flags.GTC = 0;
    st_order_flags.IOC = 0;
    st_order_flags.MF = 0;
    st_order_flags.MIT = 0;
    st_order_flags.Market = 0;
    st_order_flags.MatchedInd = 0;
    st_order_flags.Modified = 0;
    //st_order_flags.Reserved
    st_order_flags.SL = 0;
    st_order_flags.Traded = 0;
    BranchId = __bswap_16(_BranchId);
    TraderId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    //cOrdFiller[24];
    OpenClose = 'O';
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	
    SettlementPeriod = 0;
    additional_order_flags.COL = _COL;
    //additional_order_flags.Reserved1;
    //additional_order_flags.Reserved2;
    GiveUpFlag = 'P';
    //filler;
    //NnfField;
    MktReplay = 0;
  }

} MS_OE_REQUEST;

struct NSEFO_MS_ORDER_BACKUP
{
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
  NSEFO_ST_ORDER_FLAGS st_order_flags;
};


//This is a custom structure, to facilitate optimized response processing

typedef struct NSEFO_MS_OE_RESPONSE //BOARD_LOT_IN(2000)
{
  NSEFO_MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  unsigned char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  unsigned char reserved2;
  int16_t ReasonCode;
  unsigned char reserved3[4];
  uint32_t TokenNo;
  NSEFO_CONTRACT_DESC contract_desc;
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
  NSEFO_ST_ORDER_FLAGS st_order_flags;
  int16_t BranchId;
  uint32_t TraderId;
  char BrokerId[5];
  char cOrdFiller[24];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  NSEFO_ADDITIONAL_ORDER_FLAGS additional_order_flags;
  char GiveUpFlag;
  NSEFO_INSTANCE_ORDER_ID InsOrdId;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(NSEFO_MS_OE_RESPONSE) - sizeof(NSEFO_TAP_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);


    //double dOrrNo = OrderNumber;

    //int64_t llExOrderNo	= OrderNumber;
    SwapDouble((char*) &OrderNumber);
    LastModified = __bswap_32(LastModified);
    EntryDateTime = __bswap_32(EntryDateTime);
    ReasonCode = __bswap_16(ReasonCode);
    TokenNo = __bswap_32(TokenNo);
    BookType = __bswap_16(BookType);
    Volume = __bswap_32(Volume);
    TraderId = __bswap_32(TraderId);
    //NnfField					= __bswap_64(NnfField);	
    SwapDouble((char*) &NnfField);
    //MktReplay					= __bswap_64(MktReplay);	
  }
} NSEFO_MS_OE_RESPONSE;



typedef MS_OE_REQUEST NSE_FO_NNF_NEW; //BOARD_LOT_IN(2000)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_NEW_ACCEPT; //ORDER_CONFIRMATION_OUT(2073)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_NEW_REJECT; //ORDER_ERROR_OUT(2231)

typedef NSE_FO_NNF_NEW NSE_FO_NNF_MOD; //ORDER_MOD_IN(2040)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_MOD_ACCEPT; //ORDER_MOD_CONFIRM_OUT(2074)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_MOD_REJECT; //ORDER_MOD_REJ_OUT(2042)

typedef NSE_FO_NNF_NEW NSE_FO_NNF_CXL; //ORDER_CANCEL_IN(2070)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_CXL_ACCEPT; //ORDER_CANCEL_CONFIRM_OUT(2075)
typedef NSE_FO_NNF_NEW NSE_FO_NNF_CXL_REJECT; //ORDER_CXL_REJ_OUT(2072)

typedef NSE_FO_NNF_NEW NSE_FO_NNF_KILL_ALL; //KILL_SWITCH_IN(2063)

typedef struct NSEFO_MS_SPD_LEG_INFO2
{
  int32_t Token2;
  NSEFO_CONTRACT_DESC SecurityInformation2;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags2;
  char OpenClose2;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags2;
  char GiveUpFlag2;
  char FillerY;
} MS_SPD_LEG_INFO2;

typedef struct NSEFO_MS_SPD_LEG_INFO3
{
  int32_t Token3;
  NSEFO_CONTRACT_DESC SecurityInformation3;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags3;
  char OpenClose3;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags3;
  char GiveUpFlag3;
  char FillerZ;
} NSEFO_MS_SPD_LEG_INFO3;

typedef struct NSEFO_MS_SPD_OE_REQUEST
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
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
  NSEFO_CONTRACT_DESC SecurityInformation1;
  char OpBrokerId1[5];
  char Fillerx1;
  char FillerOpions1[3];
  char Fillery1;
  int16_t OrderType1;
  double OrderNumber1;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags1;
  int16_t BranchId1;
  uint32_t TraderId1;
  char BrokerId1[5];
  char cOrdFiller[24];
  char OpenClose1;
  char Settlor1[12];
  int16_t ProClient1;
  int16_t SettlementPeriod1;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  char GiveUpFlag1;
  //In NNF Doc it is defined in this way, we are changing it to int32_t
  //	unsigned char filler1to16[2];
  //	char filler17;
  //	char filler18;
  int32_t filler;
  double NnfField;
  double MktReplay;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
  int32_t PriceDiff;
  NSEFO_MS_SPD_LEG_INFO2 leg2;
  NSEFO_MS_SPD_LEG_INFO3 leg3;

  NSEFO_MS_SPD_OE_REQUEST()
  {
  }

  NSEFO_MS_SPD_OE_REQUEST(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_SPD_OE_REQUEST));
//    std::cout << "Total Spd Msg Length = " << sizeof(_MS_SPD_OE_REQUEST) << std::endl;
    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_SPD_OE_REQUEST));
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED as mentioned in NNF Doc

    msg_hdr.LogTime = 0;
    //msg_hdr.AlphaChar
    msg_hdr.TraderId = __bswap_32(_UserID);
    msg_hdr.ErrorCode = 0;
    memset(msg_hdr.Timestamp, 0, 8);
    //memset(&msg_hdr.Timestamp1, 0, 8);
    msg_hdr.Timestamp1 = 0;

    memset(msg_hdr.TimeStamp2, 0, 8);
//    std::cout << "Spd Msg Length = " << (sizeof(_MS_SPD_OE_REQUEST) - sizeof(TAP_HEADER)) << std::endl;
    msg_hdr.MessageLength = __bswap_16(sizeof(NSEFO_MS_SPD_OE_REQUEST) - sizeof(NSEFO_TAP_HEADER));


    //ParticipantType1;
    //Filler1;
    CompetitorPeriod1 = 0;
    SolicitorPeriod1 = 0;
    //ModCxlBy1;
    //Filler9;
    ReasonCode1 = 0;
    //StartAlpha;
    //EndAlpha;
    //Token1;
    //SecurityInformation1;
    SecurityInformation1.CALevel = 0;
    //OpBrokerId1;
    //Fillerx1;
    //FillerOpions1;
    //Fillery1;
    OrderType1 = 0;
    OrderNumber1 = 0;
    //AccountNUmber1;
    BookType1 = __bswap_16(1);
    //BuySell1;
    DisclosedVol1 = 0;
    DisclosedVolRemaining1 = 0;
    //TotalVolRemaining1;
    //Volume1;
    VolumeFilledToday1 = 0;
    Price1 = 0;
    TriggerPrice1 = 0;
    GoodTillDate1 = 0;
    EntryDateTime1 = 0;
    MinFillAon1 = 0;
    LastModified1 = 0;
    memset(&OrderFlags1, 0, 2);
    BranchId1 = __bswap_16(_BranchId);
    TraderId1 = __bswap_32(_UserID);
    memcpy(BrokerId1, _TMID.c_str(), _TMID.length());

    //cOrdFiller;
    OpenClose1 = 'O';
    memcpy(Settlor1, _TMID.c_str(), _TMID.length());
    ProClient1 = __bswap_16(1); //1:Client 2:Pro	
    SettlementPeriod1 = 0;
    AddtnlOrderFlags1.COL = _COL; //Need not twiddle this
    //AddtnlOrderFlags1.COL               = 0;		//Need not twiddle this
    AddtnlOrderFlags1.Reserved2 = 0;
    //GiveUpFlag1;
    //filler1to16;
    //filler17;
    //filler18;
    //NnfField;
    MktReplay = 0;
    //PriceDiff;

    //leg2.Token2;
    leg2.SecurityInformation2.CALevel = 0;
    //leg2.OpBrokerId2;
    //leg2.Fillerx2;
    leg2.OrderType2 = 0;
    //leg2.BuySell2;
    leg2.DisclosedVol2 = 0;
    leg2.DisclosedVolRemaining2 = 0;
    //leg2.TotalVolRemaining2;
    //leg2.Volume2;
    leg2.VolumeFilledToday2 = 0;
    leg2.Price2 = 0;
    leg2.TriggerPrice2 = 0;
    leg2.MinFillAon2 = 0;
    memset(&leg2.OrderFlags2, 0, 2);
    leg2.OpenClose2 = 'O';
    leg2.AddtnlOrderFlags2.COL = _COL; //Need not twiddle this
    //leg2.AddtnlOrderFlags2.COL          = 0;			//Need not twiddle this
    leg2.AddtnlOrderFlags2.Reserved2 = 0;
    //leg2.GiveUpFlag2;
    //leg2.FillerY;

    leg3.Token3 = 0;
    leg3.SecurityInformation3.ExpiryDate = 0;
    leg3.SecurityInformation3.StrikePrice = 0;
    leg3.SecurityInformation3.CALevel = 0;
    //leg3.OpBrokerId3;
    //leg3.Fillerx3;
    leg3.OrderType3 = 0;
    leg3.BuySell3 = 0;
    leg3.DisclosedVol3 = 0;
    leg3.DisclosedVolRemaining3 = 0;
    leg3.TotalVolRemaining3 = 0;
    leg3.Volume3 = 0;
    leg3.VolumeFilledToday3 = 0;
    leg3.Price3 = 0;
    leg3.TriggerPrice3 = 0;
    leg3.MinFillAon3 = 0;
    memset(&leg3.OrderFlags3, 0, 2);
    leg3.OpenClose3 = 'O';
    leg3.AddtnlOrderFlags3.COL = _COL;
    //leg3.AddtnlOrderFlags3.COL          = 0;
    leg3.AddtnlOrderFlags3.Reserved2 = 0;
    //leg3.GiveUpFlag3;
    //leg3.FillerZ;
  }

} NSEFO_MS_SPD_OE_REQUEST;

struct NSEFO_MS_SPD_ORDER_BACKUP
{
  int32_t TotalVolumeRemaining1;
  int32_t Volume1;
  int32_t PriceDiff;
  int32_t TotalVolumeRemaining2;
  int32_t Volume2;
};

struct NSEFO_MS_SPD_ML_ORDER_DETAILS //This is not the Exchange defined struct, it is our own custom defined struct
{
  NSEFO_MS_SPD_OE_REQUEST ms_spd_ml_request;
  NSEFO_MS_SPD_ORDER_BACKUP ms_spd_ml_order_backup;
  int16_t sStatus;
};

struct NSEFO_MS_ORDER_DETAILS //This is not the Exchange defined struct, it is our own custom defined struct
{
  NSEFO_MS_OE_REQUEST ms_oe_request;
  NSEFO_MS_ORDER_BACKUP ms_order_backup;
  int16_t sStatus;
};


typedef NSEFO_MS_SPD_OE_REQUEST NSE_FO_NNF_SPD_NEW; //SP_BOARD_LOT_IN(2100)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_NEW_ACCEPT; //SP_ORDER_CONFIRMATION(2124)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_NEW_REJECT; //SP_ORDER_ERROR(2154)

typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_MOD; //SP_ORDER_MOD_IN(2118)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_MOD_ACCEPT; //SP_ORDER_MOD_CON_OUT(2136)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_MOD_REJECT; //SP_ORDER_MOD_REJ_OUT(2133)

typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_CXL; //SP_ORDER_CANCEL_IN(2106)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_CXL_ACCEPT; //SP_ORDER_CXL_CONFIRMATION(2130)
typedef NSE_FO_NNF_SPD_NEW NSE_FO_NNF_SPD_CXL_REJECT; //SP_ORDER_CXL_REJ_OUT(2127)

typedef NSEFO_MS_SPD_OE_REQUEST NSE_FO_NNF_ML_NEW; //TWOL_BOARD_LOT_IN(2102) or THRL_BOARD_LOT_IN(2104)
typedef NSE_FO_NNF_ML_NEW NSE_FO_NNF_ML_NEW_ACCEPT; //TWOL_ORDER_CONFIRMATION(2125) OR THRL_ORDER_CONFIRMATION(2126)
typedef NSE_FO_NNF_ML_NEW NSE_FO_NNF_ML_NEW_REJECT; //TWOL_ORDER_ERROR(2155) OR THRL_ORDER_ERROR(2156)

typedef NSE_FO_NNF_ML_NEW NSE_FO_NNF_ML_CXL_ACCEPT; //TWOL_ORDER_CXL_CONFIRMATION(2131) OR THRL_ORDER_CXL_CONFIRMATION(2132) OR ORDER_CANCEL_CONFIRM_OUT(2075)

typedef struct NSEFO_MS_TRADE_CONFRIM
{
  //TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags;
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
  NSEFO_CONTRACT_DESC contract_desc;
  char OpenClose;
  char OldOpenClose;
  char BookType;
  int32_t NewVolume;
  char OldAccountNumber[10];
  char Participant[12];
  char OldParticipant[12];
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags;
  char ReservedFiller;
  char GiveUpTrade;
  char PAN[10];
  char oldPAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
} NSEFO_MS_TRADE_CONFIRM;

typedef NSEFO_MS_TRADE_CONFIRM NSE_FO_NNF_TRADE; //TRADE_CONFIRM(2222)

/**************************************************************/

struct NSEFO_PRICE_VOL_MOD //PRICE_VOL_MOD (2013)
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t TokenNo;
  int32_t TraderId;
  double OrderNum;
  int16_t BuySell;
  int32_t Price;
  int32_t Volume;
  int32_t LastModified;
  char Reference[4];
};

struct NSEFO_MS_TRADE_INQ_DATA
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  int32_t TokenNo;
  NSEFO_CONTRACT_DESC contract_desc;
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

struct NSEFO_MS_OE_REQUEST_TR
{
  NSEFO_TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t UserId;
  int16_t ReasonCode;
  int32_t TokenNo;
  NSEFO_CONTRACT_DESC_TR contract_desc_tr;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t Volume;
  int32_t Price;
  int32_t GoodTillDate;
  NSEFO_ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  NSEFO_INSTANCE_ORDER_ID InsOrdId;
  double NnfField;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[32];

  void Default_Init(int32_t _UserID, std::string _TMID, int16_t _BranchId, int16_t _STPC)
  {
    TraderId = __bswap_32(_UserID);
    //OrderFlags.STPC = _STPC;
    BranchId = __bswap_16(_BranchId);
    UserId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
  }  
  
  NSEFO_MS_OE_REQUEST_TR()
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_OE_REQUEST_TR));

    TransactionCode = __bswap_16(NSEFO_ADD_REQ_TR);
    
    ReasonCode = 0;
    OpenClose = 'O'; //HARDCODED
    AddtnlOrderFlags1.COL = 1; //Need not twiddle this
    BookType = __bswap_16(1);

    GoodTillDate = 0;
    OrderFlags.AON = 0;
    OrderFlags.GTC = 0;
    OrderFlags.MIT = 0;
    OrderFlags.Market = 0;
    OrderFlags.ATO = 0;
    OrderFlags.Reserved = 0;
    OrderFlags.Frozen = 0;
    OrderFlags.Modified = 0;
    OrderFlags.Traded = 0;
    OrderFlags.MatchedInd = 0;
    OrderFlags.MF = 0;

    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	
    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST_TR));
    
  }

  NSEFO_MS_OE_REQUEST_TR(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL, int16_t _STPC)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_OE_REQUEST_TR));

    TransactionCode = __bswap_16(NSEFO_ADD_REQ_TR);
    UserId = __bswap_32(_UserID);
    ReasonCode = 0;
    BranchId = __bswap_16(_BranchId);
    TraderId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    OpenClose = 'O'; //HARDCODED
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
    AddtnlOrderFlags1.COL = _COL; //Need not twiddle this
    BookType = __bswap_16(1);

    GoodTillDate = 0;
    OrderFlags.AON = 0;
    //OrderFlags.IOC      = rAddReq.OrderTerms.IOC;
    OrderFlags.GTC = 0;
    //OrderFlags.Day      = rAddReq.OrderTerms.DAY;
    OrderFlags.MIT = 0;
    //OrderFlags.SL       = rAddReq.OrderTerms.SL;
    OrderFlags.Market = 0;
    OrderFlags.ATO = 0;
    OrderFlags.Reserved = 0;
    OrderFlags.Frozen = 0;
    OrderFlags.Modified = 0;
    OrderFlags.Traded = 0;
    OrderFlags.MatchedInd = 0;
    OrderFlags.MF = 0;

    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	

    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST_TR));
    //tap_hdr.sResrvSeqNo = 0;
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED, as specified in NNF doc	
  }
};


//ORDER_QUICK_CANCEL_IN_TR(20060)
//ORDER_CANCEL_IN_TR(20070)
//ORDER_MOD_IN_TR(20040)

struct NSEFO_MS_OM_REQUEST_TR
{
  NSEFO_TAP_HEADER tap_hdr;
  int16_t TransactionCode;
  int32_t UserId;
  char ModCanBy;
  int32_t TokenNo;
  NSEFO_CONTRACT_DESC_TR contract_desc_tr;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  //int32_t filler;
  NSEFO_INSTANCE_ORDER_ID InsOrdId;
  double NnfField;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[32];
  NSEFO_MS_OM_REQUEST_TR()
  {
    
  }
  NSEFO_MS_OM_REQUEST_TR(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL, int16_t _STPC)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(NSEFO_MS_OE_REQUEST_TR));

    TransactionCode = __bswap_16(NSEFO_ADD_REQ_TR);
    UserId = __bswap_32(_UserID);
    
    BranchId = __bswap_16(_BranchId);
    TraderId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    OpenClose = 'O'; //HARDCODED
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
    AddtnlOrderFlags1.COL = _COL; //Need not twiddle this
    BookType = __bswap_16(1);

    GoodTillDate = 0;
    OrderFlags.AON = 0;
    //OrderFlags.IOC      = rAddReq.OrderTerms.IOC;
    OrderFlags.GTC = 0;
    //OrderFlags.Day      = rAddReq.OrderTerms.DAY;
    OrderFlags.MIT = 0;
    //OrderFlags.SL       = rAddReq.OrderTerms.SL;
    OrderFlags.Market = 0;
    OrderFlags.ATO = 0;
    OrderFlags.Reserved = 0;
    OrderFlags.Frozen = 0;
    OrderFlags.Modified = 0;
    OrderFlags.Traded = 0;
    OrderFlags.MatchedInd = 0;
    OrderFlags.MF = 0;

    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	

    tap_hdr.sLength = __bswap_16(sizeof(NSEFO_MS_OE_REQUEST_TR));
    //tap_hdr.sResrvSeqNo = 0;
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED, as specified in NNF doc	
  }
  //	void PopulateModFromNewReq(MS_OE_REQUEST_TR& oNewOrdReq)
  //	{
  //		tap_hdr				= oNewOrdReq.tap_hdr;
  //		TransactionCode	= __bswap_16(NSEFO_MOD_REQ_TR);
  //	}

};

//PRICE_CONFIRMATION_TR(20012)
//ORDER_ERROR_TR(20231)
//ORDER_CXL_CONFIRMATION_TR(20075)
//ORDER_MOD_CONFIRMATION_TR(20074)
//ORDER_CONFIRMATION_TR(20073)
//ORDER_CANCEL_REJECT_TR(20072)
//ORDER_MOD_REJECT_TR(20042)

struct NSEFO_MS_OE_RESPONSE_TR
{
  //TAP_HEADER tap_hdr;	
  int16_t TransactionCode;
  int32_t LogTime;
  int32_t UserId;
  int16_t ErrorCode;
  //double Timestamp1;
  int64_t TimeStamp1; //It contains Jiffy(int64) but NNF Doc defines it as double, so we have changed to to int64, to avoid type casting
  char TimeStamp2;
  char ModCanBy;
  int16_t ReasonCode;
  int32_t TokenNo;
  NSEFO_CONTRACT_DESC_TR contract_desc_tr;
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
  NSEFO_ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  //int32_t filler;
  NSEFO_INSTANCE_ORDER_ID InsOrdId;  
  double NnfField;
  int64_t Timestamp;
  char Reserved[20];
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];

  inline void Initialize(char* pchData)
  {
    //int64_t llOrderNo = 0;
    memcpy(&TransactionCode, pchData, sizeof(NSEFO_MS_OE_RESPONSE_TR));
    TransactionCode = __bswap_16(TransactionCode);
    ErrorCode = __bswap_16(ErrorCode);

    //memcpy(&llOrderNo, &OrderNumber, 8);
    //llOrderNo = OrderNumber;
    //OrderNumber			= (double)__bswap_64(OrderNumber);
    SwapDouble((char*) &OrderNumber);
    SwapDouble((char*) &NnfField);
    //OrderNumber			= (double)__bswap_64(llOrderNo);
    LastModified = __bswap_32(LastModified);
    //SwapDouble((char*)&filler);
    //filler				= __bswap_32(filler);
    //SwapDouble((char*)&Timestamp1);
  }
};

struct NSEFO_MS_ORDER_BACKUP_TR //This is not the Exchange defined struct, it is our own custom defined struct
{
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
  NSEFO_ST_ORDER_FLAGS OrderFlags;
};

struct NSEFO_MS_ORDER_DETAILS_TR //This is not the Exchange defined struct, it is our own custom defined struct
{
  NSEFO_MS_OM_REQUEST_TR ms_om_request_tr;
  NSEFO_MS_ORDER_BACKUP_TR ms_order_backup_tr;
  uint64_t  ullTSAfterSockSend;
  int16_t sStatus;
  //int16_t sOrderType;
  int32_t iOtherOrderIndex;
  int16_t sLastErrCode;
  //int64_t llCliMsgSeqNo;    //Echo Field, 
  //int32_t iStrategyOrdId;   //Echo Field,
  //uint32_t uiUserCode;      //Echo Field,  
  uint32_t IncrementalHash[4];
  //ORDER_REQUEST_INTERNAL oReq;

  NSEFO_MS_ORDER_DETAILS_TR()
  {
    ullTSAfterSockSend = 0;
    sStatus = 0;
    sLastErrCode = 0;
    //sOrderType = 0;
    iOtherOrderIndex = 0;
    //llCliMsgSeqNo = 0;  
    //iStrategyOrdId = 0;
    //uiUserCode = 0;    
  }

};

struct NSEFO_MS_ORDER_SUBSET_TR //This is not the Exchange defined struct, it is our own custom defined struct
{
  //	double OrderNumber;
  //	char AccountNumber[10];
  //	int16_t BookType;
  //	int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t TotalVolumeRemaining;
  int32_t Volume;
  int32_t VolumeFilledToday;
  int32_t Price;
  int32_t GoodTillDate;
  int32_t EntryDateTime;
  int32_t LastModified;
  NSEFO_ST_ORDER_FLAGS OrderFlags;
  //	int16_t BranchId;
  //	int32_t TraderId;
  //	char BrokerId[5];
  //	char OpenClose;
  //	char Settlor[12];
  //	int16_t ProClientIndicator;
  //	ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  //	int32_t filler;
  //	double NnfField;
};

//TRADE_CONFIRMATION_TR(20222)

struct NSEFO_TRADE_CONFIRMATION_TR
{
  //TAP_HEADER tap_hdr;	
  int16_t TransactionCode;
  int32_t LogTime;
  int32_t TraderId;
  int64_t Timestamp;
//  double Timestamp;
  int64_t Timestamp1;
//  double Timestamp2;  // Due to error in ML Testing
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
  NSEFO_ST_ORDER_FLAGS OrderFlags;
  int32_t GoodTillDate;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  int32_t Token;
  NSEFO_CONTRACT_DESC_TR contract_desc_tr;
  char OpenClose;
  char BookType;
  char Participant[12];
  NSEFO_ADDITIONAL_ORDER_FLAGS AddtnlOrderFlags1;
  char PAN[10];
  int32_t AlgoId;
  int16_t AlgoCategory;
  char Reserved3[60];
  inline void Initialize(char* pchData)
  {
    memcpy(&TransactionCode, pchData, sizeof(NSEFO_TRADE_CONFIRMATION_TR));
    TransactionCode = __bswap_16(TransactionCode);
    SwapDouble((char*) &ResponseOrderNumber);
    //ResponseOrderNumber       = __bswap_64(int64_t(ResponseOrderNumber));
    //		BuySellIndicator          = __bswap_16(BuySellIndicator);
    //		OriginalVolume            = __bswap_32(OriginalVolume);
    //		DisclosedVolume           = __bswap_32(DisclosedVolume);
    //		RemainingVolume           = __bswap_32(RemainingVolume);
    //		DisclosedVolumeRemaining  = __bswap_32(DisclosedVolumeRemaining);
    //		Price                     = __bswap_32(Price);
    FillNumber = __bswap_32(FillNumber);
    FillQuantity = __bswap_32(FillQuantity);
    FillPrice = __bswap_32(FillPrice);
    VolumeFilledToday = __bswap_32(VolumeFilledToday);
    ActivityTime = __bswap_32(ActivityTime);
    OriginalVolume = __bswap_32(OriginalVolume);
    RemainingVolume = __bswap_32(RemainingVolume);
    Token = __bswap_32(Token);
  }
};

struct NSEFO_INSTRUMENT_USER
{
  double dBranchBuyValueLimit;
  double dBranchSellValueLimit;
  double dBranchUsedBuyValueLimit;
  double dBranchUsedSellValueLimit;
  double dUserOrderBuyValueLimit;
  double dUserOrderSellValueLimit;
  double dUserOrderUsedBuyValueLimit;
  double dUserOrderUsedSellValueLimit;
};

struct NSEFO_MS_USER_ORDER_VAL_LIMIT_DATA
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int16_t sBranchId;
  char UserName[25];
  int32_t iUserId;
  int16_t sUserType;
  NSEFO_INSTRUMENT_USER InstrumentUser[2]; //0:Futures 1:Options

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(NSEFO_MS_USER_ORDER_VAL_LIMIT_DATA));

    sBranchId = __bswap_16(sBranchId);
    iUserId = __bswap_32(iUserId);
    sUserType = __bswap_16(sUserType);

    SwapDouble((char*) &InstrumentUser[0].dBranchBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dBranchSellValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dBranchUsedBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dBranchUsedSellValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dUserOrderBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dUserOrderSellValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dUserOrderUsedBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[0].dUserOrderUsedSellValueLimit);

    SwapDouble((char*) &InstrumentUser[1].dBranchBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dBranchSellValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dBranchUsedBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dBranchUsedSellValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dUserOrderBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dUserOrderSellValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dUserOrderUsedBuyValueLimit);
    SwapDouble((char*) &InstrumentUser[1].dUserOrderUsedSellValueLimit);
  }
};

struct NSEFO_DEALER_ORD_LMT
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int32_t iUserId;
  double dOrdQtyBuff;
  double dOrdValBuff;

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(NSEFO_DEALER_ORD_LMT));

    iUserId = __bswap_32(iUserId);
    SwapDouble((char*) &dOrdQtyBuff);
    SwapDouble((char*) &dOrdValBuff);
  }
};

struct NSEFO_SPD_ORD_LMT
{
  NSEFO_TAP_HEADER tap_hdr;
  NSEFO_MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int32_t iUserId;
  double SpdOrdQtyBuff;
  double SpdOrdValBuff;

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(NSEFO_SPD_ORD_LMT));

    iUserId = __bswap_32(iUserId);
    SwapDouble((char*) &SpdOrdQtyBuff);
    SwapDouble((char*) &SpdOrdValBuff);
  }
};

#pragma pack()

typedef NSEFO_MS_OE_REQUEST_TR NSE_FO_NNF_NEW_TR; //BOARD_LOT_IN(20000)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_NEW_ACCEPT_TR; //ORDER_CONFIRMATION_OUT(20073)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_NEW_REJECT_TR; //ORDER_ERROR_OUT(20231)

typedef NSEFO_MS_OM_REQUEST_TR NSE_FO_NNF_MOD_TR; //ORDER_MOD_IN(20040)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_MOD_ACCEPT_TR; //ORDER_MOD_CONFIRM_OUT(20074)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_MOD_REJECT_TR; //ORDER_MOD_REJ_OUT(20042)

typedef NSEFO_MS_OM_REQUEST_TR NSE_FO_NNF_CXL_TR; //ORDER_CANCEL_IN(20070)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_CXL_ACCEPT_TR; //ORDER_CANCEL_CONFIRM_OUT(20075)
typedef NSEFO_MS_OE_RESPONSE_TR NSE_FO_NNF_CXL_REJECT_TR; //ORDER_CXL_REJ_OUT(20072)

const int16_t sLen_NSEFO_LB_Query_Req = sizeof(NSEFO_LB_QUERY_REQ);
const int16_t sLen_NSEFO_Exch_HB_Req = sizeof(NSEFO_MS_HEART_BEAT_REQ);

const int16_t sLen_NSEFO_Logon_Req = sizeof(NSEFO_MS_SIGNON_REQ);
const int16_t sLen_NSEFO_SysInfo_Req = sizeof(NSEFO_MS_SYSTEM_INFO_REQ);
const int16_t sLen_NSEFO_UpdLDB_Req = sizeof(NSEFO_MS_UPDATE_LOCAL_DATABASE);
const int16_t sLen_NSEFO_UpdPortfolio_Req = sizeof(NSEFO_EXCH_PORTFOLIO_REQ);
const int16_t sLen_NSEFO_MsgDownload_Req = sizeof(NSEFO_MS_MESSAGE_DOWNLOAD_REQ);


const int16_t sLen_NSEFO_AddModCan_Req_NonTrim = sizeof(NSEFO_MS_OE_REQUEST);
const int16_t sLen_NSEFO_Add_Req = sizeof(NSEFO_MS_OE_REQUEST_TR);
const int16_t sLen_NSEFO_Mod_Can_Req = sizeof(NSEFO_MS_OM_REQUEST_TR);
const int16_t sLen_NSEFO_Tap_Hdr = sizeof(NSEFO_TAP_HEADER);
const int16_t sLen_NSEFO_Msg_Hdr = sizeof(NSEFO_MESSAGE_HEADER);
const int16_t sLen_NSEFO_Full_Hdr = sLen_NSEFO_Tap_Hdr + sLen_NSEFO_Msg_Hdr;
const int16_t sLen_NSEFO_Cust_Hdr = sizeof(NSEFO_CUSTOM_HEADER);
const int16_t sLen_NSEFO_Add_Req_NonTrim = sizeof(NSEFO_MS_OE_REQUEST);

const int16_t sLen_NSEFO_SpdML_Req = sizeof(NSEFO_MS_SPD_OE_REQUEST);

const int32_t sLen_NSEFO_IncrementalHash = 16;

#endif