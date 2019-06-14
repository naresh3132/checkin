
#include <iostream>
#include <time.h>
#include <fstream>
#include "Thread_MarketBook.h"
#include "ConfigReader.h"
#include "Broadcast_Struct.h"
#include "spsc_atomic1.h"
#include "BrodcastStruct.h"
#include "CommonFunctions.h"
#include "perf.h"
#include "log.h"
using namespace std;

#define FIX_SIZE_LEN 61
#define DIVISIBLE_FACTOR    100000000   //100M
#define MAX_WAIT_LIMIT 1000000
const int32_t MB_TOKEN_ARRAY_SIZE = 110000;

extern int16_t gStartMBFlag;

// Global Params Start
ConfigParamsMB1 configDataMB;
int32_t gOrder_Counter = 1;
int32_t gInternal_OrderCounter = 90000;

ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_MBtoME = NULL;
std::unordered_map<double, double> MapRawOrdIdToSerialMBOrdId;
bool filterViaSubscriptionMB = false;
int8_t filteredMBTokens[MB_TOKEN_ARRAY_SIZE] = {0};
GENERIC_ORD_MSG MBMsgIn;  

timespec tCompare_Time;
// In milis
int64_t _lastRecvOrderTime=0;
int64_t _lastRecvOrderTimeOffset=0;
int64_t _lastSendOrderTime=0;
int64_t _lastSendOrderTimeOffset=0;

uint64_t *_pOrderTimeStamps;
uint32_t _iOldestOrderTimeLocn=0;
uint64_t _ullOrderThresholdInterval=1 * 1000000000;
uint32_t _iOrderThresholdCount;

int64_t File_Starts=0,File_Ends=0;

const int32_t THRESHOLD_USLEEP_WAITTIME = 3; 


// Global Params Ends
char logBufMB[400];


void _readConfigParams(ConfigReader& lcConfigReader) {
    configDataMB.Segment = atoi(lcConfigReader.getProperty("SEGMENT").c_str());
    if (configDataMB.Segment <= 0)
    {
        configDataMB.Segment = 1;
    }
    configDataMB.processingMBSpeed = atoi(lcConfigReader.getProperty("PROCESSING_SPEED").c_str());

    strcpy(configDataMB.secFilePath, lcConfigReader.getProperty("SECURITY_FILEPATH").c_str());
    strcpy(configDataMB.contractFilePath, lcConfigReader.getProperty("CONTRACT_FILEPATH").c_str());
    configDataMB.nThrottleRate = atoi(lcConfigReader.getProperty("ORDER_THRESHOLD_COUNT").c_str());
    configDataMB.MBCoreId = atoi(lcConfigReader.getProperty("MB_CORE_ID").c_str());
    configDataMB.MaintainTimeDiff = lcConfigReader.getProperty("MAINTAIN_TIME_DIFF").c_str()[0];
    configDataMB.adjustPrice = lcConfigReader.getProperty("ADJUST_PRICE").c_str()[0];
    if (configDataMB.nThrottleRate<=0)
        configDataMB.nThrottleRate = 40;
    _iOrderThresholdCount = configDataMB.nThrottleRate;
    lcConfigReader.dump();
    std::cout << "Throttling rate = " << configDataMB.nThrottleRate << std::endl;
    configDataMB.MB_filename = lcConfigReader.getProperty("MB_filename");
    _ullOrderThresholdInterval = _ullOrderThresholdInterval/configDataMB.nThrottleRate; // Temp for RW
}

bool _ValidateOrderCountThreshold(int& sleepInterval) {
    clock_gettime(CLOCK_REALTIME, &tCompare_Time);
    uint64_t ullCurrTime = (tCompare_Time.tv_sec * 1000000000) + tCompare_Time.tv_nsec;
    
    //Validating only 1st Timestamp for MultiLegOrders also to avoid computational overhead, So Configured OrderRate should be 2-5 Orders more than actual
    if ( (_iOldestOrderTimeLocn == 0 && (ullCurrTime - _pOrderTimeStamps[_iOrderThresholdCount-1] > _ullOrderThresholdInterval) ) ||
         (_iOldestOrderTimeLocn>0 && (ullCurrTime - _pOrderTimeStamps[_iOldestOrderTimeLocn-1] > _ullOrderThresholdInterval)) ) {
        //Populate 1st Entry for all type of Reqs

//        std::cout << "ValidateOrderCountThreshold success:" 
//                  << iOldestOrderTimeLocn << "|"
//                  << ullCurrTime << "|"
//                  << pOrderTimeStamps[iOldestOrderTimeLocn] << "|"
//                  << ullOrderThresholdInterval << "|"
//                  << std::endl;
        
        _pOrderTimeStamps[_iOldestOrderTimeLocn] = ullCurrTime;
        _iOldestOrderTimeLocn++;
        
        if (_iOldestOrderTimeLocn == _iOrderThresholdCount) {
            _iOldestOrderTimeLocn = 0;
        }
        sleepInterval=0;
        return true;
    } else {
        if (_iOldestOrderTimeLocn == 0)
        {
            sleepInterval = ullCurrTime - _pOrderTimeStamps[_iOrderThresholdCount-1];
            sleepInterval = _ullOrderThresholdInterval - sleepInterval;
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
            sleepInterval = ullCurrTime - _pOrderTimeStamps[_iOldestOrderTimeLocn-1];
            sleepInterval = _ullOrderThresholdInterval - sleepInterval;
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

void _sendMBOrderToME(GENERIC_ORD_MSG& MBMsgIn) {
    int sleepInt = 0;
    uint64_t ullTempCurrTime;
    while(true)
    {
        if (_ValidateOrderCountThreshold(sleepInt))
        {
            uint64_t ullCurrTime;
//            while ( true)
//            {
                clock_gettime(CLOCK_REALTIME, &tCompare_Time);
//                ullCurrTime = (tCompare_Time.tv_sec * 1000) + tCompare_Time.tv_nsec/1000000;
                ullCurrTime = (tCompare_Time.tv_sec * 1000000000) + tCompare_Time.tv_nsec;
                _lastSendOrderTimeOffset = ullCurrTime-_lastSendOrderTime;
                
                
//                std::cout <<"Time Check 1::"<< lastRecvOrderTimeOffset<< "|" << lastSendOrderTimeOffset<< "|" << lastRecvOrderTimeOffset - lastSendOrderTimeOffset<<std::endl;
                
                if (configDataMB.MaintainTimeDiff == 'Y' && _lastRecvOrderTime!=0 && _lastSendOrderTimeOffset < _lastRecvOrderTimeOffset)
                {
//                    std::cout << "Waiting....Consecutive time gap " 
//                              << lastSendOrderTimeOffset << "|"
//                              << lastRecvOrderTimeOffset << "|"
//                              << std::endl;
                    #ifdef __MB_LOG__  
                    snprintf(logBufMB, 400, "Thread_MB|Waiting....Consecutive time gap  %ld|%ld",lastSendOrderTimeOffset, lastRecvOrderTimeOffset);
                    Logger::getLogger().log(DEBUG, logBufMB);  
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
                    
                    if ( _lastRecvOrderTimeOffset - _lastSendOrderTimeOffset > MAX_WAIT_LIMIT)  
                    {
//                        std::cout << "Waiting....MAX delay 1 milli " 
//                              << std::endl;
                      #ifdef __MB_LOG__ 
                        snprintf(logBufMB, 400, "Thread_MB|Waiting....MAX delay 1 milli ");
                        Logger::getLogger().log(DEBUG, logBufMB); 
                      #endif
                        usleep(1000);
                    }
                    else if ( _lastRecvOrderTimeOffset - _lastSendOrderTimeOffset < 1000)  
                    {
                        
                    }
                    else
                    {
                       
                         usleep((_lastRecvOrderTimeOffset-_lastSendOrderTimeOffset)/1000 );
                          
                    }
                    
//                    continue;
                }

            #ifdef __MB_LOG__
              snprintf(logBufMB, 400, "Thread_MB|IN:SeqNo:%d|MsgType:%c|OrderId:%ld|OrderType:%c|Price:%d|Oty:%d", MBMsgIn.header.nSeqNo,MBMsgIn.cMsgType,(int64_t)MBMsgIn.dblOrdID,MBMsgIn.cOrdType,MBMsgIn.nPrice,MBMsgIn.nQty);
              Logger::getLogger().log(DEBUG, logBufMB);    
            #endif
//            std::cout<<"IN::"<<MBMsgIn.header.nSeqNo<<"|"<<MBMsgIn.cMsgType<<"|"<<(int64_t)MBMsgIn.dblOrdID<<"|"<<MBMsgIn.cOrdType<<"|"<<MBMsgIn.nPrice<<"|"<<MBMsgIn.nQty<<std::endl;
            
            while (Inqptr_MBtoME->enqueue(MBMsgIn) == false)
            {
//              #ifdef __MB_LOG__
              snprintf(logBufMB, 400, "Thread_MB|Unable to enqueue the Message of seqNo %d", MBMsgIn.header.nSeqNo);
              Logger::getLogger().log(DEBUG, logBufMB);    
//              #endif
//              std::cout << "Enqueue failed" << std::endl;
              usleep(1);
            }                

            _lastSendOrderTime = ullCurrTime;
            break;

        }
        else
        {
//            std::cout << "Waiting....Threashold" << std::endl;

            if (sleepInt > THRESHOLD_USLEEP_WAITTIME * 1000)
            {
//              snprintf(logBufMB, 400, "ValidateOrderCountThreshold: sleepInt %d sleep %d",sleepInt,sleepInt/1000);
//              Logger::getLogger().log(DEBUG, logBufMB); 
                usleep(sleepInt/1000);
            
            }
            else
            {
//              snprintf(logBufMB, 400, "ValidateOrderCountThreshold: sleepInt %d THRESHOLD_USLEEP_WAITTIME %d",sleepInt,THRESHOLD_USLEEP_WAITTIME);
//              Logger::getLogger().log(DEBUG, logBufMB);
              usleep(THRESHOLD_USLEEP_WAITTIME);
            }
        }
    }
}

bool _isMBTokenAllowed(int nToken){
//bool checkTokenForFilteration(int nToken){
//  std::cout<<nToken<<std::endl;
    if (filterViaSubscriptionMB)
        return (filteredMBTokens[nToken]==1);
    else 
        return true;
}

bool _isMBTokenAllowed(char* buffer){
    
    MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
    
    if (msgHdr->cMsgType == NEW_ORDER_T || 
        msgHdr->cMsgType == ORDER_CANCELLATION ||
        msgHdr->cMsgType == ORDER_MODIFICATION)
            return _isMBTokenAllowed(((GENERIC_ORD_MSG*)buffer)->nToken);
    
    else if (msgHdr->cMsgType == TRADE_MESSAGE)
            return _isMBTokenAllowed(((TRD_MSG*)buffer)->nToken);
    
    else 
        return false;
}

bool _processMBMessage(char *buffer)
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
        
        if (!_isMBTokenAllowed(iToken))
            return true;  
        
        memcpy(&MBMsgIn,buffer,sizeof(GENERIC_ORD_MSG));
        
        clock_gettime(CLOCK_REALTIME, &tCompare_Time);
//        if (((GENERIC_ORD_MSG*) buffer)->lTimeStamp - lastRecvOrderTime > 0)
//            lastRecvOrderTimeOffset =  ((GENERIC_ORD_MSG*) buffer)->lTimeStamp - lastRecvOrderTime;
//        else
//            lastRecvOrderTimeOffset = 0;
        
        if (llTimestamp - _lastRecvOrderTime > 0)
            _lastRecvOrderTimeOffset =  llTimestamp - _lastRecvOrderTime;
        else
            _lastRecvOrderTimeOffset = 0;
        
        _sendMBOrderToME(MBMsgIn);
        _lastRecvOrderTime = llTimestamp;
        
      
        return true;
      }
      case TRADE_MESSAGE:
      {
          if ((((TRD_MSG*) buffer)->dblBuyOrdID < 1 && ((TRD_MSG*) buffer)->dblSellOrdID < 1 ) || !_isMBTokenAllowed(((TRD_MSG*) buffer)->nToken) )
          {
              return true;
          }

          // temp Approach ends
          if ((((TRD_MSG*) buffer)->dblBuyOrdID > 1 && ((TRD_MSG*) buffer)->dblSellOrdID > 1 ) )
          {
            iToken = ((TRD_MSG*) buffer)->nToken;
        
            if (!_isMBTokenAllowed(iToken))
                return true;  

          //First Modification Order Initiated
          MBMsgIn.nToken = ((TRD_MSG*) buffer)->nToken;
          MBMsgIn.cMsgType='T';
          MBMsgIn.cOrdType='B';
          MBMsgIn.dblOrdID=((TRD_MSG*) buffer)->dblBuyOrdID;
          MBMsgIn.header.nSeqNo = ((TRD_MSG*) buffer)->header.nSeqNo;
          MBMsgIn.header.wMsgLen = ((TRD_MSG*) buffer)->header.wMsgLen;
          MBMsgIn.header.wStremID = ((TRD_MSG*) buffer)->header.wStremID;
          MBMsgIn.nPrice = ((TRD_MSG*) buffer)->nTradePrice;
          MBMsgIn.nQty = ((TRD_MSG*) buffer)->nTradeQty;
            
         

       
          _sendMBOrderToME(MBMsgIn);
          
          //Second Modification Order Initiated
          MBMsgIn.nToken = ((TRD_MSG*) buffer)->nToken;
          MBMsgIn.cMsgType='T';
          MBMsgIn.cOrdType='S';
          MBMsgIn.dblOrdID=((TRD_MSG*) buffer)->dblSellOrdID;
          MBMsgIn.header.nSeqNo = ((TRD_MSG*) buffer)->header.nSeqNo;
          MBMsgIn.header.wMsgLen = ((TRD_MSG*) buffer)->header.wMsgLen;
          MBMsgIn.header.wStremID = ((TRD_MSG*) buffer)->header.wStremID;
          MBMsgIn.nPrice = ((TRD_MSG*) buffer)->nTradePrice;
          MBMsgIn.nQty = ((TRD_MSG*) buffer)->nTradeQty;
          
          

          clock_gettime(CLOCK_REALTIME, &tCompare_Time);

          if (llTimestamp - _lastRecvOrderTime > 0)
              _lastRecvOrderTimeOffset =  llTimestamp - _lastRecvOrderTime;
          else
              _lastRecvOrderTimeOffset = 0;
            
          _sendMBOrderToME(MBMsgIn);
          return true;
          }
          // temp Approach ends

          iToken = ((TRD_MSG*) buffer)->nToken;
          if (!_isMBTokenAllowed(iToken))
            return true;
          
//          double newOrderId = DIVISIBLE_FACTOR+gOrder_Counter++;
          bool isSellOrder = (((TRD_MSG*) buffer)->dblBuyOrdID<1 && ((TRD_MSG*) buffer)->dblSellOrdID>1);
          MBMsgIn.nToken = ((TRD_MSG*) buffer)->nToken;
          MBMsgIn.cMsgType='I';
          MBMsgIn.cOrdType= (isSellOrder)?'S':'B';
          if(((TRD_MSG*) buffer)->dblBuyOrdID > 1)
          {
            MBMsgIn.dblOrdID=((TRD_MSG*) buffer)->dblBuyOrdID;
          }
          else
          {
            MBMsgIn.dblOrdID=((TRD_MSG*) buffer)->dblSellOrdID;
          }
          
          MBMsgIn.header.nSeqNo = ((TRD_MSG*) buffer)->header.nSeqNo;
          MBMsgIn.header.wMsgLen = ((TRD_MSG*) buffer)->header.wMsgLen;
          MBMsgIn.header.wStremID = ((TRD_MSG*) buffer)->header.wStremID;
          MBMsgIn.nPrice = ((TRD_MSG*) buffer)->nTradePrice;
          MBMsgIn.nQty = ((TRD_MSG*) buffer)->nTradeQty;
          
          
//          lastRecvOrderTimeOffset =  ((TRD_MSG*) buffer)->lTimestamp - lastRecvOrderTime;
          _lastRecvOrderTimeOffset =  llTimestamp - _lastRecvOrderTime;
          
          _sendMBOrderToME(MBMsgIn);

          _lastRecvOrderTime = llTimestamp;
          


          
          return true;
      }
      default:
      {
          std::cout << "ERROR :Ignoring un-handled message :" << msgHdr->cMsgType << std::endl;
          #ifdef __MB_LOG__  
          snprintf(logBufMB, 400, "Thread_MB|ERROR :Ignoring un-handled message :%c", msgHdr->cMsgType );
          Logger::getLogger().log(DEBUG, logBufMB);  
          #endif
          return false;            
      }
  }
  return true;
}





bool _isMBMessageAllowed(char* buffer){
    
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

bool _updateMBOrderNumber(char* buffer){

    bool result = false;
    MSG_HEADER* msgHdr = (MSG_HEADER*) buffer;
    
    switch (msgHdr->cMsgType)
    {
        case NEW_ORDER_T:
        {
            if (((GENERIC_ORD_MSG*)buffer)->dblOrdID > 1)
            {
                double newOrderId = DIVISIBLE_FACTOR+gOrder_Counter++;
                std::pair<double, double> prMapRawOrdIdToSerialMBOrdId;

                prMapRawOrdIdToSerialMBOrdId.first = ((GENERIC_ORD_MSG*)buffer)->dblOrdID;
//                prMapRawOrdIdToSerialMBOrdId.second = newOrderId;
                prMapRawOrdIdToSerialMBOrdId.second = ((GENERIC_ORD_MSG*)buffer)->dblOrdID;
                MapRawOrdIdToSerialMBOrdId.insert(prMapRawOrdIdToSerialMBOrdId);
//                std::cout << "N:RAW:SERIAL=" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID << ":" << (int64_t)newOrderId << std::endl;
                #ifdef __MB_LOG__
                snprintf(logBufMB, 400, "Thread_MB|N:RAW:SERIAL=%ld:%ld", (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,(int64_t)newOrderId);
                Logger::getLogger().log(DEBUG, logBufMB); 
                #endif
//                ((GENERIC_ORD_MSG*)buffer)->dblOrdID = newOrderId;
                result = true;
            }
            break;
        }
        case ORDER_CANCELLATION:
        case ORDER_MODIFICATION:
        {
            if (((GENERIC_ORD_MSG*)buffer)->dblOrdID > 1)
            {
                auto val = MapRawOrdIdToSerialMBOrdId.find(((GENERIC_ORD_MSG*)buffer)->dblOrdID);
                if (val != MapRawOrdIdToSerialMBOrdId.end()) {
//                    std::cout << ((GENERIC_ORD_MSG*)buffer)->cMsgType << ":RAW:SERIAL=" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID << ":" << (int64_t)val->second << std::endl;
                    #ifdef __MB_LOG__  
                    snprintf(logBufMB, 400, "Thread_MB|%c:RAW:SERIAL=%ld:%ld",((GENERIC_ORD_MSG*)buffer)->cMsgType, (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufMB); 
                    #endif
                    ((GENERIC_ORD_MSG*)buffer)->dblOrdID = val->second;
                    result = true;
                }
                else{
//                    std::cout << "ERROR :Match for RAW Order ID" << (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID 
//                              << "  Not Found for "  << ((GENERIC_ORD_MSG*)buffer)->cMsgType << std::endl;
                    
                    #ifdef __MB_LOG__  
                    snprintf(logBufMB, 400, "Thread_MB|ERROR :Match for RAW Order ID :%ld Not Found for : %c " , (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID,((GENERIC_ORD_MSG*)buffer)->cMsgType );
                    Logger::getLogger().log(DEBUG, logBufMB);  
                    #endif
                    result = false;
                }
            }
            break;
        }
        case TRADE_MESSAGE:
        {
            if (((TRD_MSG*)buffer)->dblBuyOrdID > 1){
                auto val = MapRawOrdIdToSerialMBOrdId.find(((TRD_MSG*)buffer)->dblBuyOrdID);
                if (val != MapRawOrdIdToSerialMBOrdId.end()) {
//                    std::cout << "T:RAW:SERIAL=" << (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID << ":" << (int64_t)val->second << std::endl;
                  #ifdef __MB_LOG__  
                  snprintf(logBufMB, 400, "Thread_MB|T:RAW:SERIAL=%ld:%ld", (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufMB);  
                  #endif
                    ((TRD_MSG*)buffer)->dblBuyOrdID = val->second;
                    
                    result = true;
                }
                else{
//                    std::cout << "ERROR :TRD:Match for RAW Buy Order ID : " << (int64_t)((TRD_MSG*)buffer)->dblBuyOrdID 
//                              << ", Not Found, sending 0" << std::endl;
                    
                    #ifdef __MB_LOG__  
                    snprintf(logBufMB, 400, "Thread_MB|ERROR :TRD:Match for RAW Buy Order ID : :%ld , Not Found, sending 0 " , (int64_t)((GENERIC_ORD_MSG*)buffer)->dblOrdID );
                    Logger::getLogger().log(DEBUG, logBufMB);  
                    #endif
  
                    ((TRD_MSG*)buffer)->dblBuyOrdID = 0;
                    result = true;
                    
                }
            }
            else
            {
                result = true;
            }

            if (((TRD_MSG*)buffer)->dblSellOrdID > 1)
            {
              
                auto val = MapRawOrdIdToSerialMBOrdId.find(((TRD_MSG*)buffer)->dblSellOrdID);
                if (val != MapRawOrdIdToSerialMBOrdId.end()) {
//                    std::cout << "T:RAW:SERIAL=" << (int64_t)((TRD_MSG*)buffer)->dblSellOrdID << ":" << (int64_t)val->second << std::endl;
                  #ifdef __MB_LOG__  
                  snprintf(logBufMB, 400, "Thread_MB|T:RAW:SERIAL=%ld:%ld", (int64_t)((TRD_MSG*)buffer)->dblSellOrdID,(int64_t)val->second);
                    Logger::getLogger().log(DEBUG, logBufMB); 
                   #endif
                    ((TRD_MSG*)buffer)->dblSellOrdID = val->second;
                   
                    result = true;
                }
                else{
//                    std::cout << "ERROR :TRD:Match for RAW Sell Order ID : " << (int64_t)((TRD_MSG*)buffer)->dblSellOrdID 
//                              << ", Not Found, sending 0" << std::endl;
                    #ifdef __MB_LOG__  
                    snprintf(logBufMB, 400, "Thread_MB|ERROR :TRD:Match for RAW Sell Order ID : :%ld , Not Found, sending 0", (int64_t)((TRD_MSG*)buffer)->dblSellOrdID);
                    Logger::getLogger().log(DEBUG, logBufMB); 
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


void _processingMB (const char* dataFile){
    
    ifstream MBFile(dataFile, ios::in | ios::binary);
    
    GET_PERF_TIME(File_Starts);
    if (MBFile.is_open()) 
    {
        char buffer[128];
        while (true)
        {
            MBFile.read(buffer, FIX_SIZE_LEN);
            if (!MBFile) 
            {
              GET_PERF_TIME(File_Ends);
              std::cout << "Data File Completed,Time Taken::" <<File_Ends-File_Starts<< std::endl;
                std::cout << "ERROR : Only " << MBFile.gcount() << "bytes were able to be read out of " << FIX_SIZE_LEN << std::endl;
                std::cout << "Data File Completed" << std::endl;
                exit(0);
            }
            if (!_isMBTokenAllowed(buffer) || !_isMBMessageAllowed(buffer))
            {
                continue;
            }

            if (!_updateMBOrderNumber(buffer))
            {
//                        std::cout<< "Error! Failed to update the order Number for following order" << std::endl;
                #ifdef __MB_LOG__
                  snprintf(logBufMB, 400, "Error! Failed to update the order Number for following order");
                  Logger::getLogger().log(DEBUG, logBufMB);
                #endif
//                        printMBRecord(buffer, "Unable to update OrderNumber");
                continue;
            }
               
            _processMBMessage(buffer);
        }
        MBFile.close();

    } else {
        std::cout << "ERROR : Unable to open the file" << dataFile <<  std::endl;
    }
}



void _processingMBFO(const char * MB_filename)
{
  ifstream MBFile(MB_filename, ios::in | ios::binary);
  GET_PERF_TIME(File_Starts);
  if (MBFile.is_open()) 
  {
      char buffer[128];
      while(true)
      {
        MBFile.read(buffer, FIX_SIZE_LEN);

        if (!MBFile) 
        {
            GET_PERF_TIME(File_Ends);
            std::cout << "Data File Completed,Time Taken::" <<File_Ends-File_Starts<< std::endl;
            std::cout << "ERROR : Only " << MBFile.gcount() << "bytes were able to be read out of " << FIX_SIZE_LEN << std::endl;
//          std::cout << "Data File Completed Total Record:" << transCount << std::endl;        

            exit(0);
        }
        if (!_isMBTokenAllowed(buffer) || !_isMBMessageAllowed(buffer))
        {
          continue;
        }
        if (!_updateMBOrderNumber(buffer))
        {
          #ifdef __MB_LOG__  
          snprintf(logBufMB, 400, "Thread_MB|Error! Failed to update the order Number for following order");
          Logger::getLogger().log(DEBUG, logBufMB);
          #endif
//        std::cout<< "Error! Failed to update the order Number for following order" << std::endl;
//        printMBRecord(buffer, "Unable to update OrderNumber");
          continue;
        }
           
        _processMBMessage(buffer);//same for FO and CM
      }
      MBFile.close();
  }
}
enum EXCHANGE_SEGMENT_ID : short {
    NSEFO = 1,
    NSECM = 2,
    NSECDS = 3
};

void _initFilteredMBTokens(const char* subFilePath){
    int count=0;
    std::ifstream subscription_stream;

    memset(&filteredMBTokens, 0, MB_TOKEN_ARRAY_SIZE);
    
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
                filteredMBTokens[securityID] = 1;
                std::cout<<"Filtering for tokens:" << securityID << std::endl;
                count++;
            }
            subscription_stream.close();
        }
    }
    if (count>0)
    {
        filterViaSubscriptionMB = true;
        std::cout << "Filtering for Tokens count " << count << std::endl;
    }
}

void Start_MarketBook(ProducerConsumerQueue<GENERIC_ORD_MSG> *pInqptr_MBtoME,std::string MBConfig)
{
  
  std::cout<<"MB started::"<< std::endl;
//  while(gStartMBFlag==0)
//  {
//    
//  }
  // Parse PRSConfig.ini
  ConfigReader lszConfigReader(MBConfig);

  _readConfigParams(lszConfigReader);
  std::string MBFilePath = configDataMB.MB_filename;
  snprintf(logBufMB, 400, "Thread_MB|MB started|MB file path :%s",MBFilePath.c_str());
  Logger::getLogger().log(DEBUG, logBufMB);
  
  TaskSetCPU(configDataMB.MBCoreId);
  _iOldestOrderTimeLocn = 0;
  _pOrderTimeStamps = new uint64_t[_iOrderThresholdCount];
  for (int32_t ii = 0; ii < _iOrderThresholdCount; ii++) {
      _pOrderTimeStamps[ii] = 0;
  }
  Inqptr_MBtoME = pInqptr_MBtoME;
  
  _initFilteredMBTokens("Token.txt");
  
  if (configDataMB.Segment==NSEFO)
  {
    _processingMBFO(configDataMB.MB_filename.c_str());
  }
  else if(configDataMB.Segment==NSECM)
  {
    std::cout<<"filename::"<<configDataMB.MB_filename<<std::endl;
    _processingMB(configDataMB.MB_filename.c_str());
    
  }
}
