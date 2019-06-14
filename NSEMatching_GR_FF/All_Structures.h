
# define  SUBS_LIST_LIMIT 1200  // define limit for subscription token supported in each segment
//# define  TICK_STORE_SIZE 1200  // Tick store size should be same as above hence commented
#define CONFIG_FILE_NAME		"config.ini"

# define SUBS_LIMIT_EACH_TOKEN  50 // define limit of mdp subscription supported in each token
#define MAX_MSG_LEN             2048
#define CONNECTED     1
#define DISCONNECTED    0
#define  LOGGED_ON  1
#define LOGGED_OFF 0

//const short shmPermissions = 0666;


#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <stdint.h>
#include <byteswap.h>
#include <thread>
#include <unordered_map>
#include <atomic>
#include "log.h"


#ifndef ALL_STRUCTURES_H
#define	ALL_STRUCTURES_H
#define TS_VAR(TS)                  timespec TS;
#define GET_CURR_TIME(TS)           clock_gettime(CLOCK_REALTIME , &TS);
#define TIMESTAMP(TS)               (((TS.tv_sec * 1000000000L) + (TS.tv_nsec)))

/*Error codes*/
#define ORDER_NOT_FOUND   16060
#define ERR_MOD_CAN_REJECT   16115
#define ERR_SECURITY_NOT_AVAILABLE  16035
#define ERR_INVALID_SYMBOL  16012
#define ERR_USER_NOT_FOUND  16042
#define ERR_USER_ALREADY_SIGNED_ON  16004                
#define ERR_INVALID_USER_ID  16148     
#define INVALID_ORDER 16419
#define e$invalid_contract_comb 16627
#define  e$vc_order_rejected 16793
#define e$fok_order_cancelled 16388
#define BOOK_SIZE_CROSSED 10001
#define MEMORY_NOT_AVAILABLE 10002
#define OE_ORD_CANNOT_MODIFY 16346

#pragma pack(1)

// ............................................            Start Broadcast Queue Structures
/*typedef class SendData
{
    public:
        char msg[500];
        int msgLen;
        int FD;
};*/

/*typedef struct _Logger
{
  char LogBuf[250];
  short int action;
}LGGER;*/

typedef struct contractInfo
{
   int32_t Token;
   int32_t AssetToken;
   int32_t MinimumLotQuantity;
   int32_t BoardLotQuantity;

  contractInfo()
  {
    Token = 0;
    AssetToken = 0;
    MinimumLotQuantity = 0;
    BoardLotQuantity = 0;
  }
}CONTRACTINFO;

typedef struct cd_contractInfo
{
   int32_t Token;
   int32_t AssetToken;
   int32_t MinimumLotQuantity;
   int32_t BoardLotQuantity;
   int32_t Multiplier;

  cd_contractInfo()
  {
    Token = 0;
    AssetToken = 0;
    MinimumLotQuantity = 0;
    BoardLotQuantity = 0;
  }
}CD_CONTRACTINFO;


typedef class ConnectionInfo
{
    public:  
   std::atomic<int> status;
    std::atomic<int> recordCnt;
  char msgBuffer[3000];
  int msgBufSize;
  char IP[16];
  int32_t dealerID;
  
  ConnectionInfo()
  {
       this->status = CONNECTED;
       this->msgBufSize = 0;
       this->recordCnt = 0;
       memset (this->msgBuffer, 0, sizeof(this->msgBuffer));
      memset (this->IP, 0, sizeof(this->IP));
      this->dealerID = 0;
  }
}CONNINFO;

typedef std::unordered_map<int,CONNINFO*> connectionMap;
typedef connectionMap::iterator connectionItr;

typedef struct _IP_STATUS
{
  char IP[20];
  std::atomic<int> status;
  int COL;
  int dealerOrdIndex;
  int FD;
  CONNINFO* ptrConnInfo;
  
  _IP_STATUS()
  {
    memset(IP, 0, 16);
    status = LOGGED_OFF;
    COL = 0;
    dealerOrdIndex = -1;
    FD = -1;
    ptrConnInfo = NULL;
  }
}IP_STATUS;
typedef std::unordered_map<int32_t,IP_STATUS*> dealerInfoMap;
typedef dealerInfoMap::iterator dealerInfoItr;

typedef struct tokenordercnt
{
  int token;
  int buyordercnt;
  int sellordercnt;
  
  tokenordercnt()
  {
     token = 0;
     buyordercnt = 0;
     sellordercnt = 0;
  }
}TokenOrderCnt;



typedef struct _STD_HEADER
{
   int nMsgCode;
   short wMsgLen;
   long lSeconds;
   long lMicroSec;
   long int lSeqNo;
   long lExchangeTimestamp;
   char strExchgSeg[16];
   long lToken;
   long lEntrySeconds;
   long lEntryMicroSec;
} STD_HEADER;

typedef struct _ORDER_BOOK
{
   long lQty;
   long lPrice;
   int  nNumOrders;
   short wBuySellFlag;
} ORDER_BOOK;

typedef struct _BID_ASK_PKT
{
   STD_HEADER header;
   //long lToken;
   char strSymbol[30];
   double dblTBQ;
   double dblTSQ;
   int nNoOfRecords;
   ORDER_BOOK Records[40];
   double dblVolTradedToday;
   long lATP;
   long lLTT;
   long lNumOfTrades;
   long lTotalTradedVol;
   long lOpenPrice;
   long lClosePrice;
   long lLTP;   
} BID_ASK_PKT;


// ............................................            End External Queue Structures

// ............................................            Start  Exchange Structures

typedef struct _CLIENT_MSG
{
    char         msgBuffer[2048];
    int          nBufferLen;
}CLIENT_MSG;

typedef struct _TCP_BUFFER
{
    char         msgBuffer[204800];
    short        buffsize; 
}TCP_BUFFER;

typedef struct _DATA_RECEIVED
{     
    int MyFd;
    int32_t Transcode;
    int32_t Error;    
    char   msgBuffer[2048];
    CONNINFO* ptrConnInfo;
    int64_t recvTimeStamp;   // added by Tarun
    
    _DATA_RECEIVED()
    {
      this->MyFd      = 0;
      this->Transcode = 0;
      this->Error     = 0;    
      memset(this->msgBuffer, 0, 2048);  
      this->ptrConnInfo = NULL;
      this->recvTimeStamp = 0;
    }
    
    _DATA_RECEIVED(const _DATA_RECEIVED& stDataReceived)
    {
      this->MyFd      = stDataReceived.MyFd;
      this->Transcode = stDataReceived.Transcode;
      this->Error     = stDataReceived.Error;    
      memcpy(this->msgBuffer, stDataReceived.msgBuffer, 2048);  
      this->ptrConnInfo = stDataReceived.ptrConnInfo;
      this->recvTimeStamp = stDataReceived.recvTimeStamp;
    }
    
}DATA_RECEIVED;


typedef struct _DATA_LOG 
{     
  int LogCode;
  long lData[10];
  double dData[2];
  char   charBuffer[256];
    
  _DATA_LOG()    
  {
    LogCode   = 0;
    memset(lData, 0, 10);
    memset(dData, 0, 2);
    memset(charBuffer, 0, 256);  
  }
  
  _DATA_LOG(const _DATA_LOG& stDataLog)    
  {
    this->LogCode   = stDataLog.LogCode;
    memcpy(this->lData, stDataLog.lData, 10);
    memcpy(this->dData, stDataLog.dData, 2);
    memcpy(this->charBuffer, stDataLog.charBuffer, 256);  
  }  
    
}DATA_LOG;


struct CUSTOM_HEADER
{
  int16_t sLength;
  int32_t iSeqNo;
  //int16_t sResrvSeqNo;
  //char chCheckSum[16];
  uint32_t CheckSum[4];
  //int16_t sMsgCnt;
  int16_t sTransCode;
  
  inline void swapBytes()
  {
    sLength     = __bswap_16(sLength);
    iSeqNo      = __bswap_32(iSeqNo);
    sTransCode  = __bswap_16(sTransCode);
  } 
};


struct TRADE_CONFIRMATION_OS  
  {
    //TAP_HEADER tap_hdr;
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
    } OrderFlags;
    int32_t FillNumber;
    int32_t FillQuantity;
    int32_t FillPrice;
    int32_t VolumeFilledToday;
    char ActivityType[2];
    int32_t ActivityTime;
    struct SEC_INFO
    {
      char Symbol[10];
      char Series[2];
    } sec_info;
    int32_t TokenNo;
    int16_t BookType;
    int16_t ProClient;

  //  inline void Initialize(char* pchData)
  //  {
  //    memcpy(&TransactionCode, pchData, sizeof(TRADE_CONFIRMATION_TR));
  //    TransactionCode = __bswap_16(TransactionCode);
  //    SwapDouble((char*) &ResponseOrderNumber);
  //    //ResponseOrderNumber       = __bswap_64(int32_t(ResponseOrderNumber));
  //    //		BuySellIndicator          = __bswap_16(BuySellIndicator);
  //    //		OriginalVolume            = __bswap_32(OriginalVolume);
  //    //		DisclosedVolume           = __bswap_32(DisclosedVolume);
  //    //		RemainingVolume           = __bswap_32(RemainingVolume);
  //    //		DisclosedVolumeRemaining  = __bswap_32(DisclosedVolumeRemaining);
  //    //		Price                     = __bswap_32(Price);
  //    FillNumber = __bswap_32(FillNumber);
  //    FillQuantity = __bswap_32(FillQuantity);
  //    FillPrice = __bswap_32(FillPrice);
  //    VolumeFilledToday = __bswap_32(VolumeFilledToday);
  //    ActivityTime = __bswap_32(ActivityTime);
  //    OriginalVolume = __bswap_32(OriginalVolume);
  //    RemainingVolume = __bswap_32(RemainingVolume);
  //  }

  };
//struct Exchange_HEADER
//{
//  int16_t wLength;
//  int32_t iSeqNo;
//  uint32_t CheckSum[4];
//  inline void swapBytes()
//  {
//    sLength     = __bswap_16(sLength);
//    iSeqNo      = __bswap_32(iSeqNo);
//
//  }
//};


// ............................................            End Internal Queue Structures

typedef struct _LOG_DATA
{
    short wImportance; 
    int nErrorCode;
    int nKey1;
    int ndata1;
    int nKey2;
    int ndata2;
    int nKey3;
    int ndata3;
    int nKey4;
    int ndata4;
    int nKey5;
    int ndata5;    
    
}LOG_DATA;



// ............................................            End Logger Structures


#pragma pack()







 struct SEC_INFO_NSECM
  {
    char Symbol[10];
    char Series[2];
  };
  
  
    struct ST_ORDER_FLAGS_NSECM
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

typedef struct _NSECM_STORE_TRIM
{    
    int16_t TransactionCode;
    int32_t TraderId;
    SEC_INFO_NSECM sec_info; 
    char AccountNumber[10];
    int16_t BookType;
    int16_t BuySellIndicator;
    int32_t DisclosedVolume;
    int32_t Volume;
    int32_t Price;
    int32_t GoodTillDate;
    ST_ORDER_FLAGS_NSECM OrderFlags;
    int16_t BranchId;
    int32_t UserId;
    char BrokerId[5];
    char Suspended;
    char Settlor[12];
    int16_t ProClientIndicator;
    double NnfField;
    int32_t TransactionId;
    double OrderNumber;    
} NSECM_STORE_TRIM;





struct CONTRACT_DESC_TR_NSEFO
{
  char InstrumentName[6];
  char Symbol[10];
  uint32_t ExpiryDate;
  uint32_t StrikePrice;
  char OptionType[2];
};

struct ST_ORDER_FLAGS_NSEFO
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

 typedef struct _NSEFO_STORE_TRIM   //BOARD_LOT_IN(2000)
{
  //TAP_HEADER tap_hdr;
   int16_t TransactionCode;
  int32_t UserId;
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR_NSEFO contract_desc_tr;
  char AccountNumber[10];
  int16_t BookType;
  int16_t BuySellIndicator;
  int32_t DisclosedVolume;
  int32_t Volume;
  int32_t Price;
  int32_t GoodTillDate;
  ST_ORDER_FLAGS_NSEFO OrderFlags;
  int16_t BranchId;
  int32_t TraderId;
  char BrokerId[5];
  char OpenClose;
  char Settlor[12];
  int16_t ProClientIndicator;  
  int32_t filler;
  double NnfField;
  double OrderNumber;
} NSEFO_STORE_TRIM;




/*
 Error Codes
 * 1001 -> Invalid Token
 * 
 
 */

#endif	/* ALL_STRUCTURES_H */