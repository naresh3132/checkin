/* 
 * File:   BookBuilder.h
 * Author: NareshRK
 *
 * Created on September 12, 2018, 10:18 AM
 */

#ifndef BOOKBUILDER_H
#define	BOOKBUILDER_H


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


struct NSEFO_FIELDS
{
  int16_t ReasonCode;
  int32_t TokenNo;
  CONTRACT_DESC_TR_NSEFO contract_desc_tr;
  ST_ORDER_FLAGS_NSEFO OrderFlags;
  char OpenClose;
  int32_t filler;
};

struct NSECM_FIELDS
{
  SEC_INFO_NSECM sec_info; 
  ST_ORDER_FLAGS_NSECM OrderFlags;
  char Suspended;
  int32_t TransactionId;
};

typedef union
{
  NSECM_FIELDS NSECM;
  NSEFO_FIELDS NSEFO;
}NSECM_NSEFO;


struct OrderBook
{
  int32_t iIntOrdId;
  int64_t llExchOrderId;
  int64_t llTimeStamp;
  int32_t LMT;
  int16_t iErrorCode;
  int32_t iToken;
  int32_t iSeqNo;
  int16_t iSeg;
  int32_t iPrevQty;
  int32_t iQty;
  int32_t iOpenQty;
  int32_t iTotTrdQty;
  int32_t iPrevPrice;
  int32_t iPrice;
  int32_t iTriggerPrice;
  int16_t IsIOC;
  int16_t IsDQ;
  int16_t IsSL;
  int32_t iDQty;
  int16_t iDQRemaining;
  int32_t iDealerID;
  int16_t iTransactionCode;
  int16_t BuySellIndicator;
  int16_t iFd;
  int16_t iStatus;
  int16_t isActionable;
  NSECM_NSEFO nsecm_nsefo ;
  
  
};

#endif	/* BOOKBUILDER_H */

