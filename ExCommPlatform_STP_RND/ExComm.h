/* 
 * File:   ExComm.h
 * Author: MuditSharma
 *
 * Created on March 7, 2017, 3:42 PM
 */

#ifndef EXCOMM_H
#define	EXCOMM_H

#include <stdint.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <map>
#include <arpa/inet.h>	//inet_addr
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <chrono>
//#include "common_structs.h"
//#include "nsefo_exch_structs.h"

using namespace std;


//const uint32_t  RECONNECTION_DELAY      = 1000000;
//const uint32_t  MAX_TCP_RECV_BUFF_SIZE  = 134217728; //128MB
//const uint32_t  SEND_BUFF_SIZE          = 2048;
//const int32_t   RECV_BUFF_SIZE          = 10485760; //10MB




typedef struct contractInfo
{
   int32_t Token;
   int32_t AssetToken;
   char Symbol[9];
   char InstumentName[6];
   char OptionType[2];
   int32_t ExpiryDate;
   int32_t MinimumLotQuantity;
   int32_t BoardLotQuantity;
   int32_t HighDPR;
   int32_t LowDPR;
   int32_t Multiplier;
   int32_t StrikePrice;

  contractInfo()
  {
    Token = 0;
    AssetToken = 0;
    MinimumLotQuantity = 0;
    BoardLotQuantity = 0;
    StrikePrice = 0;
  }
}CONTRACTINFO;

 enum contractfile
 {
    Token = 1,
    AssetToken = 2,
    MinimumLotQuantity = 31,
    BoardLotQuantity = 32,
    LowDPR = 43,
    HighDPR = 44,
    Cd_Mulitplier = 52,
    InstumentName = 3,
    Symbol = 4,
    OptionType = 5,
    Expiry = 7,
    Strike = 8
     
 }; 
 
struct storePortFolioInfo
{
  int16_t ConRev;
  int32_t Token1;
  int32_t Token2;
  int32_t Token3;
  int32_t Spread;
  int32_t Strike;
};

struct storePortFolioInfo_Box
{
  int16_t ConRev;
  int32_t Token1;
  int32_t Token2;
  int32_t Token3;
  int32_t Token4;
  int32_t Spread;
  
};


bool ReceiveFromExchange(char* pchBuffer, int32_t& iBytesRemaining, int32_t iSockId, int16_t& sConnStatus);
int16_t SendOrderToExchange(char* pchBuffer, uint32_t iDataLength, int32_t iSockId);
int16_t DisconnectFromExchange(int32_t iSockId);

//
int16_t SendGRReq(string strTMID, int32_t iUserId, int32_t iSockId);
//


int16_t ConnectToNSEFOExch(string strConnectionIP, uint32_t iConnectionPort, string strTMID, uint32_t iUserID, uint16_t sBranchId, uint32_t iExchVer, string strPassword, int32_t& iSockId, int16_t& iNoOfStreams);
int16_t GetNSEFOBODDownload(uint32_t iUserID, uint16_t iStreamId, uint64_t llExchJiffy, char* pchBuffer, int32_t iSockId);
int16_t SendLogonReq(string strTMID, int32_t iUserId, int16_t sBranchId, int32_t iExchVer, string strPassword, int32_t iSockId);
void ProcessLogonResp(char* pchBuffer);
int16_t SendSysInfoReq(int32_t iUserId, int32_t iSockId);
void ProcessSysInfoResp(char* pchBuffer, int16_t& iNoOfStreams);
int16_t SendMsgDnldReq(uint32_t iUserID, uint16_t iStreamId, uint64_t llExchJiffy, char* pchBuffer, int32_t iSockId);

void ProcessExchResp(char* pchBuffer, int32_t& iBytesToBeProcessed);

//void SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF);
void SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice);
void SendNSECMAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice);

/*NK begins*/
void SendNSEFOAddReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice);
void SendNSEFOModReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t,int32_t);
void SendNSEFOCanReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t,int32_t);

void SendNSECMAddReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice);
void SendNSECMModReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t,int32_t);
void SendNSECMCanReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t,int32_t);
void ProcessExchSLTrigResp_NonTrim(char* pchBuffer, int32_t& iBytesToBeProcessed);
void ProcessExchAddModCanCMResp_NonTrim(char* pchBuffer, int32_t& iBytesToBeProcessed);
void ProcessExchAddResp_NonTrim(char* pchBuffer, int32_t& iBytesToBeProcessed);

void ProcessTradeCMResp_Trimmed(char* pchBuffer, int32_t& iBytesToBeProcessed);
/*NK ends*/
void PorcessNSECMAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed);
void PorcessNSEFOAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed);
void SendNSEFOModReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, double dEOrd, int32_t iLMT, int32_t iTTQ);
void PorcessNSEFOModResp(char* pchBuffer, int32_t& iBytesToBeProcessed);
void SendNSEFOCanReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, double dEOrd, int32_t iLMT, int32_t iTTQ);
void PorcessNSEFOCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed);

void ProcessNSEFOErrorResp(char* pchBuffer, int32_t& iBytesToBeProcessed);

void Send2LReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF);
void Process2LAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed);
void Process2LCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed);

void Send3LReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF,std::map <int,CONTRACTINFO>&,int *,int &);
void Process3LAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed);
void Process3LCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed);

void ProcessTradeResp_Trimmed(char* pchBuffer, int32_t& iBytesToBeProcessed);
void ProcessTradeResp_NonTrimmed(char* pchBuffer, int32_t& iBytesToBeProcessed);

void ProcessMsgDnld(char* pchBuffer, int32_t& iBytesToBeProcessed);

/*Portfolio Conrev order begins*/
void PortFolioFile_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
void PortFolioFile_SendNSEFOAddReq_Recovery(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
/*Portfolio order ends*/
void SingleLeg_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
void SingleLeg_SendNSECDAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
void SingleLeg_SendNSECDModReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
void SingleLeg_SendNSECDCanReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo Pfinfo[],int);
/*Portfolio Box order begins*/
void PortFolioFile_Box_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo_Box Pfinfo[],int);
void PortFolioFile_Box_SendNSEFOAddReq_Recovery(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map <int,CONTRACTINFO>&,storePortFolioInfo_Box Pfinfo[],int);
/*Portfolio order ends*/
#endif	/* EXCOMM_H */

