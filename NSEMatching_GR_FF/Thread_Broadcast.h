/* 
 * File:   Thread_Broadcast.h
 * Author: muditsharma
 *
 * Created on March 1, 2016, 6:04 PM
 */

#include "CommonFunctions.h"
#include "BrodcastStruct.h"


void StartBroadcast(ProducerConsumerQueue<BROADCAST_DATA>* qptr, const char* lszIpAddress,const char* lszBroadcast_IpAddress, int lnBroadcast_PortNumber, int iBrdcstCore,int32_t iTTLOpt, int32_t iTTLVal, short StreamID, bool startFF, const char* lszFcast_IpAddress, int lnFcast_PortNumber, int pTokenCount, int pMode);


