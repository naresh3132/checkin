/* 
 * File:   Thread_ME.h
 * Author: muditsharma
 *
 * Created on March 1, 2016, 6:04 PM
 */

#include "CommonFunctions.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"
#include "nsecm_constants.h"
#include "BrodcastStruct.h"
#include <cstdlib>
#include <functional>
#include "BookBuilder.h"
#include <unordered_map>
#include "Broadcast_Struct.h"
#include<set>

//void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* qptr1,ProducerConsumerQueue<_DATA_RECEIVED>* qptr2,ProducerConsumerQueue<_DATA_RECEIVED>* qptr3);
void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,
             ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast,
             std::unordered_map<std::string, int32_t>* pcNSECMTokenStore, int _nMode,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToLog, int iWriteAttempt, int iEpochBase, int iMECore, int* arr, int cnt,dealerInfoMap*  dealerinfoMap, CONTRACTINFO* pCntrctInfo,
             int iMaxOrders);
//int ProcessData(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_METoBroadcast);
int SendOrderCancellation_NSECM(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int SendOrderCancellation_NSEFO(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst);
int ProcessTranscodes(DATA_RECEIVED * RcvData,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer);

int AddOrderTrim(NSECM::MS_OE_REQUEST_TR * AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*);
int AddOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*);
long Addtoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL,int32_t epochTime,int,int);
//long Addtoorderbook_SL(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL ) ;
int ModOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*);
int ModOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*);
int CanOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD,CONNINFO*);
int CanOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD,CONNINFO*);
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
int ValidateModReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&);
int ValidateCanReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int&, int&);

/*ML functions*/
int AddMLOrder(NSEFO::MS_SPD_OE_REQUEST* MLOrderReq, int FD, CONNINFO* pConnInfo);
int MatchMLOrder(int noOfLegs, NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int FD, CONNINFO* pConnInfo);
int SendMLOrderLegCancellation(NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int i, int FD,CONNINFO* pConnInfo);
int SendMLOrderCancellation(NSEFO::MS_SPD_OE_REQUEST*  MLOrderResp, int FD, int noOfLegs, CONNINFO* pConnInfo);
bool validateOrderQtyExecutable(int noOfLegs);
bool validateOrderPriceExecutable(int noOfLegs);
int ValidateMLAddReq(int32_t userID, int Fd, int noOfLegs);
//int TaskSet(unsigned int nCoreID_);

int ProcessCOL (int32_t COLDealerID);

