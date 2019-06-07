/* 
 * File:   common_structs.h
 * Author: MaheshChimnani
 *
 * Created on December 7, 2016, 6:09 PM
 */

#ifndef COMMON_STRUCTS_H
#define	COMMON_STRUCTS_H


inline void SwapDouble(char* pData)
{
  char TempData = 0;
  for(int32_t nStart = 0, nEnd = 7; nStart < nEnd; nStart++, nEnd--)
  {
    TempData = pData[nStart];
    pData[nStart] = pData[nEnd];
    pData[nEnd] = TempData;
  }
}


enum EXCH_SEG
{
  NSECM = 1,
  NSEFO = 2
};

enum CONN_STATUS
{
  DISCONNECTED  = 0,
  CONNECTED     = 1,
  LOGGED_ON     = 2
};

enum ORD_STATUS
{
  UN_INITIALIZED    = 0,
  PENDING_AT_EXCH   = 1,
  OPEN_AT_EXCH      = 2,
  NEW_REJECTED      = 3,
  CANCELLED         = 4,
  PARTIALLY_FILLED  = 5,
  FILLED            = 6,
  FREEZED           = 7,
  TRANSIT_ERROR     = 8
};

enum STRTGY_ORD_STATUS
{
  ST_UN_INITIALIZED = 0,
  ST_NEW_PENDING    = 1,
  ST_OPEN           = 2,
  ST_REJECTED       = 3,
  ST_CANCEL         = 4,
  ST_MOD_PENDING    = 5,
  ST_CAN_PENDING    = 6,
  ST_FILLED         = 7,
  ST_FREEZED        = 8
};

enum STRTGY_RESP_TYPE
{
  ORD_ACCEPTED  = 1,
  ORD_REJECTED  = 2,
  ORD_UNSOLICITED = 3
};

enum STRTGY_REQ_TYPE
{
  ORD_REQUEST         = 1,
  TOKEN_DATA_CONTENT  = 2,
  DEALER_INFO_CONTENT = 3,
  TOKEN_DATA_START    = 4,
  TOKEN_DATA_END      = 5,
  EXCH_DISCONNECT_ACK = 6,
  STRTGY_TIMER_EVENT  = 101
};


enum STRTGY_ORD_REQ_TYPE
{
  ORD_ADD_REQ  = 1,
  ORD_MOD_REQ  = 2,
  ORD_CAN_REQ  = 3
};

enum STRTGY_SEND_SEQ_TYPE
{
  SEND_ONLY_BUY   = 1,
  SEND_ONLY_SELL  = 2,
  SEND_BUY_SELL   = 3,
  SEND_SELL_BUY   = 4
};

enum STRTGY_PKT_HDR
{
  ORD_RESP  = 1,
  ORD_FILL  = 2,
  GW_RToR   = 3,  //Gateway Ready to Receive Orders
  EXCH_DISCONNECT = 4,
  FULL_RECOVERY_COMPLETED = 5,
  INCREMENTAL_RECOVERY_COMPLETED = 6,
  TIMER     = 101
};


enum ORD_TYPE
{
  TRIMMED     = 0,
  NON_TRIMMED = 1,
  SPD_ML      = 3
};

const int16_t LOG_BUFF_SIZE               = 768;
const int32_t RECV_BUFF_SIZE              = 10485760; //10MB
const int32_t MAX_TCP_RECV_BUFF_SIZE      = 134217728; //128MB
const int32_t SEND_BUFF_SIZE              = 2048;
//const int32_t MAX_ORDERS_SIZE             = 100000;
//const int32_t MAX_SPD_ML_ORDERS_SIZE      = 100000;
//const int32_t MAX_SL_ORDERS_SIZE          = 100000;
const int32_t MAX_DEALERS                 = 100;
const int32_t MAX_CLIENTS                 = 1000;
const int32_t MAX_INSTANCES               = 128;

const int32_t ASCII_OFFSET_FOR_NUMBERS    = 48;

const int32_t TWO_LEG                     = 2;
const int32_t THREE_LEG                   = 3;

const int32_t MAX_NO_OF_STREAMS           = 20;

const int32_t	GAP_1970_1980 = 315532800;  //No of Seconds elapsed from 1970 to 1980
const int32_t	SECONDS_IN_A_DAY = 86400;
const int32_t	GAP_EXCH_HB_INTERVAL = 30;  //HB to be send after this GAP in Secs
const int32_t RECONNECTION_DELAY = 1000000;


#endif	/* COMMON_STRUCTS_H */

