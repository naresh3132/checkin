#ifndef NSE_CM_NNF_ORD_H_
#define NSE_CM_NNF_ORD_H_

#include<string>
#include<stdint.h>
#include<byteswap.h>
#include "common_structs.h"
#include "nsecm_constants.h"



//inline void SwapDouble(char* pData)
//{
//  char TempData = 0;
//  for(int32_t nStart = 0, nEnd = 7; nStart < nEnd; nStart++, nEnd--)
//  {
//    TempData = pData[nStart];
//    pData[nStart] = pData[nEnd];
//    pData[nEnd] = TempData;
//  }
//}

#pragma pack(2)

struct Order_Details
{
  int32_t iInterOrderNo;
  int16_t sOrderType;
};

struct FullMsgDnld_Details
{
  uint64_t NnfField;
  char chSettlor[12];
  char chAccountNumber[10];
};

struct DirectConnectIPPort
{
  int32_t iRecNo;
  char chIP[16];
  int32_t iPort;
  
  DirectConnectIPPort()
  {
    iRecNo = 0;
    memset(chIP, 0, 16);
    iPort = 0;
  }
};

struct INVITATION_MESSAGE // Trans code 15000
{
  int16_t TransactionCode;
  int16_t InvitationCnt;
};

struct TAP_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  //int16_t sResrvSeqNo;
  //char chCheckSum[16];
  uint32_t CheckSum[4];
  //int16_t sMsgCnt;
};

// This is our own custom defined header [NOT EXCHANGE DEFINED], to optimize receive side processing

struct CUSTOM_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  //int16_t sResrvSeqNo;
  //char chCheckSum[16];
  uint32_t CheckSum[4];
  //int16_t sMsgCnt;
  int16_t sTransCode;
};

struct MESSAGE_HEADER
{
  int16_t TransactionCode;
  int32_t LogTime;
  char AlphaChar[2];
  int32_t TraderId;
  int16_t ErrorCode;
  char Timestamp[8];
  int64_t Timestamp1; //It contains Jiffy(int64) but NNF Doc defines it as char[8], so we have changed to to int64, to avoid type casting	
  char TimeStamp2[8];
  int16_t MessageLength;
};

//struct INNER_MESSAGE_HEADER
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

struct INNER_MESSAGE_HEADER
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

struct BCAST_HEADER // Struct named as BROADCAST_HEADER in doc
{
  char Reserved1[4];
  char Reserved2[4];
  int32_t LogTime;
  char AlphaChar[2];
  int16_t TransCode;
  int16_t ErrorCode;
  int32_t BCSeqNo;
  char TimeStamp2[8];
  char Filler2[8];
  int16_t MessageLength;
};

struct SEC_INFO
{
	char Symbol[10];
	char Series[2];
};

struct MS_ERROR_RESPONSE
{
	MESSAGE_HEADER _message_header;
	SEC_INFO _sec_info;
	char ErrorMessage[128];
};


struct ST_BROKER_ELIGIBILITY_PER_MKT //Broker Eligibity only defined once, it has several instances on the doc file
{
  unsigned char NormalMarket : 1;
  unsigned char OddlotMarket : 1;
  unsigned char SpotMarket : 1;
  unsigned char AuctionMarket : 1;
  unsigned char CallAuction1 : 1;
 	unsigned char CallAuction2 : 1;
	unsigned char Reserved1 : 2;
	unsigned char Reserved2 : 7;
	unsigned char Preopen : 1;
};
 

struct ST_MARKET_STATUS
{
  int16_t Normal;
  int16_t Oddlot;
  int16_t Spot;
  int16_t Auction;
};

/*
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
  int32_t StrikePrice;
  char OptionType[2];
  int16_t CALevel;

  CONTRACT_DESC()
  {
    memset(this, ' ', sizeof(CONTRACT_DESC));
    CALevel = 0;
  }
};
*/
//struct ST_ORDER_FLAGS
//{
//  unsigned char ATO : 1;
//  unsigned char Market : 1;
//  unsigned char SL : 1;     //It is mentioned as OnStop in NNF
//  unsigned char Day : 1;
//  unsigned char GTC : 1;
//  unsigned char IOC : 1;    //It is mentioned as FOK in NNF (FillOrKill) 
//  unsigned char AON : 1;
//  unsigned char MF : 1;
//  unsigned char MatchedInd : 1;
//  unsigned char Traded : 1;
//  unsigned char Modified : 1;
//  unsigned char Frozen : 1;
//  unsigned char Preopen : 1;
//  unsigned char Reserved : 3;
//};

struct ST_ORDER_FLAGS
{
  unsigned char MF : 1;
  unsigned char AON : 1;  
  unsigned char IOC : 1;    //It is mentioned as FOK in NNF (FillOrKill)   
  unsigned char GTC : 1;  
  unsigned char Day : 1;
  unsigned char SL : 1;     //It is mentioned as OnStop in NNF
  unsigned char Market : 1;
  unsigned char ATO : 1;
  unsigned char Reserved : 3;  
  unsigned char Preopen : 1;  
  unsigned char Frozen : 1;
  unsigned char Modified : 1;  
  unsigned char Traded : 1;
  unsigned char MatchedInd : 1;
};


/*
struct ADDITIONAL_ORDER_FLAGS
{
  unsigned char Reserved1 : 1;
  unsigned char COL : 1;
  unsigned char Reserved2 : 6;
};

struct CONTRACT_DESC_TR
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  int32_t StrikePrice;
  char OptionType[2];

  CONTRACT_DESC_TR()
  {
    memset(this, ' ', sizeof(CONTRACT_DESC_TR));
  }
};
*/

struct ST_STOCK_ELEGIBLE_INDICATORS // Renamed as SECURITY_ELIGIBLE_INDICATORS in doc file
{
  unsigned char Reserved1 : 5;
  unsigned char BooksMerged : 1;
  unsigned char MinFill : 1;
  unsigned char AON : 1;
  char Reserved2;
};

struct LB_QUERY_REQ
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;  
  char BrokerId[5];
  char Filler;
  int16_t Reserved;
};

struct LB_QUERY_RESP
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;  
  char IPAddress[16];
  char Port[6];
  char SessionKey[32];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(LB_QUERY_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
  
};


struct MS_HEART_BEAT_REQ
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
};

/*
struct MS_ERROR_RESPONSE
{
  MESSAGE_HEADER header;
  char Key[14];
  char ErrorMessage[128];
};
*/

//NEW LOGIN CHANGES STARTS
struct MS_GR_REQUEST //gateway router Request
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
};

struct MS_GR_RESPONSE
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerID[5];
  char cFiller;
  char cIPAddress[16];
  int32_t iPort;
  char cSessionKey[8];
};

struct MS_BOX_SIGN_ON_REQUEST_IN
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cBrokerId[5];
  char cReserved[5]; 
  char cSessionKey[8];
};

struct MS_BOX_SIGN_ON_REQUEST_OUT
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t wBoxId;
  char cReserved[5];
};

//NEW LOGIN CHANGES ENDS

struct MS_SIGNON_REQ //SIGN_ON_REQUEST_IN(2300)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t UserId;
  char Passwd[8];
  char NewPassword[8];
  char TraderName[26];
  int32_t LastPasswdChangeDate;
  char BrokerId[5];
  char Reserved1;
  int16_t BranchId;
  int32_t VersionNumber;
  char Reserved2[56]; 
  int16_t UserType;
  double SequenceNumber;
  char WsClassName[14];
  char BrokerStatus;
  char ShowIndex;
  ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
  char BrokerName[26];
};

struct MS_SIGNON_RESP // SIGN_ON_REQUEST_OUT(2301)
{
  MESSAGE_HEADER msg_hdr;
  int32_t UserId;
  char Passwd[8];
  char NewPassword[8];
  char TraderName[26];
  int32_t LastPasswdChangeDate;
  char BrokerId[5];
  char Reserved1;
  int16_t BranchId;
  int32_t VersionNumber;
  int32_t EndTime;
  char Reserved2[52];
  int16_t UserType;
  double SequenceNumber;
  char Reserved4[14];
  char BrokerStatus;
  char Reserved3;
  ST_BROKER_ELIGIBILITY_PER_MKT st_broker_eligibility_per_mkt;
  char BrokerName[26];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(MS_SIGNON_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }

};

struct MS_SYSTEM_INFO_REQ //SYSTEM_INFORMATION_IN(1600)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
};

struct MS_SYSTEM_INFO_DATA //SYSTEM_INFORMATION_OUT(1601)
{
  MESSAGE_HEADER msg_hdr;
	int16_t Normal;
	int16_t Oddlot;
	int16_t Spot;
	int16_t Auction;
	int16_t CallAuction1;
	int16_t CallAuction2;
  int32_t MarketIndex;
  int16_t DefSettPeriodNormal;
  int16_t DefSettPeriodSpot;
  int16_t DefSettPeriodAuction;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  int16_t WarningPercent;
  int16_t VolumeFreezePercent;
  char Reserved1[2];
	int16_t TerminalIdleTime;
  int32_t BoardLotQty;
  int32_t TickSize;
  int16_t MaximumGtcDays;
  ST_STOCK_ELEGIBLE_INDICATORS st_stock_eligible_indicators; //Renamed as SECURITY ELIGIBLE INDICATORS in the doc file
  int16_t DiscQtyPerAllwd;
	char Reserved[6];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(MS_SYSTEM_INFO_DATA));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    DiscQtyPerAllwd = __bswap_16(DiscQtyPerAllwd);
    DefSettPeriodNormal = __bswap_16(DefSettPeriodNormal);
  }

};

struct MS_UPDATE_LOCAL_DATABASE //UPDATE_LOCALDB_IN(7300), Missing a couple of market status, cant find the struct for it
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int32_t LastUpdateSecTime;
  int32_t LastUpdateParticipantTime;
  char ReqForOpenOrders;
  char Reserved1;
	int16_t NormalMarketStatus;
	int16_t OddLotMarketStatus;
	int16_t SpotMarketStatus;
	int16_t AuctionMarketStatus;
	int16_t CallAuction1MarketStatus;
	int16_t CallAuction2MarketStatus;
};

struct MS_PARTIAL_SYS_INFO //PARTIAL_SYSTEM_INFO(7321)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  ST_MARKET_STATUS st_market_status;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr, pchData, sizeof(MS_PARTIAL_SYS_INFO));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);

    st_market_status.Normal = __bswap_16(st_market_status.Normal);
    st_market_status.Oddlot = __bswap_16(st_market_status.Oddlot);
    st_market_status.Spot = __bswap_16(st_market_status.Spot);
    st_market_status.Auction = __bswap_16(st_market_status.Auction);
  }
};

struct UPDATE_LDB_HEADER //UPDATE_LOCALDB_HEADER(7307)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char Reserved1[2];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(UPDATE_LDB_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }  
};

struct UPDATE_LDB_DATA //UPDATE_LOCALDB_DATA(7304)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  INNER_MESSAGE_HEADER inheader;
  char Data[436];
  
 inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(UPDATE_LDB_DATA));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    
    inheader.TransactionCode = __bswap_16(inheader.TransactionCode);
  }  
};

struct UPDATE_LDB_TRAILER   //UPDATE_LOCALDB_TRAILER(7308)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char Reserved1[2];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(UPDATE_LDB_TRAILER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }   
};

struct MS_INDUSTRY_INDEX_DLOAD_REQ   // Trans code 1110
{
  TAP_HEADER tap_hdr;
	MESSAGE_HEADER msg_hdr;
};

struct MS_INDUSTRY_INDEX_DLOAD_DATA
{
	int16_t IndustryCode;
	char IndexName[21];
	int32_t IndexValue;

};

struct MS_INDUSTRY_INDEX_DLOAD_RESP // Trans code 1111
{
  MESSAGE_HEADER msg_hdr;
	int16_t NumberOfRecords;
	MS_INDUSTRY_INDEX_DLOAD_DATA Index_Dload_Data[16];
  
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr, pchData, sizeof(MS_INDUSTRY_INDEX_DLOAD_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    
    NumberOfRecords = __bswap_16(NumberOfRecords);
    
    for(int iTemp = 0; iTemp < NumberOfRecords; iTemp++)
    {
      Index_Dload_Data[iTemp].IndustryCode = __bswap_16(Index_Dload_Data[iTemp].IndustryCode);
      Index_Dload_Data[iTemp].IndexValue = __bswap_32(Index_Dload_Data[iTemp].IndexValue);
    }
  }  
};

/*
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


struct MS_DOWNLOAD_INDEX_MAP //BCAST_INDEX_MAP_TABLE(7326)
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
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  int16_t NoOfRec;
  char MoreRecs;
  char Filler;
  PORTFOLIO_DATA portfolio_data[15];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(EXCH_PORTFOLIO_RESP));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
};
*/
struct MS_MESSAGE_DOWNLOAD_REQ //DOWNLOAD_REQUEST(7000)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  double SeqNo;
};

struct MS_MESSAGE_DOWNLOAD_HEADER //TRAILER_RECORD(7011)
{
  MESSAGE_HEADER msg_hdr;
	
  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(MS_MESSAGE_DOWNLOAD_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }
	
};

struct MS_MESSAGE_DOWNLOAD_DATA //DOWNLOAD_DATA(7021)
{
  MESSAGE_HEADER msg_hdr;
  INNER_MESSAGE_HEADER inner_hdr;
  char Data[436];

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(MS_MESSAGE_DOWNLOAD_DATA));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    inner_hdr.TransactionCode = __bswap_16(inner_hdr.TransactionCode);
  }

};

struct MS_MESSAGE_DOWNLOAD_TRAILER //TRAILER_RECORD(7031)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(MS_MESSAGE_DOWNLOAD_TRAILER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
  }

};

struct SIGN_OFF_REQ //SIGN_OFF_REQUEST_OUT(2320)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
};

struct SIGN_OFF_OUT //SIGN_OFF_REQUEST_OUT(2321)
{
  MESSAGE_HEADER msg_hdr;
};

/**************************************************************/

typedef struct _MS_OE_REQUEST //BOARD_LOT_IN(2000)
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  char filler;
	int16_t ReasonCode;
	char reserved2[4];
	SEC_INFO sec_info;
  int16_t AuctionNumber;
  char CounterPartyBrokerId[5];
	char Suspended;
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
	char OeRemarks[25];
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  double NnfField;
	double ExecTimeStamp;
	//char reserved3[4];
  int32_t TransactionId;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(_MS_OE_REQUEST) - sizeof(TAP_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    SwapDouble((char*) &OrderNumber);
    LastModified = __bswap_32(LastModified);
    EntryDateTime = __bswap_32(EntryDateTime);
    ReasonCode = __bswap_16(ReasonCode);
    BookType = __bswap_16(BookType);
    Volume = __bswap_32(Volume);
    TraderId = __bswap_32(TraderId);
    SwapDouble((char*) &NnfField);
  }

  _MS_OE_REQUEST()
  {
  }

  _MS_OE_REQUEST(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(_MS_OE_REQUEST));
    tap_hdr.sLength = __bswap_16(sizeof(_MS_OE_REQUEST));
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED as mentioned in NNF Doc
    msg_hdr.LogTime = 0;
    msg_hdr.TraderId = __bswap_32(_UserID);
    msg_hdr.ErrorCode = 0;
    memset(msg_hdr.Timestamp, 0, 8);
    msg_hdr.Timestamp1 = 0;

    memset(msg_hdr.TimeStamp2, 0, 8);
    msg_hdr.MessageLength = __bswap_16(sizeof(_MS_OE_REQUEST) - sizeof(TAP_HEADER));

    msg_hdr.TransactionCode = __bswap_16(NSECM_ADD_REQ);
    msg_hdr.TraderId = __bswap_32(_UserID);

    ParticipantType = 'S';
    //reserved1;
    CompetitorPeriod = 0;
    SolicitorPeriod = 0;
    //ModifiedCancelledBy;
    //filler;
    ReasonCode = 0;
    //reserved2;
    //sec_info;
    AuctionNumber = 0;
    //CounterPartyBrokerId[5];
    //Suspended;
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
    memset(&st_order_flags, 0, sizeof(st_order_flags));
    st_order_flags.Day = 1;
    BranchId = __bswap_16(_BranchId);
    TraderId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    //OeRemarks[24];
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro	
    SettlementPeriod = __bswap_16(7);
    //NnfField;
    ExecTimeStamp = 0;
    //reserved3[4];    
  }

} _MS_OE_REQUEST;

struct MS_ORDER_BACKUP
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
  ST_ORDER_FLAGS st_order_flags;
};


//This is a custom structure, to facilitate optimized response processing

typedef struct _MS_OE_RESPONSE //BOARD_LOT_IN(2000)
{
  //TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char ParticipantType;
  char reserved1;
  int16_t CompetitorPeriod;
  int16_t SolicitorPeriod;
  char ModifiedCancelledBy;
  char rfiller;
	int16_t ReasonCode;
	char reserved2[4];
	SEC_INFO sec_info;
  int16_t AuctionNumber;
  char CounterPartyBrokerId[5];
	char Suspended;
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
	char OeRemarks[25];
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  double NnfField;
	double ExecTimeStamp;
	//char reserved3[4];
  int32_t TransactionId;

  inline void Initialize(char* pchData)
  {
    memcpy(&msg_hdr.TransactionCode, pchData, sizeof(_MS_OE_REQUEST) - sizeof(TAP_HEADER));

    msg_hdr.TransactionCode = __bswap_16(msg_hdr.TransactionCode);
    msg_hdr.ErrorCode = __bswap_16(msg_hdr.ErrorCode);
    SwapDouble((char*) &OrderNumber);
    LastModified = __bswap_32(LastModified);
    EntryDateTime = __bswap_32(EntryDateTime);
    ReasonCode = __bswap_16(ReasonCode);
    BookType = __bswap_16(BookType);
    Volume = __bswap_32(Volume);
    TraderId = __bswap_32(TraderId);
    SwapDouble((char*) &NnfField);
  }
} MS_OE_RESPONSE;

typedef MS_OE_REQUEST NSE_CM_NNF_NEW; //BOARD_LOT_IN(2000)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_NEW_ACCEPT; //ORDER_CONFIRMATION_OUT(2073)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_NEW_REJECT; //ORDER_ERROR_OUT(2231)

typedef NSE_CM_NNF_NEW NSE_CM_NNF_MOD; //ORDER_MOD_IN(2040)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_MOD_ACCEPT; //ORDER_MOD_CONFIRM_OUT(2074)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_MOD_REJECT; //ORDER_MOD_REJ_OUT(2042)

typedef NSE_CM_NNF_NEW NSE_CM_NNF_CXL; //ORDER_CANCEL_IN(2070)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_CXL_ACCEPT; //ORDER_CANCEL_CONFIRM_OUT(2075)
typedef NSE_CM_NNF_NEW NSE_CM_NNF_CXL_REJECT; //ORDER_CXL_REJ_OUT(2072)

typedef NSE_CM_NNF_NEW NSE_CM_NNF_KILL_ALL; //KILL_SWITCH_IN(2063)

struct MS_ORDER_DETAILS //This is not the Exchange defined struct, it is our own custom defined struct
{
  MS_OE_REQUEST ms_oe_request;
  MS_ORDER_BACKUP ms_order_backup;
  int16_t sStatus;
};

struct MS_TRADE_CONFIRM
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
  SEC_INFO sec_info;
  char Reserved2;
	int16_t BookType;
  int32_t NewVolume;
	int16_t ProClient;
};

typedef MS_TRADE_CONFIRM NSE_CM_NNF_TRADE; //TRADE_CONFIRM(2222)

/**************************************************************/
/*
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
*/
/*
struct MS_TRADE_INQUIRY_DATA
{
  MESSAGE_HEADER msg_hdr;
  SEC_INFO sec_info;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int16_t MktType;
  int32_t NewVolume;
  char BuyParticipant[12];
  char BuyBrokerId[5];
  char SellParticipant[12];
  char SellBrokerId[5];
  int32_t TraderId;
  int16_t ReqBy;
};
*/
// NEWLY ADDED ON 10 MARCH 2015.

//BOARD_LOT_IN_TR(20000)

struct MS_OE_REQUEST_TR
{
  TAP_HEADER tap_hdr;  
  int16_t TransactionCode;
  int32_t TraderId;
  SEC_INFO sec_info; 
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t Volume;
  int32_t Price;
  int32_t GoodTillDate;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
  int32_t UserId;
  char BrokerId[5];
  char Suspended;
  char Settlor[12];
  int16_t ProClientIndicator;
  double NnfField;
	int32_t TransactionId;

  MS_OE_REQUEST_TR()
  {
  }

  MS_OE_REQUEST_TR(int32_t _UserID, std::string _TMID, int16_t _BranchId, bool _COL)
  {
    memset(&tap_hdr.sLength, ' ', sizeof(MS_OE_REQUEST_TR));

    TransactionCode = __bswap_16(NSECM_ADD_REQ_TR);
    TraderId = __bswap_32(_UserID);
    //sec_info;
    //AccountNumber;
    BookType = __bswap_16(1);
    //BuySellIndicator;
    //DisclosedVolume;
    //Volume;
    //Price;    
    GoodTillDate = 0;
    
    OrderFlags.ATO = 0;
    OrderFlags.Market = 0;
    OrderFlags.SL = 0;     //It is mentioned as OnStop in NNF
    OrderFlags.Day = 1;
    OrderFlags.GTC = 0;
    OrderFlags.IOC = 0;    //It is mentioned as FOK in NNF (FillOrKill) 
    OrderFlags.AON = 0;
    OrderFlags.MF = 0;
    OrderFlags.MatchedInd = 0;
    OrderFlags.Traded = 0;
    OrderFlags.Modified = 0;
    OrderFlags.Frozen = 0;
    OrderFlags.Preopen = 0;
    OrderFlags.Reserved = 0;    

//    memset(&OrderFlags, 0, sizeof(OrderFlags));
//    OrderFlags.Day = 1;
    BranchId = __bswap_16(_BranchId);
    UserId = __bswap_32(_UserID);
    memcpy(BrokerId, _TMID.c_str(), _TMID.length());
    //Suspended;    
    memcpy(Settlor, _TMID.c_str(), _TMID.length());
    ProClientIndicator = __bswap_16(1); //1:Client 2:Pro
    //NnfField;
    //TransactionId;

    tap_hdr.sLength = __bswap_16(sizeof(MS_OE_REQUEST_TR));
    //tap_hdr.sResrvSeqNo = 0;
    //tap_hdr.sMsgCnt = __bswap_16(1); //HARDCODED, as specified in NNF doc	
  }

};

/*
struct MS_TRADER_INT_MSG //Multiple Definitions exist in doc
{
	char Reserved;
	int16_t MsgLength;
	char Msg[239];

};

struct MS_TRADER_INT_MSG //Multiple Definitions exist in doc
{
	MESSAGE_HEADER msg_hdr;
	int16_t BranchNumber;
	char BrokerNumber[5];
	char ActionCode[3];
	char Reserved[4];
	BROADCAST_DESTINATION bdr_dst;
	int16_t MsgLength;
	char Msg[239];
};

struct BROADCAST_DESTINATION
{
	unsigned char TraderWs : 1;
	unsigned char Reserved : 7;
	char Reserved;
} 


struct MS_RPR_HDR
{
	MESSAGE_HEADER msg_hdr;
	char MsgType;
	char Reserved;
	int32_t ReportDate;
	int16_t UserType;
	char BrokerId[5];
	char BrokerName[25];
	int16_t TraderNumber;
	char TraderName[26];

};
*/
/*
struct MARKET_STATS_REPORT_DATA
{
	char Reserved;
	int16_t NumberOfRecords;

};
*/

//ORDER_QUICK_CANCEL_IN_TR(20060)
//ORDER_CANCEL_IN_TR(20070)
//ORDER_MOD_IN_TR(20040)

struct MS_OM_REQUEST_TR
{
  TAP_HEADER tap_hdr;  
  int16_t TransactionCode;
	int32_t LogTime;
	int32_t TraderId;
	int16_t ErrorCode;
	double TimeStamp1;
  char TimeStamp2;
	char ModCanBy;
	int16_t ReasonCode;
	SEC_INFO sec_info;
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
  int32_t EntryDateTime;
	int32_t LastModified;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
	int32_t UserId;  
  char BrokerId[5];
  char Suspended;
  char Settlor[12];
  int16_t ProClientIndicator;
  int16_t SettlementPeriod;
  double NnfField;  
  int32_t TransactionId;

  //	void PopulateModFromNewReq(MS_OE_REQUEST_TR& oNewOrdReq)
  //	{
  //		tap_hdr				= oNewOrdReq.tap_hdr;
  //		TransactionCode	= __bswap_16(NSECM_MOD_REQ_TR);
  //	}

};

//PRICE_CONFIRMATION_TR(20012)
//ORDER_ERROR_TR(20231)
//ORDER_CXL_CONFIRMATION_TR(20075)
//ORDER_MOD_CONFIRMATION_TR(20074)
//ORDER_CONFIRMATION_TR(20073)
//ORDER_CANCEL_REJECT_TR(20072)
//ORDER_MOD_REJECT_TR(20042)

struct MS_OE_RESPONSE_TR
{
  //TAP_HEADER tap_hdr;	
  int16_t TransactionCode;
	int32_t LogTime;
	int32_t TraderId;
	int16_t ErrorCode;
  //double TimeStamp1;
  int64_t TimeStamp1; //It contains Jiffy(int64) but NNF Doc defines it as double, so we have changed to to int64, to avoid type casting
  char TimeStamp2;
	char Modify;
	int16_t ReasonCode;
	SEC_INFO sec_info;
	double OrderNumber;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
	int32_t DisclosedVolumeRemain;
	int32_t TotalVolumeRemain;
	int32_t Volume;
  int32_t VolumeFilledToday;
	int32_t Price;
  int32_t EntryDateTime;
	int32_t LastModified;
  ST_ORDER_FLAGS OrderFlags;
  int16_t BranchId;
	int32_t UserId;  
  char BrokerId[5];
  char Suspended;
  char Settlor[12];
  int16_t ProClient;
  int16_t SettlementPeriod;
  double NnfField;  
  int32_t TransactionId;

  inline void Initialize(char* pchData)
  {
    memcpy(&TransactionCode, pchData, sizeof(MS_OE_RESPONSE_TR));
    TransactionCode = __bswap_16(TransactionCode);
    ErrorCode = __bswap_16(ErrorCode);

    SwapDouble((char*) &OrderNumber);
    SwapDouble((char*) &NnfField);
    EntryDateTime = __bswap_32(EntryDateTime);
    LastModified = __bswap_32(LastModified);
  }
};

struct MS_ORDER_BACKUP_TR //This is not the Exchange defined struct, it is our own custom defined struct
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
  ST_ORDER_FLAGS OrderFlags;
};

struct MS_ORDER_DETAILS_TR //This is not the Exchange defined struct, it is our own custom defined struct
{
  MS_OM_REQUEST_TR ms_om_request_tr;
  MS_ORDER_BACKUP_TR ms_order_backup_tr;
  uint64_t  ullTSAfterSockSend;  
  int16_t sStatus;
  int16_t sOrderType;
  int32_t iOtherOrderIndex;
  int32_t iToken;
  int64_t llReqOutTime;
  int64_t llCliMsgSeqNo;    //Echo Field, 
  int32_t iStrategyOrdId;   //Echo Field,
  uint32_t uiUserCode;      //Echo Field,
  int16_t iExchBookType;

  MS_ORDER_DETAILS_TR()
  {
    ullTSAfterSockSend = 0;
    sStatus = 0;
    sOrderType = 0;
    iOtherOrderIndex = 0;
    iToken = 0;    
    llReqOutTime = 0;
    llCliMsgSeqNo = 0;  
    iStrategyOrdId = 0;
    uiUserCode = 0;
    iExchBookType = 0;
  }

};


struct MS_ORDER_SUBSET_TR //This is not the Exchange defined struct, it is our own custom defined struct
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
  ST_ORDER_FLAGS OrderFlags;
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

struct TRADE_CONFIRMATION_TR  // Trans code 20222
{
  int16_t TransactionCode;
  int32_t LogTime;
  int32_t UserId;
  double Timestamp;
  double Timestamp1;
  double ResponseOrderNumber;
  char   Timestamp2;
  char BrokerId[5];
	int32_t TraderNum;
  int16_t BuySellIndicator;
  char AccountNumber[10];
  int32_t OriginalVolume;
  int32_t DisclosedVolume;
  int32_t RemainingVolume;
  int32_t DisclosedVolumeRemaining;
  int32_t Price;
  ST_ORDER_FLAGS OrderFlags;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  SEC_INFO sec_info;
  int16_t BookType;
	int16_t ProClient;

  inline void Initialize(char* pchData)
  {
    memcpy(&TransactionCode, pchData, sizeof(TRADE_CONFIRMATION_TR));
    TransactionCode = __bswap_16(TransactionCode);
    SwapDouble((char*) &ResponseOrderNumber);
    //ResponseOrderNumber       = __bswap_64(int32_t(ResponseOrderNumber));
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
  }
	
};

struct INSTRUMENT_USER
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

/*
struct MS_USER_ORDER_VAL_LIMIT_DATA
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int16_t sBranchId;
  char UserName[25];
  int32_t iUserId;
  int16_t sUserType;
  INSTRUMENT_USER InstrumentUser[2]; //0:Futures 1:Options

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(MS_USER_ORDER_VAL_LIMIT_DATA));

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

struct DEALER_ORD_LMT
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int32_t iUserId;
  double dOrdQtyBuff;
  double dOrdValBuff;

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(DEALER_ORD_LMT));

    iUserId = __bswap_32(iUserId);
    SwapDouble((char*) &dOrdQtyBuff);
    SwapDouble((char*) &dOrdValBuff);
  }
};

struct SPD_ORD_LMT
{
  TAP_HEADER tap_hdr;
  MESSAGE_HEADER msg_hdr;
  char BrokerId[5];
  int32_t iUserId;
  double SpdOrdQtyBuff;
  double SpdOrdValBuff;

  void Initialize(char* pchData)
  {
    memcpy(this, pchData, sizeof(SPD_ORD_LMT));

    iUserId = __bswap_32(iUserId);
    SwapDouble((char*) &SpdOrdQtyBuff);
    SwapDouble((char*) &SpdOrdValBuff);
  }
};
*/
typedef MS_OE_REQUEST_TR NSE_CM_NNF_NEW_TR; //BOARD_LOT_IN(20000)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_NEW_ACCEPT_TR; //ORDER_CONFIRMATION_OUT(20073)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_NEW_REJECT_TR; //ORDER_ERROR_OUT(20231)

typedef MS_OM_REQUEST_TR NSE_CM_NNF_MOD_TR; //ORDER_MOD_IN(20040)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_MOD_ACCEPT_TR; //ORDER_MOD_CONFIRM_OUT(20074)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_MOD_REJECT_TR; //ORDER_MOD_REJ_OUT(20042)

typedef MS_OM_REQUEST_TR NSE_CM_NNF_CXL_TR; //ORDER_CANCEL_IN(20070)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_CXL_ACCEPT_TR; //ORDER_CANCEL_CONFIRM_OUT(20075)
typedef MS_OE_RESPONSE_TR NSE_CM_NNF_CXL_REJECT_TR; //ORDER_CXL_REJ_OUT(20072)

const int16_t sLen_LB_Query_Req = sizeof(LB_QUERY_REQ);
const int16_t sLen_Exch_HB_Req = sizeof(MS_HEART_BEAT_REQ);
const int16_t sLen_NSECM_AddModCan_Req_NonTrim = sizeof(_MS_OE_REQUEST);
const int16_t sLen_Logon_Req = sizeof(MS_SIGNON_REQ);
const int16_t sLen_SysInfo_Req = sizeof(MS_SYSTEM_INFO_REQ);
const int16_t sLen_UpdLDB_Req = sizeof(MS_UPDATE_LOCAL_DATABASE);
const int16_t sLen_IndexDownload_Req = sizeof(MS_INDUSTRY_INDEX_DLOAD_REQ);
const int16_t sLen_MsgDownload_Req = sizeof(MS_MESSAGE_DOWNLOAD_REQ);


const int16_t sLen_Add_Req = sizeof(MS_OE_REQUEST_TR);
const int16_t sLen_Mod_Can_Req = sizeof(MS_OM_REQUEST_TR);
const int16_t sLen_Tap_Hdr = sizeof(TAP_HEADER);
const int16_t sLen_Msg_Hdr = sizeof(MESSAGE_HEADER);
const int16_t sLen_Full_Hdr = sLen_Tap_Hdr + sLen_Msg_Hdr;
const int16_t sLen_Cust_Hdr = sizeof(CUSTOM_HEADER);
const int16_t sLen_Add_Req_NonTrim = sizeof(_MS_OE_REQUEST);

#pragma pack()

#endif
