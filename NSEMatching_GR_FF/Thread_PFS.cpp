#include <iostream>
#include <time.h>
#include <fstream>
#include "Thread_PFS.h"
#include "ConfigReader.h"
#include "Broadcast_Struct.h"
#include "spsc_atomic1.h"
#include "BrodcastStruct.h"
#include "CommonFunctions.h"
#include "perf.h"
#include <vector>
using namespace std; 

#define FIX_SIZE_LEN 61
#define     DIVISIBLE_FACTOR    100000000   //100M
#define MAX_WAIT_LIMIT 1000000 
const int32_t PFS_TOKEN_ARRAY_SIZE = 110000;

extern int16_t gStartPFSFlag;
extern int16_t gPFSWithClient = 0;

// Global Params Start
ConfigParams1 configData;
int32_t gOrderCounter = 1;
int32_t gInternalOrderCounter = 90000;

ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_PFStoME = NULL;
std::unordered_map<double, double> MapRawOrdIdToSerialTbtOrdId;
bool filterViaSubscription = false;
int8_t filteredTokens[PFS_TOKEN_ARRAY_SIZE] = {0};
GENERIC_ORD_MSG tbtMsgIn;  

timespec tCompareTime;
// In milis
int64_t lastRecvOrderTime=0;
int64_t lastRecvOrderTimeOffset=0;
int64_t lastSendOrderTime=0;
int64_t lastSendOrderTimeOffset=0;

uint64_t *pOrderTimeStamps;
uint32_t iOldestOrderTimeLocn=0;
uint64_t ullOrderThresholdInterval=1 * 1000000000;
uint32_t iOrderThresholdCount;

int64_t FileStarts=0,FileEnds=0;

const int32_t THRESHOLD_USLEEP_WAITTIME = 3; 

char prevBuffer[128];
bool isPrevBufferPopulated = false;
bool isDQPossibleCaseInitiated = false;
bool flushSTPBuffer=false;
std::vector<TBTRecord> STPBuffer;
// Global Params Ends
char logBufPFS[400];

struct DQHandler {
    char prevBuffer[128];
    char initiatorMsg[128];
    
    bool isPrevBufferPopulated = false;    
    bool patternInitiated = false;
    bool patternEnded = false;
    bool activeOrderDQ = false;
    
    bool flushBuffer = false;
    
    int nTotalQtyOfInitiatorOrder = 0;
    int nNumTrade = 0;
    int nNumMod = 0;
    
    std::vector<TBTRecord> Buffer;    
    
    int32_t getTokenNumber(char* buffer){
        
        MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
        
        if (msgHdr->cMsgType == NEW_ORDER_T || 
            msgHdr->cMsgType == ORDER_CANCELLATION ||
            msgHdr->cMsgType == ORDER_MODIFICATION)
                return ((GENERIC_ORD_MSG*)buffer)->nToken;
    
        else if (msgHdr->cMsgType == TRADE_MESSAGE)
            return ((TRD_MSG*)buffer)->nToken;
    }
    
    bool IsPossibleDQRecord(char* buffer) {
        MSG_HEADER* currRec = (MSG_HEADER*) buffer;
        
        if (currRec->cMsgType == NEW_ORDER_T || 
            currRec->cMsgType == ORDER_MODIFICATION || 
            currRec->cMsgType == TRADE_MESSAGE )
        {
            if (currRec->cMsgType == TRADE_MESSAGE)
            {
                if (((TRD_MSG*)buffer)->dblBuyOrdID <1 && ((TRD_MSG*)buffer)->dblSellOrdID <1)
                    return false;
            }
            return true;
        }
        return false;
    }
    
    bool IsPossibleDQRecords(char* prev, char* buffer, bool performSeqCheck) {
        MSG_HEADER* prevRec = (MSG_HEADER*) prev;
        MSG_HEADER* currRec = (MSG_HEADER*) buffer;
        
        if (!IsPossibleDQRecord(prev) || !IsPossibleDQRecord(buffer))
        {
            return false;
        }
        
        if (getTokenNumber(prev) != getTokenNumber(buffer))
        {
            return false;
        }
        
        if (performSeqCheck)
        {
            return (prevRec->nSeqNo+1 == currRec->nSeqNo);
        }
        return true;
    }
    
    enum DQPatternResultType{
        DQPatternNotPossible = 1,
        DQPatternNotSure = 2,
        DQPatternInitiated = 3,        
        DQPatternContinuing = 4,
        DQPatternCompleted = 5
    };
    
    enum DQBufferAction{
        DQBufferActionContinue = 1,
        DQBufferActionFLUSH = 2,
        DQBufferActionPASS = 3
    };
    
    void printBuffer(){
        for (int i=0; i<Buffer.size(); i++){
//            printTBTRecord(Buffer[i].rec, "DQ Buffer: ");
        }
    }
    
    DQBufferAction checkBufferForDQPattern(char* buffer) {
        MSG_HEADER* currRec = (MSG_HEADER*) buffer;
        
        if (Buffer.size() == 0) {
            
            if (currRec->cMsgType == NEW_ORDER_T || 
                currRec->cMsgType == ORDER_MODIFICATION) 
            {
                Buffer.push_back(buffer);
                return DQBufferActionContinue;
            }
            else
                return DQBufferActionPASS;
        }
        
        else if (Buffer.size() > 0 && IsPossibleDQRecord(buffer))
        {
            MSG_HEADER* firstRec = (MSG_HEADER*) Buffer[0].rec;
            if (((GENERIC_ORD_MSG*)firstRec)->cMsgType != ORDER_MODIFICATION &&
                ((GENERIC_ORD_MSG*)firstRec)->cMsgType != NEW_ORDER_T )
            {
                return DQBufferActionFLUSH;
            }
            
            if (IsPossibleDQRecords(Buffer[Buffer.size()-1].rec, buffer, true))
            {
                
                MSG_HEADER* currRec = (MSG_HEADER*) buffer;
                
                // Check if DQ Pattern is breaking due to addition of the rec
                // 1. SeqNum and token checks are done
                // 2. if Msg type is New and Cancel
                if (currRec->cMsgType == NEW_ORDER_T || currRec->cMsgType == ORDER_CANCELLATION)
                {
                    if (patternInitiated)
                    {
                        patternEnded = true;
                        #ifdef __PFS_LOG__  
                        snprintf(logBufPFS, 400, "Thread_PFS|Pattern Ended: Due to :%c",currRec->cMsgType );
                        Logger::getLogger().log(DEBUG, logBufPFS);  
                        #endif
//                        std::cout<<"Pattern Ended: Due to :" << currRec->cMsgType << std::endl;
                    }
                    return DQBufferActionFLUSH;
                }
                
                if (currRec->cMsgType == TRADE_MESSAGE){
                    
                    if (((GENERIC_ORD_MSG*)firstRec)->cMsgType != ORDER_MODIFICATION &&
                        ((GENERIC_ORD_MSG*)firstRec)->cMsgType != NEW_ORDER_T )
                    {
                        
                      #ifdef __PFS_LOG__  
                      snprintf(logBufPFS, 400, "Thread_PFS|DQ Checking Something is wrong, Got %c As first msg in DQ Buffer",((GENERIC_ORD_MSG*)firstRec)->cMsgType );
                      Logger::getLogger().log(DEBUG, logBufPFS);  
                      #endif
//                      std::cout<<"DQ Checking Something is wrong, Got " << ((GENERIC_ORD_MSG*)firstRec)->cMsgType 
//                                 << "As first msg in DQ Buffer" << std::endl;
                        
//                        printTBTRecord(Buffer[0].rec, "DQ Error");
                        exit(-1);
                    }
                    
                    if ((((TRD_MSG*)currRec)->dblBuyOrdID != ((GENERIC_ORD_MSG*)firstRec)->dblOrdID) &&
                        (((TRD_MSG*)currRec)->dblSellOrdID != ((GENERIC_ORD_MSG*)firstRec)->dblOrdID)) 
                    {
                        if (patternInitiated)
                        {
                            patternEnded = true;
                             #ifdef __PFS_LOG__  
                              snprintf(logBufPFS, 400, "Thread_PFS|Pattern Ended: Due to :",currRec->cMsgType );
                              Logger::getLogger().log(DEBUG, logBufPFS);  
                             #endif
//                            std::cout<<"Pattern Ended: Due to :" << currRec->cMsgType << std::endl;
                        }
                        if (Buffer.size() > 1)
                            printBuffer();
                        return DQBufferActionFLUSH;
                    }
                }
                
                Buffer.push_back(buffer);
                
                firstRec = (MSG_HEADER*) Buffer[0].rec;
                // Check for DQPattern Start confirmation
                // Checking for MT
                if (!patternInitiated && Buffer.size() > 2 && 
                     currRec->cMsgType == TRADE_MESSAGE)
                {
                    MSG_HEADER* secLastRec = (MSG_HEADER*) Buffer[Buffer.size()-2].rec;
                    MSG_HEADER* lastRec = (MSG_HEADER*) Buffer[Buffer.size()-1].rec;
                    
                    if ((firstRec->cMsgType == NEW_ORDER_T || firstRec->cMsgType == ORDER_MODIFICATION) &&
                         secLastRec->cMsgType == ORDER_MODIFICATION && 
                         lastRec->cMsgType == TRADE_MESSAGE && 
                        // check that orderMod and initiator order is opp side
//                        (((GENERIC_ORD_MSG*)firstRec)->cOrdType != ((GENERIC_ORD_MSG*)secLastRec)->cOrdType) && 
                        // check that initiator order is present in one of the traded order Id
                        (((GENERIC_ORD_MSG*)firstRec)->dblOrdID == ((TRD_MSG*)lastRec)->dblBuyOrdID || ((GENERIC_ORD_MSG*)firstRec)->dblOrdID == ((TRD_MSG*)lastRec)->dblSellOrdID)
                        )
                    {
                        if (((GENERIC_ORD_MSG*)firstRec)->cOrdType == ((GENERIC_ORD_MSG*)secLastRec)->cOrdType)
                        {
                            activeOrderDQ = true;
                        }
//                        std::cout << "DQ Pattern Initiated:" << std::endl;
                        #ifdef __PFS_LOG__  
                        snprintf(logBufPFS, 400, "Thread_PFS|DQ Pattern Initiated:" );
                        Logger::getLogger().log(DEBUG, logBufPFS);  
                       #endif
                        patternInitiated = true;
                        nTotalQtyOfInitiatorOrder = ((GENERIC_ORD_MSG*)firstRec)->nQty;                        
                    }
                }
                return DQBufferActionContinue;
            }
            else
            {
                if (patternInitiated)
                {
                    patternEnded = true;
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|Pattern Ended: Due to SeqNo:" );
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                   #endif
//                    std::cout<<"Pattern Ended: Due to SeqNo:" << std::endl;
                }
                return DQBufferActionFLUSH;
            }
        }
        else{
            if (patternInitiated)
                patternEnded = true;
            return DQBufferActionFLUSH;
        }
    }
    
    bool processBufferDQPrevention() 
    {
        if (patternInitiated && patternEnded) 
        {
//            std::cout<<"Preventing DQ" << std::endl;
            #ifdef __PFS_LOG__  
            snprintf(logBufPFS, 400, "Thread_PFS|Preventing DQ" );
            Logger::getLogger().log(DEBUG, logBufPFS);  
           #endif
            if (activeOrderDQ)
            {
                bool incr = true;
                int tradedQty = nTotalQtyOfInitiatorOrder;
                for (auto it=Buffer.begin(); it!=Buffer.end(); )
                {
                    incr = true;
                    MSG_HEADER* currRec = (MSG_HEADER*) it->rec;
                    
                    if (it != Buffer.begin() && currRec->cMsgType == ORDER_MODIFICATION)
                    {
                        nTotalQtyOfInitiatorOrder = ((GENERIC_ORD_MSG*)currRec)->nQty;
                        it = Buffer.erase(it);
                        incr = false;
                    }
                    else if (currRec->cMsgType == TRADE_MESSAGE)
                    {
                        tradedQty = ((TRD_MSG*)currRec)->nTradeQty;
                        memcpy(currRec, Buffer[0].rec, FIX_SIZE_LEN);
                        ((GENERIC_ORD_MSG*)currRec)->cMsgType = NEW_ORDER_T;

                        // check for any pending order Qty
                        if (it == Buffer.end())
                        {
                            ((GENERIC_ORD_MSG*)currRec)->nQty = nTotalQtyOfInitiatorOrder;
                            nTotalQtyOfInitiatorOrder -= tradedQty;
                        }
                        else{
                            nTotalQtyOfInitiatorOrder -= tradedQty;
                            ((GENERIC_ORD_MSG*)currRec)->nQty = tradedQty;
                            /*NK*/
                            ((GENERIC_ORD_MSG*)currRec)->dblOrdID = DIVISIBLE_FACTOR+gOrderCounter++;
//                            ((GENERIC_ORD_MSG*)currRec)->dblOrdID = gInternalOrderCounter++;
                            /*NK*/
                        }
                    }
                    if (incr)
                    {
                        it++;
                    }
                }
            }
            else
            {
                int tradedQty = nTotalQtyOfInitiatorOrder;
                for (int i=0; i<Buffer.size(); i++){
                    MSG_HEADER* currRec = (MSG_HEADER*) Buffer[i].rec;
                    if (currRec->cMsgType == TRADE_MESSAGE)
                    {
                        tradedQty = ((TRD_MSG*)currRec)->nTradeQty;
                        memcpy(currRec, Buffer[0].rec, FIX_SIZE_LEN);
                        ((GENERIC_ORD_MSG*)currRec)->cMsgType = NEW_ORDER_T;

                        if (i == Buffer.size()-1)
                        {
                            ((GENERIC_ORD_MSG*)currRec)->nQty = nTotalQtyOfInitiatorOrder;
                            nTotalQtyOfInitiatorOrder -= tradedQty;
                        }
                        else{
                            nTotalQtyOfInitiatorOrder -= tradedQty;
                            ((GENERIC_ORD_MSG*)currRec)->nQty = tradedQty;
                            /*NK*/
                            ((GENERIC_ORD_MSG*)currRec)->dblOrdID = DIVISIBLE_FACTOR+gOrderCounter++;
//                            ((GENERIC_ORD_MSG*)currRec)->dblOrdID = gInternalOrderCounter++;
                            /*NK*/
                        }
                    }
                }

                if (nTotalQtyOfInitiatorOrder > 0)
                {
//                    std::cout<<"Should not be here" << std::endl;
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|Should not be here" );
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                    #endif
                }
            }
            Buffer.erase(Buffer.begin());
        }
    }
    
    // Return true if current buffer is not allowed to pass on
    bool IsDQPatternPossible(char* buffer)
    {
        bool result = false;
//        std::cout << "Checking for DQ for Seq No:" << ((MSG_HEADER*) buffer)->nSeqNo << std::endl;
        #ifdef __PFS_LOG__  
        snprintf(logBufPFS, 400, "Thread_PFS|Checking for DQ for Seq No:%d",((MSG_HEADER*) buffer)->nSeqNo );
        Logger::getLogger().log(DEBUG, logBufPFS);  
       #endif
        DQBufferAction retVal = checkBufferForDQPattern(buffer);
        
        if (retVal == DQBufferActionPASS)
            return false;
        else if (retVal == DQBufferActionFLUSH)
        {
            if (patternInitiated && Buffer.size() > 1)
                printBuffer();
            
            processBufferDQPrevention();
            flushBuffer = true;
            
            memcpy(prevBuffer, buffer, FIX_SIZE_LEN);
            isPrevBufferPopulated = true;
            
            patternEnded = patternInitiated = false;
            activeOrderDQ = false;
            
            return true;
        }
        else
            return true;
    }
};

void readConfigParams(ConfigReader& lcConfigReader) {
    configData.Segment = atoi(lcConfigReader.getProperty("SEGMENT").c_str());
    if (configData.Segment <= 0)
    {
        configData.Segment = 1;
    }
    configData.ProcessingSpeed = atoi(lcConfigReader.getProperty("PROCESSING_SPEED").c_str());

    strcpy(configData.secFilePath, lcConfigReader.getProperty("SECURITY_FILEPATH").c_str());
    strcpy(configData.contractFilePath, lcConfigReader.getProperty("CONTRACT_FILEPATH").c_str());
    configData.nThrottleRate = atoi(lcConfigReader.getProperty("ORDER_THRESHOLD_COUNT").c_str());
    configData.PFSCoreId = atoi(lcConfigReader.getProperty("PFS_CORE_ID").c_str());
    configData.MaintainTimeDiff = lcConfigReader.getProperty("MAINTAIN_TIME_DIFF").c_str()[0];
    configData.adjustPrice = lcConfigReader.getProperty("ADJUST_PRICE").c_str()[0];
    if (configData.nThrottleRate<=0)
        configData.nThrottleRate = 40;
    iOrderThresholdCount = configData.nThrottleRate;
    lcConfigReader.dump();
    std::cout << "Throttling rate = " << configData.nThrottleRate << std::endl;
    configData.tbt_filename = lcConfigReader.getProperty("TBT_filename");
    ullOrderThresholdInterval = ullOrderThresholdInterval/configData.nThrottleRate; // Temp for RW
    configData.cPFSWithClient = lcConfigReader.getProperty("PFSWithClient").c_str()[0];
    configData.PFSDelay = atoi(lcConfigReader.getProperty("PFSDelayInSec").c_str());
}

bool ValidateOrderCountThreshold(int& sleepInterval) {
    clock_gettime(CLOCK_REALTIME, &tCompareTime);
    uint64_t ullCurrTime = (tCompareTime.tv_sec * 1000000000) + tCompareTime.tv_nsec;
    
    //Validating only 1st Timestamp for MultiLegOrders also to avoid computational overhead, So Configured OrderRate should be 2-5 Orders more than actual
    if ( (iOldestOrderTimeLocn == 0 && (ullCurrTime - pOrderTimeStamps[iOrderThresholdCount-1] > ullOrderThresholdInterval) ) ||
         (iOldestOrderTimeLocn>0 && (ullCurrTime - pOrderTimeStamps[iOldestOrderTimeLocn-1] > ullOrderThresholdInterval)) ) {
        //Populate 1st Entry for all type of Reqs

//        std::cout << "ValidateOrderCountThreshold success:" 
//                  << iOldestOrderTimeLocn << "|"
//                  << ullCurrTime << "|"
//                  << pOrderTimeStamps[iOldestOrderTimeLocn] << "|"
//                  << ullOrderThresholdInterval << "|"
//                  << std::endl;
        
        pOrderTimeStamps[iOldestOrderTimeLocn] = ullCurrTime;
        iOldestOrderTimeLocn++;
        
        if (iOldestOrderTimeLocn == iOrderThresholdCount) {
            iOldestOrderTimeLocn = 0;
        }
        sleepInterval=0;
        return true;
    } else {
        if (iOldestOrderTimeLocn == 0)
        {
            sleepInterval = ullCurrTime - pOrderTimeStamps[iOrderThresholdCount-1];
            sleepInterval = ullOrderThresholdInterval - sleepInterval;
//            std::cout << "ValidateOrderCountThreshold Failed:" 
//                  << iOldestOrderTimeLocn << "|"
//                  << ullCurrTime << "|"
//                  << pOrderTimeStamps[iOrderThresholdCount-1] << "|"
//                  << ullOrderThresholdInterval << "|" 
//                  << sleepInterval << "|" 
//                  << std::endl;
        }
        else
        {
            sleepInterval = ullCurrTime - pOrderTimeStamps[iOldestOrderTimeLocn-1];
            sleepInterval = ullOrderThresholdInterval - sleepInterval;
//            std::cout << "ValidateOrderCountThreshold Failed:" 
//                  << iOldestOrderTimeLocn << "|"
//                  << ullCurrTime << "|"
//                  << pOrderTimeStamps[iOldestOrderTimeLocn-1] << "|"
//                  << ullOrderThresholdInterval << "|" 
//                  << sleepInterval << "|" 
//                  << std::endl;
        }
        
        return false;
    }
}

void sendOrderToME(GENERIC_ORD_MSG& tbtMsgIn) {
    int sleepInt = 0;
    uint64_t ullTempCurrTime;
    while(true)
    {
        if (ValidateOrderCountThreshold(sleepInt))
        {
            uint64_t ullCurrTime;
//            while ( true)
//            {
                clock_gettime(CLOCK_REALTIME, &tCompareTime);
//                ullCurrTime = (tCompareTime.tv_sec * 1000) + tCompareTime.tv_nsec/1000000;
                ullCurrTime = (tCompareTime.tv_sec * 1000000000) + tCompareTime.tv_nsec;
                lastSendOrderTimeOffset = ullCurrTime-lastSendOrderTime;
                
                
//                std::cout <<"Time Check 1::"<< lastRecvOrderTimeOffset<< "|" << lastSendOrderTimeOffset<< "|" << lastRecvOrderTimeOffset - lastSendOrderTimeOffset<<std::endl;
                
                if (configData.MaintainTimeDiff == 'Y' && lastRecvOrderTime!=0 && lastSendOrderTimeOffset < lastRecvOrderTimeOffset)
                {
//                    std::cout << "Waiting....Consecutive time gap " 
//                              << lastSendOrderTimeOffset << "|"
//                              << lastRecvOrderTimeOffset << "|"
//                              << std::endl;
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|Waiting....Consecutive time gap  %ld|%ld",lastSendOrderTimeOffset, lastRecvOrderTimeOffset);
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                   #endif
                    
//                    if ( lastRecvOrderTimeOffset - lastSendOrderTimeOffset > 10)
//                    {
//                        std::cout << "Waiting....MAX delay 10 millis " 
//                              << std::endl;
//                        usleep(10000);
//                    }
//                    else
//                    {
//                        usleep((lastRecvOrderTimeOffset-lastSendOrderTimeOffset) * 1000);
//                          
//                    }
                    
                    if ( lastRecvOrderTimeOffset - lastSendOrderTimeOffset > MAX_WAIT_LIMIT)  
                    {
//                        std::cout << "Waiting....MAX delay 1 milli " 
//                              << std::endl;
                      #ifdef __PFS_LOG__ 
                        snprintf(logBufPFS, 400, "Thread_PFS|Waiting....MAX delay 1 milli ");
                        Logger::getLogger().log(DEBUG, logBufPFS); 
                      #endif
                        usleep(1000);
                    }
                    else if ( lastRecvOrderTimeOffset - lastSendOrderTimeOffset < 1000)  
                    {
                        
                    }
                    else
                    {
                       
                         usleep((lastRecvOrderTimeOffset-lastSendOrderTimeOffset)/1000 );
                          
                    }
                    
//                    continue;
                }

            #ifdef __PFS_LOG__
              snprintf(logBufPFS, 400, "Thread_PFS|IN:SeqNo:%d|MsgType:%c|OrderId:%ld|OrderType:%c|Price:%d|Oty:%d", tbtMsgIn.header.nSeqNo,tbtMsgIn.cMsgType,(int64_t)tbtMsgIn.dblOrdID,tbtMsgIn.cOrdType,tbtMsgIn.nPrice,tbtMsgIn.nQty);
              Logger::getLogger().log(DEBUG, logBufPFS);    
            #endif
//            std::cout<<"IN::"<<tbtMsgIn.header.nSeqNo<<"|"<<tbtMsgIn.cMsgType<<"|"<<(int64_t)tbtMsgIn.dblOrdID<<"|"<<tbtMsgIn.cOrdType<<"|"<<tbtMsgIn.nPrice<<"|"<<tbtMsgIn.nQty<<std::endl;
            
            while (Inqptr_PFStoME->enqueue(tbtMsgIn) == false)
            {
//              #ifdef __PFS_LOG__
              snprintf(logBufPFS, 400, "Thread_PFS|Unable to enqueue the Message of seqNo %d", tbtMsgIn.header.nSeqNo);
              Logger::getLogger().log(DEBUG, logBufPFS);    
//              #endif
//              std::cout << "Enqueue failed" << std::endl;
              usleep(1);
            }                

            lastSendOrderTime = ullCurrTime;
            break;

        }
        else
        {
//            std::cout << "Waiting....Threashold" << std::endl;

            if (sleepInt > THRESHOLD_USLEEP_WAITTIME * 1000)
            {
//              snprintf(logBufPFS, 400, "ValidateOrderCountThreshold: sleepInt %d sleep %d",sleepInt,sleepInt/1000);
//              Logger::getLogger().log(DEBUG, logBufPFS); 
                usleep(sleepInt/1000);
            
            }
            else
            {
//              snprintf(logBufPFS, 400, "ValidateOrderCountThreshold: sleepInt %d THRESHOLD_USLEEP_WAITTIME %d",sleepInt,THRESHOLD_USLEEP_WAITTIME);
//              Logger::getLogger().log(DEBUG, logBufPFS);
              usleep(THRESHOLD_USLEEP_WAITTIME);
            }
        }
    }
}

bool isTokenAllowed(int nToken){
//bool checkTokenForFilteration(int nToken){
//  std::cout<<nToken<<std::endl;
    if (filterViaSubscription)
        return (filteredTokens[nToken]==1);
    else 
        return true;
}

bool isTokenAllowed(char* buffer){
    
    MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
    
    if (msgHdr->cMsgType == NEW_ORDER_T || 
        msgHdr->cMsgType == ORDER_CANCELLATION ||
        msgHdr->cMsgType == ORDER_MODIFICATION)
            return isTokenAllowed(((GENERIC_ORD_MSG*)buffer)->nToken);
    
    else if (msgHdr->cMsgType == TRADE_MESSAGE)
            return isTokenAllowed(((TRD_MSG*)buffer)->nToken);
    
    else 
        return false;
}

bool processMessage(char *buffer)
{
  uint64_t llTimestamp = ((CompositeBcastMsg*)buffer)->tv.tv_sec*1000000000 + ((CompositeBcastMsg*)buffer)->tv.tv_nsec;
  
  
  MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
  int count = 0;
  int32_t iToken = 0;
  
  switch (msgHdr->cMsgType) 
  {
      case NEW_ORDER_T:
      case ORDER_CANCELLATION:
      case ORDER_MODIFICATION:
      {
        iToken = ((GENERIC_ORD_MSG*) buffer)->nToken;
        
        if (!isTokenAllowed(iToken))
            return true;  
        
        memcpy(&tbtMsgIn,buffer,sizeof(GENERIC_ORD_MSG));
        
        clock_gettime(CLOCK_REALTIME, &tCompareTime);
//        if (((GENERIC_ORD_MSG*) buffer)->lTimeStamp - lastRecvOrderTime > 0)
//            lastRecvOrderTimeOffset =  ((GENERIC_ORD_MSG*) buffer)->lTimeStamp - lastRecvOrderTime;
//        else
//            lastRecvOrderTimeOffset = 0;
        
        if (llTimestamp - lastRecvOrderTime > 0)
            lastRecvOrderTimeOffset =  llTimestamp - lastRecvOrderTime;
        else
            lastRecvOrderTimeOffset = 0;
        
        sendOrderToME(tbtMsgIn);
        lastRecvOrderTime = llTimestamp;
        
      
        return true;
      }
      case TRADE_MESSAGE:
      {
          if ((((TRD_MSG*) buffer)->dblBuyOrdID < 1 && ((TRD_MSG*) buffer)->dblSellOrdID < 1 ) || !isTokenAllowed(((TRD_MSG*) buffer)->nToken) )
          {
              return true;
          }

          // temp Approach ends
          if ((((TRD_MSG*) buffer)->dblBuyOrdID > 1 && ((TRD_MSG*) buffer)->dblSellOrdID > 1 ) || !isTokenAllowed(((TRD_MSG*) buffer)->nToken) )
          {
              return true;
          }
          // temp Approach ends

          iToken = ((TRD_MSG*) buffer)->nToken;
          if (!isTokenAllowed(iToken))
            return true;
          
          double newOrderId = DIVISIBLE_FACTOR+gOrderCounter++;
          bool isBuyOrder = (((TRD_MSG*) buffer)->dblBuyOrdID<1 && ((TRD_MSG*) buffer)->dblSellOrdID>1);
          tbtMsgIn.nToken = ((TRD_MSG*) buffer)->nToken;
          tbtMsgIn.cMsgType='I';
          tbtMsgIn.cOrdType= (isBuyOrder)?'B':'S';
          if(((TRD_MSG*) buffer)->dblBuyOrdID > 1)
          {
            tbtMsgIn.dblOrdID=newOrderId;
          }
          else
          {
            tbtMsgIn.dblOrdID=newOrderId;
          }
          
          tbtMsgIn.header.nSeqNo = ((TRD_MSG*) buffer)->header.nSeqNo;
          tbtMsgIn.header.wMsgLen = ((TRD_MSG*) buffer)->header.wMsgLen;
          tbtMsgIn.header.wStremID = ((TRD_MSG*) buffer)->header.wStremID;
          tbtMsgIn.nPrice = ((TRD_MSG*) buffer)->nTradePrice;
          tbtMsgIn.nQty = ((TRD_MSG*) buffer)->nTradeQty;
          
          
//          lastRecvOrderTimeOffset =  ((TRD_MSG*) buffer)->lTimestamp - lastRecvOrderTime;
          lastRecvOrderTimeOffset =  llTimestamp - lastRecvOrderTime;
          
          sendOrderToME(tbtMsgIn);

          lastRecvOrderTime = llTimestamp;
          
//          sendOrderToME(tbtMsgIn);

          
          return true;
      }
      default:
      {
          std::cout << "ERROR :Ignoring un-handled message :" << msgHdr->cMsgType << std::endl;
          #ifdef __PFS_LOG__  
          snprintf(logBufPFS, 400, "Thread_PFS|ERROR :Ignoring un-handled message :%c", msgHdr->cMsgType );
          Logger::getLogger().log(DEBUG, logBufPFS);  
          #endif
          return false;            
      }
  }
  return true;
}


//void processing(const char* TBT_filename)
//{
//  ifstream tbtFile(TBT_filename, ios::in | ios::binary);
//  if (tbtFile.is_open()) 
//  {
//    while(1)
//    {
//      char buffer[128];
//      tbtFile.read(buffer, FIX_SIZE_LEN);
//      if (!tbtFile) 
//      {
//          std::cout << "ERROR : Only " << tbtFile.gcount() << "bytes were able to be read out of " << FIX_SIZE_LEN << std::endl;
////          std::cout << "Data File Completed Total Record:" << transCount << std::endl;        
//
//          exit(0);
//      }
//      
//      processMessage(buffer);
//    }
//  }
//    
//}


bool isMessageAllowed(char* buffer){
    
    MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
    
    if (msgHdr->cMsgType == NEW_ORDER_T || 
        msgHdr->cMsgType == ORDER_CANCELLATION ||
        msgHdr->cMsgType == ORDER_MODIFICATION || 
        (msgHdr->cMsgType == TRADE_MESSAGE //&& 
//         (( ((TRD_MSG*)buffer)->dblSellOrdID>1 && ((TRD_MSG*)buffer)->dblBuyOrdID<1) || 
//          (( ((TRD_MSG*)buffer)->dblSellOrdID<1 && ((TRD_MSG*)buffer)->dblBuyOrdID>1)) )
        ))
            return true;
    else 
        return false;
}

bool updateOrderNumber(char* buffer){

    bool result = false;
    MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
    
    switch (msgHdr->cMsgType)
    {
        case NEW_ORDER_T:
        {
            if (((GENERIC_ORD_MSG*)buffer)->dblOrdID > 1)
            {
                double newOrderId = DIVISIBLE_FACTOR+gOrderCounter++;
                std::pair<double, double> prMapRawOrdIdToSerialTbtOrdId;

                prMapRawOrdIdToSerialTbtOrdId.first = ((GENERIC_ORD_MSG*)buffer)->dblOrdID;
                prMapRawOrdIdToSerialTbtOrdId.second = newOrderId;
                MapRawOrdIdToSerialTbtOrdId.insert(prMapRawOrdIdToSerialTbtOrdId);
//                std::cout << "N:RAW:SERIAL=" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID << ":" << (int64_t)newOrderId << std::endl;
                #ifdef __PFS_LOG__
                snprintf(logBufPFS, 400, "Thread_PFS|N:RAW:SERIAL=%ld:%ld", (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,(int64_t)newOrderId);
                Logger::getLogger().log(DEBUG, logBufPFS); 
                #endif
                ((GENERIC_ORD_MSG*)buffer)->dblOrdID = newOrderId;
                result = true;
            }
            break;
        }
        case ORDER_CANCELLATION:
        case ORDER_MODIFICATION:
        {
            if (((GENERIC_ORD_MSG*)buffer)->dblOrdID > 1)
            {
                auto val = MapRawOrdIdToSerialTbtOrdId.find(((GENERIC_ORD_MSG*)buffer)->dblOrdID);
                if (val != MapRawOrdIdToSerialTbtOrdId.end()) {
//                    std::cout << ((GENERIC_ORD_MSG*)buffer)->cMsgType << ":RAW:SERIAL=" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID << ":" << (int64_t)val->second << std::endl;
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|%c:RAW:SERIAL=%ld:%ld",((GENERIC_ORD_MSG*)buffer)->cMsgType, (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufPFS); 
                    #endif
                    ((GENERIC_ORD_MSG*)buffer)->dblOrdID = val->second;
                    result = true;
                }
                else{
//                    std::cout << "ERROR :Match for RAW Order ID" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID 
//                              << "  Not Found for "  << ((GENERIC_ORD_MSG*)buffer)->cMsgType << std::endl;
                    
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|ERROR :Match for RAW Order ID :%ld Not Found for : %c " , (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,((GENERIC_ORD_MSG*)buffer)->cMsgType );
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                    #endif
                    result = false;
                }
            }
            break;
        }
        case TRADE_MESSAGE:
        {
            if (((TRD_MSG*)buffer)->dblBuyOrdID > 1){
                auto val = MapRawOrdIdToSerialTbtOrdId.find(((TRD_MSG*)buffer)->dblBuyOrdID);
                if (val != MapRawOrdIdToSerialTbtOrdId.end()) {
//                    std::cout << "T:RAW:SERIAL=" << (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID << ":" << (int64_t)val->second << std::endl;
                  #ifdef __PFS_LOG__  
                  snprintf(logBufPFS, 400, "Thread_PFS|T:RAW:SERIAL=%ld:%ld", (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                  #endif
                    ((TRD_MSG*)buffer)->dblBuyOrdID = val->second;
                    result = true;
                }
                else{
//                    std::cout << "ERROR :TRD:Match for RAW Buy Order ID : " << (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID 
//                              << ", Not Found, sending 0" << std::endl;
                    
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|ERROR :TRD:Match for RAW Buy Order ID : :%ld , Not Found, sending 0 " , (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID );
                    Logger::getLogger().log(DEBUG, logBufPFS);  
                    #endif

                    ((TRD_MSG*)buffer)->dblBuyOrdID = 0;
                    result = true;
//                    break; //to-do remove
                }
            }
            else
            {
                result = true;
            }

            if (((TRD_MSG*)buffer)->dblSellOrdID > 1)
            {
                auto val = MapRawOrdIdToSerialTbtOrdId.find(((TRD_MSG*)buffer)->dblSellOrdID);
                if (val != MapRawOrdIdToSerialTbtOrdId.end()) {
//                    std::cout << "T:RAW:SERIAL=" << (int64_t)((TRD_MSG*)buffer)->dblSellOrdID << ":" << (int64_t)val->second << std::endl;
                  #ifdef __PFS_LOG__  
                  snprintf(logBufPFS, 400, "Thread_PFS|T:RAW:SERIAL=%ld:%ld", (int64_t)((TRD_MSG*)buffer)->dblSellOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufPFS); 
                   #endif
                    ((TRD_MSG*)buffer)->dblSellOrdID = val->second;
                    result = true;
                }
                else{
//                    std::cout << "ERROR :TRD:Match for RAW Sell Order ID : " << (int64_t)((TRD_MSG*)buffer)->dblSellOrdID 
//                              << ", Not Found, sending 0" << std::endl;
                    #ifdef __PFS_LOG__  
                    snprintf(logBufPFS, 400, "Thread_PFS|ERROR :TRD:Match for RAW Sell Order ID : :%ld , Not Found, sending 0", (int64_t)((TRD_MSG*)buffer)->dblSellOrdID);
                    Logger::getLogger().log(DEBUG, logBufPFS); 
                   #endif
                    ((TRD_MSG*)buffer)->dblSellOrdID = 0;
                    result = true;
                }
            }
            else 
                result = true;
            break;
        }
    }
    return result;
}

bool IsSelfTradePossible(char* buffer){
    
//    std::cout << "Reading From File" << std::endl;
  #ifdef __PFS_LOG__  
  snprintf(logBufPFS, 400, "Thread_PFS|Reading From File");
  Logger::getLogger().log(DEBUG, logBufPFS);
  #endif
    bool result = false;
    
    // Check for fresh Record
    if (isPrevBufferPopulated==false){
        result = true;
        memcpy(prevBuffer, buffer, FIX_SIZE_LEN);
        isPrevBufferPopulated=true;
        
//        printTBTRecord(buffer, "Saving in STP Buffer 1:");
    }
    
    else {
        MSG_HEADER* prevRec = (MSG_HEADER*) prevBuffer;
        MSG_HEADER* currRec = (MSG_HEADER*) buffer;
        
        // case 1
        if (prevRec->cMsgType == NEW_ORDER_T && 
            currRec->cMsgType == ORDER_CANCELLATION && 
            prevRec->nSeqNo+1 == currRec->nSeqNo && 
            ((GENERIC_ORD_MSG*)prevRec)->nToken == ((GENERIC_ORD_MSG*)currRec)->nToken &&
            ((GENERIC_ORD_MSG*)prevRec)->dblOrdID == ((GENERIC_ORD_MSG*)currRec)->dblOrdID
           )
        {
            // Ignore Both
            result = true;
//            printTBTRecord(prevBuffer, "Ignored due to STP 1:");
//            printTBTRecord(buffer, "Ignored due to STP 1:");
            isPrevBufferPopulated = false;
        }

        // case 2
        else if (prevRec->cMsgType == NEW_ORDER_T && 
            currRec->cMsgType == ORDER_CANCELLATION && 
            prevRec->nSeqNo+1 == currRec->nSeqNo && 
            ((GENERIC_ORD_MSG*)prevRec)->nToken == ((GENERIC_ORD_MSG*)currRec)->nToken &&
            ((GENERIC_ORD_MSG*)prevRec)->dblOrdID != ((GENERIC_ORD_MSG*)currRec)->dblOrdID &&
            ((GENERIC_ORD_MSG*)prevRec)->cOrdType != ((GENERIC_ORD_MSG*)currRec)->cOrdType
           )
        {
            // send Both
            result = false;
            isPrevBufferPopulated = false;
            
            // send n+1 before n
            STPBuffer.push_back(buffer);  
//            std::cout << "Current Buffer size1:" << STPBuffer.size() << std::endl;
            #ifdef __PFS_LOG__
            snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size1:%d", STPBuffer.size());
            Logger::getLogger().log(DEBUG, logBufPFS); 
            #endif
            STPBuffer.push_back(prevBuffer);
//            std::cout << "Current Buffer size2:" << STPBuffer.size() << std::endl;
            #ifdef __PFS_LOG__
            snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size2:%d", STPBuffer.size());
            Logger::getLogger().log(DEBUG, logBufPFS); 
            #endif
            flushSTPBuffer=true;
            
//            printTBTRecord(buffer, "ReSequenced due to STP 2");
//            printTBTRecord(prevBuffer, "ReSequenced Due to STP 2");
        }
        
        // case 3 a
        else if (prevRec->cMsgType == NEW_ORDER_T && 
                currRec->cMsgType == TRADE_MESSAGE && 
                prevRec->nSeqNo+1+STPBuffer.size() == currRec->nSeqNo && 
                ((GENERIC_ORD_MSG*)prevRec)->nToken == ((TRD_MSG*)currRec)->nToken 
           )
        {
            // save new            
            if (STPBuffer.size() == 0){
                STPBuffer.push_back(prevBuffer);
//                std::cout << "Current Buffer size3:" << STPBuffer.size() << std::endl;
                #ifdef __PFS_LOG__
                snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size3:%d", STPBuffer.size());
                Logger::getLogger().log(DEBUG, logBufPFS);
                #endif
//                printTBTRecord(prevBuffer, "Saving New : STP3:");
            }
            
            // save trade
            STPBuffer.push_back(buffer);        
//            std::cout << "Current Buffer size4:" << STPBuffer.size() << std::endl;
            #ifdef __PFS_LOG__
            snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size4:%d", STPBuffer.size());
            Logger::getLogger().log(DEBUG, logBufPFS);
            #endif
//            printTBTRecord(buffer, "Saving Trade : STP3:");
            result = true;
        }        
        // case 3 b        
        else if (!isDQPossibleCaseInitiated && 
                 prevRec->cMsgType == NEW_ORDER_T && 
                 currRec->cMsgType == ORDER_CANCELLATION && 
//                prevRec->nSeqNo+1+STPBuffer.size() == currRec->nSeqNo && 
                prevRec->nSeqNo+STPBuffer.size() == currRec->nSeqNo && 
                ((GENERIC_ORD_MSG*)prevRec)->nToken == ((GENERIC_ORD_MSG*)currRec)->nToken &&
                ((GENERIC_ORD_MSG*)prevRec)->cOrdType != ((GENERIC_ORD_MSG*)currRec)->cOrdType
           )
        {
            // save send Cxl first and then all the other things                 
//            STPBuffer.insert(STPBuffer.begin(), (char*)prevBuffer);
//            printTBTRecord(prevBuffer, "ReSequenced due to STP 3:");
//            std::cout << "Current Buffer size5:" << STPBuffer.size() << std::endl;
            
            STPBuffer.insert(STPBuffer.begin(), (char*)buffer);            
//            printTBTRecord(buffer, "ReSequenced due to STP 3:");
//            std::cout << "Current Buffer size6:" << STPBuffer.size() << std::endl;
            #ifdef __PFS_LOG__
            snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size6:%d", STPBuffer.size());
            Logger::getLogger().log(DEBUG, logBufPFS);
            #endif
            /*NK*/
            isPrevBufferPopulated = false;
            /*NK added on 11 May 2017*/
            flushSTPBuffer=true;
            result = true;
        }

//        // case 4a DQ Thing NMT->MNT
//        else if ( (prevRec->cMsgType == NEW_ORDER_T || prevRec->cMsgType == ORDER_MODIFICATION) &&
//                  currRec->cMsgType == ORDER_MODIFICATION && 
//                  ((gDQNumMod == 0 && prevRec->nSeqNo+1 == currRec->nSeqNo) || (gDQNumMod > 0 && prevRec->nSeqNo+STPBuffer.size() == currRec->nSeqNo)) &&
//                  (((GENERIC_ORD_MSG*)prevRec)->nToken == ((GENERIC_ORD_MSG*)currRec)->nToken) &&
//                  ((GENERIC_ORD_MSG*)prevRec)->cOrdType != ((GENERIC_ORD_MSG*)currRec)->cOrdType
//                )   
//            {
//            isDQPossibleCaseInitiated = true;
//            std::cout<< "Possible NMT Seq," << std::endl;
//
//            // save New
//            if (STPBuffer.size()==0){
//                STPBuffer.insert(STPBuffer.begin(), prevBuffer);
//                std::cout << "Current Buffer size8:" << STPBuffer.size() << std::endl;
//                nTotalDQQtyOfNewOrder = ((GENERIC_ORD_MSG*)prevRec)->nQty;
//                printTBTRecord(prevBuffer, "Saving New DQ : STP3:");
//            }
//            
//            // save mod
////            STPBuffer.insert(STPBuffer.begin()+1+gDQNumMod, buffer);        
//            STPBuffer.push_back(buffer);     
//            std::cout << "Current Buffer size9:" << STPBuffer.size() << std::endl;
//            gDQNumMod++;
//            std::cout << "DQNumMod=" << gDQNumMod << std::endl;
//            printTBTRecord(buffer, "Saving MOD DQ : STP3:");
//            result = true;
//        }
//        
//        else if (false && isDQPossibleCaseInitiated)
//        {
//            if (prevRec->cMsgType == NEW_ORDER_T &&
//                currRec->cMsgType == TRADE_MESSAGE && 
//                prevRec->nSeqNo+STPBuffer.size() == currRec->nSeqNo &&         
////                ((GENERIC_ORD_MSG*)STPBuffer[1].rec)->cMsgType == ORDER_MODIFICATION &&        
//                ((GENERIC_ORD_MSG*)prevRec)->nToken == ((TRD_MSG*)currRec)->nToken 
//               )   
//            {
//                // Generate New for tradedQty
//                char tempBuffer[128];
//                memcpy(tempBuffer, prevBuffer, FIX_SIZE_LEN);
//                ((GENERIC_ORD_MSG*)tempBuffer)->cMsgType = NEW_ORDER_T;
//                ((GENERIC_ORD_MSG*)tempBuffer)->header.nSeqNo = currRec->nSeqNo;
//                
//                if (nTotalDQQtyOfNewOrder >= ((TRD_MSG*)currRec)->nTradeQty)
//                {
//                    ((GENERIC_ORD_MSG*)tempBuffer)->dblOrdID = gInternalOrderCounter++;
//                    ((GENERIC_ORD_MSG*)tempBuffer)->nQty = ((TRD_MSG*)currRec)->nTradeQty;
//                    nTotalDQQtyOfNewOrder -= ((TRD_MSG*)currRec)->nTradeQty;
//                    std::cout << "Pending Qty for new DQ Order is " << nTotalDQQtyOfNewOrder << std::endl;
//                }
//                else
//                {
//                    ((GENERIC_ORD_MSG*)tempBuffer)->dblOrdID = gInternalOrderCounter++;
//                    ((GENERIC_ORD_MSG*)tempBuffer)->nQty = nTotalDQQtyOfNewOrder;
//                    std::cout << "Error Total Qty for new DQ Order utilized is more"  << std::endl;
//                }
//                
//                // save Trade
////                STPBuffer.push_back(buffer);        
//                STPBuffer.push_back(tempBuffer);
//                std::cout << "Current Buffer size10:" << STPBuffer.size() << std::endl;
//                printTBTRecord(buffer, "Generating New for Trade DQ : STP4:");
//                printTBTRecord(tempBuffer, "Generating New for Trade DQ : STP4:");
//                std::cout << STPBuffer.size() - gDQNumMod - 1 << std::endl;
//            }
//            else
//            {
//                if (STPBuffer.size() > gDQNumMod + 1 )
//                {
//                    // Reseqence New
//                    char tempBuffer[128];
//                    memcpy(tempBuffer, STPBuffer[0].rec, FIX_SIZE_LEN);
//                    STPBuffer.erase(STPBuffer.begin());
////                    STPBuffer.push_back(tempBuffer);                    
//                    
//                    if (nTotalDQQtyOfNewOrder > 0)
//                    {
//                        char tempBuffer[128];
//                        memcpy(tempBuffer, prevBuffer, FIX_SIZE_LEN);
//                        ((GENERIC_ORD_MSG*)tempBuffer)->cMsgType = NEW_ORDER_T;
//                        ((GENERIC_ORD_MSG*)tempBuffer)->dblOrdID = gInternalOrderCounter++;
//                        ((GENERIC_ORD_MSG*)tempBuffer)->nQty = nTotalDQQtyOfNewOrder;
//                        std::cout << "DQ Generating final New order of Qty: " << nTotalDQQtyOfNewOrder << std::endl;
//                        STPBuffer.push_back(tempBuffer);
//                        std::cout << "Current Buffer size11:" << STPBuffer.size() << std::endl;
//                        printTBTRecord(tempBuffer, "Generating Final New for Trade DQ : STP4:");
//                    }
//                }
//
//                
//                flushSTPBuffer = true;
//                isDQPossibleCaseInitiated = false;
//                gDQNumMod = 0;
//                nTotalDQQtyOfNewOrder = 0;
//                
//                isPrevBufferPopulated = false;
//                
//                memcpy(prevBuffer, buffer, FIX_SIZE_LEN);
//                isPrevBufferPopulated=true;
//                printTBTRecord(buffer, "Saving in STP Buffer 3:");
//            }
//            result = true;
//        }               
        else{
            
            // Send prev records in buffer 
            if (isPrevBufferPopulated )
            {
              if(STPBuffer.size()> 0 && 
                ((STREAM_HEADER*)prevRec)->nSeqNo == ((STREAM_HEADER*)(STPBuffer[0].rec))->nSeqNo)
              {
//                 std::cout << "Same Order Encountered:" << STPBuffer.size() << std::endl;
              }
              else
              {
                STPBuffer.push_back(prevBuffer);
              }
//              std::cout << "Current Buffer size7:" << STPBuffer.size() << std::endl;
              #ifdef __PFS_LOG__
              snprintf(logBufPFS, 400, "Thread_PFS|Current Buffer size7:%d", STPBuffer.size());
              Logger::getLogger().log(DEBUG, logBufPFS);
              #endif
              flushSTPBuffer=true;
              result=true;
            }
            flushSTPBuffer = true;
            
            memcpy(prevBuffer, buffer, FIX_SIZE_LEN);
            isPrevBufferPopulated=true;
//            printTBTRecord(buffer, "Saving in STP Buffer 2:");
            
//            STPBuffer.clear();
        }
    }
    
//    if (result == false)
//    {
//        memcpy(prevBuffer, buffer, FIX_SIZE_LEN);
//        isPrevBufferPopulated=true;
//    }
    return result;
}

void processing (const char* dataFile){
    DQHandler objDQHandler;
    ifstream tbtFile(dataFile, ios::in | ios::binary);
    processingState currState = processingStateReadFile;
    
    GET_PERF_TIME(FileStarts);
    if (tbtFile.is_open()) 
    {
        char buffer[128];
        while (true)
        {
            if (flushSTPBuffer)
            { 
                currState=processingStateFlushSTP;
            }
//            else if (objDQHandler.flushBuffer)
            if (objDQHandler.flushBuffer)
                currState=processingStateFlushDQ;
            
            switch(currState)
            {
                case processingStateReadFile:
                {
                    tbtFile.read(buffer, FIX_SIZE_LEN);
                    if (!tbtFile) 
                    {
                      GET_PERF_TIME(FileEnds);
                      std::cout << "Data File Completed,Time Taken::" <<FileEnds-FileStarts<< std::endl;
                        std::cout << "ERROR : Only " << tbtFile.gcount() << "bytes were able to be read out of " << FIX_SIZE_LEN << std::endl;
                        std::cout << "Data File Completed" << std::endl;
                        exit(0);
                    }
                    if (!isTokenAllowed(buffer) || !isMessageAllowed(buffer))
                    {
                        continue;
                    }

                    if (!updateOrderNumber(buffer))
                    {
//                        std::cout<< "Error! Failed to update the order Number for following order" << std::endl;
                        #ifdef __PFS_LOG__
                          snprintf(logBufPFS, 400, "Error! Failed to update the order Number for following order");
                          Logger::getLogger().log(DEBUG, logBufPFS);
                        #endif
//                        printTBTRecord(buffer, "Unable to update OrderNumber");
                        continue;
                    }
                    currState = processingStateCheckSTP;
                    continue;
                }

                case processingStateCheckSTP:
                {
                    if (IsSelfTradePossible(buffer)){
                        currState = processingStateReadFile;
                    }
                    continue;
                }
                
                case processingStateFlushSTP:
                {
                    if (flushSTPBuffer)
                    {
                        if (STPBuffer.size() > 0 )
                        {
//                            std::cout << "STP Flushing " << STPBuffer.size() << ":"<< std::endl;
                          #ifdef __PFS_LOG__  
                          snprintf(logBufPFS, 400, "Thread_PFS|STP Flushing : %d", STPBuffer.size());
                          Logger::getLogger().log(DEBUG, logBufPFS);
                          #endif
                          memcpy(buffer, STPBuffer[0].rec, FIX_SIZE_LEN);

                            STPBuffer.erase(STPBuffer.begin());
                        }
                        else
                        {
                            STPBuffer.clear();
                            flushSTPBuffer = false;
                            currState = processingStateReadFile;
                            continue;
                        }
                    }
//                    continue;
                }                
                
                case processingStateCheckDQ:
                {
                    if (objDQHandler.IsDQPatternPossible(buffer))
                    {
                        continue;    
                    }
                    else{
                        break;
                        currState = processingStateReadFile;
                    }
                    
                    break;
                }
                
                case processingStateFlushDQ:
                {
                    if (objDQHandler.Buffer.size() > 0)
                    {
//                        std::cout << "DQ Flushing " << objDQHandler.Buffer.size() << ":"<< std::endl;
                        #ifdef __PFS_LOG__  
                        snprintf(logBufPFS, 400, "Thread_PFS|DQ Flushing : %d", objDQHandler.Buffer.size());
                        Logger::getLogger().log(DEBUG, logBufPFS);
                        #endif
                        memcpy(buffer, objDQHandler.Buffer[0].rec, FIX_SIZE_LEN);

                        objDQHandler.Buffer.erase(objDQHandler.Buffer.begin());
                        break;
                    }
                    else
                    {
                        objDQHandler.Buffer.clear();
                        objDQHandler.flushBuffer = false;
                        if (objDQHandler.isPrevBufferPopulated)
                        {
                            objDQHandler.Buffer.push_back(objDQHandler.prevBuffer);
                            objDQHandler.isPrevBufferPopulated = false;
                            currState = processingStateReadFile;
                            continue;
                        }
                    }
                }                
            }
            processMessage(buffer);
        }
        tbtFile.close();

    } else {
        std::cout << "ERROR : Unable to open the file" << dataFile <<  std::endl;
    }
}



void processingFO(const char * TBT_filename)
{
  STPBuffer.clear();
  ifstream tbtFile(TBT_filename, ios::in | ios::binary);
  GET_PERF_TIME(FileStarts);
  if (tbtFile.is_open()) 
  {
      char buffer[128];
      while(true)
      {
          if (flushSTPBuffer)
          {
              if (STPBuffer.size() > 0 )
              {
//                  std::cout << "Flushing " << STPBuffer.size() << ":"<< std::endl;
                  #ifdef __PFS_LOG__  
                  snprintf(logBufPFS, 400, "Thread_PFS|Flushing : %d", STPBuffer.size());
                  Logger::getLogger().log(DEBUG, logBufPFS);
                  #endif
                  memcpy(buffer, STPBuffer[0].rec, FIX_SIZE_LEN);

                  STPBuffer.erase(STPBuffer.begin());
              }
              else
              {
                  STPBuffer.clear();
                  flushSTPBuffer = false;
                  continue;
              }
          }
          else
          {
              tbtFile.read(buffer, FIX_SIZE_LEN);
              
//              {
//                std::cout<<((CompositeBcastMsg*)buffer)->stBcastMsg.stHeader.nSeqNo << "|"
//                         <<((CompositeBcastMsg*)buffer)->tv.tv_sec << ":"
//                         <<((CompositeBcastMsg*)buffer)->tv.tv_nsec<< std::endl;
//              }
              if (!tbtFile) 
              {
                  GET_PERF_TIME(FileEnds);
                  std::cout << "Data File Completed,Time Taken::" <<FileEnds-FileStarts<< std::endl;
                  std::cout << "ERROR : Only " << tbtFile.gcount() << "bytes were able to be read out of " << FIX_SIZE_LEN << std::endl;
//                  std::cout << "Data File Completed Total Record:" << transCount << std::endl;        
                  tbtFile.close();
                  return ;
              }
              if (!isTokenAllowed(buffer) || !isMessageAllowed(buffer))
              {
                continue;
              }
              if (!updateOrderNumber(buffer))
              {
                #ifdef __PFS_LOG__  
                snprintf(logBufPFS, 400, "Thread_PFS|Error! Failed to update the order Number for following order");
                Logger::getLogger().log(DEBUG, logBufPFS);
                #endif
//                  std::cout<< "Error! Failed to update the order Number for following order" << std::endl;
//                  printTBTRecord(buffer, "Unable to update OrderNumber");
                  continue;
              }
              if (IsSelfTradePossible(buffer))
              {
                  continue;
              }
          }
          
          processMessage(buffer);//same for FO and CM
      }
      tbtFile.close();
  }
}
enum EXCHANGE_SEGMENT_ID : short {
    NSEFO = 1,
    NSECM = 2,
    NSECDS = 3
};

void initFilteredTokens(const char* subFilePath){
    int count=0;
    std::ifstream subscription_stream;

    memset(&filteredTokens, 0, PFS_TOKEN_ARRAY_SIZE);
    
    if( strlen(subFilePath) > 0 )
    {
        subscription_stream.open(subFilePath, std::ifstream::in);
        if( subscription_stream.is_open())
        {
            char buff[24];
            int securityID = 0;
            while(!subscription_stream.eof())
            {
                subscription_stream.getline(buff, 24);
                char * ptr = strstr(buff, ",");
                if( ptr )
                    *ptr = 0;
                
                
                securityID = atoi(buff);
                if (securityID == 0)
                    continue;
                filteredTokens[securityID] = 1;
                std::cout<<"Filtering for tokens:" << securityID << std::endl;
                count++;
            }
            subscription_stream.close();
        }
    }
    if (count>0)
    {
        filterViaSubscription = true;
        std::cout << "Filtering for Tokens count " << count << std::endl;
    }
}

void Start_PFS(ProducerConsumerQueue<GENERIC_ORD_MSG> *pInqptr_PFStoME,std::string PFSConfig)
{
  
//  std::cout<<"PFS started::"<< std::endl;
  while(gStartPFSFlag==0)
  {
    
  }
  
  // Parse PRSConfig.ini
  ConfigReader lszConfigReader(PFSConfig);
  
  readConfigParams(lszConfigReader);
  if(configData.cPFSWithClient=='Y'|| configData.cPFSWithClient=='y')
  {
    snprintf(logBufPFS, 400, "Thread_PFS|Wait for Client Connection");
    Logger::getLogger().log(DEBUG, logBufPFS);
    while(gPFSWithClient==0)
    {
      
    }
    snprintf(logBufPFS, 400, "Thread_PFS|Client Connected|PFS go ahead");
    Logger::getLogger().log(DEBUG, logBufPFS);
  }
  if(configData.PFSDelay > 0)
  {
    snprintf(logBufPFS, 400, "Thread_PFS|Wait for %d seconds",configData.PFSDelay);
    Logger::getLogger().log(DEBUG, logBufPFS);
    
    usleep(configData.PFSDelay * 1000000);
  }
  std::string TBTFilePath = configData.tbt_filename;
  snprintf(logBufPFS, 400, "Thread_PFS|PFS started|TBT file path :%s",TBTFilePath.c_str());
  Logger::getLogger().log(DEBUG, logBufPFS);
  
  TaskSetCPU(configData.PFSCoreId);
  iOldestOrderTimeLocn = 0;
  pOrderTimeStamps = new uint64_t[iOrderThresholdCount];
  for (int32_t ii = 0; ii < iOrderThresholdCount; ii++) {
      pOrderTimeStamps[ii] = 0;
  }
  Inqptr_PFStoME = pInqptr_PFStoME;
  
  initFilteredTokens("Token.txt");
  while(1)
  {
    if (configData.Segment==NSEFO)
    {
      processingFO(configData.tbt_filename.c_str());
    }
    else if(configData.Segment==NSECM)
    {
      processing(configData.tbt_filename.c_str());
    }
  } 
}
