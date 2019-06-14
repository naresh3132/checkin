/* 
 * File:   Thread_MarketBook.h
 * Author: NareshRK
 *
 * Created on June 23, 2017, 11:34 AM
 */

#ifndef THREAD_MARKETBOOK_H
#define	THREAD_MARKETBOOK_H

#include "CommonFunctions.h"
#include "BrodcastStruct.h"
#define NEW_ORDER_T               'N' 
#define ORDER_MODIFICATION        'M'
#define ORDER_CANCELLATION        'X'
#define TRADE_MESSAGE             'T'
#define FIX_SIZE_LEN     61


//const int16_t PRINT_ONLY = 0;


//int64_t transCount = 0, gTotalCountAllowed=0;
void Start_MarketBook(ProducerConsumerQueue<GENERIC_ORD_MSG> *pInqptr_PFStoME,std::string tbt_filename);

//typedef struct _TBTRecord{
//    char rec[FIX_SIZE_LEN];
//    
//    _TBTRecord(const char* buf){
//        memcpy(rec, buf, FIX_SIZE_LEN);
//    }
//}TBTRecord;

typedef struct _ConfigParamsMB{
    
    int32_t nThrottleRate=0;  // Per Second
    char secFilePath[512];
    char contractFilePath[512];
    char MaintainTimeDiff = 'N';
    int32_t MBCoreId = -1;
    char adjustPrice = 'N';
    int32_t Segment=0;
    int32_t processingMBSpeed = 1;
    std::string MB_filename= "";
} ConfigParamsMB1;


#endif	/* THREAD_MARKETBOOK_H */

