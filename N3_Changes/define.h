/* 
 * File:   define.h
 * Author: NareshRK
 *
 * Created on September 6; 2018; 11:09 AM
 */

#ifndef DEFINE_H
#define	DEFINE_H

enum ORDER_STATUS
{
  NEW_PENDING       = 1, 
  OPEN              = 2, 
  REJECTED          = 3, 
  CANCELED          = 4, 
  MOD_PENDING       = 5, 
  CAN_PENDING       = 6,
  PARTIAL_FILLED    = 7,
  COMPLETELY_FILLED = 8, 
  ORDER_FREEZE      = 9
};




const int16_t  LOGIN_UI_REQ               =    11001;
const int16_t  LOGIN_UI_RES               =    11002;

const int16_t  HEARTBEAT_INFO             =    11003;

const int16_t  LOGOUT_UI_REQ              =    11005;
const int16_t  LOGOUT_UI_RES              =    11006;

const int16_t  DOWNLOAD_START             =    11011;
const int16_t  CONTRACT_DOWNLOAD_START    =    11013;
const int16_t  CONTRACT_DOWNLOAD          =    11014;
const int16_t  CONTRACT_DOWNLOAD_END      =    11015;
const int16_t  ERROR_CODES_DOWNLOAD_START =    11016;
const int16_t  ERROR_CODES_DOWNLOAD       =    11017;
const int16_t  ERROR_CODES_DOWNLOAD_END   =    11018;

const int16_t  ORDER_DOWNLOAD_REQ         =    11025;
const int16_t  ORDER_DOWNLOAD_START       =    11026;
const int16_t  ORDER_DOWNLOAD             =    11027;
const int16_t  ORDER_DOWNLOAD_END         =    11028;

const int16_t  TRADE_DOWNLOAD_REQ         =    11030;
const int16_t  TRADE_DOWNLOAD_START       =    11031;
const int16_t  TRADE_DOWNLOAD             =    11032;
const int16_t  TRADE_DOWNLOAD_END         =    11033;

const int16_t  DOWNLOAD_END               =    11012;

const int16_t  ADD_ORDER_REQ              =    11051;
const int16_t  MOD_ORDER_REQ              =    11052;
const int16_t  CAN_ORDER_REQ              =    11053;

const int16_t  UNSOLICITED_ORDER_RES      =    11054;
const int16_t  TRADE_ORDER_RES            =    11055;

const int16_t  DEPTH_REQUEST              =    11060;
const int16_t  DEPTH_RESPONSE             =    11061;


#define DISCONNECTED                  0
#define CONNECTED                     1
#define DISCONNECT_CLIENT             2
#define LOGGED_ON                     1
#define LOGGED_OFF                    0



#endif	/* DEFINE_H */

