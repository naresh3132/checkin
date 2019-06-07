//#include "TCPHandler.h"
#include <vector>
#include <unordered_map>
#include <utility>

#include "AllMsgHandling.h"

char chLogBuff[400];
int32_t iExchSendSeqNo = 0;
int32_t GlobalSeqNo = 0;
int32_t gInternalOrderId = 1;
CONNINFO *gpConnFO;
CONNINFO *gpConnCM;
std::vector<OrderBook> vOrderBook;

typedef std::unordered_map<int64_t,int32_t> ExOdrIdToSysOrdId;
typedef ExOdrIdToSysOrdId::iterator IterExOdrIdToSysOrdId;
ExOdrIdToSysOrdId mExOdrIdToSysOrdId;


typedef std::vector<int32_t> vOrderId;
typedef std::unordered_map<int32_t,vOrderId> UserIdToOrderId;
typedef UserIdToOrderId::iterator iterUserIdToOrderId;
UserIdToOrderId mUserIdToOrderId;


typedef std::vector<DEALER::TRD_MSG> vTrdStore;
typedef std::unordered_map<int32_t,vTrdStore> UserIdToTrdStore;
typedef UserIdToTrdStore::iterator iterUserIdToTrdStore;
UserIdToTrdStore mUserIdToTrdStore;


TCPCONNECTION *oTcp;




void PrintOrderBook(int iIndex)
{
  snprintf(chLogBuff, 500, "PRINT|FD %d||Order# %d|Price::%d|Volume::%d| Token %d|B/S %d|ExchId %ld", 
      vOrderBook[iIndex].iFd,vOrderBook[iIndex].iIntOrdId, vOrderBook[iIndex].iPrice,vOrderBook[iIndex].iQty, vOrderBook[iIndex].iToken, vOrderBook[iIndex].BuySellIndicator, (int64_t)vOrderBook[iIndex].llExchOrderId);
  Logger::getLogger().log(DEBUG, chLogBuff);
  
}

int ValidateTokenInfo(DEALER::GENERIC_ORD_MSG* pGenOrd, ContractInfoMap& ContInfoMap)
{
  int32_t iToken = pGenOrd->iTokenNo;
  int32_t iPrice = pGenOrd->iPrice;
  
  if(pGenOrd->header.iSeg == SEG_NSEFO)
  {
    auto it = ContInfoMap.find(iToken);
    if(it  != ContInfoMap.end())
    {
      CONTRACTINFO* pConrt = it->second;
      if(iPrice > pConrt->HighDPR || iPrice < pConrt->LowDPR)
      {
        return PRICE_RANGE_CHECK_FAIL;
      }
    }
    else
    {
      return TOKEN_NOT_FOUND;
    }
  }
  
  return 0;
}

inline int ValidateDealer(int32_t iDealer, dealerInfoMap& DealInfoMap)
{
  auto it = DealInfoMap.find(iDealer);
  if(it  != DealInfoMap.end())
  {
    return 0;
  }
  
  return DEALER_NOT_FOUND;
}

int ValidateGenOrder(DEALER::GENERIC_ORD_MSG* pGenOrd,dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap)
{
  int32_t iDealer   = pGenOrd->header.iDealerID;
  int32_t iError;
  
  iError = ValidateDealer(iDealer,DealInfoMap);
  if(iError != 0)
  {
    return iError;
  }
  
  iError = ValidateTokenInfo(pGenOrd, ContInfoMap);
  if(iError != 0)
  {
    return iError;
  }
  
  return iError;
}

int32_t CheckStatusErrorCode(int iIndex)
{
  switch(vOrderBook[iIndex].iStatus)
  {
    case NEW_PENDING:
    {
      return ADD_ORDER_PENDING;
    }
    break;
    
    case MOD_PENDING:
    {
      return MOD_ORDER_PENDING;
    }
    break;
    
    case CAN_PENDING:
    {
      return CAN_ORDER_PENDING;
    }
    break;
    
    case REJECTED:
    {
      return ORDER_NOT_FOUND;
    }
    break;
    
    case COMPLETELY_FILLED:
    {
      return ORDER_NOT_FOUND;
    }
    break;
    
    case CANCELED:
    {
      return ORDER_NOT_FOUND;
    }
    break;
    
    default:
      break;
  }
  
  
}

int AddOrderTrimRespFO(NSEFO::MS_OE_RESPONSE_TR *pOrderRespExch,connectionMap& ConnMap)
{
  DEALER::GENERIC_ORD_MSG oOrderResp;
  CONNINFO *pConn;
  
  int32_t InternalOrderId =  pOrderRespExch->filler;
  int OrdBkIdx = InternalOrderId - 1; 
  
  
  
  if(__bswap_16(pOrderRespExch->ErrorCode) != 0)
  {
    oOrderResp.iOrdStatus             = REJECTED;
    oOrderResp.isActionable           = (int16_t)Actionable::FALSE;
    
    vOrderBook[OrdBkIdx].iStatus      = oOrderResp.isActionable;
    vOrderBook[OrdBkIdx].isActionable = oOrderResp.iOrdStatus;
  }
  else
  {
    oOrderResp.iOrdStatus             = OPEN;
    oOrderResp.isActionable           = (int16_t)Actionable::TRUE;
    
    vOrderBook[OrdBkIdx].iStatus      = oOrderResp.iOrdStatus;
    vOrderBook[OrdBkIdx].isActionable = oOrderResp.isActionable;
  }
  
  double dEOrd                        = pOrderRespExch->OrderNumber;
  SwapDouble((char*)&dEOrd);
  
  std::pair<IterExOdrIdToSysOrdId, bool>lcRetVal; 
  lcRetVal = mExOdrIdToSysOrdId.insert(std::make_pair(dEOrd,InternalOrderId));
  if (false == lcRetVal.second)
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|ADD ORDER RES|ExchOrder %ld|COrd %d|Failed to Store in map",dEOrd, InternalOrderId);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }      

  vOrderBook[OrdBkIdx].llExchOrderId  = (int64_t)dEOrd;
  vOrderBook[OrdBkIdx].llTimeStamp    = __bswap_64(pOrderRespExch->Timestamp1);
  vOrderBook[OrdBkIdx].LMT            = __bswap_32(pOrderRespExch->LastModified);
  
  oOrderResp.header.llTimeStamp           = (vOrderBook[OrdBkIdx].llTimeStamp)/1000000000;
  oOrderResp.header.iDealerID             = vOrderBook[OrdBkIdx].iDealerID;
  oOrderResp.header.iErrorCode            = __bswap_16(pOrderRespExch->ErrorCode);
  oOrderResp.header.iMsgLen               = sizeof(oOrderResp);
  oOrderResp.header.iSeqNo                = vOrderBook[OrdBkIdx].iSeqNo;
  oOrderResp.header.iSeg                  = vOrderBook[OrdBkIdx].iSeg;
  oOrderResp.header.iTransCode            = UNSOLICITED_ORDER_RES;
  
  oOrderResp.BuySellIndicator             = vOrderBook[OrdBkIdx].BuySellIndicator;
  oOrderResp.iIntOrdID                    = pOrderRespExch->filler;
  oOrderResp.iTokenNo                     = vOrderBook[OrdBkIdx].iToken;
  oOrderResp.iPrice                       = vOrderBook[OrdBkIdx].iPrice;
  oOrderResp.iQty                         = vOrderBook[OrdBkIdx].iQty;
  oOrderResp.iExchOrdID                   = dEOrd;
  oOrderResp.iDisclosedVolume             = vOrderBook[OrdBkIdx].iDQRemaining;
  
  int sockFd = vOrderBook[OrdBkIdx].iFd;
  auto it = ConnMap.find(sockFd);
  if(it != ConnMap.end())
  {
    pConn = it->second;
    oTcp->SendData(sockFd,(char *)&oOrderResp, oOrderResp.header.iMsgLen,pConn);
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|ADD ORDER RES|ExchOrder %ld|COrd %d|ErrorCode %d|%d|Token %d|DealerID %d|%d|Status %d|Actionable %d ", sockFd, (int64_t)dEOrd, InternalOrderId, __bswap_16(pOrderRespExch->ErrorCode), oOrderResp.header.iErrorCode,
      __bswap_32(pOrderRespExch->TokenNo), __bswap_32(pOrderRespExch->UserId), oOrderResp.header.iDealerID,oOrderResp.iOrdStatus,oOrderResp.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|ADD ORDER RES|ExchOrder %ld|COrd %d|Fd not found|Token %d|DealerID %d|%d", sockFd, dEOrd, InternalOrderId, __bswap_32(pOrderRespExch->TokenNo), __bswap_32(pOrderRespExch->UserId), oOrderResp.header.iDealerID);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
}

void AddOrderTrimReqFO(ConfigFile& oConfigdata,int iSockIdFO)
{
  int OdrBkIdx = gInternalOrderId - 2 ;
  
  
  NSEFO::MS_OE_REQUEST_TR   oExchAddReq(oConfigdata.iUserID_FO, oConfigdata.sTMID_FO, oConfigdata.sBranchId_FO, 1, 1);

  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  oExchAddReq.NnfField         = oConfigdata.iNNF_FO;
  SwapDouble((char*)&oExchAddReq.NnfField);
  
  
  memcpy(oExchAddReq.contract_desc_tr.InstrumentName, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName, 6);
  memcpy(oExchAddReq.contract_desc_tr.OptionType, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.OptionType, 2);
  memcpy(oExchAddReq.contract_desc_tr.Symbol, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.Symbol, 10);
  
  oExchAddReq.TokenNo                                     = __bswap_32(vOrderBook[ OdrBkIdx ].iToken);
  oExchAddReq.contract_desc_tr.ExpiryDate                 = __bswap_32(vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.ExpiryDate);
  oExchAddReq.contract_desc_tr.StrikePrice                = __bswap_32(-1);
  oExchAddReq.BuySellIndicator                            = __bswap_16(vOrderBook[ OdrBkIdx ].BuySellIndicator);
  oExchAddReq.Volume                                      = __bswap_32(vOrderBook[ OdrBkIdx ].iQty);

  oExchAddReq.Price                                       = __bswap_32(vOrderBook[ OdrBkIdx ].iPrice);

  
  oExchAddReq.DisclosedVolume                             = oExchAddReq.Volume;
  oExchAddReq.filler                                      = vOrderBook[ OdrBkIdx ].iIntOrdId;     
  oExchAddReq.tap_hdr.iSeqNo                              = __bswap_32(++iExchSendSeqNo);
  
  oTcp->SendData( iSockIdFO,(char*)&oExchAddReq.tap_hdr.sLength, sizeof(NSEFO::MS_OE_REQUEST_TR),gpConnFO);
  
  snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d|UserId %d", 
      iSockIdFO, oExchAddReq.filler, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo),__bswap_32(oExchAddReq.TraderId) );
  Logger::getLogger().log(DEBUG, chLogBuff);
  
}

void AddToOrderBook(OrderBook &Book_Detail)
{
  vOrderBook.push_back(Book_Detail);
}

bool AddOrderUI(DEALER::GENERIC_ORD_MSG* pAddOrdReq, int iFd, dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap, CONNINFO* pConn)
{
  bool ret = true;
  DEALER::GENERIC_ORD_MSG oAddOrdRes;
  
  memcpy(&oAddOrdRes.header,&pAddOrdReq->header,sizeof(oAddOrdRes.header));
  
  oAddOrdRes.header.iTransCode    = UNSOLICITED_ORDER_RES;
  oAddOrdRes.header.iErrorCode    = ValidateGenOrder(pAddOrdReq, DealInfoMap, ContInfoMap);
  if(oAddOrdRes.header.iErrorCode != 0)
  {
    oAddOrdRes.iOrdStatus   = REJECTED;
    ret = false;
  }
  else
  {
    oAddOrdRes.iOrdStatus = NEW_PENDING;
  }
  
  oAddOrdRes.iIntOrdID          = gInternalOrderId++;
  oAddOrdRes.BuySellIndicator   = pAddOrdReq->BuySellIndicator;
  oAddOrdRes.iPrice             = pAddOrdReq->iPrice;
  oAddOrdRes.iQty               = pAddOrdReq->iQty;
  oAddOrdRes.iTTQ               = 0;
  oAddOrdRes.iTokenNo           = pAddOrdReq->iTokenNo;
  oAddOrdRes.isActionable       = (int16_t)Actionable::FALSE;
  
  oTcp->SendData(iFd, (char *)&oAddOrdRes, oAddOrdRes.header.iMsgLen, pConn);
  
  sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|ADD ORDER REQ|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d| Token %d|Side %d|Status %d|Actionable %d", oAddOrdRes.header.iDealerID, oAddOrdRes.header.iMsgLen,
          oAddOrdRes.header.iErrorCode, oAddOrdRes.header.iTransCode,oAddOrdRes.header.iSeqNo,oAddOrdRes.iIntOrdID,oAddOrdRes.iQty,oAddOrdRes.iPrice,oAddOrdRes.iTokenNo,oAddOrdRes.BuySellIndicator,oAddOrdRes.iOrdStatus,oAddOrdRes.isActionable);
  Logger::getLogger().log(DEBUG, chLogBuff);
  
  OrderBook bookdetails;
  bookdetails.iSeg                  = pAddOrdReq->header.iSeg;
  bookdetails.iFd                   = iFd;
  bookdetails.IsIOC                 = 0;
  bookdetails.IsDQ                  = 0;
  bookdetails.iToken                = oAddOrdRes.iTokenNo;
  bookdetails.iIntOrdId             = oAddOrdRes.iIntOrdID;
  bookdetails.iPrice                = oAddOrdRes.iPrice;
  bookdetails.iQty                  = oAddOrdRes.iQty;
  bookdetails.iDealerID             = oAddOrdRes.header.iDealerID;
  bookdetails.iDQty                 = oAddOrdRes.iDisclosedVolume;
  bookdetails.iTransactionCode      = oAddOrdRes.header.iTransCode;
  bookdetails.iSeqNo                = oAddOrdRes.header.iSeqNo;
  bookdetails.llExchOrderId         = 0;
  bookdetails.iErrorCode            = oAddOrdRes.header.iErrorCode;
  bookdetails.BuySellIndicator      = oAddOrdRes.BuySellIndicator;
  bookdetails.iTotTrdQty            = 0;
  bookdetails.iStatus               = oAddOrdRes.iOrdStatus;
  bookdetails.isActionable          = oAddOrdRes.isActionable;
  
  auto it = ContInfoMap.find(pAddOrdReq->iTokenNo);
  if(it  != ContInfoMap.end())
  {
    CONTRACTINFO* pConrt = it->second;
    bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.ExpiryDate   = pConrt->ExpiryDate;
    bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.StrikePrice  = pConrt->StrikePrice;
    memcpy(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName,pConrt->InstumentName,sizeof(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName));
    memcpy(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.Symbol,pConrt->Symbol,sizeof(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.Symbol));
    memcpy(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.OptionType,pConrt->OptionType,sizeof(bookdetails.nsecm_nsefo.NSEFO.contract_desc_tr.OptionType));
  }
  else
  {
   sprintf(chLogBuff,"Token not found in Contract Map|Token %d",pAddOrdReq->iTokenNo);
   Logger::getLogger().log(DEBUG, chLogBuff);
  }
  mUserIdToOrderId[pAddOrdReq->header.iDealerID].push_back(oAddOrdRes.iIntOrdID);

  AddToOrderBook(bookdetails);
  
  return ret;
}

int ModOrderTrimRespFO(NSEFO::MS_OE_RESPONSE_TR *pOrderRespExch,connectionMap& ConnMap)
{
  DEALER::GENERIC_ORD_MSG oOrderResp;
  CONNINFO *pConn;
  
  int32_t InternalOrderId =  pOrderRespExch->filler;
  int OrdBkIdx = InternalOrderId - 1; 
  
  if(0 != __bswap_16(pOrderRespExch->ErrorCode) )
  {
    vOrderBook[OrdBkIdx].iPrice = vOrderBook[OrdBkIdx].iPrevPrice;
    vOrderBook[OrdBkIdx].iQty   = vOrderBook[OrdBkIdx].iPrevQty;
  }
  
  oOrderResp.iOrdStatus             = OPEN;
  oOrderResp.isActionable           = (int16_t)Actionable::TRUE;
  
  vOrderBook[OrdBkIdx].iStatus      = oOrderResp.iOrdStatus;
  vOrderBook[OrdBkIdx].isActionable = oOrderResp.isActionable;
  
  
  double dEOrd                            = pOrderRespExch->OrderNumber;
  SwapDouble((char*)&dEOrd);
  
  vOrderBook[OrdBkIdx].llExchOrderId      = (int64_t)dEOrd;
  vOrderBook[OrdBkIdx].llTimeStamp        = __bswap_64(pOrderRespExch->Timestamp1);
  vOrderBook[OrdBkIdx].LMT                = __bswap_32(pOrderRespExch->LastModified);
  
  oOrderResp.header.llTimeStamp           = (vOrderBook[OrdBkIdx].llTimeStamp)/1000000000;
  oOrderResp.header.iDealerID             = vOrderBook[OrdBkIdx].iDealerID;
  oOrderResp.header.iErrorCode            = __bswap_16(pOrderRespExch->ErrorCode);
  oOrderResp.header.iMsgLen               = sizeof(oOrderResp);
  oOrderResp.header.iSeqNo                = vOrderBook[OrdBkIdx].iSeqNo;
  oOrderResp.header.iSeg                  = vOrderBook[OrdBkIdx].iSeg;
  oOrderResp.header.iTransCode            = UNSOLICITED_ORDER_RES;
  
  oOrderResp.BuySellIndicator             = vOrderBook[OrdBkIdx].BuySellIndicator;
  oOrderResp.iOrdStatus                   = vOrderBook[OrdBkIdx].iStatus;
  oOrderResp.iIntOrdID                    = pOrderRespExch->filler;
  oOrderResp.iTokenNo                     = vOrderBook[OrdBkIdx].iToken;
  oOrderResp.iPrice                       = vOrderBook[OrdBkIdx].iPrice;
  oOrderResp.iQty                         = vOrderBook[OrdBkIdx].iQty;
  oOrderResp.iExchOrdID                   = dEOrd;
  oOrderResp.iDisclosedVolume             = vOrderBook[OrdBkIdx].iDQRemaining;
  oOrderResp.iTTQ                         = vOrderBook[OrdBkIdx].iTotTrdQty;
  
  
  int sockFd = vOrderBook[OrdBkIdx].iFd;
  auto it = ConnMap.find(sockFd);
  if(it != ConnMap.end())
  {
    pConn = it->second;
    oTcp->SendData(sockFd,(char *)&oOrderResp, oOrderResp.header.iMsgLen,pConn);
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|MOD ORDER RES|ExchOrder %ld|COrd %d|ErrorCode %d|Token %d|DealerID %d|Status %d|Actionable %d", sockFd, (int64_t)dEOrd, InternalOrderId,
       oOrderResp.header.iErrorCode, __bswap_32(pOrderRespExch->TokenNo), oOrderResp.header.iDealerID,oOrderResp.iOrdStatus,oOrderResp.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|MOD ORDER RES|ExchOrder %ld|COrd %d|Fd not found|Token %d|DealerID %d", sockFd, dEOrd, InternalOrderId, __bswap_32(pOrderRespExch->TokenNo),  oOrderResp.header.iDealerID);
    Logger::getLogger().log(DEBUG, chLogBuff); 
  }
  
}

void ModOrderTrimReqFO(ConfigFile& oConfigdata,int iSockIdFO,int32_t OrderId)
{
  
  int OdrBkIdx = OrderId - 1;
  
  NSEFO::MS_OM_REQUEST_TR oExchModReq(oConfigdata.iUserID_FO, oConfigdata.sTMID_FO, oConfigdata.sBranchId_FO, 1);

  memset(oExchModReq.AccountNumber, ' ', 10);
  memcpy(oExchModReq.AccountNumber, "AAK",3);
  oExchModReq.OrderFlags.IOC   = 0;
  oExchModReq.OrderFlags.Day   = 1;
  oExchModReq.OrderFlags.SL    = 0;
  oExchModReq.NnfField         = oConfigdata.iNNF_FO;
  SwapDouble((char*)&oExchModReq.NnfField);
  
  oExchModReq.OrderNumber       = vOrderBook[ OdrBkIdx ].llExchOrderId;
  SwapDouble((char*)&oExchModReq.OrderNumber);
  memcpy(oExchModReq.contract_desc_tr.InstrumentName, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName, 6);
  memcpy(oExchModReq.contract_desc_tr.OptionType, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.OptionType, 2);
  memcpy(oExchModReq.contract_desc_tr.Symbol, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.Symbol, 10);
  
  oExchModReq.TransactionCode                             = __bswap_16(NSECM_MOD_REQ_TR);
  oExchModReq.TokenNo                                     = __bswap_32(vOrderBook[ OdrBkIdx ].iToken);
  oExchModReq.contract_desc_tr.ExpiryDate                 = __bswap_32(vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.ExpiryDate);
  oExchModReq.contract_desc_tr.StrikePrice                = __bswap_32(-1);
  oExchModReq.BuySellIndicator                            = __bswap_16(vOrderBook[ OdrBkIdx ].BuySellIndicator);
  oExchModReq.Volume                                      = __bswap_32(vOrderBook[ OdrBkIdx ].iQty);

  oExchModReq.Price                                       = __bswap_32(vOrderBook[ OdrBkIdx ].iPrice);

  oExchModReq.DisclosedVolume                             = oExchModReq.Volume;
  oExchModReq.filler                                      = vOrderBook[ OdrBkIdx ].iIntOrdId;     
  oExchModReq.tap_hdr.iSeqNo                              = __bswap_32(++iExchSendSeqNo);
  oExchModReq.LastModified                                = __bswap_32(vOrderBook[ OdrBkIdx ].LMT);
  oTcp->SendData( iSockIdFO,(char*)&oExchModReq, sizeof(NSEFO::MS_OM_REQUEST_TR),gpConnFO);
  
  snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|MOD ORDER REQ|Exch %ld|Order# %d|Price::%d|Volume::%d|Token %d|B/S %d|UserId %d|Size %d|%d", 
      iSockIdFO, vOrderBook[ OdrBkIdx ].llExchOrderId, oExchModReq.filler, __bswap_32(oExchModReq.Price), __bswap_32(oExchModReq.Volume), __bswap_32(oExchModReq.TokenNo),__bswap_16(oExchModReq.BuySellIndicator), __bswap_32(oExchModReq.TraderId),__bswap_16(oExchModReq.tap_hdr.sLength), sizeof(NSEFO::MS_OM_REQUEST_TR) );
  Logger::getLogger().log(DEBUG, chLogBuff);
  
}

bool ModOrderUI(DEALER::GENERIC_ORD_MSG* pModOrdReq, int iFd, dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap, CONNINFO* pConn)
{
  bool ret = true;
  int iIndex = pModOrdReq->iIntOrdID - 1;
  DEALER::GENERIC_ORD_MSG oModOrdRes;
  memcpy(&oModOrdRes.header,&pModOrdReq->header,sizeof(oModOrdRes.header));
  
  oModOrdRes.header.iTransCode     = UNSOLICITED_ORDER_RES;
  
  oModOrdRes.BuySellIndicator      = pModOrdReq->BuySellIndicator;
  oModOrdRes.iPrice                = pModOrdReq->iPrice;
  oModOrdRes.iQty                  = pModOrdReq->iQty;
  oModOrdRes.iTokenNo              = pModOrdReq->iTokenNo;
  oModOrdRes.iIntOrdID             = pModOrdReq->iIntOrdID;
  oModOrdRes.iExchOrdID            = vOrderBook[iIndex].llExchOrderId;
  oModOrdRes.iTTQ                  = vOrderBook[iIndex].iTotTrdQty;      
  int16_t actionable = (int16_t)Actionable::FALSE;
  if(vOrderBook[iIndex].isActionable == actionable) 
  {
    oModOrdRes.header.iErrorCode   = CheckStatusErrorCode(iIndex);
    oModOrdRes.isActionable        = actionable;
    oModOrdRes.iOrdStatus          = vOrderBook[iIndex].iStatus;
    oTcp->SendData(iFd, (char *)&oModOrdRes, oModOrdRes.header.iMsgLen, pConn);
    
    sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|MOD ORDER REQ|NOT ACTIONABLE|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d|B/S %d|Token %d|B/S %d|Status %d|Actionable %d", oModOrdRes.header.iDealerID, oModOrdRes.header.iMsgLen,
            oModOrdRes.header.iErrorCode, oModOrdRes.header.iTransCode,oModOrdRes.header.iSeqNo,oModOrdRes.iIntOrdID,oModOrdRes.iQty,oModOrdRes.iPrice,oModOrdRes.BuySellIndicator,oModOrdRes.iTokenNo,oModOrdRes.BuySellIndicator,oModOrdRes.iOrdStatus,oModOrdRes.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
    
    return false;
  }
  
  oModOrdRes.header.iErrorCode   = ValidateGenOrder(pModOrdReq, DealInfoMap, ContInfoMap);
  
  if(oModOrdRes.header.iErrorCode != 0)
  {
    oModOrdRes.iOrdStatus         = OPEN;
    oModOrdRes.isActionable       = (int16_t)Actionable::TRUE;
    
    vOrderBook[iIndex].iStatus      = oModOrdRes.iOrdStatus;
    vOrderBook[iIndex].isActionable = oModOrdRes.isActionable;
    
    oTcp->SendData(iFd, (char *)&oModOrdRes, oModOrdRes.header.iMsgLen, pConn);
    
    sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|MOD ORDER REQ|ERROR|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d| Token %d|B/S %d|Status %d|Actionable %d ", oModOrdRes.header.iDealerID, oModOrdRes.header.iMsgLen,
            oModOrdRes.header.iErrorCode, oModOrdRes.header.iTransCode,oModOrdRes.header.iSeqNo,oModOrdRes.iIntOrdID,oModOrdRes.iQty,oModOrdRes.iPrice,oModOrdRes.iTokenNo,oModOrdRes.BuySellIndicator,oModOrdRes.iOrdStatus,oModOrdRes.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
    
    return false;
  }
  
  oModOrdRes.iOrdStatus           = MOD_PENDING;
  oModOrdRes.isActionable         =(int16_t)Actionable::FALSE;
  
  vOrderBook[iIndex].iStatus      = oModOrdRes.iOrdStatus;
  vOrderBook[iIndex].isActionable = oModOrdRes.isActionable;
  
  oTcp->SendData(iFd, (char *)&oModOrdRes, oModOrdRes.header.iMsgLen, pConn);
  sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|MOD ORDER REQ|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d|B/S %d|Token %d|B/S %d|Status %d|Actionable %d ", oModOrdRes.header.iDealerID, oModOrdRes.header.iMsgLen,
            oModOrdRes.header.iErrorCode, oModOrdRes.header.iTransCode,oModOrdRes.header.iSeqNo,oModOrdRes.iIntOrdID,oModOrdRes.iQty,oModOrdRes.iPrice,oModOrdRes.BuySellIndicator,oModOrdRes.iTokenNo,oModOrdRes.BuySellIndicator,oModOrdRes.iOrdStatus,oModOrdRes.isActionable);
  Logger::getLogger().log(DEBUG, chLogBuff);
    
  
  
  
  if(vOrderBook.size() >= iIndex && vOrderBook[iIndex].iIntOrdId == (iIndex + 1) )
  {
    vOrderBook[iIndex].iPrevPrice            = vOrderBook[iIndex].iPrice;
    vOrderBook[iIndex].iPrevQty              = vOrderBook[iIndex].iQty;

    vOrderBook[iIndex].iPrice                = oModOrdRes.iPrice;
    vOrderBook[iIndex].iQty                  = oModOrdRes.iQty;
    vOrderBook[iIndex].BuySellIndicator      = oModOrdRes.BuySellIndicator;
    vOrderBook[iIndex].iDQty                 = oModOrdRes.iDisclosedVolume;

    vOrderBook[iIndex].iTransactionCode      = oModOrdRes.header.iTransCode;
    vOrderBook[iIndex].iSeqNo                = oModOrdRes.header.iSeqNo;
    vOrderBook[iIndex].iErrorCode            = oModOrdRes.header.iErrorCode;
    
    snprintf(chLogBuff, 500, "MAIN|MOD ORDER BOOK|Mod Successful|Order Id %d|%d|Order Book Size %d",iIndex+1,vOrderBook[iIndex].iIntOrdId,vOrderBook.size());
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|MOD ORDER BOOK|Wrong memory access for Order %d|%d|Order Book Size %d",iIndex+1,vOrderBook[iIndex].iIntOrdId,vOrderBook.size());
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
  return ret;
}

int CanOrderTrimRespFO(NSEFO::MS_OE_RESPONSE_TR *pOrderRespExch,connectionMap& ConnMap)
{
  DEALER::GENERIC_ORD_MSG oOrderResp;
  CONNINFO *pConn;
  
  int32_t InternalOrderId =  pOrderRespExch->filler;
  int OrdBkIdx = InternalOrderId - 1; 
  
  vOrderBook[OrdBkIdx].LMT          = __bswap_32(pOrderRespExch->LastModified);
  if(__bswap_16(pOrderRespExch->ErrorCode) != 0)
  {
    oOrderResp.iOrdStatus             = OPEN;
    oOrderResp.isActionable           = (int16_t)Actionable::TRUE;
  }
  else
  {
    oOrderResp.iOrdStatus             = CANCELLED;
    oOrderResp.isActionable           = (int16_t)Actionable::FALSE;
  }
  vOrderBook[OrdBkIdx].iStatus      = oOrderResp.iOrdStatus;
  vOrderBook[OrdBkIdx].isActionable = oOrderResp.isActionable;
  
  
  double dEOrd                            = pOrderRespExch->OrderNumber;
  SwapDouble((char*)&dEOrd);
  
  vOrderBook[OrdBkIdx].llExchOrderId      = (int64_t)dEOrd;
  vOrderBook[OrdBkIdx].llTimeStamp        = __bswap_64(pOrderRespExch->Timestamp1);
  
  oOrderResp.header.llTimeStamp           = (vOrderBook[OrdBkIdx].llTimeStamp)/1000000000;
  oOrderResp.header.iDealerID             = vOrderBook[OrdBkIdx].iDealerID;
  oOrderResp.header.iErrorCode            = __bswap_16(pOrderRespExch->ErrorCode);
  oOrderResp.header.iMsgLen               = sizeof(oOrderResp);
  oOrderResp.header.iSeqNo                = vOrderBook[OrdBkIdx].iSeqNo;
  oOrderResp.header.iSeg                  = vOrderBook[OrdBkIdx].iSeg;
  oOrderResp.header.iTransCode            = UNSOLICITED_ORDER_RES;
  
  oOrderResp.BuySellIndicator             = vOrderBook[OrdBkIdx].BuySellIndicator;
  oOrderResp.iOrdStatus                   = vOrderBook[OrdBkIdx].iStatus;
  oOrderResp.iIntOrdID                    = pOrderRespExch->filler;
  oOrderResp.iTokenNo                     = vOrderBook[OrdBkIdx].iToken;
  oOrderResp.iPrice                       = vOrderBook[OrdBkIdx].iPrice;
  oOrderResp.iQty                         = vOrderBook[OrdBkIdx].iQty;
  oOrderResp.iExchOrdID                   = dEOrd;
  oOrderResp.iDisclosedVolume             = vOrderBook[OrdBkIdx].iDQRemaining;
  oOrderResp.iTTQ                         = vOrderBook[OrdBkIdx].iTotTrdQty;
  
  int sockFd = vOrderBook[OrdBkIdx].iFd;
  auto it = ConnMap.find(sockFd);
  if(it != ConnMap.end())
  {
    pConn = it->second;
    oTcp->SendData(sockFd,(char *)&oOrderResp, oOrderResp.header.iMsgLen,pConn);
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|CAN ORDER RES|ExchOrder %ld|COrd %d|ErrorCode %d|Token %d|Exch DealerID %d|UI DealerId %d|Status %d|Actionable %d ", sockFd, (int64_t)dEOrd, InternalOrderId,
       oOrderResp.header.iErrorCode, oOrderResp.iTokenNo, __bswap_32(pOrderRespExch->UserId), oOrderResp.header.iDealerID ,oOrderResp.iOrdStatus,oOrderResp.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|CAN ORDER RES|Fd not found|ExchOrder %ld|COrd %d|ErrorCode %d|Token %d|N3 DealerID %d|UI UserId %d", sockFd,
      (int64_t)dEOrd, InternalOrderId,oOrderResp.header.iErrorCode, __bswap_32(pOrderRespExch->TokenNo), __bswap_32(pOrderRespExch->UserId), oOrderResp.header.iDealerID);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
}

void CanOrderTrimReqFO(ConfigFile& oConfigdata,int iSockIdFO,int32_t OrderId)
{
  
  int OdrBkIdx = OrderId - 1;
//  PrintOrderBook(OdrBkIdx);
  
  
  NSEFO::MS_OM_REQUEST_TR oExchCanReq(oConfigdata.iUserID_FO, oConfigdata.sTMID_FO, oConfigdata.sBranchId_FO, 1);

  memset(oExchCanReq.AccountNumber, ' ', 10);
  memcpy(oExchCanReq.AccountNumber, "AAK",3);
  oExchCanReq.OrderFlags.IOC   = 0;
  oExchCanReq.OrderFlags.Day   = 1;
  oExchCanReq.OrderFlags.SL    = 0;
  oExchCanReq.NnfField         = oConfigdata.iNNF_FO;
  SwapDouble((char*)&oExchCanReq.NnfField);
  
  oExchCanReq.OrderNumber       = vOrderBook[ OdrBkIdx ].llExchOrderId;
  SwapDouble((char*)&oExchCanReq.OrderNumber);
  memcpy(oExchCanReq.contract_desc_tr.InstrumentName, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName, 6);
  memcpy(oExchCanReq.contract_desc_tr.OptionType, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.OptionType, 2);
  memcpy(oExchCanReq.contract_desc_tr.Symbol, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.Symbol, 10);
  
  oExchCanReq.TransactionCode                             = __bswap_16(NSECM_CAN_REQ_TR);
  oExchCanReq.TokenNo                                     = __bswap_32(vOrderBook[ OdrBkIdx ].iToken);
  oExchCanReq.contract_desc_tr.ExpiryDate                 = __bswap_32(vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.ExpiryDate);
  oExchCanReq.contract_desc_tr.StrikePrice                = __bswap_32(-1);
  oExchCanReq.BuySellIndicator                            = __bswap_16(vOrderBook[ OdrBkIdx ].BuySellIndicator);
  oExchCanReq.Volume                                      = __bswap_32(vOrderBook[ OdrBkIdx ].iQty);

  oExchCanReq.Price                                       = __bswap_32(vOrderBook[ OdrBkIdx ].iPrice);

  oExchCanReq.DisclosedVolume                             = oExchCanReq.Volume;
  oExchCanReq.filler                                      = vOrderBook[ OdrBkIdx ].iIntOrdId;     
  oExchCanReq.tap_hdr.iSeqNo                              = __bswap_32(++iExchSendSeqNo);
  oExchCanReq.LastModified                                = __bswap_32(vOrderBook[ OdrBkIdx ].LMT);
  
  oTcp->SendData( iSockIdFO,(char*)&oExchCanReq.tap_hdr.sLength, sizeof(NSEFO::MS_OM_REQUEST_TR),gpConnFO);
  
  snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|CAN ORDER REQ|Exch %ld|Order# %d|Price::%d|Volume::%d| Token %d|UserId %d", 
      iSockIdFO, vOrderBook[ OdrBkIdx ].llExchOrderId, oExchCanReq.filler, __bswap_32(oExchCanReq.Price), __bswap_32(oExchCanReq.Volume), __bswap_32(oExchCanReq.TokenNo), __bswap_32(oExchCanReq.TraderId) );
  Logger::getLogger().log(DEBUG, chLogBuff);
  
}

bool CanOrderUI(DEALER::GENERIC_ORD_MSG* pCanOrdReq, int iFd, dealerInfoMap& DealInfoMap,ContractInfoMap& ContInfoMap, CONNINFO* pConn)
{
  bool ret = true;
  int32_t iIndex = pCanOrdReq->iIntOrdID - 1;
  DEALER::GENERIC_ORD_MSG oCanOrdRes;
  memcpy(&oCanOrdRes.header,&pCanOrdReq->header,sizeof(oCanOrdRes.header));
  
  oCanOrdRes.header.iTransCode     = UNSOLICITED_ORDER_RES;
  oCanOrdRes.BuySellIndicator      = pCanOrdReq->BuySellIndicator;
  oCanOrdRes.iPrice                = pCanOrdReq->iPrice;
  oCanOrdRes.iQty                  = pCanOrdReq->iQty;
  oCanOrdRes.iTokenNo              = pCanOrdReq->iTokenNo;
  oCanOrdRes.iIntOrdID             = pCanOrdReq->iIntOrdID;
  oCanOrdRes.iExchOrdID            = vOrderBook[iIndex].llExchOrderId;
  oCanOrdRes.iTTQ                  = vOrderBook[iIndex].iTotTrdQty;      
  int16_t actionable = (int16_t)Actionable::FALSE;
  if(vOrderBook[iIndex].isActionable == actionable)
  {
    oCanOrdRes.header.iErrorCode   = CheckStatusErrorCode(iIndex);

    oCanOrdRes.isActionable        = actionable;
    oCanOrdRes.iOrdStatus          = vOrderBook[iIndex].iStatus;
    oTcp->SendData(iFd, (char *)&oCanOrdRes, oCanOrdRes.header.iMsgLen, pConn);
    
    sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|CAN ORDER REQ|NOT ACTIONABLE|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d|Token %d|B/S %d|Status %d|Actionable %d ", oCanOrdRes.header.iDealerID, oCanOrdRes.header.iMsgLen,
            oCanOrdRes.header.iErrorCode, oCanOrdRes.header.iTransCode,oCanOrdRes.header.iSeqNo,oCanOrdRes.iIntOrdID,oCanOrdRes.iQty,oCanOrdRes.iPrice,oCanOrdRes.iTokenNo,oCanOrdRes.BuySellIndicator,oCanOrdRes.iOrdStatus,oCanOrdRes.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
    
    return false;
  }
  
  oCanOrdRes.header.iErrorCode   = ValidateDealer(pCanOrdReq->header.iDealerID,DealInfoMap);
  
  if(oCanOrdRes.header.iErrorCode != 0)
  {
    oCanOrdRes.iOrdStatus             = OPEN;
    oCanOrdRes.isActionable           = (int16_t)Actionable::TRUE;
    vOrderBook[iIndex].iStatus        = oCanOrdRes.iOrdStatus;
    vOrderBook[iIndex].isActionable   = oCanOrdRes.isActionable;
    oTcp->SendData(iFd, (char *)&oCanOrdRes, oCanOrdRes.header.iMsgLen, pConn);
    
    sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|CAN ORDER REQ|ERROR|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d| Token %d|B/S %d|Status %d|Actionable %d  ", oCanOrdRes.header.iDealerID, oCanOrdRes.header.iMsgLen,
            oCanOrdRes.header.iErrorCode, oCanOrdRes.header.iTransCode,oCanOrdRes.header.iSeqNo,oCanOrdRes.iIntOrdID,oCanOrdRes.iQty,oCanOrdRes.iPrice,oCanOrdRes.iTokenNo,oCanOrdRes.BuySellIndicator,oCanOrdRes.iOrdStatus,oCanOrdRes.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
    
    return false;
  }
  
  oCanOrdRes.iOrdStatus             = CAN_PENDING;
  oCanOrdRes.isActionable           = (int16_t)Actionable::FALSE;
  
  vOrderBook[iIndex].iStatus        = oCanOrdRes.iOrdStatus;
  vOrderBook[iIndex].isActionable   = oCanOrdRes.isActionable;
  
  oTcp->SendData(iFd, (char *)&oCanOrdRes, oCanOrdRes.header.iMsgLen, pConn);
  sprintf(chLogBuff,"MAIN|UI_DEALER|NSEFO|CAN ORDER REQ|Dealer %d|iMsgLen %d|ErrorCode %d|Transcode %d|SeqNo %d|Order Id %d|Qty %d|Price %d| Token %d|B/S %d|Status %d|Actionable %d ", oCanOrdRes.header.iDealerID, oCanOrdRes.header.iMsgLen,
            oCanOrdRes.header.iErrorCode, oCanOrdRes.header.iTransCode,oCanOrdRes.header.iSeqNo,oCanOrdRes.iIntOrdID,oCanOrdRes.iQty,oCanOrdRes.iPrice,oCanOrdRes.iTokenNo,oCanOrdRes.BuySellIndicator,oCanOrdRes.iOrdStatus,oCanOrdRes.isActionable);
  Logger::getLogger().log(DEBUG, chLogBuff);
  
  
  if(vOrderBook.size() >= iIndex && vOrderBook[iIndex].iIntOrdId == (iIndex + 1) )
  {
    vOrderBook[iIndex].iTransactionCode      = oCanOrdRes.header.iTransCode;
    vOrderBook[iIndex].iSeqNo                = oCanOrdRes.header.iSeqNo;
    vOrderBook[iIndex].iErrorCode            = oCanOrdRes.header.iErrorCode;
    
    snprintf(chLogBuff, 500, "MAIN|CAN ORDER BOOK|CAN SUCCESSFUL|Order Id %d|%d|Order Book Size %d",iIndex+1,vOrderBook[iIndex].iIntOrdId,vOrderBook.size());
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|CAN ORDER BOOK|Wrong memory access for Order %d|%d|Order Book Size %d",iIndex+1,vOrderBook[iIndex].iIntOrdId,vOrderBook.size());
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
  return ret;
}

int TradeOrderRespFO(NSEFO::TRADE_CONFIRMATION_TR* pTradeExch,connectionMap& ConnMap)
{
  DEALER::TRD_MSG oTrdMsg;
  CONNINFO *pConn;
  
  double dEOrd = pTradeExch->ResponseOrderNumber;
  SwapDouble((char*)&dEOrd);
  
  int32_t iIntrnlOrdId;
  auto iter = mExOdrIdToSysOrdId.find(dEOrd);
  if(iter != mExOdrIdToSysOrdId.end())
  {
    iIntrnlOrdId = iter->second;
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|TRD ORDER RES|ExchOrder %ld|Internal Order Id Not Found", (int64_t)dEOrd);
    Logger::getLogger().log(DEBUG, chLogBuff);
    return 0;
  }
  
  int32_t OrdBkIdx = iIntrnlOrdId - 1;
  
  vOrderBook[OrdBkIdx].llExchOrderId  = (int64_t)dEOrd;
  vOrderBook[OrdBkIdx].llTimeStamp    = __bswap_64(pTradeExch->Timestamp1);
  vOrderBook[OrdBkIdx].LMT            = __bswap_32(pTradeExch->ActivityTime);
  
  oTrdMsg.header.llTimeStamp           = (vOrderBook[OrdBkIdx].llTimeStamp)/1000000000;
  oTrdMsg.header.iDealerID             = vOrderBook[OrdBkIdx].iDealerID;
  oTrdMsg.header.iErrorCode            = 0;
  oTrdMsg.header.iMsgLen               = sizeof(oTrdMsg);
  oTrdMsg.header.iSeqNo                = vOrderBook[OrdBkIdx].iSeqNo;
  oTrdMsg.header.iSeg                  = 0;
  oTrdMsg.header.iTransCode            = TRADE_ORDER_RES;
  
  oTrdMsg.iExchOrdID                   = pTradeExch->ResponseOrderNumber;
  oTrdMsg.iTTQ                         = __bswap_32(pTradeExch->VolumeFilledToday);
  oTrdMsg.iIntOrdID                    = iIntrnlOrdId;
  oTrdMsg.iTokenNo                     = vOrderBook[OrdBkIdx].iToken;
  oTrdMsg.iTradePrice                  = __bswap_32( pTradeExch->FillPrice);
  oTrdMsg.iTradeQty                    = __bswap_32( pTradeExch->FillQuantity );
  oTrdMsg.iExchOrdID                   = dEOrd;
  oTrdMsg.BuySellIndicator             = __bswap_16(pTradeExch->BuySellIndicator);
  oTrdMsg.iTradeId                     = __bswap_32(pTradeExch->FillNumber);
  oTrdMsg.iQtyRemaining                = __bswap_32(pTradeExch->RemainingVolume);
  
  vOrderBook[OrdBkIdx].iTotTrdQty   = oTrdMsg.iTTQ;
  vOrderBook[OrdBkIdx].iQty         = oTrdMsg.iQtyRemaining;
  
  if(oTrdMsg.iQtyRemaining == 0)
  {
    oTrdMsg.iOrdStatus                = COMPLETELY_FILLED;
    oTrdMsg.isActionable              = (int16_t)Actionable::FALSE;
    
    vOrderBook[OrdBkIdx].iStatus      = oTrdMsg.iOrdStatus;
    vOrderBook[OrdBkIdx].isActionable = oTrdMsg.isActionable;
  }
  else
  {
    oTrdMsg.iOrdStatus                = OPEN;
    oTrdMsg.isActionable              = (int16_t)Actionable::TRUE;
    
    vOrderBook[OrdBkIdx].iStatus      = oTrdMsg.iOrdStatus;
    vOrderBook[OrdBkIdx].isActionable = oTrdMsg.isActionable;
  }
  
  int sockFd = vOrderBook[OrdBkIdx].iFd;
  auto it = ConnMap.find(sockFd);
  if(it != ConnMap.end())
  {
    pConn = it->second;
    oTcp->SendData(sockFd,(char *)&oTrdMsg, oTrdMsg.header.iMsgLen,pConn);
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|TRD ORDER RES|ExchOrder %ld|COrd %d|TrdId %d|TrdedPrice %d|TrdedQty %d|QtyRem %d|Token %d|Exch DealerID %d|UI DealerId %d|Status %d|Actionable %d ", sockFd, (int64_t)dEOrd, iIntrnlOrdId, oTrdMsg.iTradeId, oTrdMsg.iTradePrice,
      oTrdMsg.iTradeQty,oTrdMsg.iQtyRemaining, oTrdMsg.iTokenNo,__bswap_32(pTradeExch->UserId), oTrdMsg.header.iDealerID,oTrdMsg.iOrdStatus,oTrdMsg.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  else
  {
    snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|TRD ORDER RES|ExchOrder %ld|COrd %d|Fd not found|TrdId %d|TrdedPrice %d|TrdedQty %d|QtyRem %d|Token %d|Exch DealerID %d|UI DealerId %d", sockFd, (int64_t)dEOrd, iIntrnlOrdId, oTrdMsg.iTradeId, oTrdMsg.iTradePrice, oTrdMsg.iTradeQty,oTrdMsg.iQtyRemaining, oTrdMsg.iTokenNo,__bswap_32(pTradeExch->UserId), oTrdMsg.header.iDealerID);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
  DEALER::GENERIC_ORD_MSG oGenOrdRes;
  
  oGenOrdRes.header.llTimeStamp           = (vOrderBook[OrdBkIdx].llTimeStamp)/1000000000;
  oGenOrdRes.header.iDealerID             = vOrderBook[OrdBkIdx].iDealerID;
  oGenOrdRes.header.iMsgLen               = sizeof(oGenOrdRes);
  oGenOrdRes.header.iSeqNo                = vOrderBook[OrdBkIdx].iSeqNo;
  oGenOrdRes.header.iSeg                  = SEGMENT_IDENTIFIER::SEG_NSEFO;
  oGenOrdRes.header.iTransCode            = UNSOLICITED_ORDER_RES;
  oGenOrdRes.header.iErrorCode            = 0;
  
  oGenOrdRes.iOrdStatus         = oTrdMsg.iOrdStatus;
  oGenOrdRes.iIntOrdID          = oTrdMsg.iIntOrdID;
  oGenOrdRes.BuySellIndicator   = oTrdMsg.BuySellIndicator;
  oGenOrdRes.iPrice             = vOrderBook[OrdBkIdx].iPrice;
  oGenOrdRes.iQty               = vOrderBook[OrdBkIdx].iQty;
  oGenOrdRes.iTTQ               = oTrdMsg.iTTQ;
  oGenOrdRes.iTokenNo           = vOrderBook[OrdBkIdx].iToken;
  oGenOrdRes.isActionable       = oTrdMsg.isActionable;
  
  oTcp->SendData(sockFd, (char *)&oGenOrdRes, oGenOrdRes.header.iMsgLen, pConn);
  
  
  mUserIdToTrdStore[oTrdMsg.header.iDealerID].push_back(oTrdMsg); //User Id Order info stored for Trade Download Msg
  
  
}


void OrdDwnldSend(DATA_RECEIVED& RecvData,DEALER::ORDER_DOWNLOAD_REQ* psOdrDwnldReq)
{
  DEALER::ORDER_DOWNLOAD oOrdDwnld;
  memcpy(&oOrdDwnld.GenOrd.header,&psOdrDwnldReq->header,sizeof(DEALER::MSG_HEADER));
  oOrdDwnld.GenOrd.header.iMsgLen     = sizeof(oOrdDwnld);
  oOrdDwnld.GenOrd.header.iTransCode  = ORDER_DOWNLOAD;
  
  int32_t iDealerId = psOdrDwnldReq->header.iDealerID;
  
  auto it = mUserIdToOrderId.find(iDealerId);
  
  if(it != mUserIdToOrderId.end())
  {

    vOrderId& vOrderIdStore = it->second;
    int iNoOfOrder = it->second.size();
    for( int i = 0 ; i < iNoOfOrder ; i++ )
    {
      int iIndex = vOrderIdStore[i] - 1;
      oOrdDwnld.GenOrd.iIntOrdID        = vOrderIdStore[i];
      oOrdDwnld.GenOrd.BuySellIndicator = vOrderBook[iIndex].BuySellIndicator;
      oOrdDwnld.GenOrd.iTokenNo         = vOrderBook[iIndex].iToken;
      oOrdDwnld.GenOrd.iExchOrdID       = vOrderBook[iIndex].llExchOrderId;
      oOrdDwnld.GenOrd.iPrice           = vOrderBook[iIndex].iPrice;
      oOrdDwnld.GenOrd.iQty             = vOrderBook[iIndex].iQty;
      oOrdDwnld.GenOrd.iOrdStatus       = vOrderBook[iIndex].iStatus;
      oOrdDwnld.GenOrd.isActionable     = vOrderBook[iIndex].isActionable;
      oOrdDwnld.GenOrd.iDisclosedVolume = vOrderBook[iIndex].iDQty;
      oOrdDwnld.GenOrd.iTTQ             = vOrderBook[iIndex].iTotTrdQty;
      
      oTcp->SendData(RecvData.MyFd, (char *)&oOrdDwnld, oOrdDwnld.GenOrd.header.iMsgLen,RecvData.ptrConnInfo);
      
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|ORDER DOWNLOAD|DealerID %d|ExchOrder %ld|COrd %d|Token %d|TTQ %d|Status %d|Actionable %d ", RecvData.MyFd,oOrdDwnld.GenOrd.header.iDealerID, (int64_t)oOrdDwnld.GenOrd.iExchOrdID, oOrdDwnld.GenOrd.iIntOrdID,
      oOrdDwnld.GenOrd.iTokenNo,oOrdDwnld.GenOrd.iTTQ, oOrdDwnld.GenOrd.iOrdStatus,oOrdDwnld.GenOrd.isActionable);
    Logger::getLogger().log(DEBUG, chLogBuff);
    }
  }
  else
  {
    sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|No data found for UserId %d ",RecvData.MyFd,iDealerId);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
  
}


void TrdDwnldSend(DATA_RECEIVED& RecvData,DEALER::TRADE_DOWNLOAD_REQ* psTrdDwnldReq)
{
  DEALER::TRADE_DOWNLOAD oTrdDwnldMsg;
  memcpy(&oTrdDwnldMsg.TradeStore.header,&psTrdDwnldReq->header,sizeof(DEALER::MSG_HEADER));
  oTrdDwnldMsg.TradeStore.header.iMsgLen   = sizeof(oTrdDwnldMsg);
  oTrdDwnldMsg.TradeStore.header.iTransCode= TRADE_DOWNLOAD;
  
  int32_t iDealerId = psTrdDwnldReq->header.iDealerID;
  
  auto it = mUserIdToTrdStore.find(iDealerId);
  
  if( it != mUserIdToTrdStore.end())
  {
    vTrdStore& oTrdDwnld = it->second;
    int iNoOfMsgs = it->second.size();
    
    for( int i = 0 ; i < iNoOfMsgs ; i++ )
    {
      oTrdDwnldMsg.TradeStore.iTokenNo         = oTrdDwnld[i].iTokenNo;
      oTrdDwnldMsg.TradeStore.BuySellIndicator = oTrdDwnld[i].BuySellIndicator;
      oTrdDwnldMsg.TradeStore.iExchOrdID       = oTrdDwnld[i].iExchOrdID;
      oTrdDwnldMsg.TradeStore.iIntOrdID        = oTrdDwnld[i].iIntOrdID;
      oTrdDwnldMsg.TradeStore.iTTQ             = oTrdDwnld[i].iTTQ;
      oTrdDwnldMsg.TradeStore.iTradeQty        = oTrdDwnld[i].iTradeQty;
      oTrdDwnldMsg.TradeStore.iTradePrice      = oTrdDwnld[i].iTradePrice;
      oTrdDwnldMsg.TradeStore.iOrdStatus       = oTrdDwnld[i].iOrdStatus;
      oTrdDwnldMsg.TradeStore.isActionable     = oTrdDwnld[i].isActionable;      
      oTrdDwnldMsg.TradeStore.iTradeId         = oTrdDwnld[i].iTradeId;      
      oTrdDwnldMsg.TradeStore.iQtyRemaining    = oTrdDwnld[i].iQtyRemaining;      
      oTrdDwnldMsg.TradeStore.iTimeStamp       = oTrdDwnld[i].iTimeStamp;      
      
      oTcp->SendData(RecvData.MyFd, (char *)&oTrdDwnldMsg, oTrdDwnldMsg.TradeStore.header.iMsgLen,RecvData.ptrConnInfo);
      
      snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|TRADE DOWNLOAD|UI DealerId %d|TrdId %d|ExchOrder %ld|COrd %d|TrdedPrice %d|TrdedQty %d|QtyRem %d|Token %d|TTQ %d|Status %d|Actionable %d ", RecvData.MyFd, oTrdDwnld[i].header.iDealerID, oTrdDwnld[i].iTradeId, (int64_t)oTrdDwnld[i].iExchOrdID, (int64_t)oTrdDwnld[i].iIntOrdID, oTrdDwnld[i].iTradePrice,
              oTrdDwnld[i].iTradeQty,oTrdDwnld[i].iQtyRemaining, oTrdDwnld[i].iTokenNo, oTrdDwnld[i].iTTQ,oTrdDwnld[i].iOrdStatus,oTrdDwnld[i].isActionable);
      Logger::getLogger().log(DEBUG, chLogBuff);

    }
  }
  else
  {
    sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|No data found for UserId %d ",RecvData.MyFd,iDealerId);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
  
  
}

void CanOnLogOut(int32_t iDealerID,int iSockIdFO, int iSockIdCM, ConfigFile& oConfigdata)
{
  auto it = mUserIdToOrderId.find(iDealerID);
  if(it != mUserIdToOrderId.end())
  {
    vOrderId& vOrderIdStore = it->second;
    int iNoOfOrder = it->second.size();
    for( int i = 0 ; i < iNoOfOrder ; i++ )
    {
      int OdrBkIdx = vOrderIdStore[i] - 1;
      
      if(vOrderBook[OdrBkIdx].iStatus != REJECTED && vOrderBook[OdrBkIdx].iStatus != COMPLETELY_FILLED && vOrderBook[OdrBkIdx].iStatus != CANCELED && vOrderBook[OdrBkIdx].iStatus != CAN_PENDING)
      {
        switch(vOrderBook[OdrBkIdx].iSeg)
        {
          case SEG_NSEFO:
          {
            NSEFO::MS_OM_REQUEST_TR oExchCanReq(oConfigdata.iUserID_FO, oConfigdata.sTMID_FO, oConfigdata.sBranchId_FO, 1);

            memset(oExchCanReq.AccountNumber, ' ', 10);
            memcpy(oExchCanReq.AccountNumber, "AAK",3);
            oExchCanReq.OrderFlags.IOC   = 0;
            oExchCanReq.OrderFlags.Day   = 1;
            oExchCanReq.OrderFlags.SL    = 0;
            oExchCanReq.NnfField         = oConfigdata.iNNF_FO;
            SwapDouble((char*)&oExchCanReq.NnfField);

            oExchCanReq.OrderNumber       = vOrderBook[ OdrBkIdx ].llExchOrderId;
            SwapDouble((char*)&oExchCanReq.OrderNumber);
            memcpy(oExchCanReq.contract_desc_tr.InstrumentName, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.InstrumentName, 6);
            memcpy(oExchCanReq.contract_desc_tr.OptionType, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.OptionType, 2);
            memcpy(oExchCanReq.contract_desc_tr.Symbol, vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.Symbol, 10);

            oExchCanReq.TransactionCode                             = __bswap_16(NSECM_CAN_REQ_TR);
            oExchCanReq.TokenNo                                     = __bswap_32(vOrderBook[ OdrBkIdx ].iToken);
            oExchCanReq.contract_desc_tr.ExpiryDate                 = __bswap_32(vOrderBook[ OdrBkIdx ].nsecm_nsefo.NSEFO.contract_desc_tr.ExpiryDate);
            oExchCanReq.contract_desc_tr.StrikePrice                = __bswap_32(-1);
            oExchCanReq.BuySellIndicator                            = __bswap_16(vOrderBook[ OdrBkIdx ].BuySellIndicator);
            oExchCanReq.Volume                                      = __bswap_32(vOrderBook[ OdrBkIdx ].iQty);

            oExchCanReq.Price                                       = __bswap_32(vOrderBook[ OdrBkIdx ].iPrice);

            oExchCanReq.DisclosedVolume                             = oExchCanReq.Volume;
            oExchCanReq.filler                                      = vOrderBook[ OdrBkIdx ].iIntOrdId;     
            oExchCanReq.tap_hdr.iSeqNo                              = __bswap_32(++iExchSendSeqNo);
            oExchCanReq.LastModified                                = __bswap_32(vOrderBook[ OdrBkIdx ].LMT);

            vOrderBook[OdrBkIdx].iStatus                            = CAN_PENDING;
            
            oTcp->SendData( iSockIdFO,(char*)&oExchCanReq.tap_hdr.sLength, sizeof(NSEFO::MS_OM_REQUEST_TR),gpConnFO);

            snprintf(chLogBuff, 500, "MAIN|NSEFO|FD %d|COL|UI Id %d|Exch %ld|Order# %d|Price::%d|Volume::%d| Token %d|UserId %d", 
                iSockIdFO, iDealerID, vOrderBook[ OdrBkIdx ].llExchOrderId, oExchCanReq.filler, __bswap_32(oExchCanReq.Price), __bswap_32(oExchCanReq.Volume), __bswap_32(oExchCanReq.TokenNo), __bswap_32(oExchCanReq.TraderId) );
            Logger::getLogger().log(DEBUG, chLogBuff);    
          }
          break;

          case SEG_NSECM:
          {
            
          }
          break;
          
          default:
            break;
        }
      }
      
    }
  }
  else
  {
    sprintf(chLogBuff, "MAIN|UI_DEALER|UserId %d|No Data for COL",iDealerID);
    Logger::getLogger().log(DEBUG, chLogBuff);
  }
}

void SendContractInfoDownload(DATA_RECEIVED& RecvData,DEALER::LOG_IN_RES& sLogInResp, ContractInfoMap& ContInfoMap, TOKENSUBSCRIPTION& oTokenSub)
{
  DEALER::CONTRACT_DOWNLOAD_START oConrctDwnldStart;
  memcpy(&oConrctDwnldStart.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
  oConrctDwnldStart.header.iTransCode = CONTRACT_DOWNLOAD_START;
  oConrctDwnldStart.header.iMsgLen    = sizeof(oConrctDwnldStart);


  oTcp->SendData(RecvData.MyFd, (char *)&oConrctDwnldStart, oConrctDwnldStart.header.iMsgLen,RecvData.ptrConnInfo);

  sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|CONTRACT_DOWNLOAD_START|RES Size %d|Transcode %d",RecvData.MyFd,oConrctDwnldStart.header.iDealerID,sizeof(oConrctDwnldStart),oConrctDwnldStart.header.iTransCode);
  Logger::getLogger().log(DEBUG, chLogBuff);


  DEALER::CONTRACT_DOWNLOAD oContrctDwnld;
  memcpy(&oContrctDwnld.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
  oContrctDwnld.header.iMsgLen     = sizeof(oContrctDwnld);
  oContrctDwnld.header.iTransCode  = CONTRACT_DOWNLOAD;


  CONTRACTINFO* ConrtInfo;

  for(int i = 0 ; i < oTokenSub.iNoOfToken ; i++)
  {
    auto it = ContInfoMap.find(oTokenSub.iaTokenArr[i]);

    if(it != ContInfoMap.end())
    {
      ConrtInfo = it->second;
      oContrctDwnld.ConrctData.iTickSize           = ConrtInfo->iTickSize;
      oContrctDwnld.ConrctData.iSeg                = ConrtInfo->iSeg;
      oContrctDwnld.ConrctData.AssetToken          = ConrtInfo->AssetToken;
      oContrctDwnld.ConrctData.Token               = ConrtInfo->Token;
      oContrctDwnld.ConrctData.ExpiryDate          = ConrtInfo->ExpiryDate;
      oContrctDwnld.ConrctData.HighDPR             = ConrtInfo->HighDPR;
      oContrctDwnld.ConrctData.LowDPR              = ConrtInfo->LowDPR;
      oContrctDwnld.ConrctData.MinimumLotQuantity  = ConrtInfo->MinimumLotQuantity;
      memcpy(oContrctDwnld.ConrctData.Symbol,ConrtInfo->Symbol,sizeof(oContrctDwnld.ConrctData.Symbol));
      memcpy(oContrctDwnld.ConrctData.OptionType,ConrtInfo->OptionType,sizeof(oContrctDwnld.ConrctData.OptionType));
      memcpy(oContrctDwnld.ConrctData.InstumentName,ConrtInfo->InstumentName,sizeof(oContrctDwnld.ConrctData.InstumentName));
      memcpy(oContrctDwnld.ConrctData.cSeries,ConrtInfo->cSeries,sizeof(oContrctDwnld.ConrctData.cSeries));
      memcpy(oContrctDwnld.ConrctData.TradingSymbols,ConrtInfo->TradingSymbols,sizeof(oContrctDwnld.ConrctData.TradingSymbols));
      oContrctDwnld.ConrctData.StrikePrice         = ConrtInfo->StrikePrice;

      oTcp->SendData(RecvData.MyFd, (char *)&oContrctDwnld, oContrctDwnld.header.iMsgLen,RecvData.ptrConnInfo);

      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|Contract download send|Token %d|Symbol %s|RES Size %d|Transcode %d",RecvData.MyFd,oContrctDwnld.header.iDealerID,oTokenSub.iaTokenArr[i],oContrctDwnld.ConrctData.Symbol,sizeof(oContrctDwnld),oContrctDwnld.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
    }
    else
    {
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|Contract Detail Not Found|Token %d",RecvData.MyFd,oContrctDwnld.header.iDealerID,oTokenSub.iaTokenArr[i]);
      Logger::getLogger().log(DEBUG, chLogBuff);
    }
  }

  DEALER::CONTRACT_DOWNLOAD_END oConrctDwnldEnd;
  memcpy(&oConrctDwnldEnd.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));  
  oConrctDwnldEnd.header.iTransCode = CONTRACT_DOWNLOAD_END;
  oConrctDwnldEnd.header.iMsgLen    = sizeof(oConrctDwnldEnd);


  oTcp->SendData(RecvData.MyFd, (char *)&oConrctDwnldEnd, oConrctDwnldEnd.header.iMsgLen,RecvData.ptrConnInfo);
  sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|CONTRACT_DOWNLOAD_END|RES Size %d|Transcode %d",RecvData.MyFd,oConrctDwnldEnd.header.iDealerID,sizeof(oConrctDwnldEnd),oConrctDwnldEnd.header.iTransCode);
  Logger::getLogger().log(DEBUG, chLogBuff);
}

void SendErrorInfoDownload(DATA_RECEIVED& RecvData,DEALER::LOG_IN_RES& sLogInResp,ERRORCODEINFO loErrInfo[] )
{
  DEALER::ERROR_DOWNLOAD_START oErrDwnldStart;
  memcpy(&oErrDwnldStart.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
  oErrDwnldStart.header.iTransCode  = ERROR_CODES_DOWNLOAD_START;
  oErrDwnldStart.header.iMsgLen     = sizeof(oErrDwnldStart);
  oErrDwnldStart.header.iErrorCode  = 0;
  oErrDwnldStart.header.llTimeStamp = 0; 

  oTcp->SendData(RecvData.MyFd, (char *)&oErrDwnldStart, oErrDwnldStart.header.iMsgLen,RecvData.ptrConnInfo);
  
  sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|ERROR_DOWNLOAD_START|RES Size %d|Transcode %d",RecvData.MyFd,oErrDwnldStart.header.iDealerID,sizeof(oErrDwnldStart),oErrDwnldStart.header.iTransCode);
  Logger::getLogger().log(DEBUG, chLogBuff);
          
          
  DEALER::ERROR_DOWNLOAD oErrDwnld;
  memcpy(&oErrDwnld.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
  oErrDwnld.header.iTransCode  = ERROR_CODES_DOWNLOAD;
  oErrDwnld.header.iMsgLen     = sizeof(oErrDwnld);
  oErrDwnld.header.iErrorCode  = 0;
  oErrDwnld.header.llTimeStamp = 0;
  int index = 0;
  while(1)
  {
    if(loErrInfo[index].iErrorCode != 0 )
    {
      oErrDwnld.iErrorCode = loErrInfo[index].iErrorCode;
      
      memcpy(oErrDwnld.cErrString, loErrInfo[index].cErrString, sizeof(oErrDwnld.cErrString));
      
      oTcp->SendData(RecvData.MyFd, (char *)&oErrDwnld, oErrDwnld.header.iMsgLen,RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|ERROR_DOWNLOAD|RES Size %d|Transcode %d|ErrorCode %d|ErrString %s",RecvData.MyFd,oErrDwnld.header.iDealerID,sizeof(oErrDwnld),oErrDwnld.header.iTransCode,oErrDwnld.iErrorCode,oErrDwnld.cErrString);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
      index++;
    }
    else
    {
      break;
    }
  }
  
  DEALER::ERROR_DOWNLOAD_END oErrDwnldEnd;
  memcpy(&oErrDwnldEnd.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
  oErrDwnldEnd.header.iTransCode  = ERROR_CODES_DOWNLOAD_END;
  oErrDwnldEnd.header.iMsgLen     = sizeof(oErrDwnldEnd);
  oErrDwnldEnd.header.iErrorCode  = 0;
  oErrDwnldEnd.header.llTimeStamp = 0; 

  oTcp->SendData(RecvData.MyFd, (char *)&oErrDwnldEnd, oErrDwnldEnd.header.iMsgLen,RecvData.ptrConnInfo);
  
  sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|ERROR_DOWNLOAD_END|RES Size %d|Transcode %d",RecvData.MyFd,oErrDwnldEnd.header.iDealerID,sizeof(oErrDwnldEnd),oErrDwnldEnd.header.iTransCode);
  Logger::getLogger().log(DEBUG, chLogBuff);
   

}

int ProcessLogonResp(char* pchBuffer)
{
  NSEFO::MS_SIGNON_RESP oExLogonResp;
  char chBrokerName[26] = {0};
  oExLogonResp.Initialize(pchBuffer);

  if(oExLogonResp.msg_hdr.ErrorCode != 0)
  {
    NSEFO::MS_ERROR_RESPONSE *pError = (NSEFO::MS_ERROR_RESPONSE *)(pchBuffer);
    sprintf(chLogBuff,"RSP|Login Failed: %d | ErrorMsg:[%128.128s]", oExLogonResp.msg_hdr.ErrorCode, pError->ErrorMessage);
    Logger::getLogger().log(DEBUG, chLogBuff);
    return -1;
  }
  else
  {
    memcpy(chBrokerName, oExLogonResp.BrokerName, 25);
    time_t rawtime = 0;
    time(&rawtime);
    int32_t iDaysSinceLastPwdChanged = (rawtime - (__bswap_32(oExLogonResp.LastPasswdChangeDate) + GAP_1970_1980)) / SECONDS_IN_A_DAY;

    sprintf(chLogBuff, "MAIN|NSEFO|LOGIN RESP|Logon Successful|TransCode %d|Err %d|BrokerName %s|Strm %d|LastPwdChngDate %d|DaysSinceLastPwdChanged %d|", oExLogonResp.msg_hdr.TransactionCode, oExLogonResp.msg_hdr.ErrorCode, chBrokerName, oExLogonResp.msg_hdr.TimeStamp2[7], __bswap_32(oExLogonResp.LastPasswdChangeDate)+GAP_1970_1980, iDaysSinceLastPwdChanged);
    Logger::getLogger().log(DEBUG, chLogBuff);
    return 0;
  }
}

int ProcRcvdMsgFO(char* pchBuffer, int32_t& iBytesToBeProcessed, int32_t iSockId, CONNINFO* pConn,connectionMap& ConnMap)
{
  int iRetVal;
  NSEFO::CUSTOM_HEADER* pCustHdr = NULL;
	NSEFO::CUSTOM_HEADER  oCustHdr;
  
  pCustHdr = (NSEFO::CUSTOM_HEADER*)pchBuffer;
  oCustHdr.sLength    = __bswap_16(pCustHdr->sLength);
  oCustHdr.iSeqNo     = __bswap_32(pCustHdr->iSeqNo);
  oCustHdr.sTransCode = __bswap_16(pCustHdr->sTransCode);

  switch(oCustHdr.sTransCode)
  {
    case SIGN_ON_REQUEST_OUT :	//2301
    {
       iRetVal = ProcessLogonResp(pchBuffer+sizeof(NSEFO::TAP_HEADER));
       if(iRetVal < 0)
       {
         exit(1);
       }
    }
    break;
    
    case NSECM_ADD_CNF_TR:
    {
      NSEFO::MS_OE_RESPONSE_TR *pNewOrderResp = (NSEFO::MS_OE_RESPONSE_TR *)pchBuffer ;
      iRetVal = AddOrderTrimRespFO(pNewOrderResp,ConnMap);
    }
    break;
    
    case NSECM_MOD_CNF_TR:
    {
      NSEFO::MS_OE_RESPONSE_TR *pModOrderResp = (NSEFO::MS_OE_RESPONSE_TR *)pchBuffer ;
      iRetVal = ModOrderTrimRespFO(pModOrderResp,ConnMap);
    }
    break;
    
    case NSECM_CAN_CNF_TR:
    {
      NSEFO::MS_OE_RESPONSE_TR *pCanOrderResp = (NSEFO::MS_OE_RESPONSE_TR *)pchBuffer ;
      iRetVal = CanOrderTrimRespFO(pCanOrderResp,ConnMap);
    }
    break;
    
    
    case(NSECM_TRD_CNF_TR):	//20222				
    {
      NSEFO::TRADE_CONFIRMATION_TR* pExTrdData = (NSEFO::TRADE_CONFIRMATION_TR*)(pchBuffer);
      iRetVal = TradeOrderRespFO(pExTrdData, ConnMap);
      
    }
    break;
    
  }
  
  iBytesToBeProcessed = iBytesToBeProcessed - oCustHdr.sLength;
  memcpy(pchBuffer,pchBuffer + oCustHdr.sLength,iBytesToBeProcessed);
  
//  sprintf(chLogBuff, "|RemBytes %d|ProcessExchResp", iBytesToBeProcessed);
//  Logger::getLogger().log(DEBUG, chLogBuff);
  
}

int SendLogonReqFO(int iSockFdFO, ConfigFile& oConfigStore,  CONNINFO *conn)
{
    
    NSEFO::MS_SIGNON_REQ oExFOLognReq;
    memset(&oExFOLognReq.msg_hdr.sLength, ' ', sizeof(NSEFO::MS_SIGNON_REQ));
    oExFOLognReq.msg_hdr.TransactionCode	=   __bswap_16(SIGN_ON_REQUEST_IN);
    oExFOLognReq.msg_hdr.LogTime          =   0;
    oExFOLognReq.msg_hdr.TraderId         =   __bswap_32(oConfigStore.iUserID_FO);
    oExFOLognReq.msg_hdr.ErrorCode        =   0;
    oExFOLognReq.msg_hdr.MessageLength		=   __bswap_16(sizeof(NSEFO::MS_SIGNON_REQ) - sizeof(NSEFO::TAP_HEADER));
    oExFOLognReq.UserId                   =   __bswap_32(oConfigStore.iUserID_FO);
    oExFOLognReq.LastPasswdChangeDate     =   0;
    oExFOLognReq.BranchId                 =   __bswap_16(oConfigStore.sBranchId_FO);
    oExFOLognReq.VersionNumber            =   __bswap_32(oConfigStore.iExchVer_FO);
    oExFOLognReq.UserType                 =   0;
    oExFOLognReq.SequenceNumber           =   0;
    oExFOLognReq.ShowIndex                =   'T';  //T: for Trimmed NNF Protocol
    oExFOLognReq.UserType                 =   0;		//1:Trading Member Only
    oExFOLognReq.msg_hdr.sLength          =   __bswap_16(sizeof(NSEFO::MS_SIGNON_REQ));
    oExFOLognReq.msg_hdr.iSeqNo           =   __bswap_32(++iExchSendSeqNo);

    memcpy(oExFOLognReq.Passwd,oConfigStore.sPassword_FO.c_str(), oConfigStore.sPassword_FO.length());
    memcpy(oExFOLognReq.NewPassword, "        ", 8);
    memcpy(oExFOLognReq.BrokerId, oConfigStore.sTMID_FO.c_str(), oConfigStore.sTMID_FO.length());
    sprintf(chLogBuff, "MAIN|NSEFO|LOGIN REQ|Sending SendLogonReq to Exchange");
    Logger::getLogger().log(DEBUG, chLogBuff);

    int iMsgSize = sizeof(NSEFO::MS_SIGNON_REQ);
    
    oTcp->SendData(iSockFdFO, (char *)&oExFOLognReq, iMsgSize,conn);

}

void SendTokenDepth(DATA_RECEIVED *pSendData)
{
  DEALER::TOKEN_DEPTH_RESPONSE *oTokDepRes = new DEALER::TOKEN_DEPTH_RESPONSE;
  oTokDepRes = (DEALER::TOKEN_DEPTH_RESPONSE*)pSendData->msgBuffer;
  
  oTcp->SendData(pSendData->MyFd, (char*)oTokDepRes, sizeof(DEALER::TOKEN_DEPTH_RESPONSE),pSendData->ptrConnInfo);
}

int ProcessRecvUIMessage(DATA_RECEIVED& RecvData, dealerToIdMap& DealToIdMap, dealerInfoMap& DealInfoMap, ContractInfoMap& ContInfoMap, TOKENSUBSCRIPTION& oTokenSub, ConfigFile& oConfigdata,int iSockFD_CM, int iSockFD_FO, ERRORCODEINFO loErrInfo[], ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain)
{
//  DATA_RECEIVED SendData; 
  bool ret;
  DEALER::MSG_HEADER *psMsgHdr = (DEALER::MSG_HEADER*)RecvData.msgBuffer;
  int16_t lnSeg       = psMsgHdr->iSeg;
  int16_t lnTransCode = psMsgHdr->iTransCode;
  
  CONNINFO* connSts = RecvData.ptrConnInfo;
  switch(lnTransCode)
  {
    case LOGIN_UI_REQ:
    {
      DEALER::LOG_IN_RES sLogInResp;
      DEALER::LOG_IN_REQ *psLogInReq = (DEALER::LOG_IN_REQ*)RecvData.msgBuffer;
      
      memcpy(&sLogInResp.header,&psLogInReq->header,sizeof(DEALER::MSG_HEADER));
      sLogInResp.header.iMsgLen     = sizeof(DEALER::LOG_IN_RES);
      sLogInResp.header.iTransCode  = LOGIN_UI_RES;
      
      auto it = DealToIdMap.find(psLogInReq->cDealerName);
      if(it == DealToIdMap.end())
      {

        sLogInResp.header.iErrorCode = DEALER_NOT_FOUND;
        
        oTcp->SendData(RecvData.MyFd, (char *)&sLogInResp, sLogInResp.header.iMsgLen,RecvData.ptrConnInfo);
        
        sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|IP %s|UserId %s not found|ErrorCode %d|Transcode %d",RecvData.MyFd,RecvData.ptrConnInfo->IP,psLogInReq->cDealerName,sLogInResp.header.iErrorCode,sLogInResp.header.iTransCode);
        Logger::getLogger().log(DEBUG, chLogBuff);
        
      }
      else
      {
        auto iter = DealInfoMap.find(it->second);
        DEALERINFO* pDealerInfo = iter->second;

        if(pDealerInfo->iStatus == LOGGED_ON)
        {
          sLogInResp.header.iErrorCode = DEALER_ALREADY_LOGGED_IN;
          
          oTcp->SendData(RecvData.MyFd, (char *)&sLogInResp, sLogInResp.header.iMsgLen,RecvData.ptrConnInfo);
          
          sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|IP %s|UserId %s Already Logged In|ErrorCode %d|Transcode %d",RecvData.MyFd,RecvData.ptrConnInfo->IP,psLogInReq->cDealerName,sLogInResp.header.iErrorCode,sLogInResp.header.iTransCode);
          Logger::getLogger().log(DEBUG, chLogBuff);
          
        }
        else if(pDealerInfo->iStatus == LOGGED_OFF)
        {
          pDealerInfo->iStatus          = LOGGED_ON;
          int32_t iDealerID             = it->second;
          connSts->dealerID             = iDealerID;
          connSts->status               = CONNECTED;
          sLogInResp.header.iDealerID   = iDealerID;
          
          sLogInResp.header.iErrorCode  = 0;

          oTcp->SendData(RecvData.MyFd, (char *)&sLogInResp, sLogInResp.header.iMsgLen,RecvData.ptrConnInfo);
          
          sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|IP %s|UserId %s signed on|Errorcode %d|RES Size %d|Transcode %d",RecvData.MyFd,RecvData.ptrConnInfo->IP,psLogInReq->cDealerName,sLogInResp.header.iErrorCode,sizeof(sLogInResp),sLogInResp.header.iTransCode);
          Logger::getLogger().log(DEBUG, chLogBuff);
          
          /*=======================================================================================================*/
          
          
          
          /*Send Dwnld Info*/
          DEALER::DOWNLOAD_START_NOTIFICATION oDwnldStart;
          memcpy(&oDwnldStart.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));
          oDwnldStart.header.iTransCode = DOWNLOAD_START;
          oDwnldStart.header.iMsgLen    = sizeof(oDwnldStart);
          
          
          oTcp->SendData(RecvData.MyFd, (char *)&oDwnldStart, oDwnldStart.header.iMsgLen,RecvData.ptrConnInfo);
          
          sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|DOWNLOAD_START_NOTIFICATION|RES Size %d|Transcode %d",RecvData.MyFd,oDwnldStart.header.iDealerID,sizeof(oDwnldStart),oDwnldStart.header.iTransCode);
          Logger::getLogger().log(DEBUG, chLogBuff);
          
          SendContractInfoDownload(RecvData,sLogInResp,ContInfoMap,oTokenSub);
          
          SendErrorInfoDownload(RecvData,sLogInResp,loErrInfo);
           
          DEALER::DOWNLOAD_END_NOTIFICATION oDwnldEnd;
          memcpy(&oDwnldEnd.header,&sLogInResp.header,sizeof(DEALER::MSG_HEADER));  
          oDwnldEnd.header.iTransCode = DOWNLOAD_END;
          oDwnldEnd.header.iMsgLen    = sizeof(oDwnldEnd);
          
          
          oTcp->SendData(RecvData.MyFd, (char *)&oDwnldEnd, oDwnldEnd.header.iMsgLen,RecvData.ptrConnInfo);
          sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|DOWNLOAD_END_NOTIFICATION|RES Size %d|Transcode %d",RecvData.MyFd,oDwnldEnd.header.iDealerID,sizeof(oDwnldEnd),oDwnldEnd.header.iTransCode);
          Logger::getLogger().log(DEBUG, chLogBuff);

        }
        
        // Send Download Info End
        
      }
      if (sLogInResp.header.iErrorCode != 0)
      {
        connSts->status = DISCONNECTED;
        return -1;
      }
        
 
    }
    break;
    
    case ORDER_DOWNLOAD_REQ:
    {
      DEALER::ORDER_DOWNLOAD_START oOrdDwnldStart;
      DEALER::ORDER_DOWNLOAD_REQ *psOdrDwnldReq = (DEALER::ORDER_DOWNLOAD_REQ*)RecvData.msgBuffer;
      memcpy(&oOrdDwnldStart.header,&psOdrDwnldReq->header,sizeof(oOrdDwnldStart.header));
      
      oOrdDwnldStart.header.iTransCode = ORDER_DOWNLOAD_START;
      oOrdDwnldStart.header.iMsgLen    = sizeof(oOrdDwnldStart);
      
      oTcp->SendData(RecvData.MyFd, (char *)&oOrdDwnldStart, oOrdDwnldStart.header.iMsgLen,RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|ORDER DOWNLOAD START|RES Size %d|Transcode %d",RecvData.MyFd,oOrdDwnldStart.header.iDealerID,sizeof(oOrdDwnldStart),oOrdDwnldStart.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
      OrdDwnldSend(RecvData,psOdrDwnldReq);
      
      DEALER::ORDER_DOWNLOAD_END oOrdDwnldEnd;
      memcpy(&oOrdDwnldEnd.header,&psOdrDwnldReq->header,sizeof(oOrdDwnldEnd.header));
      
      oOrdDwnldEnd.header.iTransCode = ORDER_DOWNLOAD_END;
      oOrdDwnldEnd.header.iMsgLen    = sizeof(oOrdDwnldEnd);
      
      oTcp->SendData(RecvData.MyFd, (char *)&oOrdDwnldEnd, oOrdDwnldEnd.header.iMsgLen,RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|ORDER DOWNLOAD END|RES Size %d|Transcode %d",RecvData.MyFd,oOrdDwnldEnd.header.iDealerID,sizeof(oOrdDwnldEnd),oOrdDwnldEnd.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
    }
    break;
    
    case TRADE_DOWNLOAD_REQ:
    {
      DEALER::TRADE_DOWNLOAD_START oTrdDwnldStart;
      DEALER::TRADE_DOWNLOAD_REQ *psTrdDwnldReq = (DEALER::TRADE_DOWNLOAD_REQ*)RecvData.msgBuffer;
      memcpy(&oTrdDwnldStart.header,&psTrdDwnldReq->header,sizeof(oTrdDwnldStart.header));
      
      oTrdDwnldStart.header.iTransCode = TRADE_DOWNLOAD_START;
      oTrdDwnldStart.header.iMsgLen    = sizeof(oTrdDwnldStart);
      
      oTcp->SendData(RecvData.MyFd, (char *)&oTrdDwnldStart, oTrdDwnldStart.header.iMsgLen,RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|TRADE DOWNLOAD START|RES Size %d|Transcode %d",RecvData.MyFd,oTrdDwnldStart.header.iDealerID,sizeof(oTrdDwnldStart),oTrdDwnldStart.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
      TrdDwnldSend(RecvData,psTrdDwnldReq);
      
      DEALER::TRADE_DOWNLOAD_END oTrdDwnldEnd;
      memcpy(&oTrdDwnldEnd.header,&psTrdDwnldReq->header,sizeof(oTrdDwnldEnd.header));
      
      oTrdDwnldEnd.header.iTransCode = TRADE_DOWNLOAD_END;
      oTrdDwnldEnd.header.iMsgLen    = sizeof(oTrdDwnldEnd);
      
      oTcp->SendData(RecvData.MyFd, (char *)&oTrdDwnldEnd, oTrdDwnldEnd.header.iMsgLen, RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Fd %d|UserId %d|TRADE DOWNLOAD END|RES Size %d|Transcode %d",RecvData.MyFd,oTrdDwnldEnd.header.iDealerID,sizeof(oTrdDwnldEnd),oTrdDwnldEnd.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
    }
    break;
    
    case DEPTH_REQUEST:
    {
      Inqptr_MainToFF->enqueue(RecvData);
    }
    break;
    
    case LOGOUT_UI_REQ:
    {
      DEALER::LOG_OUT_RES oLogOutRes;
      DEALER::LOG_OUT_REQ *psLogOutReq = (DEALER::LOG_OUT_REQ*)RecvData.msgBuffer;
      memcpy(&oLogOutRes.header,&psLogOutReq->header,sizeof(DEALER::MSG_HEADER));
      oLogOutRes.header.iTransCode = LOGOUT_UI_RES;
      oLogOutRes.header.iErrorCode = 0;
      CONNINFO* connSts            = RecvData.ptrConnInfo;
      connSts->status              = DISCONNECT_CLIENT;
      
      oTcp->SendData(RecvData.MyFd, (char *)&oLogOutRes, oLogOutRes.header.iMsgLen,RecvData.ptrConnInfo);
      sprintf(chLogBuff, "MAIN|UI_DEALER|Logout Request|Fd %d|UserId %d|Msg Size %d|Transcode %d",RecvData.MyFd,oLogOutRes.header.iDealerID,sizeof(oLogOutRes),oLogOutRes.header.iTransCode);
      Logger::getLogger().log(DEBUG, chLogBuff);
      
      CanOnLogOut(psLogOutReq->header.iDealerID, iSockFD_FO, iSockFD_CM, oConfigdata);
      return -1;
    }
    break;
    
    case ADD_ORDER_REQ:
    {
      
      switch(lnSeg)
      {
        case SEG_NSEFO:
        {
          DEALER::GENERIC_ORD_MSG* pAddOrdReq = (DEALER::GENERIC_ORD_MSG*)RecvData.msgBuffer;
          ret = AddOrderUI(pAddOrdReq,RecvData.MyFd, DealInfoMap, ContInfoMap,RecvData.ptrConnInfo);
          
          if(ret == true)
          {
            AddOrderTrimReqFO(oConfigdata,iSockFD_FO);
          }
          
        }
        break;
        
        case SEG_NSECM:
        {
          
        }
        break;
      }
    }
    break;
    
    case MOD_ORDER_REQ:
    {
      switch(lnSeg)
      {
        case SEG_NSEFO:
        {
          DEALER::GENERIC_ORD_MSG* pModOrdReq = (DEALER::GENERIC_ORD_MSG*)RecvData.msgBuffer;
          ret = ModOrderUI(pModOrdReq,RecvData.MyFd, DealInfoMap, ContInfoMap,RecvData.ptrConnInfo);
          
          if(ret == true)
          {
            ModOrderTrimReqFO(oConfigdata,iSockFD_FO,pModOrdReq->iIntOrdID);
          }
        }
        break;
        
        case SEG_NSECM:
        {
          
        }
        break;
      }
    }
    break;
    
    case CAN_ORDER_REQ:
    {
      switch(lnSeg)
      {
        case SEG_NSEFO:
        {
          DEALER::GENERIC_ORD_MSG* pCanOrdReq = (DEALER::GENERIC_ORD_MSG*)RecvData.msgBuffer;
          ret = CanOrderUI(pCanOrdReq,RecvData.MyFd, DealInfoMap, ContInfoMap,RecvData.ptrConnInfo);
          
          if(ret == true)
          {
            CanOrderTrimReqFO(oConfigdata,iSockFD_FO,pCanOrdReq->iIntOrdID);
          }
        }
        break;
        
        case SEG_NSECM:
        {
          
        }
        break;
      }
    }
    break;
    
    default:
      break;
      
  }
  
}
