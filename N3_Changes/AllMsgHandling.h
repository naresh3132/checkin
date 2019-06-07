/* 
 * File:   AllMsgHandling.h
 * Author: NareshRK
 *
 * Created on September 5, 2018, 12:07 PM
 */

#ifndef ALLMSGHANDLING_H
#define	ALLMSGHANDLING_H


#include "TCPHandler.h"

void CanOnLogOut(int32_t dealerID,int iSockIdFO,int iSockIdCM, ConfigFile& oConfigdata);

int ValidateTokenInfo(DEALER::GENERIC_ORD_MSG* pGenOrd, ContractInfoMap& ContInfoMap);
inline int ValidateDealer(int32_t iDealer, dealerInfoMap& DealInfoMap);
int ValidateGenOrder(DEALER::GENERIC_ORD_MSG* pGenOrd, dealerInfoMap& DealInfoMap, ContractInfoMap& ContInfoMap);

bool AddOrderUI(DEALER::GENERIC_ORD_MSG* pAddOrdReq,int iFD, dealerInfoMap& DealInfoMap, ContractInfoMap& ContInfoMap,CONNINFO* pConn);

int ProcessRecvUIMessage(DATA_RECEIVED& RecvData,dealerToIdMap& DealToIdMap , dealerInfoMap& DealInfoMap, ContractInfoMap& ContInfoMap, TOKENSUBSCRIPTION& oTokenSub, ConfigFile& oConfigdata, int iSockFD_FO, int iSockFD_CM, ERRORCODEINFO [], ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain);

int SendLogonReqFO(int iSockFdFO, ConfigFile& oConfigStore, CONNINFO* pConn);
int ProcRcvdMsgFO(char* pchBuffer, int32_t& iBytesToBeProcessed, int32_t iSockId, CONNINFO* pConn,connectionMap&);
void SendTokenDepth(DATA_RECEIVED* RecvData);
//ProcessUIMsg


#endif	/* ALLMSGHANDLING_H */

