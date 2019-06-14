/* 
 * File:   Broadcast_Struct.h
 * Author: muditsharma
 *
 * Created on March 22, 2016, 5:39 PM
 */

#pragma pack(1)




typedef struct _STD_HEADER_BROADCAST
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
} STD_HEADER_BROADCAST;

typedef struct _ORDER_BOOK_BROADCAST
{
   long lQty;
   long lPrice;
   int  nNumOrders;
   short wBuySellFlag;
} ORDER_BOOK_BROADCAST;

typedef struct _BID_ASK_PKT_BROADCAST
{
   STD_HEADER_BROADCAST header;
   //long lToken;
   char strSymbol[30];
   double dblTBQ;
   double dblTSQ;
   int nNoOfRecords;
   ORDER_BOOK_BROADCAST Records[40];
   double dblVolTradedToday;
   long lATP;
   long lLTT;
   long lNumOfTrades;
   long lTotalTradedVol;
   long lOpenPrice;
   long lClosePrice;
   long lLTP;   
} BID_ASK_PKT_BROADCAST;

//typedef struct _BROADCAST_DATA
//{
//    int ExchSeg;
//    BID_ASK_PKT_BROADCAST BidAsk;
//} BROADCAST_DATA;

#pragma pack()