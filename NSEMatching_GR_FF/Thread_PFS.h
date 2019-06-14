/* 
 * File:   Thread_PFS.h
 * Author: NareshRK
 *
 * Created on May 2, 2017, 1:13 PM
 */

#ifndef THREAD_PFS_H
#define	THREAD_PFS_H
#include "CommonFunctions.h"
#include "BrodcastStruct.h"
#define NEW_ORDER_T               'N' 
#define ORDER_MODIFICATION        'M'
#define ORDER_CANCELLATION        'X'
#define TRADE_MESSAGE             'T'
#define FIX_SIZE_LEN     61


const int16_t PRINT_ONLY = 0;


//int64_t transCount = 0, gTotalCountAllowed=0;
void Start_PFS(ProducerConsumerQueue<GENERIC_ORD_MSG> *pInqptr_PFStoME,std::string tbt_filename);

typedef struct _TBTRecord{
    char rec[FIX_SIZE_LEN];
    
    _TBTRecord(const char* buf){
        memcpy(rec, buf, FIX_SIZE_LEN);
    }
}TBTRecord;

enum processingState{
    processingStateReadFile = 1,
    processingStateCheckSTP = 2,
    processingStateFlushSTP = 3,
    processingStateCheckDQ = 4,
    processingStateFlushDQ = 5
};


typedef struct _ConfigParams{
    
    int32_t nThrottleRate=0;  // Per Second
    char secFilePath[512];
    char contractFilePath[512];
    char MaintainTimeDiff = 'N';
    int32_t PFSCoreId = -1;
    char adjustPrice = 'N';
    int32_t Segment=0;
    int32_t ProcessingSpeed = 1;
    std::string tbt_filename= "";
    char cPFSWithClient='N';
    int16_t PFSDelay=0;
} ConfigParams1;




#endif	/* THREAD_PFS_H */

