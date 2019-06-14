/* 
 * File:   Thread_ME.h
 * Author: muditsharma
 *
 * Created on March 1, 2016, 6:04 PM
 */

#include "CommonFunctions.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"
#include "nsecd_exch_structs.h"
#include "nsecm_constants.h"
#include "BrodcastStruct.h"
#include <cstdlib>
#include <functional>
#include "BookBuilder.h"
#include <unordered_map>
#include "Broadcast_Struct.h"
#include "SimMD5/MsgValidator.h"
#include<set>


//void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* qptr1,ProducerConsumerQueue<_DATA_RECEIVED>* qptr2,ProducerConsumerQueue<_DATA_RECEIVED>* qptr3);
void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,
             ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast,
             ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_PFStoME,
             ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_MBtoME,
             std::unordered_map<std::string, int32_t>* pcNSECMTokenStore, int _nMode,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToLog, int iWriteAttempt, int iEpochBase, int iMECore, int* arr, int cnt,dealerInfoMap*  dealerinfoMap, CONTRACTINFO* pCntrctInfo, CD_CONTRACTINFO* pCDCntrctInfo, bool bEnableBrdcst,short StreamID, short MLmode, bool startPFS, int SimulatorMode, const char* pcIpaddress, int nPortNumber,int iInitialBookSize, int16_t sBookSizeThreshold,bool enableValMsg);
//int ProcessData(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_METoBroadcast);
int SendOrderCancellation_NSECM(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int SendOrderCancellation_NSEFO(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int SendOrderCancellation_NSECD(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int ProcessTranscodes(DATA_RECEIVED * RcvData,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer);


/*NK PFS starts*/
int handlePFSMsg(GENERIC_ORD_MSG* GenOrd);
int AddPFSOrderTrim(GENERIC_ORD_MSG *AddPFSOrder, int IsIOC, int IsDQ, int64_t recvTime);
int ModPFSOrderTrim(GENERIC_ORD_MSG *ModPFSOrder,int IsIOC,int IsDQ,int64_t recvTime);
long AddPFSOrdertoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int16_t BuySellSide, int IsIOC,int IsDQ, int tokenIndex);
long ModPFSOrdertoorderbook(ORDER_BOOK_DTLS * Mybookdetails,int16_t BuySellSide, int32_t Token, int IsIOC,int IsDQ, int tokenIndex);
int CanPFSOrderTrim(GENERIC_ORD_MSG *CanPFSOrder, int64_t recvTime);
int ValidateAddPFSReq(int iOrderSide, long&  Token, int&);
int ValidateModPFSReq(double dOrderNo, int iOrderSide, long& Token ,int&);
int ValidateCanPFSReq(double dOrderNo, int iOrderSide, long& Token, int&);
long CanPFSOrdertoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int16_t BuySellSide, int32_t Token,int32_t tokenIndex);

/*NK Book Builder Begins*/
int handleMBMsg(GENERIC_ORD_MSG* GenOrd);
int HandleMBTrade(GENERIC_ORD_MSG *TradeMBOrder, int IsIOC, int IsDQ, int64_t recvTime);
long TradeQuantityModificationMB(ORDER_BOOK_DTLS_MB * Mybookdetails, int16_t BuySellSide,int32_t token, int IsIOC, int tokenIndex);
int AddMBOrderTrim(GENERIC_ORD_MSG *AddMBOrder, int IsIOC, int IsDQ, int64_t recvTime);
int ModMBOrderTrim(GENERIC_ORD_MSG *ModMBOrder,int IsIOC,int IsDQ,int64_t recvTime);
long AddMBOrdertoorderbook(ORDER_BOOK_DTLS_MB * Mybookdetails, int16_t BuySellSide, int IsIOC,int IsDQ, int tokenIndex);
long ModMBOrdertoorderbook(ORDER_BOOK_DTLS_MB * Mybookdetails,int16_t BuySellSide, int32_t Token, int IsIOC,int IsDQ, int tokenIndex);
int CanMBOrderTrim(GENERIC_ORD_MSG *CanMBOrder, int64_t recvTime);
int ValidateAddMBReq(int iOrderSide, long&  Token, int&);
int ValidateModMBReq(double dOrderNo, int iOrderSide, int& Token ,int&);
int ValidateCanMBReq(double dOrderNo, int iOrderSide, long& Token, int&);
long CanMBOrdertoorderbook(ORDER_BOOK_DTLS_MB * Mybookdetails, int16_t BuySellSide, int32_t Token,int32_t tokenIndex);
int MatchingBookBuilder(long Token, int FD,int IsIOC,int IsDQ,CONNINFO*, int);
/*NK Book Builder Begins*/

/*NK PFS ends*/

/*NK SL Non Trim Order Starts*/
int SendOrderCancellationNonTrim_NSECM(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int SendOrderCancellationNonTrim_NSEFO(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);

int AddOrderNonTrim(NSECM::MS_OE_REQUEST * AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int AddOrderNonTrim(NSEFO::MS_OE_REQUEST *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
long Addtoorderbook_NonTrim(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL,int32_t epochTime,int,int);

int ModOrderNonTrim(NSECM::MS_OE_REQUEST *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int ModOrderNonTrim(NSEFO::MS_OE_REQUEST *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int CanOrderNonTrim(NSECM::MS_OE_REQUEST *ModOrder,int FD,CONNINFO*, int64_t recvTime);
int CanOrderNonTrim(NSEFO::MS_OE_REQUEST *ModOrder,int FD,CONNINFO*, int64_t recvTime);
int ValidateAddReqNonTrim(int32_t iUserID, int Fd, double dOrderNo, int iOrderSide, long&  Token, char* symbol, char*series, int&, int&);
int ValidateModReqNonTrim(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&, int&, int&);
int ValidateCanReqNonTrim(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&, int&, int&);
long Modtoorderbook_NonTrim(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL,int32_t epochTime, int tokenindex); // 1 Buy , 2 Sell
long Cantoorderbook_NonTrim(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int32_t epochTime, int, int); // 1 Buy , 2 Sell

int PrintPassiveBook(long Token);
int SortBuySidePassiveBook(long Token);
int SortSellSidePassiveBook(long Token);
/*NK SL Non Trim Order Ends*/

/*NK All Trade Functionality begins*/
int AddnTradeOrderTrim(NSECM::MS_OE_REQUEST_TR * AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int AddnTradeOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);


int AddnTradeMLOrder(NSEFO::MS_SPD_OE_REQUEST* MLOrderReq, int FD, CONNINFO* pConnInfo);
int AddnTradeMLOrder(NSECD::MS_SPD_OE_REQUEST* MLOrderReq, int FD, CONNINFO* pConnInfo);
/*NK All Trade Functionality Ends*/


int AddOrderTrim(NSECM::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int AddOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int AddOrderTrim(NSECD::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
long Addtoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL,int32_t epochTime,int,int);
//long Addtoorderbook_SL(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL ) ;
int ModOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime );
int ModOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int ModOrderTrim(NSECD::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*, int64_t recvTime);
int CanOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD,CONNINFO*, int64_t recvTime);
int CanOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD,CONNINFO*, int64_t recvTime);
int CanOrderTrim(NSECD::MS_OM_REQUEST_TR *ModOrder,int FD,CONNINFO*, int64_t recvTime);
int SortBuySideBook(long Token);
//int SortBuySideBook_SL(long Token);
long Modtoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL,int32_t epochTime, int tokenindex); // 1 Buy , 2 Sell
long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int32_t epochTime, int, int); // 1 Buy , 2 Sell
//int processSignOnRequest(CLIENT_MSG *client_message);
int SortSellSideBook(long Token);
//int SortSellSideBook_SL(long Token);
int Matching(long Token, int FD,int IsIOC,int IsDQ,CONNINFO*, int);
int PrintBook(long Token);
static inline int32_t getCurrentTimeMicro();

int  SendDnldData(int32_t TraderId, int64_t SeqNo, int Fd,CONNINFO*);
int SendToClient(int FD, char* msg, int msgLen, CONNINFO* pConnInfo);
long Filltoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int,int);

int ValidateUser(int32_t iUserID, int fd, int&);
int ValidateAddReq(int32_t iUserID, int Fd, double dOrderNo, int iOrderSide, long&  Token, char* symbol, char*series, int&, int&);
int ValidateModReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&, int&, int&);
int ValidateCanReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&, int&, int&);

/*ML functions*/
int AddMLOrder(NSEFO::MS_SPD_OE_REQUEST* MLOrderReq, int FD, CONNINFO* pConnInfo);
int AddMLOrder(NSECD::MS_SPD_OE_REQUEST* MLOrderReq, int FD, CONNINFO* pConnInfo);
int MatchMLOrder(int noOfLegs, NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int FD, CONNINFO* pConnInfo);
int SendMLOrderLegCancellation(NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int i, int FD,CONNINFO* pConnInfo);
int SendMLOrderCancellation(NSEFO::MS_SPD_OE_REQUEST*  MLOrderResp, int FD, int noOfLegs, CONNINFO* pConnInfo);
int MatchMLOrder(int noOfLegs, NSECD::MS_SPD_OE_REQUEST* MLOrderResp, int FD, CONNINFO* pConnInfo);
int SendMLOrderLegCancellation(NSECD::MS_SPD_OE_REQUEST* MLOrderResp, int i, int FD,CONNINFO* pConnInfo);
int SendMLOrderCancellation(NSECD::MS_SPD_OE_REQUEST*  MLOrderResp, int FD, int noOfLegs, CONNINFO* pConnInfo);
bool validateOrderQtyExecutable(int noOfLegs);
bool validateOrderPriceExecutable(int noOfLegs);
int ValidateMLAddReq(NSEFO::MS_SPD_OE_REQUEST* Req, int Fd, int noOfLegs);
int ValidateMLAddReq(NSECD::MS_SPD_OE_REQUEST* Req, int Fd, int noOfLegs);
//int TaskSet(unsigned int nCoreID_);

int ProcessCOL (int32_t COLDealerID);
int32_t tradeNoBase(short streamID);

