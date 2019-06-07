/* 
 * File:   nsefo_constants.h
 * Author: root
 *
 * Created on March 20, 2015, 3:31 PM
 */

#ifndef NSEFO_CONSTANTS_H
#define	NSEFO_CONSTANTS_H

#define	NSEFO_CONTRACTS_SEED  35000
#define	NSEFO_CONTRACTS_SIZE  100000
#define NSEFO_RESP_STORE_SIZE 1000
#define NSEFO_MAX_OUTGOING_RESP_COUNT 10
#define NSEFO_MAX_EXCH_REQ_COUNT 10

const int16_t NSEFO_GR_REQUEST                  = 2400;
const int16_t NSEFO_GR_RESPONSE                 = 2401;

const int16_t NSEFO_BOX_SIGN_ON_REQUEST_IN      = 23000;
const int16_t NSEFO_BOX_SIGN_ON_REQUEST_OUT     = 23001;

const int16_t NSEFO_SIGN_ON_REQUEST_IN          = 2300;
const int16_t NSEFO_SIGN_ON_REQUEST_OUT         = 2301;

const int16_t NSEFO_SYSTEM_INFORMATION_IN       = 1600;
const int16_t NSEFO_SYSTEM_INFORMATION_OUT      = 1601;

const int16_t NSEFO_UPDATE_LOCALDB_IN           = 7300;
const int16_t NSEFO_PARTIAL_SYSTEM_INFO         = 7321;
const int16_t NSEFO_UPDATE_LOCALDB_TRAILER      = 7308;

const int16_t NSEFO_EXCH_PORTF_IN               = 1775;
const int16_t NSEFO_EXCH_PORTF_OUT              = 1776;

const int16_t NSEFO_DOWNLOAD_REQUEST            = 7000;
const int16_t NSEFO_DOWNLOAD_HEADER             = 7011;
const int16_t NSEFO_DOWNLOAD_DATA               = 7021;
const int16_t NSEFO_DOWNLOAD_TRAILER            = 7031;

const int16_t NSEFO_ADD_REQ               = 2000;
const int16_t NSEFO_ADD_CNF               = 2073;
const int16_t NSEFO_ADD_REJ               = 2231;
const int16_t NSEFO_MOD_REQ               = 2040;
const int16_t NSEFO_MOD_CNF               = 2074;
const int16_t NSEFO_MOD_REJ               = 2042;
const int16_t NSEFO_CAN_REQ               = 2070;
const int16_t NSEFO_CAN_CNF               = 2075;
const int16_t NSEFO_CAN_REJ               = 2072;

const int16_t NSEFO_SL_TRIGGER            = 2212;
const int16_t NSEFO_TRD_CNF               = 2222;

const int16_t NSEFO_FREEZE                = 2170;

const int16_t NSEFO_ADD_REQ_TR            = 20000;
const int16_t NSEFO_ADD_CNF_TR            = 20073;
const int16_t NSEFO_ADD_REJ_TR            = 20231;

const int16_t NSEFO_MOD_REQ_TR            = 20040;
const int16_t NSEFO_MOD_CNF_TR            = 20074;
const int16_t NSEFO_MOD_REJ_TR            = 20042;

const int16_t NSEFO_CAN_REQ_TR            = 20070;
const int16_t NSEFO_CAN_CNF_TR            = 20075;
const int16_t NSEFO_CAN_REJ_TR            = 20072;

const int16_t NSEFO_MKT_CNF_TR            = 20012;
const int16_t NSEFO_TRD_CNF_TR            = 20222;

const int16_t NSEFO_SP_BOARD_LOT_IN             = 2100;
const int16_t NSEFO_SP_ORDER_CONFIRMATION       = 2124;
const int16_t NSEFO_SP_ORDER_ERROR              = 2154;
const int16_t NSEFO_SP_ORDER_MOD_IN             = 2118;
const int16_t NSEFO_SP_ORDER_MOD_CON_OUT        = 2136;
const int16_t NSEFO_SP_ORDER_MOD_REJ_OUT        = 2133;
const int16_t NSEFO_SP_ORDER_CANCEL_IN          = 2106;
const int16_t NSEFO_SP_ORDER_CXL_CONFIRMATION   = 2130;
const int16_t NSEFO_SP_ORDER_CXL_REJ_OUT        = 2127;

const int16_t NSEFO_TWOL_BOARD_LOT_IN = 2102;
const int16_t NSEFO_TWOL_ORDER_CONFIRMATION     = 2125;
const int16_t NSEFO_TWOL_ORDER_ERROR            = 2155;
const int16_t NSEFO_TWOL_ORDER_CXL_CONFIRMATION = 2131;

const int16_t NSEFO_THRL_BOARD_LOT_IN           = 2104;
const int16_t NSEFO_THRL_ORDER_CONFIRMATION     = 2126;
const int16_t NSEFO_THRL_ORDER_ERROR = 2156;
const int16_t NSEFO_THRL_ORDER_CXL_CONFIRMATION = 2132;
const int16_t NSEFO_KILL_SWITCH_IN = 2062;

const int16_t NSEFO_LB_QUERY                    = 2400;
const int16_t NSEFO_LB_RESPONSE                 = 2401;
const int16_t NSEFO_EXCH_HEARTBEAT              = 23506;



const int16_t NSEFO_ACCEPTED_SUCCESSFULLY             = 0;
const int16_t NSEFO_ORDER_CANCELLED_BY_SYSTEM         = 16388;
const int16_t NSEFO_OE_ADMIN_SUSP_CAN                 = 16404;
const int16_t NSEFO_OE_QTY_FREEZE_CAN                 = 16307;
const int16_t NSEFO_OE_PRICE_FREEZE_CAN               = 16308;
const int16_t NSEFO_DELETE_ALL_ORDERS                 = 16589;
const int16_t NSEFO_ORDER_CANCELLED_FOR_VC            = 16795;
const int16_t NSEFO_ORDER_CANCELLED_FOR_SSD           = 16796;
const int16_t NSEFO_ORDER_CANCELLED_FOR_TPP           = 17070;
const int16_t NSEFO_ORDER_CANCELLED_FOR_SELF_TRADE    = 17071;

const int16_t NSEFO_USER_ORDER_LIMIT_UPDATE_OUT       = 5731;
const int16_t NSEFO_DEALER_LIMIT_UPDATE_OUT           = 5733;
const int16_t NSEFO_SPD_ORD_LIMIT_UPDATE_OUT          = 5772;


const int32_t GW_NSEFO_SL_NOT_ALLOWED                 = -51001;
const int32_t GW_NSEFO_STALE_REQ                      = -51002;
const int32_t GW_NSEFO_INVALID_ORDER_OR_TOKEN         = -51003;
const int32_t GW_NSEFO_INVALID_ORDERNO                = -51004;
const int32_t GW_NSEFO_TIME_MISMATCH                  = -51005;
const int32_t GW_NSEFO_ORDERNO_MISMATCH               = -51006;
const int32_t GW_NSEFO_ORDER_COUNT_THRESHOLD_EXCEEDED = -51007;
const int32_t GW_NSEFO_TTQ_MISMATCH                   = -51008;
const int32_t GW_NSEFO_INVALID_PRICE_QTY              = -51009;   
const int32_t GW_NSEFO_EXCH_SEND_FAIL                 = -51010;
const int32_t GW_NSEFO_GW_NOT_READY_TO_RECV           = -51011;

#define APPLICATION_VERSION_X_NFO_DIRECT_GW         "2.0.0.000"


#endif	/* NSEFO_CONSTANTS_H */

