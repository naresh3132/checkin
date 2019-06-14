/* 
 * File:   BrodcastStruct.h
 * Author: muditsharma
 *
 * Created on April 29, 2016, 6:41 PM
 */

#ifndef BRODCASTSTRUCT_H
#define	BRODCASTSTRUCT_H

#include <stdio.h>
#include <iostream>
#include <sys/time.h>

#define BEGIN_OF_MASTER           'B'
#define CONTRACT_INFORMATION      'C'
#define SPREAD_INFORMATION        'P'
#define END_OF_MASTER             'E'

#define NEW_ORDER                 'N' 
#define ORDER_MODIFICATION        'M'
#define ORDER_CANCELLATION        'X'
#define TRADE_MESSAGE             'T'

#define SPREAD_NEW_ORDER          'G' 
#define SPREAD_ORDER_MODIFICATION 'H'
#define SPREAD_ORDER_CANCELLATION 'J'
#define SPREAD_TRADE_MESSAGE      'K'

#define RECOVERY_REQUEST          'R'
#define RECOVERY_RESPONSE         'Y'

#define HEART_BEAT_MESSAGE        'Z'

#define REQUEST_SUCCESS           'S'
#define REQUEST_ERROR             'E'

#pragma pack(1)

struct STREAM_HEADER
{
  short wMsgLen;
  short wStremID;
  int nSeqNo;
};

struct MSG_HEADER
{
  short wMsgLen;
  short wStremID;
  int nSeqNo;
  char cMsgType;
};

struct MST_DATA_HEADER
{
  STREAM_HEADER header;
  char cMsgType;
  int nTokenCount;
};

struct CONTRACT_INFO
{
  STREAM_HEADER header;
  char cMsgType;
  short wStreamID;
  int nToken;
  char strInstType[6];
  char strSymbol[10];
  int nExpiry;
  int nStrikePrice;
  char strOptType[2];
};

struct SPRD_CONTRACT_INFO
{
  STREAM_HEADER header;
  char cMsgType;
  short wStreamID;
  int nToken1;
  int nToken2;
};

struct MST_DATA_TRAILER
{
  STREAM_HEADER header;
  char cMsgType;
  int nTokenCount;
};

struct GENERIC_ORD_MSG
{
  STREAM_HEADER header;
  char cMsgType;
  long lTimeStamp;
  double dblOrdID;
  int nToken;
  char cOrdType;
  int nPrice;
  int nQty;
};

struct TRD_MSG
{
  STREAM_HEADER header;
  char cMsgType;
  long lTimestamp;
  double dblBuyOrdID;
  double dblSellOrdID;
  int nToken;
  int nTradePrice;
  int nTradeQty;
};

struct RECOVERY_REQ
{
  char cMsgType;
  short wStreamID;
  int nBegSeqNo;
  int nEndSeqNo;
};

struct RECOVERY_RESP
{
  STREAM_HEADER header;
  char cMsgType;
  char cReqStatus;
};

struct HEARTBEAT_MSG
{
  STREAM_HEADER header;
  char cMsgType;
  int nLastSeqNo;
};

union BcastMsg
{
  STREAM_HEADER       stHeader;
  MSG_HEADER          stMsgHeader;
  MST_DATA_HEADER     stMSTDataHeader;
  CONTRACT_INFO       stContractInfo;
  SPRD_CONTRACT_INFO  stSprdContractInfo;
  MST_DATA_TRAILER    stMSTDataTrailer;
  GENERIC_ORD_MSG     stGegenricOrdMsg;
  TRD_MSG             stTrdMsg;
  RECOVERY_RESP       stRecoveryResp;
};

struct BROADCAST_DATA
{
  short wPacketType;
  BcastMsg            stBcastMsg;
  struct timespec     tv;    
};
struct CompositeBcastMsg
{
  BcastMsg            stBcastMsg;
  struct timespec     tv;    
};

#endif	/* BRODCASTSTRUCT_H */
