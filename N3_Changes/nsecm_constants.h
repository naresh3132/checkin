/* 
 * File:   nsecm_constants.h
 * Author: root
 *
 * Created on March 20, 2015, 3:31 PM
 */

#ifndef NSECM_CONSTANTS_H
#define	NSECM_CONSTANTS_H

enum CONN_STATUS
{
  eDISCONNECTED  = 0,
  eCONNECTED     = 1,
  eLOGGED_ON     = 2
};

enum SEGMENT_IDENTIFIER
{
  SEG_NSECM = 1,
  SEG_NSEFO = 2
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

enum ORD_TYPE
{
  TRIMMED     = 0,
  NON_TRIMMED = 1,
  SPD_ML      = 3
};

const int32_t RECV_BUFF_SIZE              = 10485760; //10MB
const int32_t MAX_TCP_RECV_BUFF_SIZE      = 134217728; //128MB
const int32_t SEND_BUFF_SIZE              = 2048;
const int32_t NSECM_SCRIPS_SEED           = 0;
const int32_t CM_SCRIP_SIZE               = 35000;

const int32_t MAX_CLIENTS                 = 1000;

const int32_t ASCII_OFFSET_FOR_NUMBERS    = 48;

const int32_t MAX_NO_OF_STREAMS           = 10;

const int32_t	GAP_1970_1980 = 315532800;  //No of Seconds elapsed from 1970 to 1980
const int32_t	SECONDS_IN_A_DAY = 86400;
const int32_t	GAP_EXCH_HB_INTERVAL = 30;  //HB to be send after this GAP in Secs



const int16_t INVITATION_MSG              = 15000;

const int16_t GR_REQUEST                  = 2400;
const int16_t GR_RESPONSE                 = 2401;

const int16_t BOX_SIGN_ON_REQUEST_IN      = 23000;
const int16_t BOX_SIGN_ON_REQUEST_OUT     = 23001;

const int16_t SIGN_ON_REQUEST_IN          = 2300;
const int16_t SIGN_ON_REQUEST_OUT         = 2301;

const int16_t SYSTEM_INFORMATION_IN       = 1600;
const int16_t SYSTEM_INFORMATION_OUT      = 1601;

const int16_t UPDATE_LOCALDB_IN           = 7300;
const int16_t UPDATE_LOCALDB_HEADER       = 7307;
const int16_t UPDATE_LOCALDB_DATA         = 7304;
const int16_t PARTIAL_SYSTEM_INFO         = 7321;
const int16_t UPDATE_LOCALDB_TRAILER      = 7308;

const int16_t INDUSTRY_INDEX_DLOAD_IN     = 1110;
const int16_t INDUSTRY_INDEX_DLOAD_OUT    = 1111;

const int16_t DOWNLOAD_REQUEST            = 7000;
const int16_t DOWNLOAD_HEADER             = 7011;
const int16_t DOWNLOAD_DATA               = 7021;
const int16_t DOWNLOAD_TRAILER            = 7031;

const int16_t NSECM_ADD_REQ               = 2000;
const int16_t NSECM_ADD_CNF               = 2073;
const int16_t NSECM_ADD_REJ               = 2231;
const int16_t NSECM_MOD_REQ               = 2040;
const int16_t NSECM_MOD_CNF               = 2074;
const int16_t NSECM_MOD_REJ               = 2042;
const int16_t NSECM_CAN_REQ               = 2070;
const int16_t NSECM_CAN_CNF               = 2075;
const int16_t NSECM_CAN_REJ               = 2072;

const int16_t NSECM_SL_TRIGGER            = 2212;
const int16_t NSECM_TRD_CNF               = 2222;

const int16_t NSECM_FREEZE                = 2170;

const int16_t NSECM_ADD_REQ_TR            = 20000;
const int16_t NSECM_ADD_CNF_TR            = 20073;
const int16_t NSECM_ADD_REJ_TR            = 20231;

const int16_t NSECM_MOD_REQ_TR            = 20040;
const int16_t NSECM_MOD_CNF_TR            = 20074;
const int16_t NSECM_MOD_REJ_TR            = 20042;

const int16_t NSECM_CAN_REQ_TR            = 20070;
const int16_t NSECM_CAN_CNF_TR            = 20075;
const int16_t NSECM_CAN_REJ_TR            = 20072;

const int16_t NSECM_MKT_CNF_TR            = 20012;
const int16_t NSECM_TRD_CNF_TR            = 20222;

const int16_t KILL_SWITCH_IN = 2062;

const int16_t LB_QUERY                    = 2400;
const int16_t LB_RESPONSE                 = 2401;
const int16_t EXCH_HEARTBEAT              = 23506;


const int16_t ACCEPTED_SUCCESSFULLY       = 0;
const int16_t ORDER_CANCELLED_BY_SYSTEM   = 16388;
//const int16_t OE_ADMIN_SUSP_CAN         = 16404;
const int16_t OE_QTY_FREEZE_CAN = 16307;
//const int16_t OE_PRICE_FREEZE_CAN 			= 16308;
const int16_t DELETE_ALL_ORDERS           = 16589;
const int16_t ORDER_CANCELLED_FOR_VC      = 16795;
const int16_t ORDER_CANCELLED_FOR_SSD     = 16796;
const int16_t ORDER_CANCELLED_FOR_TPP     = 17070;
const int16_t ORDER_CANCELLED_FOR_SELF_TRADE = 17080;

const int16_t USER_ORDER_LIMIT_UPDATE_OUT = 5731;
const int16_t DEALER_LIMIT_UPDATE_OUT     = 5733;
const int16_t SPD_ORD_LIMIT_UPDATE_OUT    = 5772;
const int16_t COL   = 0001;
//const int16_t DISCONNECT_CLIENT = 0002;



// ------------------- Added For FO

const int16_t EXCH_PORTF_OUT              = 1776;
static const int16_t NSEFO_SPD_ADD_REQ             = 2100;
static const int16_t NSEFO_SPD_ADD_CNF             = 2124;
static const int16_t NSEFO_SPD_ADD_REJ             = 2154;

static const int16_t NSEFO_SPD_MOD_REQ             = 2118;
static const int16_t NSEFO_SPD_MOD_CNF             = 2136;
static const int16_t NSEFO_SPD_MOD_REJ             = 2133;

static const int16_t NSEFO_SPD_CAN_REQ             = 2106;
static const int16_t NSEFO_SPD_CAN_CNF             = 2130;
static const int16_t NSEFO_SPD_CAN_REJ             = 2127;

static const int16_t NSEFO_2LEG_ADD_REQ            = 2102;
static const int16_t NSEFO_2LEG_ADD_CNF            = 2125;
static const int16_t NSEFO_2LEG_ADD_REJ            = 2155;

static const int16_t NSEFO_2LEG_CAN_REQ            = 0000;
static const int16_t NSEFO_2LEG_CAN_CNF            = 2131;
static const int16_t NSEFO_2LEG_CAN_REJ            = 0000;

static const int16_t NSEFO_3LEG_ADD_REQ            = 2104;
static const int16_t NSEFO_3LEG_ADD_CNF            = 2126;
static const int16_t NSEFO_3LEG_ADD_REJ            = 2156;

static const int16_t NSEFO_3LEG_CAN_REQ            = 0000;
static const int16_t NSEFO_3LEG_CAN_CNF            = 2132;
static const int16_t NSEFO_3LEG_CAN_REJ            = 0000;



// -------------------  End for FO

#endif	/* NSECM_CONSTANTS_H */

