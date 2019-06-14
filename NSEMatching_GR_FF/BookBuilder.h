/* 
 * File:   BookBuilder.h
 * Author: muditsharma
 *
 * Created on March 11, 2016, 12:18 PM
 */
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include<cstring>
#include<stdint.h>
#include<byteswap.h>
#include <string>
#include <string.h>

#include "All_Structures.h"

#define BOOKSIZE 5000
#define BOOKSIZE_PASSIVE 1000
//#define BOOKSIZE 10000
//#define NOOFTOKENS 50
//#define MAXDEALERS 50
//#define MAXTOKENS 50

#ifdef __NSECM__
    #define NOOFTOKENS 35000
#else
    #define NOOFTOKENS 100000
#endif

#pragma pack(2)

  enum NSECM_CONTRACT_FILE_INDEX
  {  
    SECURITY_TOKEN              = 1,				
		SECURITY_SYMBOL             = 2,			
		SECURITY_SERIES             = 3,					
		SECURITY_INSTRUMENT_TYPE		= 4,
		SECURITY_PERMITTED_TO_TRADE	= 6,		
		SECURITY_CREDIT_RATING			= 7,				
		SECURITY_SECURITY_STATUS		= 8,			
		SECURITY_ELIGIBILITY				= 9,	
		SECURITY_BROAD_QTY          = 20,						
		SECURITY_TICK_SIZE          = 21,
		SECURITY_NAME               = 22,		
		SECURITY_DELETE_FLAG				= 52,		
		SECURITY_ISIN_NAME          = 54,		        
  };

  typedef struct _TOKEN_INDEX_MAPPING
{
      long Token[NOOFTOKENS];
}TOKEN_INDEX_MAPPING;

typedef struct
{
  SEC_INFO_NSECM sec_info; 
  ST_ORDER_FLAGS_NSECM OrderFlags;
  char Suspended;
  int32_t TransactionId;
}NSECM_FIELDS;

typedef struct
{
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR_NSEFO contract_desc_tr;
  ST_ORDER_FLAGS_NSEFO OrderFlags;
  char OpenClose;
  int32_t filler;
}NSEFO_FIELDS;

typedef struct
{
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR_NSEFO contract_desc_tr;
  ST_ORDER_FLAGS_NSEFO OrderFlags;
  char OpenClose;
  int32_t filler;
}NSECD_FIELDS;



typedef struct 
{
     long SeqNo;
     long lQty;
     long lPrice;
     double OrderNo;
     short IsIOC;
     short IsDQ;
     int16_t BuySellIndicator;
}ORDER_BOOK_DTLS_MB;

typedef struct 
{  
   long TradeNo; 
   long Token;
   int BuyRecords;
   int SellRecords;
   int BuySeqNo;
   int SellSeqNo;   
   int LTP;
   int BuyBookSize;
   int SellBookSize;
   bool BuyBookFull;
   bool SellBookFull;
   ORDER_BOOK_DTLS_MB *Buy;
   ORDER_BOOK_DTLS_MB *Sell;
}ORDER_BOOK_TOKEN_MB;

typedef struct
{    
    ORDER_BOOK_TOKEN_MB* OrderBook;
}ORDER_BOOK_ME_MB;

typedef union
{
  NSECM_FIELDS NSECM;
  NSEFO_FIELDS NSEFO;
  NSECD_FIELDS NSECD;
}NSECM_NSEFO;

typedef struct 
{
     long SeqNo;
     long lQty;
     long OpenQty;
     long lPrice;
     long TriggerPrice;
     double OrderNo;
     long TTQ;
     short IsIOC;
     short IsDQ;
     short IsSL;
     long DQty;
     int DQRemaining;
     int FD;
     int32_t dealerID;
     int16_t TransactionCode;
     int32_t TraderId;
     char AccountNumber[10];
     int16_t BookType;
     int16_t BuySellIndicator;
     int32_t Volume;
     int32_t GoodTillDate; 
     int16_t BranchId;
     int32_t UserId;
     char BrokerId[5];
     char Settlor[12];
     int16_t ProClientIndicator;
     double NnfField;
     char PAN[10];
     int32_t AlgoId;
     int16_t AlgoCategory;
     char Reserved3[60];
     NSECM_NSEFO nsecm_nsefo_nsecd ;
     int32_t LastModified;
     CONNINFO* connInfo;
     
}ORDER_BOOK_DTLS;

typedef struct 
{  
   long TradeNo; 
   long Token;
   int BuyRecords;
   int SellRecords;
   int BuySeqNo;
   int SellSeqNo;   
   int LTP;
   int BuyBookSize;
   int SellBookSize;
   bool BuyBookFull;
   bool SellBookFull;
   ORDER_BOOK_DTLS *Buy;
   ORDER_BOOK_DTLS *Sell;
}ORDER_BOOK_TOKEN;


typedef struct
{    
    ORDER_BOOK_TOKEN OrderBook[NOOFTOKENS];
}ORDER_BOOK_MAIN;

typedef struct
{    
    ORDER_BOOK_TOKEN* OrderBook;
}ORDER_BOOK_ME;




typedef struct // Do not use this use TRADE_CONFIRMATION_TR instead
{
  long SeqNo;
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
  NSECM::ST_ORDER_FLAGS OrderFlags;
  int32_t FillNumber;
  int32_t FillQuantity;
  int32_t FillPrice;
  int32_t VolumeFilledToday;
  char ActivityType[2];
  int32_t ActivityTime;
  NSECM::SEC_INFO sec_info;
  int16_t BookType;
	int16_t ProClient;
}OrderTradeInfo;
        

  typedef struct OMSScripInfo
  {
    int32_t                 nExpiryDate;    
    int32_t                 nStrikePrice;    
    int32_t                 nToken; 
    int32_t                 nBoardLotQty;
    int32_t                 nMinLotQty;	
    int32_t                 nTickSize;
    int16_t                 nExchangeSegment;
    int16_t                 nCustomInstrumentType;
    char                    cSymbol[10 + 1];	            
    char                    cSeries[2 + 1];	                
    char                    cSpreadContract; // Y for YES N NO
    char                    cSymbolName[25 + 1]; //empty in case of Spread contract as not received from exchange
    int32_t                 nToken2; //Spread Leg2  contract
    int32_t                 nBoardLotQty2;
    int32_t                 nMinLotQty2;
    int32_t                 nTickSize2;    
    int32_t                 nExpiryDate2;      
  }OMS_ScripInfo;
  
  #pragma pack()