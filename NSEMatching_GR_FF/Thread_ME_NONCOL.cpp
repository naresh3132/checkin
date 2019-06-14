
/* 
 * File:   Thread_ME.cpp
 * Author: muditsharma
 *
 * Created on March 1, 2016, 5:00 PM
 */

#include "Thread_ME.h"
#include "spsc_atomic1.h"
#include "All_Structures.h"
#include "nsecm_exch_structs.h"
#include "BrodcastStruct.h"
#include <chrono>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fstream>
#include<sys/socket.h>


/*switch(_nSegMode)
{
  case NSECM:
  {

  }
  break;
  case NSEFO:
  {
  }
  break;
  default:
  break;
}*/

#define STORESIZE 20000
//#define STORESIZE 200000
#define FOOFFSET 35000
#define ORDERNO 1000000000000000

typedef std::unordered_map<std::string, int32_t> NSECMToken;
typedef NSECMToken::iterator TokenItr;

ORDER_BOOK_MAIN ME_OrderBook;
TOKEN_INDEX_MAPPING TokenIndexMapping;
//ORDER_BOOK_MAIN ME_SL_OrderBook;
//Both trade packets are same for NSECM and NSEFO hence any can be used for this array
//TRADE_CONFIRMATION_OS OrderInfofortrade[STORESIZE];
NSECM_STORE_TRIM Order_Store_NSECM[STORESIZE];
NSEFO_STORE_TRIM Order_Store_NSEFO[STORESIZE];


//NSEFO::TRADE_CONFIRMATION_TR OrderInfofortrade[100000];
long ME_OrderNumber;
long GlobalIncrement; // Only for sending incremental timestamp1 - Jiffy
long GlobalSeqNo; // For incrementing order / mod seq
long GlobalBrodcastSeqNo;
NSECMToken *pNSECMContract;
int*  TokenStore;
int TokenCount;
dealerInfoMap* dealerInfoMapGlobal;
CONTRACTINFO* contractInfoGlobal;
  
BROADCAST_DATA FillData;
BROADCAST_DATA AddModCan;
ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast_Global;
ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MeToLog_Global;
DATA_RECEIVED LogData;
int _nSegMode;
int iMaxWriteAttempt_Global;
int iEpochBase_Global;
char logBuf[500];

NSEFO::MultiLegOrderInfo MLOrderInfo[3];
int noOfLegs;
#define strMove(d,s) memmove((d), (s), strlen(s) + 1)

char *LTrim(char *str)
{
	char *obuf;
   if (str)
   {
		for (obuf = str; *obuf && isspace(*obuf); ++obuf);
		if (str != obuf)
			strMove(str, obuf);
   }
   return str;
}

char *RTrim(char *str)
{
	int i;
   if (str && 0 != (i = strlen(str)))
   {
		while (--i >= 0)
      {
			if (!isspace(str[i]))
				break;
		}
		str[++i] = '\0';
	}
   return str;
}

char *Trim(char *str)
{
	LTrim(str);
	RTrim(str);
	return str;
}
  
 
 int _gcd(int u, int v)
 {
   int r = 0;
   while (v != 0)
   {
     r = u%v;
     u = v;
     v = r;
   }
   return u;
}

int FileDigesterCM (char* pStrFileName)
{
  assert(pStrFileName);
  
  int nRet = 0;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    snprintf(logBuf, 500, "Thread_ME|Error while opening file %s", pStrFileName);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout << " Error while opening " << pStrFileName << std::endl;
    return nRet;
  }  

  int lnCount = 0;
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remove the first line containing the file version 
  {
    free(pTemp);
  }
  
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;

    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      std::string strLine(pLine);
      size_t startPoint = 0, endPoint = strLine.length();
      int  i = 1;
      OMSScripInfo lstOMSScripInfo;
      memset(&lstOMSScripInfo, 0, sizeof(OMSScripInfo));
      while(i <= 56)//startPoint != string::npos)
      {
        endPoint = strLine.find('|',startPoint);
        if(endPoint != std::string::npos)
        {
          switch(i)
          {
            case SECURITY_TOKEN:
              lstOMSScripInfo.nToken  = atol((strLine.substr(startPoint,endPoint - startPoint)).c_str());
              startPoint = endPoint + 1;
              break;
             
            case SECURITY_SYMBOL:
              strncpy(lstOMSScripInfo.cSymbol, (strLine.substr(startPoint, endPoint - startPoint)).c_str(), 10);
              startPoint = endPoint + 1;
              break;
              
              
            case SECURITY_SERIES:
              strncpy(lstOMSScripInfo.cSeries, (strLine.substr(startPoint, endPoint - startPoint)).c_str(), 10);
              startPoint = endPoint + 1;
              break;  
              
            default:
              startPoint = endPoint + 1;  
              break; 
          } //switch(i) 
        } //if(endPoint != string::npos)
        i++;
      }//while(i <= 68)
      
      if(pLine != NULL)
      {free(pLine);}
    } //if(getline(&pLine,&sizeLine,fp)
    else
    {
      //cout << errno << " returned while reading line." << endl; 
    } 
  } //while(!feof(fp)) 
  
  return 1;
} 



static inline int64_t getCurrentTimeInNano()
{
    TS_VAR(currTime);
    GET_CURR_TIME(currTime);
    return TIMESTAMP(currTime);
}

static inline int32_t getCurrentTimeMicro()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec)* 1000000 + tv.tv_usec)/100;
}

static inline int32_t getEpochTime()
{
    int32_t epochTime = time(0);
    if (iEpochBase_Global == 1980)
    {
      epochTime =  epochTime - 315532800;  /*315532800 = No of Seconds elapsed from 1970 to 1980*/
    }
    
    epochTime = epochTime + 19800;  /*GMT + 5:30 Indian time*/
    return epochTime;
}

int SendToClient(int FD, char* msg, int msgLen, CONNINFO* pConnInfo)
{
    int bytesSent = 0;
    int retVal = 0;

    if (pConnInfo->status != CONNECTED)
    {
	return -1;
    }
        //i = write( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR));
       // i = send(FD, msg, msgLen, MSG_NOSIGNAL);
           int dataSent = 0;
           bool bExit= false;
           int iWriteAttempt = 0;   
           while (bExit == false)
           {
                   //bytesSent = write (FD, (char*)(msg + dataSent), (msgLen-dataSent));
                   bytesSent = send(FD,(char*) (msg + dataSent), (msgLen - dataSent), MSG_DONTWAIT | MSG_NOSIGNAL);
                   if (bytesSent != msgLen)
                   {
                       snprintf(logBuf,500,"Thread_ME|FD %d|SendToClient|Partial data sent|%d|%d|%d",FD, bytesSent, msgLen, errno);
                       Logger::getLogger().log(DEBUG, logBuf);
                        //std::cout<<"SendToClient|Partial data sent|"<<bytesSent<<"|"<<msgLen<<"|"<<errno<<std::endl;
                   }
                   if (bytesSent == -1)
                  {
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                      usleep(10000);
                      if (iMaxWriteAttempt_Global <= iWriteAttempt)
                      {
                         snprintf (logBuf, 500, "Thread_ME|FD %d|Disconnecting slow client|Error %d", FD, errno);
                         Logger::getLogger().log(DEBUG, logBuf);
                         //std::cout <<"FD "<<FD<<"|Disconnecting slow client|" << errno << std::endl;
                         pConnInfo->status = DISCONNECTED;
                         bExit = true;  
                         retVal = -1;
                      }
                       iWriteAttempt++;
                    }
                     else
	{
          snprintf(logBuf, 500, "Thread_ME|FD %d|Issue received|Drop connection| last error code|%d", FD, errno);
          Logger::getLogger().log(DEBUG, logBuf);
	     //std::cout <<"FD "<<FD<<"|Issue received|Drop connection| last error code| " << errno << std::endl;
                          pConnInfo->status = DISCONNECTED;
                          bExit = true;  
                          retVal = -1;
	}	
                  }
                  else
                  {
                     iWriteAttempt = 0;
                     dataSent += bytesSent;
                     if (dataSent == msgLen) 
                     {
                        bExit = true;
                     } 
                  }
           } 
    
   
    return bytesSent;
}




//std::ref(Inqptr_TCPServerToMe),std::ref(Inqptr_MeToTCPServer),std::ref(Inqptr_METoBroadcast)
void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,
             ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast, 
             std::unordered_map<std::string, int32_t>* pcNSECMTokenStore, int _nMode,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToLog, int iMaxWriteAttempt, int iEpochBase, int iMECore,
             int* Tokenarr, int TokenCnt, dealerInfoMap* dealerInfomap, CONTRACTINFO* pCntrctInfo)
{
    TaskSetCPU(iMECore);

    /* 
    ORDER_BOOK_TOKEN*  pOrderBookToken = new ORDER_BOOK_TOKEN[TokenCnt];
    ME_OrderBook.OrderBook = pOrderBookToken;
     */
    snprintf(logBuf,500, "Thread_ME|Matching Thread started");
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<< "Matching Thread started" << std::endl;   
     snprintf(logBuf,500, "Thread_ME|NOOFTOKENS: %d", NOOFTOKENS);
     Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<< "NOOFTOKENS: "<<NOOFTOKENS<< std::endl;       
    snprintf(logBuf,500, "Thread_ME|STORESIZE: %d", STORESIZE);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<< "STORESIZE: "<<STORESIZE<< std::endl;   
     snprintf(logBuf,500, "Thread_ME|BOOKSIZE: %d", BOOKSIZE);
    Logger::getLogger().log(DEBUG, logBuf);
     //std::cout<< "BOOKSIZE: "<<BOOKSIZE<< std::endl;   
     snprintf(logBuf,500, "Thread_ME|FOOFFSET: %d", FOOFFSET);
    Logger::getLogger().log(DEBUG, logBuf);
      //std::cout<< "FOOFFSET: "<<FOOFFSET<< std::endl;   
    
    DATA_RECEIVED RcvData;  
    Inqptr_METoBroadcast_Global = Inqptr_METoBroadcast;
    Inqptr_MeToLog_Global = Inqptr_MeToLog; /*Sneha*/
    iMaxWriteAttempt_Global = iMaxWriteAttempt; /*Sneha*/
    iEpochBase_Global = iEpochBase; /*Sneha*/
    pNSECMContract = pcNSECMTokenStore;
    TokenStore = Tokenarr;
    TokenCount = TokenCnt;
    dealerInfoMapGlobal = dealerInfomap;    
    contractInfoGlobal = pCntrctInfo;
    
    //ME_OrderNumber = (rand() % 100) * 1000 ;
    ME_OrderNumber = 1;
    GlobalIncrement = 1;
    GlobalBrodcastSeqNo = 1;
    GlobalSeqNo = 1;
    noOfLegs = 0;
    _nSegMode = _nMode;
    
    
    // Set Broadcast Structures
    
    FillData.stBcastMsg.stTrdMsg.header.wStremID = 1 ;
    FillData.stBcastMsg.stTrdMsg.cMsgType = 'T';
    FillData.wPacketType = 2;  // Add - Mod - Can
    AddModCan.stBcastMsg.stGegenricOrdMsg.header.wStremID=1 ;  
    AddModCan.wPacketType = 1;
    
    // Done Broadcast Structures
    
    
    
    for(int i=0;i< NOOFTOKENS;i++)
    {
        ME_OrderBook.OrderBook[i].BuyRecords = 0;
        ME_OrderBook.OrderBook[i].SellRecords = 0;
        ME_OrderBook.OrderBook[i].TradeNo = 0;
        ME_OrderBook.OrderBook[i].BuySeqNo =0;
        ME_OrderBook.OrderBook[i].SellSeqNo=0;
        
        
        for(int j=0; j < BOOKSIZE; j++)
        {
            ME_OrderBook.OrderBook[i].Buy[j].lPrice = 0;
            ME_OrderBook.OrderBook[i].Buy[j].DQty = 0;
            ME_OrderBook.OrderBook[i].Buy[j].IsDQ =0 ;
            ME_OrderBook.OrderBook[i].Buy[j].IsIOC = 0;
            ME_OrderBook.OrderBook[i].Buy[j].OpenQty = 0;
            ME_OrderBook.OrderBook[i].Buy[j].OrderNo = 0;
            ME_OrderBook.OrderBook[i].Buy[j].SeqNo = 0;
            ME_OrderBook.OrderBook[i].Buy[j].TTQ = 0;
            ME_OrderBook.OrderBook[i].Buy[j].lQty = 0;
            
            
            ME_OrderBook.OrderBook[i].Sell[j].lPrice = 999999999999;
            ME_OrderBook.OrderBook[i].Sell[j].DQty = 0;
            ME_OrderBook.OrderBook[i].Sell[j].IsDQ =0;
            ME_OrderBook.OrderBook[i].Sell[j].IsIOC = 0;
            ME_OrderBook.OrderBook[i].Sell[j].OpenQty = 0;
            ME_OrderBook.OrderBook[i].Sell[j].OrderNo = 0;
            ME_OrderBook.OrderBook[i].Sell[j].SeqNo = 0;
            ME_OrderBook.OrderBook[i].Sell[j].TTQ = 0;
            ME_OrderBook.OrderBook[i].Sell[j].lQty = 0;
            
        }    
        
        
    }    
    int milisec = 2; // length of time to sleep, in miliseconds
    struct timespec req = {0};

    while(1)
    {
        for(int j=0; j < 10 ; j++) 
        {
            if(Inqptr_TCPServerToMe->dequeue(RcvData))
            {
            //ME_MESSAGE_HEADER *tapHdr=(ME_MESSAGE_HEADER *)client_message.msgBuffer;  
            //tapHdr->swapBytes();

            //CLIENT_MSG *client_message = (CLIENT_MSG *)RcvData.msgBuffer;
               ProcessTranscodes(&RcvData,std::ref(Inqptr_MeToTCPServer));
               (RcvData.ptrConnInfo)->recordCnt--;
              
                //std::cout << " Packet received" << std::endl;
            
                memset(&RcvData, 0, sizeof(RcvData));
                
                //sleep(2);
             }
        }            
         req.tv_sec = 0;
         req.tv_nsec = milisec * 1000000L;
         //nanosleep(&req, (struct timespec *)NULL); 
    }        
}  


int SendOrderCancellation_NSECM(long OrderNumber, long Token,short Segment,int FD,CONNINFO* pConnInfo)
{
    
    NSECM::MS_OE_RESPONSE_TR CanOrdResp;
    //memcpy(&CanOrdResp.tap_hdr,& Order_Store_NSECM[OrderNumber], sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = Order_Store_NSECM[OrderNumber].TraderId;
    CanOrdResp.ErrorCode = 0;
    CanOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.TimeStamp1 = __bswap_64(CanOrdResp.TimeStamp1); /*sneha*/
    CanOrdResp.TimeStamp2 = '1'; /*Sneha*/
    CanOrdResp.BookType = Order_Store_NSECM[OrderNumber].BookType;
    memcpy(&CanOrdResp.AccountNumber,&Order_Store_NSECM[OrderNumber].AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&Order_Store_NSECM[OrderNumber].BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = Order_Store_NSECM[OrderNumber].DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemain = Order_Store_NSECM[OrderNumber].DisclosedVolume;
    CanOrdResp.TotalVolumeRemain = Order_Store_NSECM[OrderNumber].Volume;
    CanOrdResp.Volume = Order_Store_NSECM[OrderNumber].Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = Order_Store_NSECM[OrderNumber].Price;
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    CanOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&CanOrdResp.OrderFlags,& Order_Store_NSECM[OrderNumber].OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = Order_Store_NSECM[OrderNumber].BranchId;
    CanOrdResp.UserId = Order_Store_NSECM[OrderNumber].UserId;        
    memcpy(&CanOrdResp.BrokerId,&Order_Store_NSECM[OrderNumber].BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    CanOrdResp.Suspended = Order_Store_NSECM[OrderNumber].Suspended;       
    memcpy(&CanOrdResp.Settlor,&Order_Store_NSECM[OrderNumber].Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClient = Order_Store_NSECM[OrderNumber].ProClientIndicator;
    CanOrdResp.SettlementPeriod =  __bswap_16(1);       
    memcpy(&CanOrdResp.sec_info,&Order_Store_NSECM[OrderNumber].sec_info,sizeof(CanOrdResp.sec_info));
    memcpy(&CanOrdResp.NnfField,&Order_Store_NSECM[OrderNumber].NnfField,sizeof(CanOrdResp.NnfField));       
    CanOrdResp.TransactionId=Order_Store_NSECM[OrderNumber].TransactionId;
    CanOrdResp.OrderNumber = Order_Store_NSECM[OrderNumber].OrderNumber;
     //std::cout <<  "Order Number print " << CanOrdResp.OrderNumber << "   "  << __bswap_32(CanOrdResp.TransactionId) <<  std::endl;
    //SwapDouble((char*) &CanOrdResp.OrderNumber);
    CanOrdResp.tap_hdr.swapBytes(); 
     //std::cout <<  "Order Number print " << CanOrdResp.OrderNumber << "   "  << OrderNumber <<  std::endl;
     SwapDouble((char*) &CanOrdResp.OrderNumber);  
   
    char Symbol[10 + 1] = {0};
    //strncpy(Symbol, ModOrder->sec_info.Symbol, 10);
    Trim(Symbol);
    
    char lcSeries[2 + 1] = {0};
    //strncpy(lcSeries, ModOrder->sec_info.Series, 2);
    
    std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
    TokenItr itSymbol = pNSECMContract->find(lszSymbol);
    //long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),itSymbol->second /*1 Please replace token number here*/ );    
    //SwapDouble((char*) &CanOrdResp.OrderNumber);    
    
    int i = 0;
     
    i = SendToClient(FD, (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
    
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d", FD, OrderNumber, i);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout << "Unsol Can|Order # "<<CanOrdResp.OrderNumber<<"|Bytes Sent " << i << std::endl; 
}

int SendOrderCancellation_NSEFO(long OrderNumber, long Token,short Segment,int FD,CONNINFO* pConnInfo)
{
    // Order Cancellation NSEFO - Trim
    
    NSEFO::MS_OE_RESPONSE_TR CanOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));
    
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    //memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = Order_Store_NSEFO[OrderNumber].TraderId;
    CanOrdResp.ErrorCode = 0;       
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1);
    CanOrdResp.Timestamp2 = '1'; /*Sneha*/
    CanOrdResp.BookType = Order_Store_NSEFO[OrderNumber].BookType;
    memcpy(&CanOrdResp.AccountNumber,&Order_Store_NSEFO[OrderNumber].AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&Order_Store_NSEFO[OrderNumber].BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = Order_Store_NSEFO[OrderNumber].DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemaining =Order_Store_NSEFO[OrderNumber].DisclosedVolume;
    CanOrdResp.TotalVolumeRemaining = Order_Store_NSEFO[OrderNumber].Volume;
    CanOrdResp.Volume = Order_Store_NSEFO[OrderNumber].Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = Order_Store_NSEFO[OrderNumber].Price;
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    CanOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&CanOrdResp.OrderFlags,& Order_Store_NSEFO[OrderNumber].OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = Order_Store_NSEFO[OrderNumber].BranchId;
    CanOrdResp.UserId = Order_Store_NSEFO[OrderNumber].UserId;        
    memcpy(&CanOrdResp.BrokerId,&Order_Store_NSEFO[OrderNumber].BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&CanOrdResp.Settlor,&Order_Store_NSEFO[OrderNumber].Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = Order_Store_NSEFO[OrderNumber].ProClientIndicator;
    
    //OrderResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    CanOrdResp.TokenNo = Order_Store_NSEFO[OrderNumber].TokenNo;
    CanOrdResp.filler = Order_Store_NSEFO[OrderNumber].filler;
    CanOrdResp.NnfField = Order_Store_NSEFO[OrderNumber].NnfField;   
    
    //CanOrdResp.filler = ModOrder->filler;
    CanOrdResp.OrderNumber = Order_Store_NSEFO[OrderNumber].OrderNumber;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    CanOrdResp.tap_hdr.swapBytes(); 
    
    //std::cout << " bookdetails.lQty  " << bookdetails.lQty <<  "   " <<  __bswap_32(AddOrder->Volume) <<  std::endl;
    
    //long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),__bswap_32(ModOrder->TokenNo) /*1 Please replace token number here*/ );    
   // SwapDouble((char*) &CanOrdResp.OrderNumber);    
    int i = 0;
  
     i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d", FD, OrderNumber, i);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<< "Unsol Can|Order # "<<CanOrdResp.OrderNumber<<"|Bytes Sent " << i << std::endl; 
}


int ValidateAddReq(int32_t iUserID, int Fd, double dOrderNo, int iOrderSide, long&  Token, char* symbol, char*series)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, Fd);
    if (errCode != 0)
    {
      return errCode;
    }
    
    if (_nSegMode == SEG_NSECM)
    {
       char Symbol[10 +1 ] = {0};
       strncpy(Symbol, symbol, 10);
       Trim(Symbol);
    
       char lcSeries[2 + 1] = {0};
       strncpy(lcSeries, series, 2);
       
        std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
        
        TokenItr itSymbol = pNSECMContract->find(lszSymbol);
        
        if (itSymbol == pNSECMContract->end())
        {
            //snprintf(logBuf,500,"Thread_ME|Error:Token not found in Security file");
            std::cout<<"Thread_ME|Token not found in Security file."<<std::endl;
            errCode = ERR_INVALID_SYMBOL;
            return errCode;
        }
        else
        {
            (Token) = itSymbol->second;
        }
    }
   
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, NULL);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), NULL);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }

    
    return errCode;
}

int AddOrderTrim(NSECM::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO*  pConnInfo)
{
    int MyTime = GlobalSeqNo++;
    long OrderNumber = ME_OrderNumber++;
    ORDER_BOOK_DTLS bookdetails;
    long Token = 0;
    NSECM::MS_OE_RESPONSE_TR OrderResponse;
    
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD,0, __bswap_16(AddOrder->BuySellIndicator), Token,(char *)& (AddOrder->sec_info.Symbol),(char *)&(AddOrder->sec_info.Series));
    //std::cout<<"Token = "<<Token<<std::endl;;
    
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.TimeStamp1 =  getCurrentTimeInNano();
    OrderResponse.TimeStamp1 = __bswap_64(OrderResponse.TimeStamp1); /*sneha*/
    OrderResponse.TimeStamp2 = '1'; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemain = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemain = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = __bswap_32(getEpochTime());
    memcpy(&OrderResponse.OrderFlags,& AddOrder->OrderFlags, sizeof(OrderResponse.OrderFlags));
    OrderResponse.BranchId = AddOrder->BranchId;
    OrderResponse.UserId = AddOrder->UserId;        
    memcpy(&OrderResponse.BrokerId,&AddOrder->BrokerId,sizeof(OrderResponse.BrokerId)) ;      
    OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&OrderResponse.Settlor,&AddOrder->Settlor,sizeof(OrderResponse.Settlor));
    OrderResponse.ProClient = AddOrder->ProClientIndicator;
    OrderResponse.SettlementPeriod =  __bswap_16(1);       
    memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    memcpy(&OrderResponse.NnfField,&AddOrder->NnfField,sizeof(OrderResponse.NnfField));       
    OrderResponse.TransactionId = AddOrder->TransactionId;
    OrderResponse.OrderNumber = OrderNumber ;
    
    OrderResponse.tap_hdr.swapBytes();           
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
     if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|ErrorCode %d|Symbol %s|Series %s|Token %ld",FD, OrderNumber, OrderResponse.ErrorCode, AddOrder->sec_info.Symbol, AddOrder->sec_info.Series, Token );
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Symbol "<<AddOrder->sec_info.Symbol<<"|Series "<<AddOrder->sec_info.Series<<std::endl;
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
        
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 1; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
        
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        
        if (OrderResponse.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }

    bookdetails.FD = FD; /*Sneha - multiple connection changes:15/07/16*/
    bookdetails.IsDQ = IsDQ;
    bookdetails.IsIOC = 0;
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    
    if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
 
    
// Add to order Store  ------------------------------------------------------------------------------------------------- Start
    Order_Store_NSECM[OrderNumber].TraderId = OrderResponse.TraderId;
    Order_Store_NSECM[OrderNumber].BookType = OrderResponse.BookType;
    memcpy(&Order_Store_NSECM[OrderNumber].AccountNumber,&OrderResponse.AccountNumber,sizeof(Order_Store_NSECM[OrderNumber].AccountNumber));
    memcpy(&Order_Store_NSECM[OrderNumber].BuySellIndicator,&OrderResponse.BuySellIndicator,sizeof(Order_Store_NSECM[OrderNumber].BuySellIndicator));
    Order_Store_NSECM[OrderNumber].DisclosedVolume = OrderResponse.DisclosedVolume;
    Order_Store_NSECM[OrderNumber].Volume = OrderResponse.Volume;
    Order_Store_NSECM[OrderNumber].Price = OrderResponse.Price;
    memcpy(&Order_Store_NSECM[OrderNumber].OrderFlags,& OrderResponse.OrderFlags, sizeof(Order_Store_NSECM[OrderNumber].OrderFlags));
    Order_Store_NSECM[OrderNumber].BranchId = OrderResponse.BranchId;
    Order_Store_NSECM[OrderNumber].UserId = OrderResponse.UserId;        
    memcpy(&Order_Store_NSECM[OrderNumber].BrokerId,&OrderResponse.BrokerId,sizeof(Order_Store_NSECM[OrderNumber].BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&Order_Store_NSECM[OrderNumber].Settlor,&OrderResponse.Settlor,sizeof(Order_Store_NSECM[OrderNumber].Settlor));
    Order_Store_NSECM[OrderNumber].ProClientIndicator = OrderResponse.ProClient;
    Order_Store_NSECM[OrderNumber].NnfField = OrderResponse.NnfField;   
    Order_Store_NSECM[OrderNumber].OrderNumber = OrderResponse.OrderNumber;   
    Order_Store_NSECM[OrderNumber].TransactionId = OrderResponse.TransactionId;
    memcpy(&Order_Store_NSECM[OrderNumber].sec_info,&AddOrder->sec_info,sizeof(Order_Store_NSECM[OrderNumber].sec_info));
    
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    //OrderResponse.OrderNumber =  Ord_No;     
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
    
    // Add to order Store  ------------------------------------------------------------------------------------------------- END
    /*char Symbol[10 +1 ] = {0};
    strncpy(Symbol, AddOrder->sec_info.Symbol, 10);
    Trim(Symbol);
    
    char lcSeries[2 + 1] = {0};
    strncpy(lcSeries, AddOrder->sec_info.Series, 2);
 
    std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;

    TokenItr itSymbol = pNSECMContract->find(lszSymbol);*/
 
    
    long datareturn;

    // ---- Store in Data info table for Trade        
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|IOC %d|DQ %d|DQty %d|Qty %d", FD, OrderNumber, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty);    
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<"|Qty "<< bookdetails.lQty<< std::endl;
  
    datareturn = Addtoorderbook(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator),Token/*1 Please replace token number here*/,IsIOC,IsDQ,IsSL, __bswap_constant_32(OrderResponse.LastModified));
   
    int i = 0;
    i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
      
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
        //int SendOrderCancellation_NSECM(long OrderNumber, long Token,short Segment,int FD)
        SendOrderCancellation_NSECM(OrderNumber,Token,0,FD,pConnInfo);
    }    
  
    datareturn = Matching(Token/*1 Please replace token number here*/,FD,IsIOC,IsDQ,pConnInfo);
}

int AddOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO* pConnInfo )
{
    
    int MyTime = GlobalSeqNo++;
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR OrderResponse;
   long Token = (__bswap_32(AddOrder->TokenNo) - FOOFFSET);    
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD, 0, __bswap_16(AddOrder->BuySellIndicator), Token, NULL, NULL);
    
    int OrderNumber =  ME_OrderNumber++;
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.Timestamp1 =  getCurrentTimeInNano();
    OrderResponse.Timestamp1 = __bswap_64(OrderResponse.Timestamp1);/*sneha*/
    OrderResponse.Timestamp2 = '1'; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemaining = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemaining = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = __bswap_32(getEpochTime());
    memcpy(&OrderResponse.OrderFlags,& AddOrder->OrderFlags, sizeof(OrderResponse.OrderFlags));
    OrderResponse.BranchId = AddOrder->BranchId;
    OrderResponse.UserId = AddOrder->UserId;        
    memcpy(&OrderResponse.BrokerId,&AddOrder->BrokerId,sizeof(OrderResponse.BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&OrderResponse.Settlor,&AddOrder->Settlor,sizeof(OrderResponse.Settlor));
    OrderResponse.ProClientIndicator = AddOrder->ProClientIndicator;
    
    //OrderResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    OrderResponse.TokenNo = AddOrder->TokenNo;
    OrderResponse.NnfField = AddOrder->NnfField;       
    OrderResponse.filler = AddOrder->filler;
    OrderResponse.OrderNumber = OrderNumber;
       
    OrderResponse.tap_hdr.swapBytes();       
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|ErrorCode %d|Token %ld", FD, OrderNumber, OrderResponse.ErrorCode, (Token+FOOFFSET));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Token "<<(Token+FOOFFSET)<<std::endl;
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_TR), pConnInfo);
    
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 1; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        if (OrderResponse.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    

    //std::cout << "OrderResponse.filler : " << OrderResponse.filler << " AddOrder->filler : " << AddOrder->filler << std::endl;
    bookdetails.FD = FD; /*Sneha - multiple connection changes:15/07/16*/
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = IsDQ;
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    
    if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|IOC %d|DQ %d|DQty %d", FD,OrderNumber, bookdetails.IsIOC,  bookdetails.IsDQ, bookdetails.DQty);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
    long datareturn = Addtoorderbook(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator), (__bswap_32(AddOrder->TokenNo) - FOOFFSET) /*1 Please replace token number here*/,IsIOC,IsDQ,IsSL , __bswap_constant_32(OrderResponse.LastModified));
   
    // ---- Store in Data info table for Trade
    long ArrayIndex = (long)OrderResponse.OrderNumber;

            
    // ---- Store in Data info table for Trade        
    
    
    //Add Data to Order Store -------------------------------------------------------------------Start
    
    

    Order_Store_NSEFO[OrderNumber].TraderId = OrderResponse.TraderId;
    Order_Store_NSEFO[OrderNumber].BookType = OrderResponse.BookType;
    memcpy(&Order_Store_NSEFO[OrderNumber].AccountNumber,&OrderResponse.AccountNumber,sizeof(Order_Store_NSEFO[OrderNumber].AccountNumber));
    memcpy(&Order_Store_NSEFO[OrderNumber].BuySellIndicator,&OrderResponse.BuySellIndicator,sizeof(Order_Store_NSEFO[OrderNumber].BuySellIndicator));
    Order_Store_NSEFO[OrderNumber].DisclosedVolume = OrderResponse.DisclosedVolume;
    Order_Store_NSEFO[OrderNumber].Volume = OrderResponse.Volume;
    Order_Store_NSEFO[OrderNumber].Price = OrderResponse.Price;
    memcpy(&Order_Store_NSEFO[OrderNumber].OrderFlags,& OrderResponse.OrderFlags, sizeof(Order_Store_NSEFO[OrderNumber].OrderFlags));
    Order_Store_NSEFO[OrderNumber].BranchId = OrderResponse.BranchId;
    Order_Store_NSEFO[OrderNumber].UserId = OrderResponse.UserId;        
    memcpy(&Order_Store_NSEFO[OrderNumber].BrokerId,&OrderResponse.BrokerId,sizeof(Order_Store_NSEFO[OrderNumber].BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&Order_Store_NSEFO[OrderNumber].Settlor,&OrderResponse.Settlor,sizeof(Order_Store_NSEFO[OrderNumber].Settlor));
    Order_Store_NSEFO[OrderNumber].ProClientIndicator = OrderResponse.ProClientIndicator;
    
    //OrderResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    Order_Store_NSEFO[OrderNumber].TokenNo = OrderResponse.TokenNo;
    Order_Store_NSEFO[OrderNumber].NnfField = OrderResponse.NnfField;       
    //CanOrdResp.filler = ModOrder->filler;
    Order_Store_NSEFO[OrderNumber].OrderNumber = OrderResponse.OrderNumber;
    Order_Store_NSEFO[OrderNumber].filler = OrderResponse.filler;
        
    
    //Add Data to Order Store -------------------------------------------------------------------End
    
    
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    //OrderResponse.OrderNumber =  Ord_No;     
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    int i = 0;
    
     i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    //std::cout << "MS_OE_RESPONSE_TR :: Order Number "  <<  ME_OrderNumber <<  "  Bytes Sent " << i << std::endl;    
    /*Sneha*/
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
        //int SendOrderCancellation_NSECM(long OrderNumber, long Token,short Segment,int FD)
        SendOrderCancellation_NSEFO(OrderNumber,(__bswap_32(AddOrder->TokenNo) - FOOFFSET),0,FD,pConnInfo);
    } 
    datareturn = Matching((__bswap_32(AddOrder->TokenNo) - FOOFFSET)/*1 Please replace token number here*/,FD,IsIOC,IsDQ,pConnInfo);
    
}

long Addtoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime ) // 1 Buy , 2 Sell
{
    if(BuySellSide == 1)
    {
        // Start Handling IOC Order -------------------------------------------------
           if(Mybookdetails->lPrice < ME_OrderBook.OrderBook[Token].Sell[0].lPrice &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }    
        
        // End Handling IOC Order -------------------------------------------------        
           
           
           
        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {
                
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[Token].BuySeqNo = ME_OrderBook.OrderBook[Token].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[Token].BuyRecords = ME_OrderBook.OrderBook[Token].BuyRecords + 1;
               
                
           // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
        
               ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].IsIOC = Mybookdetails->IsIOC; 
               ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lQty = Mybookdetails->lQty;
                ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[Token].BuySeqNo = ME_OrderBook.OrderBook[Token].BuySeqNo + 1;
                
            
           }
//            std::cout << " Buy OrderNo  " << ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].OrderNo
//                    << " Price " << ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lPrice
//                    << " Qty  " << ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].lQty
//                    << " Disc Qty  " << ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].DQty
//                    << " Remaining Qty  " << ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].DQRemaining                    
//                    << "  Seq No  " <<  ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].SeqNo
//                    << " IOC " << Mybookdetails->IsIOC 
//                     << std::endl;
           /*Sneha - multiple connection changes:15/07/16*/
           ME_OrderBook.OrderBook[Token].Buy[ME_OrderBook.OrderBook[Token].BuyRecords].FD = Mybookdetails->FD;
           ME_OrderBook.OrderBook[Token].BuyRecords = ME_OrderBook.OrderBook[Token].BuyRecords + 1;
            SortBuySideBook(Token);
            //return ME_OrderBook.OrderBook[Token].BuySeqNo;
    }  
    else
    {
        
        // Start Handling IOC Order -------------------------------------------------
           if(((Mybookdetails->lPrice) > ME_OrderBook.OrderBook[Token].Buy[0].lPrice) &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }           
        
        
        // End Handling IOC Order -------------------------------------------------               


        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {    
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[Token].SellSeqNo = ME_OrderBook.OrderBook[Token].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords + 1;
               
                
           // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    

              ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].IsIOC = Mybookdetails->IsIOC ; 
              ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].OrderNo = Mybookdetails->OrderNo;
              ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lPrice = Mybookdetails->lPrice;
              ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lQty = Mybookdetails->lQty;
              ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].SeqNo = GlobalSeqNo++;
              ME_OrderBook.OrderBook[Token].SellSeqNo = ME_OrderBook.OrderBook[Token].SellSeqNo + 1;
            
           }    
//            std::cout << " Sell OrderNo  " << ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].OrderNo
//                    << " Price " << ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lPrice
//                    << " Qty  " << ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].lQty
//                    << " Disc Qty  " << ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].DQty
//                    << " Remaining Qty  " << ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].DQRemaining
//                    << "  Seq No  " <<  ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].SeqNo
//                    << " IOC " << Mybookdetails->IsIOC
//                    << std::endl;  
           /*Sneha - multiple connection changes:15/07/16*/
            ME_OrderBook.OrderBook[Token].Sell[ME_OrderBook.OrderBook[Token].SellRecords].FD = Mybookdetails->FD;
            ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords + 1;
            SortSellSideBook(Token);
            
            
    }    
            // Enqueue Broadcast Packet 
    AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
    AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'N';
    if(BuySellSide == 1)
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
    }
           
    else    
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;   
    if (_nSegMode == SEG_NSEFO){
       AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
    Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    // End Enqueue Broadcast Packet               
    return 0; 
}

int  SendDnldData(int32_t TraderId, int64_t SeqNo, int Fd,CONNINFO*  pConnInfo)
{
    //std::ifstream infile;
    FILE* infile = NULL;
    char filename[20] = {"\0"}; 
    sprintf (filename, "%d", TraderId);
    strcat(filename, "Dnld");
    SeqNo = __bswap_64(SeqNo);
    snprintf(logBuf, 500, "Thread_ME|FD %d|MsgDnld Req|UserId %s|SeqNo %ld", Fd, filename, SeqNo);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<Fd<<"|MsgDnld Req."<<"|UserId "<<filename<<"|SeqNo "<<SeqNo<<std::endl;
    //infile.open(filename, std::ios_base::in);
    infile = fopen(filename, "r");
    int bytesRead = 0, retVal = 0;
    char buf[3000] = {0};
    bool bConnDrop = false;
    size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
    if (infile != NULL)
    {
        snprintf(logBuf, 500, "Thread_ME|FD %d|File opened for sending DnldMsg..!!", Fd);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"File opened for sending DnldMsg..!!"<<std::endl;
        //while(infile.read(buf, sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA))){
       //while(fread(buf, 1, sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA), infile) == sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA))
        while(((bytesRead = fread(buf, 1, msgdnldSize, infile)) == msgdnldSize) && bConnDrop == false)
        {
          NSECM::MS_MESSAGE_DOWNLOAD_DATA* Dnld_Data= (NSECM::MS_MESSAGE_DOWNLOAD_DATA*)buf;
          int64_t Timestamp1 = __bswap_64 (Dnld_Data->inner_hdr.Timestamp1); 
          snprintf(logBuf, 500, "Thread_ME|FD %d|TimeStamp in file %ld", Fd, Timestamp1);
          Logger::getLogger().log(DEBUG, logBuf);
          //std::cout<<"TimeStamp in file "<<Timestamp1<<std::endl;
          if (SeqNo == 0 || (Timestamp1 >= SeqNo))
          {
              //Dnld_Data->tap_hdr.CheckSum; Fill this....
              Dnld_Data->tap_hdr.sLength = msgdnldSize;
              Dnld_Data->msg_hdr.TransactionCode = DOWNLOAD_DATA;
              Dnld_Data->msg_hdr.AlphaChar[0] = 1;
              Dnld_Data->msg_hdr.ErrorCode = 0;
              Dnld_Data->msg_hdr.LogTime = getCurrentTimeInNano();
              Dnld_Data->msg_hdr.MessageLength = msgdnldSize;
              Dnld_Data->msg_hdr.TimeStamp2[7] = '1';

              Dnld_Data->tap_hdr.swapBytes();
              Dnld_Data->msg_hdr.swapBytes(); 
              
              int16_t tempTransCode = __bswap_16(Dnld_Data->inner_hdr.TransactionCode);
              
             /* if (tempTransCode != NSEFO_2LEG_ADD_CNF && tempTransCode != NSEFO_2LEG_ADD_REJ &&
                   tempTransCode ! = NSEFO_2LEG_CAN_CNF && tempTransCode != NSEFO_3LEG_ADD_CNF &&
                   tempTransCode != NSEFO_3LEG_ADD_REJ && tempTransCode != NSEFO_3LEG_CAN_CNF)*/
              if (tempTransCode >= 20000)
              {
                 tempTransCode = tempTransCode - 18000;
                 Dnld_Data->inner_hdr.TransactionCode = __bswap_16(tempTransCode);
              }
             
               int dnldBytes = SendToClient (Fd, (char*)buf, msgdnldSize, pConnInfo);
               snprintf(logBuf, 500, "Thread_ME|FD %d|Sending MsgDnld|TransCode1 %d|TransCode2  %d|SeqNo %ld|Bytes written %d|Error %d", Fd, __bswap_16(Dnld_Data->msg_hdr.TransactionCode), __bswap_16(Dnld_Data->inner_hdr.TransactionCode), Timestamp1, dnldBytes, errno);
               Logger::getLogger().log(DEBUG, logBuf);
               //std::cout<<"Sending MsgDnld|TransCode1 "<<__bswap_16(Dnld_Data->msg_hdr.TransactionCode)<<"|TransCode2 "<<__bswap_16(Dnld_Data->inner_hdr.TransactionCode)<<"|SeqNo "<<Timestamp1<<"|Bytes written "<<dnldBytes<<"|Error "<<errno<<std::endl;   
              if (pConnInfo->status == DISCONNECTED)
              {
                  bConnDrop = true;
              }
             
              memset(buf, 0, sizeof(buf)); 
           }
        }
        if (bytesRead > 0 && bytesRead != msgdnldSize && bConnDrop == false)
        {
            snprintf(logBuf, 500, "Thread_ME|FD %d|fread FAILED..!!Bytes Read %d", Fd, bytesRead);
            Logger::getLogger().log(DEBUG, logBuf);
            //std::cout<<"fread FAILED..!!Bytes Read "<<bytesRead<<std::endl;
	      sleep(10);	
            exit(1);
        }
        else if(bConnDrop ==  false)
        {
          snprintf(logBuf, 500, "Thread_ME|FD %d|Sending Message Download over..!!",Fd);
          Logger::getLogger().log(DEBUG, logBuf);
          //std::cout<<"Sending Message Download over..!!"<<std::endl;
        }
    }
    else
    {
      snprintf(logBuf, 500, "Thread_ME|FD %d|Could not open file for Sending MsgDnld messages..!!",Fd);
      Logger::getLogger().log(DEBUG, logBuf);
      //std::cout<<"Could not open file for Sending MsgDnld messages..!!"<<std::endl; 
    }
    //infile.close();
    if (infile != NULL)
    {
      fclose(infile);
    }
    
    return retVal;
}

int ValidateUser(int32_t iUserID, int fd)
{
     int ret = 0;
     dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
      if (itr != dealerInfoMapGlobal->end())
     {
            IP_STATUS* pIPSsts = itr->second;
            if (pIPSsts->status == LOGGED_OFF)
            {
                snprintf(logBuf, 500, "Thread_ME|FD %d|ValidateUser|Dealer %d not logged ON", fd,  iUserID);
                Logger::getLogger().log(DEBUG, logBuf);
                ret = ERR_INVALID_USER_ID;
            }
      }
     else
      {
           snprintf(logBuf, 500, "Thread_ME|FD %d|ValidateUser|Dealer %d not found", fd,  iUserID);
            Logger::getLogger().log(DEBUG, logBuf);
           ret = ERR_INVALID_USER_ID;
      }
     
     return ret;
}

bool getConnInfo(int iUserID, int* FD, CONNINFO** ConnInfo)
{
    dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
     if (itr != dealerInfoMapGlobal->end())
     {
           if ((itr->second)->status == LOGGED_ON)
           {
               (*ConnInfo) =  (itr->second)->ptrConnInfo;
               (*FD) = (itr->second)->FD;
                return true;
           }
           else
           {
                snprintf(logBuf, 500, "Thread_ME|FD %d|getConnInfo|Dealer %d NOT Logged ON", FD, iUserID);
                Logger::getLogger().log(DEBUG, logBuf);
           }
     }
     else
     {
          snprintf(logBuf, 500, "Thread_ME|FD %d|getConnInfo|Dealer %d not found", FD, iUserID);
          Logger::getLogger().log(DEBUG, logBuf);
     }
    return false;
}

int ProcessTranscodes(DATA_RECEIVED * RcvData,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer)
{
    DATA_RECEIVED SendData;
    //CUSTOM_HEADER definition moved to All_Structures.h
    CUSTOM_HEADER *tapHdr=(CUSTOM_HEADER *)RcvData->msgBuffer;  
    tapHdr->swapBytes();    
    int transcode = tapHdr->sTransCode;
    int IsIOC = 0;
    int IsDQ = 0;
    int IsSL = 0;
    int tempIOC = 0;
    int tempSL = 0;
    tapHdr->swapBytes(); 
    switch(transcode)
    {
    case SIGN_ON_REQUEST_IN:
        {
            bool result = false;
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                NSECM::MS_SIGNON_RESP SignonResp;
                NSECM::MS_SIGNON_REQ *SignonReq=(NSECM::MS_SIGNON_REQ *)RcvData->msgBuffer; 
                               
                //MS_SIGNON_REQ *SignonReq=(MS_SIGNON_REQ *)client_message->msgBuffer;  
                //SignonResp.Header.swapBytes();   

                SignonResp.Header.TransactionCode = SIGN_ON_REQUEST_OUT;
                SignonResp.Header.ErrorCode = 0;
                SignonResp.Header.TimeStamp2[7] = 1;  
                SignonResp.Header.sLength = sizeof(NSECM::MS_SIGNON_RESP);
                //memcpy(&SendData.msgBuffer,&SignonResp,sizeof(SendData.msgBuffer));
        //msgBuffer.msgLen = msgHdr->MessageLength ;
                //SendData.Transcode = SIGN_ON_REQUEST_OUT;
                //Inqptr_MeToTCPServer->enqueue(SendData);
                //MS_SIGNON_RESP * SignonResp1=(MS_SIGNON_RESP *)SendData.msgBuffer;
                int Fd = RcvData->MyFd;
                /*Dealer Validation - S*/
                CONNINFO* connSts = RcvData->ptrConnInfo;
                int32_t iUserID = __bswap_32(SignonReq->UserId);
                dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN ON request|UserID %d|IP %s", Fd, iUserID, connSts->IP);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"SIGN ON request|UserID "<<iUserID<<"|IP "<<connSts->IP<<std::endl;
                if (itr != dealerInfoMapGlobal->end())
                {
                     IP_STATUS* pIPSsts = itr->second;
                     int ret = strcmp((connSts->IP), (pIPSsts->IP));
                     if (ret == 0)
                     {
                          if (pIPSsts->status == LOGGED_OFF)
                          {
                                pIPSsts->status = LOGGED_ON;
                                connSts->dealerID = iUserID;
                                pIPSsts->FD = Fd;
                                pIPSsts->ptrConnInfo = connSts;
                          }
                          else if (pIPSsts->status == LOGGED_ON)
                          {
                                snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: User already logged ON", Fd);
                                Logger::getLogger().log(DEBUG, logBuf);
                                //std::cout<<"ERROR: User already logged ON|"<<std::endl;  
                                SignonResp.Header.ErrorCode = ERR_USER_ALREADY_SIGNED_ON;
                          }
                     }
                     else
                     {
                          snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: IP do not match|%s|%s",Fd,connSts->IP,pIPSsts->IP);
                          Logger::getLogger().log(DEBUG, logBuf);
                          //std::cout<<"ERROR: IP do not match|"<<connSts->IP<<"|"<<pIPSsts->IP<<std::endl;  
                          SignonResp.Header.ErrorCode = ERR_INVALID_USER_ID;
                     }
                }
                else
                {
                     snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: Dealer not found",Fd);
                     Logger::getLogger().log(DEBUG, logBuf);
                     //std::cout<<"ERROR: Dealer not found|"<<std::endl;  
                     SignonResp.Header.ErrorCode = ERR_USER_NOT_FOUND;
                }
                /*Dealer Validation - E*/
                
          
                int bytesWritten = 0;

                SignonResp.Header.swapBytes();     
                bytesWritten = SendToClient( Fd , (char *)&SignonResp , sizeof(NSECM::MS_SIGNON_RESP), (RcvData->ptrConnInfo));
                SignonResp.Header.swapBytes();  
                
                if (SignonResp.Header.ErrorCode != 0)
                {
                   connSts->status = DISCONNECTED;
                }
                //int i = write( Fd , (char *)"Hi" , 2);
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN_ON_REQUEST_IN NSECM %d|Error Code %d|Bytes Sent  %d",Fd,SignonResp.Header.TransactionCode,SignonResp.Header.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"FD "<<Fd<<"|SIGN_ON_REQUEST_IN NSECM " <<SignonResp.Header.TransactionCode <<"|Error Code "<<SignonResp.Header.ErrorCode<<"|Bytes Sent " << bytesWritten << std::endl;
              }
              break;
              case SEG_NSEFO:
              {
                NSEFO::MS_SIGNON_RESP SignonResp;
                NSEFO::MS_SIGNON_REQ *SignonReq=(NSEFO::MS_SIGNON_REQ *)RcvData->msgBuffer; 
                //MS_SIGNON_REQ *SignonReq=(MS_SIGNON_REQ *)client_message->msgBuffer;  
                //SignonResp.Header.swapBytes();   


                SignonResp.msg_hdr.TransactionCode = SIGN_ON_REQUEST_OUT;
                SignonResp.msg_hdr.ErrorCode = 0;
                SignonResp.msg_hdr.TimeStamp2[7] = 1;  
                SignonResp.msg_hdr.sLength = sizeof(NSEFO::MS_SIGNON_RESP);
                //memcpy(&SendData.msgBuffer,&SignonResp,sizeof(SendData.msgBuffer));
                 int Fd = RcvData->MyFd;
                 /*Dealer Validation - S*/
                CONNINFO* connSts = RcvData->ptrConnInfo;
                int32_t iUserID = __bswap_32(SignonReq->UserId);
                dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN ON request|UserID %d|IP %s", Fd, iUserID, connSts->IP);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"SIGN ON request|UserID "<<iUserID<<"|IP "<<connSts->IP<<std::endl;
                if (itr != dealerInfoMapGlobal->end())
                {
                     IP_STATUS* pIPSsts = itr->second;
                     int ret = strcmp((connSts->IP), (pIPSsts->IP));
                     if (ret == 0)
                     {
                          if (pIPSsts->status == LOGGED_OFF)
                          {
                                pIPSsts->status = LOGGED_ON;
                                connSts->dealerID = iUserID;
                                pIPSsts->FD = Fd;
                                pIPSsts->ptrConnInfo = connSts;
                          }
                          else if (pIPSsts->status == LOGGED_ON)
                          {
                                snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: User already logged ON",Fd);
                                Logger::getLogger().log(DEBUG, logBuf);
                                //std::cout<<"ERROR: User already logged ON|"<<std::endl;  
                                SignonResp.msg_hdr.ErrorCode = ERR_USER_ALREADY_SIGNED_ON;
                          }
                     }
                     else
                     {
                          snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: IP do not match|%s|%s",Fd,connSts->IP,pIPSsts->IP);
                          Logger::getLogger().log(DEBUG, logBuf);
                          //std::cout<<"ERROR: IP do not match|"<<connSts->IP<<"|"<<pIPSsts->IP<<std::endl;  
                          SignonResp.msg_hdr.ErrorCode = ERR_INVALID_USER_ID;
                     }
                }
                else
                {
                     snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: Dealer not found",Fd); 
                     Logger::getLogger().log(DEBUG, logBuf);
                     //std::cout<<"ERROR: Dealer not found|"<<std::endl;  
                     SignonResp.msg_hdr.ErrorCode = ERR_USER_NOT_FOUND;
                }
                /*Dealer Validation - E*/
                
                SignonResp.msg_hdr.swapBytes();     
                //SendData.Transcode = SIGN_ON_REQUEST_OUT;
                //Inqptr_MeToTCPServer->enqueue(SendData);
                //MS_SIGNON_RESP * SignonResp1=(MS_SIGNON_RESP *)SendData.msgBuffer;
                            int bytesWritten = 0;

                bytesWritten = SendToClient( Fd , (char *)&SignonResp , sizeof(NSEFO::MS_SIGNON_RESP), (RcvData->ptrConnInfo));
                SignonResp.msg_hdr.swapBytes();    
                if (SignonResp.msg_hdr.ErrorCode != 0)
                {
                   connSts->status = DISCONNECTED;
                }
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN_ON_REQUEST_IN NSEFO %d|Error Code %d|Bytes Sent %d",Fd,SignonResp.msg_hdr.TransactionCode,SignonResp.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"FD "<<Fd<<"|SIGN_ON_REQUEST_IN NSEFO " <<SignonResp.msg_hdr.TransactionCode<<"|Error Code "<<SignonResp.msg_hdr.ErrorCode <<"|Bytes Sent " << bytesWritten << std::endl;
              }
              break;
              default:
              break;
            }
            
            //sleep(2);          
        }
        break;
    case SYSTEM_INFORMATION_IN:
        { 
          switch(_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_SYSTEM_INFO_DATA System_Info;//SYSTEM_INFORMATION_OUT(1601)
              NSECM::MS_SYSTEM_INFO_REQ* pSysReq = (NSECM::MS_SYSTEM_INFO_REQ*)RcvData->msgBuffer; 

              int Fd = RcvData->MyFd;
              System_Info.msg_hdr.AlphaChar[0] = 1;
              System_Info.msg_hdr.TimeStamp2[7]  = 1;
              System_Info.msg_hdr.TransactionCode = 1601;
              System_Info.msg_hdr.ErrorCode = 0;
              System_Info.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pSysReq->msg_hdr.TraderId), Fd);
              System_Info.msg_hdr.sLength = sizeof(NSECM::MS_SYSTEM_INFO_DATA);
              System_Info.msg_hdr.swapBytes();
              
              int bytesWritten = 0;
  
              bytesWritten = SendToClient( Fd , (char *)&System_Info , sizeof(NSECM::MS_SYSTEM_INFO_DATA),(RcvData->ptrConnInfo));
              System_Info.msg_hdr.swapBytes();        
               snprintf(logBuf, 500, "Thread_ME|FD %d|SYSTEM_INFORMATION_IN NSECM %d|Error Code %d|Bytes Sent %d",Fd,System_Info.msg_hdr.TransactionCode,System_Info.msg_hdr.ErrorCode, bytesWritten);
               Logger::getLogger().log(DEBUG, logBuf);
               
               if (System_Info.msg_hdr.ErrorCode != 0)
               {
                  (RcvData->ptrConnInfo)->status = DISCONNECTED;
               }
              //std::cout<<"FD "<<Fd<<"|SYSTEM_INFORMATION_IN NSECM " <<System_Info.msg_hdr.TransactionCode <<"|Bytes Sent " << bytesWritten << std::endl;
            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_SYSTEM_INFO_DATA System_Info;//SYSTEM_INFORMATION_OUT(1601)
              NSEFO::MS_SYSTEM_INFO_REQ* pSysReq = (NSEFO::MS_SYSTEM_INFO_REQ*)RcvData->msgBuffer; 
              
              int Fd = RcvData->MyFd;
              System_Info.msg_hdr.AlphaChar[0] = 1;
              System_Info.msg_hdr.TimeStamp2[7]  = 1;
              System_Info.msg_hdr.TransactionCode = 1601;
              System_Info.msg_hdr.ErrorCode = 0;
              System_Info.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pSysReq->msg_hdr.TraderId), Fd);
              System_Info.msg_hdr.sLength = sizeof(NSEFO::MS_SYSTEM_INFO_DATA);
              System_Info.msg_hdr.swapBytes();
              
              int bytesWritten = 0;
              bytesWritten = SendToClient( Fd , (char *)&System_Info , sizeof(NSEFO::MS_SYSTEM_INFO_DATA),(RcvData->ptrConnInfo));
              System_Info.msg_hdr.swapBytes();                 
              snprintf(logBuf, 500, "Thread_ME|FD %d|SYSTEM_INFORMATION_IN NSEFO %d|Error Code %d|Bytes Sent %d",Fd,System_Info.msg_hdr.TransactionCode, System_Info.msg_hdr.ErrorCode, bytesWritten);
              Logger::getLogger().log(DEBUG, logBuf);
              if (System_Info.msg_hdr.ErrorCode != 0)
              {
                (RcvData->ptrConnInfo)->status = DISCONNECTED;
              }
              //std::cout<<"FD "<<Fd<<"|SYSTEM_INFORMATION_IN NSEFO " <<System_Info.msg_hdr.TransactionCode<<"|Bytes Sent " << bytesWritten << std::endl;
            }
            break;
            default:
            break;
          }
            
            
        }
        break;
    case UPDATE_LOCALDB_IN:        
        {               
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                 NSECM::UPDATE_LDB_HEADER Update_Hdr;
                 NSECM::MS_UPDATE_LOCAL_DATABASE* pUptDBReq = ( NSECM::MS_UPDATE_LOCAL_DATABASE* )RcvData->msgBuffer;
                 
                 int Fd = RcvData->MyFd;
                 Update_Hdr.msg_hdr.AlphaChar[0] = 1;
                 Update_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                 Update_Hdr.msg_hdr.TransactionCode = UPDATE_LOCALDB_HEADER;
                 Update_Hdr.msg_hdr.ErrorCode = 0;
                 Update_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pUptDBReq->msg_hdr.TraderId), Fd);
                 Update_Hdr.msg_hdr.sLength = sizeof(NSECM::UPDATE_LDB_HEADER);
                 Update_Hdr.msg_hdr.swapBytes();
                
                 int bytesWritten = 0;

                 bytesWritten = SendToClient( Fd , (char *)&Update_Hdr , sizeof(NSECM::UPDATE_LDB_HEADER),(RcvData->ptrConnInfo));
                 Update_Hdr.msg_hdr.swapBytes();              
                 snprintf(logBuf, 500, "Thread_ME|FD %d|UPDATE_LDB_HEADER NSECM %d|Error Code %d|Bytes Sent %d", Fd, Update_Hdr.msg_hdr.TransactionCode, Update_Hdr.msg_hdr.ErrorCode, bytesWritten);
                 Logger::getLogger().log(DEBUG, logBuf);
                 if (Update_Hdr.msg_hdr.ErrorCode != 0)
                 {
                   (RcvData->ptrConnInfo)->status = DISCONNECTED;
                 }
                 //std::cout<<"FD "<<Fd<<"|UPDATE_LDB_HEADER NSECM " <<Update_Hdr.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<< std::endl;            


                  NSECM::MS_PARTIAL_SYS_INFO Update_Trail;
                  Update_Trail.msg_hdr.AlphaChar[0] = 1;
                  Update_Trail.msg_hdr.TimeStamp2[7]  = 1;

                  Update_Trail.msg_hdr.TransactionCode = UPDATE_LOCALDB_TRAILER;
                  Update_Trail.msg_hdr.ErrorCode = 0;
                  Update_Trail.msg_hdr.ErrorCode = Update_Hdr.msg_hdr.ErrorCode;
                  Update_Trail.msg_hdr.sLength = sizeof(NSECM::MS_PARTIAL_SYS_INFO);
                  Update_Trail.msg_hdr.swapBytes();
                  //int Fd = RcvData->MyFd;
                  
                  bytesWritten = SendToClient( Fd , (char *)&Update_Trail , sizeof(NSECM::MS_PARTIAL_SYS_INFO),(RcvData->ptrConnInfo));
                  Update_Trail.msg_hdr.swapBytes();               
                  snprintf(logBuf, 500, "Thread_ME|FD %d|MS_PARTIAL_SYS_INFO NSECM %d|Error Code %d|Bytes Sent %d", Fd, Update_Trail.msg_hdr.TransactionCode, Update_Trail.msg_hdr.ErrorCode , bytesWritten);
                  Logger::getLogger().log(DEBUG, logBuf);
                  //std::cout<<"FD "<<Fd<<"|MS_PARTIAL_SYS_INFO NSECM " <<Update_Trail.msg_hdr.TransactionCode<<"|Bytes Sent " <<bytesWritten<<std::endl;  
              }
              break;
              case SEG_NSEFO:
              {
                 NSEFO::EXCH_PORTFOLIO_RESP Exchg_Portf_Resp;
                 NSEFO::EXCH_PORTFOLIO_REQ*  pPortFolioReq = (NSEFO::EXCH_PORTFOLIO_REQ*)RcvData->msgBuffer;
                 
                 int Fd = RcvData->MyFd;
                 Exchg_Portf_Resp.msg_hdr.AlphaChar[0] = 1;
                 Exchg_Portf_Resp.msg_hdr.TimeStamp2[7]  = 1;
                 Exchg_Portf_Resp.msg_hdr.TransactionCode = EXCH_PORTF_OUT;
                 Exchg_Portf_Resp.msg_hdr.ErrorCode = 0;
                 Exchg_Portf_Resp.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pPortFolioReq->msg_hdr.TraderId),Fd);
                 Exchg_Portf_Resp.msg_hdr.sLength= sizeof(NSEFO::EXCH_PORTFOLIO_RESP);

                 Exchg_Portf_Resp.MoreRecs = 'N';
                 Exchg_Portf_Resp.msg_hdr.swapBytes();
              
                 int bytesWritten = 0;
                 bytesWritten = SendToClient( Fd , (char *)&Exchg_Portf_Resp , sizeof(NSEFO::EXCH_PORTFOLIO_RESP),(RcvData->ptrConnInfo));
                 Exchg_Portf_Resp.msg_hdr.swapBytes();                 
                 snprintf(logBuf, 500, "Thread_ME|FD %d|EXCH_PORTFOLIO_RESP NSEFO %d|Error Code %d|Bytes Sent %d",Fd, Exchg_Portf_Resp.msg_hdr.TransactionCode, Exchg_Portf_Resp.msg_hdr.ErrorCode, bytesWritten);
                 Logger::getLogger().log(DEBUG, logBuf);
                 if (Exchg_Portf_Resp.msg_hdr.ErrorCode != 0)
                 {
                    (RcvData->ptrConnInfo)->status = DISCONNECTED;
                 }
                 //std::cout<<"FD "<<Fd<<"|EXCH_PORTFOLIO_RESP NSEFO "<<Exchg_Portf_Resp.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<< std::endl;            
               }
              break;
              default:
              break;
            }
                             
        }
        break;
    case INDUSTRY_INDEX_DLOAD_IN:
        {           
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                NSECM::MS_INDUSTRY_INDEX_DLOAD_RESP Industry_Dload_index;
                NSECM::MS_INDUSTRY_INDEX_DLOAD_REQ* pIndusDnldReq = (NSECM::MS_INDUSTRY_INDEX_DLOAD_REQ*) RcvData->msgBuffer;
                
                 int Fd = RcvData->MyFd;
                Industry_Dload_index.msg_hdr.TimeStamp2[7]  = 1;
                Industry_Dload_index.msg_hdr.TransactionCode = INDUSTRY_INDEX_DLOAD_OUT;
                Industry_Dload_index.msg_hdr.ErrorCode = 0;
                Industry_Dload_index.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pIndusDnldReq->msg_hdr.TraderId), Fd);
                Industry_Dload_index.msg_hdr.sLength = sizeof(NSECM::MS_INDUSTRY_INDEX_DLOAD_RESP);
                Industry_Dload_index.NumberOfRecords =__bswap_16(1);
                strcpy(Industry_Dload_index.Index_Dload_Data[0].IndexName,"S & P NIFTY");
                Industry_Dload_index.Index_Dload_Data[0].IndexValue = 0;
                Industry_Dload_index.Index_Dload_Data[0].IndustryCode = 0;  
                Industry_Dload_index.msg_hdr.swapBytes();
               
                int bytesWritten = 0;
                bytesWritten = SendToClient( Fd , (char *)&Industry_Dload_index , sizeof(NSECM::MS_INDUSTRY_INDEX_DLOAD_RESP),(RcvData->ptrConnInfo));
                Industry_Dload_index.msg_hdr.swapBytes();
                snprintf(logBuf, 500, "Thread_ME|FD %d|INDUSTRY_INDEX_DLOAD_IN NSECM %d|Error Code %d|Bytes Sent %d", Fd, Industry_Dload_index.msg_hdr.TransactionCode,Industry_Dload_index.msg_hdr.ErrorCode ,bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                if (Industry_Dload_index.msg_hdr.ErrorCode != 0)
                {
                  (RcvData->ptrConnInfo)->status = DISCONNECTED;
                }
                //std::cout<<"FD "<<Fd<<"|INDUSTRY_INDEX_DLOAD_IN NSECM "<<Industry_Dload_index.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl;
              }
              break;
              default:
              break;
            }
                                  
        }
        break;
    case DOWNLOAD_REQUEST:
        {
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                NSECM::MS_MESSAGE_DOWNLOAD_HEADER Dload_Hdr;
                NSECM::MS_MESSAGE_DOWNLOAD_REQ* MsgDnldRequest=(NSECM::MS_MESSAGE_DOWNLOAD_REQ *)RcvData->msgBuffer; 
                
                int Fd = RcvData->MyFd;
                Dload_Hdr.msg_hdr.AlphaChar[0] = 1;
                Dload_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                Dload_Hdr.msg_hdr.TransactionCode = DOWNLOAD_HEADER;
                Dload_Hdr.msg_hdr.ErrorCode = 0;
                Dload_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(MsgDnldRequest->msg_hdr.TraderId), Fd);
                Dload_Hdr.msg_hdr.sLength = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_HEADER);
                Dload_Hdr.msg_hdr.swapBytes();
     
                int bytesWritten = 0;
               bytesWritten = SendToClient( Fd , (char *)&Dload_Hdr , sizeof(NSECM::MS_MESSAGE_DOWNLOAD_HEADER), (RcvData->ptrConnInfo));
                Dload_Hdr.msg_hdr.swapBytes();        
                snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_HEADER NSECM %d|Error Code %d|Bytes Sent %d", Fd, Dload_Hdr.msg_hdr.TransactionCode, Dload_Hdr.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                if (Dload_Hdr.msg_hdr.ErrorCode != 0)
                {
                  (RcvData->ptrConnInfo)->status = DISCONNECTED;
                }
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_HEADER NSECM " <<Dload_Hdr.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<< std::endl;  

                /*Send Msg download data - S*/
                if ((RcvData->ptrConnInfo)->status == DISCONNECTED)
                {
                    return 0;
                }
             
                int32_t TraderId = __bswap_32(MsgDnldRequest->msg_hdr.TraderId);
                int dnldRetVal = SendDnldData(TraderId, MsgDnldRequest->SeqNo, RcvData->MyFd, RcvData->ptrConnInfo);
               if ((RcvData->ptrConnInfo)->status == DISCONNECTED)
                {
                    return 0;
                }
                /*Send Msg download data - E*/
                
                NSECM::MS_MESSAGE_DOWNLOAD_TRAILER Dload_Trail; //const int16_t DOWNLOAD_TRAILER            = 7031;
                Dload_Trail.msg_hdr.AlphaChar[0] = 1;
                Dload_Trail.msg_hdr.TimeStamp2[7]  = 1;

                Dload_Trail.msg_hdr.TransactionCode = DOWNLOAD_TRAILER;
                Dload_Trail.msg_hdr.ErrorCode = 0;
                Dload_Trail.msg_hdr.ErrorCode = Dload_Hdr.msg_hdr.ErrorCode;
                Dload_Trail.msg_hdr.sLength = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_TRAILER);
                Dload_Trail.msg_hdr.swapBytes();
                //int Fd = RcvData->MyFd;
              
                bytesWritten = SendToClient( Fd , (char *)&Dload_Trail , sizeof(NSECM::MS_MESSAGE_DOWNLOAD_TRAILER),(RcvData->ptrConnInfo));
                Dload_Trail.msg_hdr.swapBytes();
                snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_TRAILER NSECM %d|Error Code %d|Bytes Sent %d", Fd, Dload_Trail.msg_hdr.TransactionCode, Dload_Trail.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_TRAILER NSECM "<<Dload_Trail.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl; 
              }
              break;
              case SEG_NSEFO:
              {
                NSEFO::MS_MESSAGE_DOWNLOAD_HEADER Dload_Hdr;
                NSEFO::MS_MESSAGE_DOWNLOAD_REQ* MsgDnldRequest=(NSEFO::MS_MESSAGE_DOWNLOAD_REQ *)RcvData->msgBuffer; 
                
                int Fd = RcvData->MyFd;
                Dload_Hdr.msg_hdr.AlphaChar[0] = 1;
                Dload_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                Dload_Hdr.msg_hdr.TransactionCode = DOWNLOAD_HEADER;
                Dload_Hdr.msg_hdr.ErrorCode = 0;
                Dload_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(MsgDnldRequest->msg_hdr.TraderId), Fd);
                Dload_Hdr.msg_hdr.sLength = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_HEADER);
                Dload_Hdr.msg_hdr.swapBytes();
                
                int bytesWritten = 0;
                bytesWritten = SendToClient( Fd , (char *)&Dload_Hdr , sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_HEADER),(RcvData->ptrConnInfo));
                Dload_Hdr.msg_hdr.swapBytes();      
                 snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_HEADER NSEFO %d|Error Code %d|Bytes Sent %d", Fd, Dload_Hdr.msg_hdr.TransactionCode,Dload_Hdr.msg_hdr.ErrorCode, bytesWritten);
                 Logger::getLogger().log(DEBUG, logBuf);
                 if (Dload_Hdr.msg_hdr.ErrorCode != 0)
                 {
                     (RcvData->ptrConnInfo)->status = DISCONNECTED;
                 }
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_HEADER NSEFO "<<Dload_Hdr.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl;  

                 /*Send Msg download data - S*/
                if ((RcvData->ptrConnInfo)->status == DISCONNECTED)
                {
                    return 0;
                }
                
                int32_t TraderId = __bswap_32(MsgDnldRequest->msg_hdr.TraderId);
                SendDnldData(TraderId, MsgDnldRequest->SeqNo, RcvData->MyFd,RcvData->ptrConnInfo);        
                 if ((RcvData->ptrConnInfo)->status == DISCONNECTED)
                {
                    return 0;
                }
                /*Send Msg download data - E*/
                
                NSECM::MS_MESSAGE_DOWNLOAD_TRAILER Dload_Trail; //const int16_t DOWNLOAD_TRAILER            = 7031;
                Dload_Trail.msg_hdr.AlphaChar[0] = 1;
                Dload_Trail.msg_hdr.TimeStamp2[7]  = 1;

                Dload_Trail.msg_hdr.TransactionCode = DOWNLOAD_TRAILER;
                Dload_Trail.msg_hdr.ErrorCode = 0;
                Dload_Trail.msg_hdr.ErrorCode = Dload_Hdr.msg_hdr.ErrorCode;
                Dload_Trail.msg_hdr.sLength = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_TRAILER);
                Dload_Trail.msg_hdr.swapBytes();
                //int Fd = RcvData->MyFd;
               
                bytesWritten = SendToClient( Fd , (char *)&Dload_Trail , sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_TRAILER),(RcvData->ptrConnInfo));
                Dload_Trail.msg_hdr.swapBytes();
                snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_TRAILER NSEFO %d|Error Code %d|Bytes Sent %d", Fd, Dload_Trail.msg_hdr.TransactionCode,Dload_Trail.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_TRAILER NSEFO " <<Dload_Trail.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl;
              }
              break;
              default:
              break;
            }
                         
        }
        break;
    case NSECM_ADD_REQ:
        {
            
        }
        break;
    case NSECM_MOD_REQ:
        {

        }
        break;
    case NSECM_CAN_REQ:
        {

        }
        break;
    case NSECM_ADD_REQ_TR:
        {   
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                NSECM::MS_OE_REQUEST_TR *OrderRequest=(NSECM::MS_OE_REQUEST_TR *)RcvData->msgBuffer;  
                //OrderRequest->tap_hdr.swapBytes();            
                int Fd = RcvData->MyFd;     
                CONNINFO* connSts = RcvData->ptrConnInfo;
                if(OrderRequest->OrderFlags.IOC == 1)
                {
                    IsIOC = 1;
                } 
           
                if(OrderRequest->OrderFlags.SL == 1)
                {
                    IsSL = 1;
                }                 
                if((OrderRequest->DisclosedVolume > 0) && (OrderRequest->DisclosedVolume != OrderRequest->Volume))
                {
                    IsDQ = 1;
                }   
                
                AddOrderTrim(OrderRequest ,Fd,IsIOC,IsDQ,IsSL, connSts);
              }
              break;
              case SEG_NSEFO:
              {
                NSEFO::MS_OE_REQUEST_TR *OrderRequest=(NSEFO::MS_OE_REQUEST_TR *)RcvData->msgBuffer;  
                //OrderRequest->tap_hdr.swapBytes();            
                int Fd = RcvData->MyFd;     
                CONNINFO* connSts = RcvData->ptrConnInfo;
                
                if(OrderRequest->OrderFlags.IOC == 1)
                {
                    IsIOC = 1;
                } 
            
                if(OrderRequest->OrderFlags.SL == 1)
                {
                    IsSL = 1;
                }                 
                if((OrderRequest->DisclosedVolume > 0) && (OrderRequest->DisclosedVolume != OrderRequest->Volume))
                {
                    IsDQ = 1;
                }   

                AddOrderTrim(OrderRequest ,Fd,IsIOC,IsDQ,IsSL,connSts);
              }
              break;
              default:
              break;
            }
            
        }
        break;
    case NSECM_MOD_REQ_TR:
        {
          switch(_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_OM_REQUEST_TR *OrderRequest=(NSECM::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;   
               CONNINFO* connSts = RcvData->ptrConnInfo;
               
                if(OrderRequest->OrderFlags.IOC == 1)
                {
                    IsIOC = 1;
                } 
                
                if(OrderRequest->OrderFlags.SL == 1)
                {
                    IsSL = 1;
                }                 
                if(OrderRequest->DisclosedVolume > 0 && (OrderRequest->DisclosedVolume != OrderRequest->Volume))
                {
                    IsDQ = 1;
                }                
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              ModOrderTrim(OrderRequest,Fd,IsIOC,IsDQ,IsSL,connSts);
            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_OM_REQUEST_TR *OrderRequest=(NSEFO::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              
              int Fd = RcvData->MyFd;   
             CONNINFO* connSts = RcvData->ptrConnInfo;
             
                if(OrderRequest->OrderFlags.IOC == 1)
                {
                    IsIOC = 1;
                } 
                
                if(OrderRequest->OrderFlags.SL == 1)
                {
                    IsSL = 1;
                }                 
                if(OrderRequest->DisclosedVolume > 0 && (OrderRequest->DisclosedVolume != OrderRequest->Volume))
                {
                    IsDQ = 1;
                }    
              
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              //AddOrderTrim(OrderRequest ,Fd);            
              ModOrderTrim(OrderRequest,Fd,IsIOC,IsDQ,IsSL,connSts);
            }
            break;
            default:
            break;
          }
            
        }
        break;
    case NSECM_CAN_REQ_TR:
        {
          switch(_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_OM_REQUEST_TR *OrderRequest=(NSECM::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
               
              //AddOrderTrim(OrderRequest ,Fd);            
              CanOrderTrim(OrderRequest,Fd,connSts);
            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_OM_REQUEST_TR *OrderRequest=(NSEFO::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
              
              //AddOrderTrim(OrderRequest ,Fd);            
              CanOrderTrim(OrderRequest,Fd,connSts);
            }
            break;
            default:
            break;
          }
            
        }
        break;
    case NSEFO_2LEG_ADD_REQ:
        {
            NSEFO::MS_SPD_OE_REQUEST* pMLOrderReq = (NSEFO::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
            int Fd = RcvData->MyFd;      
            CONNINFO* connSts = RcvData->ptrConnInfo;
            AddMLOrder(pMLOrderReq, Fd, connSts);
        }
        break;
    case NSEFO_3LEG_ADD_REQ:
        {
            NSEFO::MS_SPD_OE_REQUEST* pMLOrderReq = (NSEFO::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
            int Fd = RcvData->MyFd;      
            CONNINFO* connSts = RcvData->ptrConnInfo;
            AddMLOrder(pMLOrderReq, Fd, connSts);
        }
        break;
    case NSEFO_SPD_ADD_REQ:
    case NSEFO_SPD_MOD_REQ:
    case NSEFO_SPD_CAN_REQ:
    default:
        {
            //std::cout<<"\nERROR:Invalid Transaction code received :"<<transCode ;
        }
        break;
    }

    return 0;
    
}

int SortBuySideBook(long Token)
{
    long swpPrice;
    long swpQty;
    long SwpSeq;
    long SwpOrdNo;
    long swpDQ;
    short swpIsDQ;    
    short swpIsIOC;
    long swpOpenQty;
    long swpTTQ;
    int swpDQRemaining;
//   long SeqNo;
//   long lQty;
//   long OpenQty;
//   long lPrice;
//   long OrderNo;
//   long TTQ;
//   short IsIOC;
//   short IsDQ;
//   long DQty;
//   int DQRemaining;   

    ORDER_BOOK_DTLS swpbookdtls;
    int i=0;
    
    
   
    for( ; i < (ME_OrderBook.OrderBook[Token].BuyRecords) ; i++)        
    {
        for(int j=0; j< (ME_OrderBook.OrderBook[Token].BuyRecords ); j++)
        {    
            if(ME_OrderBook.OrderBook[Token].Buy[j].lPrice < ME_OrderBook.OrderBook[Token].Buy[j+1].lPrice )
            {
               
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[Token].Buy[j+1],sizeof(swpbookdtls));

                
                memcpy(&ME_OrderBook.OrderBook[Token].Buy[j+1],&ME_OrderBook.OrderBook[Token].Buy[j], sizeof(ME_OrderBook.OrderBook[Token].Buy[j]));

               
          
                memcpy(&ME_OrderBook.OrderBook[Token].Buy[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[Token].Buy[j]));
              
            } 

            if((ME_OrderBook.OrderBook[Token].Buy[j].lPrice == ME_OrderBook.OrderBook[Token].Buy[j+1].lPrice )&& (ME_OrderBook.OrderBook[Token].Buy[j].SeqNo > ME_OrderBook.OrderBook[Token].Buy[j+1].SeqNo ))
            {
               
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[Token].Buy[j+1],sizeof(swpbookdtls));
                memcpy(&ME_OrderBook.OrderBook[Token].Buy[j+1],&ME_OrderBook.OrderBook[Token].Buy[j], sizeof(ME_OrderBook.OrderBook[Token].Buy[j]));
                memcpy(&ME_OrderBook.OrderBook[Token].Buy[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[Token].Buy[j]));
            } 

        }    
    }  
    PrintBook(Token);
 
}

int PrintBook(long Token)
{       
    return 0;
    int BookDepth;
    if(ME_OrderBook.OrderBook[Token].BuyRecords > ME_OrderBook.OrderBook[Token].SellRecords)
    {
        BookDepth = ME_OrderBook.OrderBook[Token].BuyRecords;
    }   
    else
    {
        BookDepth = ME_OrderBook.OrderBook[Token].SellRecords;
    }    
            
    for(int j = 0 ; j < BookDepth ; j++)                
    {
            std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --" << "-- DQty --"   << "Buy Side Book" << std::endl;   
            std::cout << ME_OrderBook.OrderBook[Token].Buy[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[Token].Buy[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[Token].Buy[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[Token].Buy[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[Token].Buy[j].DQRemaining << "---- " 
                    << std::endl;  

            std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --"  << "-- DQty --"  << "Sell Side Book" << std::endl;           
            std::cout << ME_OrderBook.OrderBook[Token].Sell[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[Token].Sell[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[Token].Sell[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[Token].Sell[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[Token].Sell[j].DQRemaining << "---- " 
                    << std::endl;            
        
    } 
}

int SortSellSideBook(long Token)
{
    long swpPrice;
    long swpQty;
    long SwpSeq;
    long SwpOrdNo;
    long swpDQ;
    short swpIsDQ;    
    short swpIsIOC;
    long swpOpenQty;
    long swpTTQ;
    int swpDQRemaining;    
    
    ORDER_BOOK_DTLS swpbookdtls;
    int i=0;
    
    
   
    for( ; i < (ME_OrderBook.OrderBook[Token].SellRecords) ; i++)        
    {
        for(int j=0; j< (ME_OrderBook.OrderBook[Token].SellRecords); j++)
        {    
            if(ME_OrderBook.OrderBook[Token].Sell[j].lPrice > ME_OrderBook.OrderBook[Token].Sell[j+1].lPrice )
            {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[Token].Sell[j+1],sizeof(swpbookdtls));
               

                memcpy(&ME_OrderBook.OrderBook[Token].Sell[j+1],&ME_OrderBook.OrderBook[Token].Sell[j], sizeof(ME_OrderBook.OrderBook[Token].Sell[j]));
                 

                memcpy(&ME_OrderBook.OrderBook[Token].Sell[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[Token].Sell[j]));
            
            } 
            if((ME_OrderBook.OrderBook[Token].Sell[j].lPrice == ME_OrderBook.OrderBook[Token].Sell[j+1].lPrice) && (ME_OrderBook.OrderBook[Token].Sell[j].SeqNo > ME_OrderBook.OrderBook[Token].Sell[j].SeqNo))
            {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[Token].Sell[j+1],sizeof(swpbookdtls));

                memcpy(&ME_OrderBook.OrderBook[Token].Sell[j+1],&ME_OrderBook.OrderBook[Token].Sell[j], sizeof(ME_OrderBook.OrderBook[Token].Sell[j]));

                memcpy(&ME_OrderBook.OrderBook[Token].Sell[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[Token].Sell[j]));
            } 

    
        }    
    }  
    PrintBook(Token);

    
}

//int SortSellSideBook_SL(long Token)
//{
//    long swpPrice;
//    long swpQty;
//    long SwpSeq;
//    long SwpOrdNo;
//    
//    int i=0;
//    
//    
//   
//    for( ; i < (ME_SL_OrderBook.OrderBook[Token].SellRecords) ; i++)        
//    {
//        for(int j=0; j< (ME_SL_OrderBook.OrderBook[Token].SellRecords); j++)
//        {    
//            if(ME_SL_OrderBook.OrderBook[Token].Sell[j].lPrice > ME_SL_OrderBook.OrderBook[Token].Sell[j+1].lPrice )
//            {
//                swpPrice = ME_SL_OrderBook.OrderBook[Token].Sell[j+1].lPrice;
//                swpQty = ME_SL_OrderBook.OrderBook[Token].Sell[j+1].lQty;
//                SwpSeq = ME_SL_OrderBook.OrderBook[Token].Sell[j+1].SeqNo;        
//                SwpOrdNo = ME_SL_OrderBook.OrderBook[Token].Sell[j+1].OrderNo;
//
//                ME_SL_OrderBook.OrderBook[Token].Sell[j+1].lPrice = ME_SL_OrderBook.OrderBook[Token].Sell[j].lPrice;
//                ME_SL_OrderBook.OrderBook[Token].Sell[j+1].lQty = ME_SL_OrderBook.OrderBook[Token].Sell[j].lQty;
//                ME_SL_OrderBook.OrderBook[Token].Sell[j+1].SeqNo = ME_SL_OrderBook.OrderBook[Token].Sell[j].SeqNo;        
//                ME_SL_OrderBook.OrderBook[Token].Sell[j+1].OrderNo = ME_SL_OrderBook.OrderBook[Token].Sell[j].OrderNo;   
//
//                ME_SL_OrderBook.OrderBook[Token].Sell[j].lPrice = swpPrice;
//                ME_SL_OrderBook.OrderBook[Token].Sell[j].lQty = swpQty;
//                ME_SL_OrderBook.OrderBook[Token].Sell[j].SeqNo = SwpSeq;        
//                ME_SL_OrderBook.OrderBook[Token].Sell[j].OrderNo = SwpOrdNo;               
//            } 
//        }    
//    }  
//    PrintBook(Token);
//
//    
//}


int ValidateModReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, FD);
    if (errCode != 0)
    {
      return errCode;
    }
    
    if (_nSegMode ==  SEG_NSECM)
    {
       char Symbol[10 +1 ] = {0};
       strncpy(Symbol, symbol, 10);
       Trim(Symbol);
    
       char lcSeries[2 + 1] = {0};
       strncpy(lcSeries, series, 2);
 
        std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
        TokenItr itSymbol = pNSECMContract->find(lszSymbol);
        
        if (itSymbol == pNSECMContract->end())
        {
            errCode = ERR_INVALID_SYMBOL;
            return errCode;
        }
        else
        {
            Token = itSymbol->second;
        }
    }
    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, NULL);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), NULL);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[Token].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[Token].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
             }   
         }
     }
    
     if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}



int ModOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO* pconnInfo )
{
    ORDER_BOOK_DTLS bookdetails;
    int MyTime = GlobalSeqNo++;   
    NSECM::MS_OE_RESPONSE_TR ModOrdResp;
    int i = 0;
    long Token = 0;
    
   /* char Symbol[10 + 1] = {0};
    strncpy(Symbol, ModOrder->sec_info.Symbol, 10);
    Trim(Symbol);
    char lcSeries[2 + 1] = {0};
    strncpy(lcSeries, ModOrder->sec_info.Series, 2);
    std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
    TokenItr itSymbol = pNSECMContract->find(lszSymbol);*/
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo); 
    ModOrdResp.ErrorCode = ValidateModReq(__bswap_32(ModOrder->TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token, ModOrder->sec_info.Symbol, ModOrder->sec_info.Series);
    
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    ModOrdResp.TransactionCode = __bswap_16(20074);
    ModOrdResp.LogTime = __bswap_16(1);
    ModOrdResp.TraderId = ModOrder->TraderId;
    //ModOrdResp.ErrorCode = 0;
    ModOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    ModOrdResp.TimeStamp1 = __bswap_64(ModOrdResp.TimeStamp1); /*Sneha*/
    ModOrdResp.TimeStamp2 = '1'; /*sneha*/
    ModOrdResp.BookType = ModOrder->BookType;
    memcpy(&ModOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(ModOrdResp.AccountNumber));
    memcpy(&ModOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(ModOrdResp.BuySellIndicator));
    ModOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    ModOrdResp.DisclosedVolumeRemain = ModOrder->DisclosedVolume;
    ModOrdResp.TotalVolumeRemain = ModOrder->Volume;
    ModOrdResp.Volume = ModOrder->Volume;
    ModOrdResp.VolumeFilledToday = 0;
    ModOrdResp.Price = ModOrder->Price;
    ModOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    ModOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&ModOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(ModOrdResp.OrderFlags));
    ModOrdResp.BranchId = ModOrder->BranchId;
    ModOrdResp.UserId = ModOrder->UserId;        
    memcpy(&ModOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(ModOrdResp.BrokerId)) ;      
    ModOrdResp.Suspended = ModOrder->Suspended;       
    memcpy(&ModOrdResp.Settlor,&ModOrder->Settlor,sizeof(ModOrdResp.Settlor));
    ModOrdResp.ProClient = ModOrder->ProClientIndicator;
    ModOrdResp.SettlementPeriod =  __bswap_16(1);       
    memcpy(&ModOrdResp.sec_info,&ModOrder->sec_info,sizeof(ModOrdResp.sec_info));
    ModOrdResp.NnfField = ModOrder->NnfField;       
    ModOrdResp.TransactionId = ModOrder->TransactionId;
    ModOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    //std::cout << "ModOrdResp.TransactionId  " << ModOrdResp.TransactionId << std::endl;
    
    ModOrdResp.tap_hdr.swapBytes(); 
    
    if (ModOrdResp.ErrorCode != 0)
    {
         snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|ErrorCode %d|Symbol %s|Series %s|Token %ld",FD,int(dOrderNo),ModOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token);
         Logger::getLogger().log(DEBUG, logBuf);
         //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
         ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
         i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pconnInfo);
         
         memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 1; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
         ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
         
         if (ModOrdResp.ErrorCode == ERR_INVALID_USER_ID)
         {
             pconnInfo->status = DISCONNECTED;
         }
         return 0;
    }
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    int OrderNumber = ModOrdResp.OrderNumber;
    
    bookdetails.OrderNo =  ModOrdResp.OrderNumber;
    //SwapDouble((char*) &bookdetails.OrderNo);
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.IsDQ = IsDQ;
    /*Sneha - E*/
        
    
    // Mod to order Store  ------------------------------------------------------------------------------------------------- Start
    Order_Store_NSECM[OrderNumber].TraderId = ModOrdResp.TraderId;
    Order_Store_NSECM[OrderNumber].BookType = ModOrdResp.BookType;
    memcpy(&Order_Store_NSECM[OrderNumber].AccountNumber,&ModOrdResp.AccountNumber,sizeof(Order_Store_NSEFO[OrderNumber].AccountNumber));
    memcpy(&Order_Store_NSECM[OrderNumber].BuySellIndicator,&ModOrdResp.BuySellIndicator,sizeof(Order_Store_NSEFO[OrderNumber].BuySellIndicator));
    Order_Store_NSECM[OrderNumber].DisclosedVolume = ModOrdResp.DisclosedVolume;
    Order_Store_NSECM[OrderNumber].Volume = ModOrdResp.Volume;
    Order_Store_NSECM[OrderNumber].Price = ModOrdResp.Price;
    memcpy(&Order_Store_NSECM[OrderNumber].OrderFlags,& ModOrdResp.OrderFlags, sizeof(Order_Store_NSEFO[OrderNumber].OrderFlags));
    Order_Store_NSECM[OrderNumber].BranchId = ModOrdResp.BranchId;
    Order_Store_NSECM[OrderNumber].UserId = ModOrdResp.UserId;        
    memcpy(&Order_Store_NSECM[OrderNumber].BrokerId,&ModOrdResp.BrokerId,sizeof(Order_Store_NSEFO[OrderNumber].BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&Order_Store_NSECM[OrderNumber].Settlor,&ModOrdResp.Settlor,sizeof(Order_Store_NSEFO[OrderNumber].Settlor));
    Order_Store_NSECM[OrderNumber].ProClientIndicator = ModOrdResp.ProClient;
    Order_Store_NSECM[OrderNumber].NnfField = ModOrdResp.NnfField;   
    Order_Store_NSECM[OrderNumber].OrderNumber = ModOrdResp.OrderNumber;     
    
    // Mod to order Store  ------------------------------------------------------------------------------------------------- END    
   snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|IOC %d|DQ %d|DQty %d",FD, OrderNumber,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty); 
   Logger::getLogger().log(DEBUG, logBuf);
   //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
   long datareturn = Modtoorderbook(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),Token /* Please replace token number here*/,IsIOC,IsDQ,IsSL ,__bswap_constant_32(ModOrdResp.LastModified));    
    //SwapDouble((char*) &ModOrdResp.OrderNumber);
     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pconnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    

    //std::cout << "MS_OM_REQUEST_TR Mod :: Order Number "  <<  ModOrdResp.OrderNumber <<  "  Bytes Sent " << i << std::endl;   
   
    datareturn = Matching(Token /*1 Please replace token number here*/,FD,IsIOC,IsDQ,pconnInfo);
}

int ModOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD,int IsIOC,int IsDQ,int IsSL,CONNINFO* pConnInfo )
{
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR ModOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));
    
    int MyTime = GlobalSeqNo++; 
    
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    ModOrdResp.ErrorCode = ValidateModReq(__bswap_32(ModOrder->TraderId), FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token,NULL,NULL);
    
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.TransactionCode = __bswap_16(20074);
    ModOrdResp.LogTime = __bswap_16(1);
    ModOrdResp.TraderId = ModOrder->TraderId;
    ModOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    //ModOrdResp.ErrorCode = 0;       
    ModOrdResp.Timestamp1 =  getCurrentTimeInNano();
    ModOrdResp.Timestamp1 = __bswap_64(ModOrdResp.Timestamp1); /*sneha*/
    ModOrdResp.Timestamp2 = '1'; /*sneha*/
    ModOrdResp.BookType = ModOrder->BookType;
    memcpy(&ModOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(ModOrdResp.AccountNumber));
    memcpy(&ModOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(ModOrdResp.BuySellIndicator));
    ModOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    ModOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    ModOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    ModOrdResp.Volume = ModOrder->Volume;
    ModOrdResp.VolumeFilledToday = 0;
    ModOrdResp.Price = ModOrder->Price;
    ModOrdResp.EntryDateTime =ModOrder->EntryDateTime;
    ModOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&ModOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(ModOrdResp.OrderFlags));
    ModOrdResp.BranchId = ModOrder->BranchId;
    ModOrdResp.UserId = ModOrder->UserId;        
    memcpy(&ModOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(ModOrdResp.BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&ModOrdResp.Settlor,&ModOrder->Settlor,sizeof(ModOrdResp.Settlor));
    ModOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    
    //OrderResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    ModOrdResp.TokenNo = ModOrder->TokenNo;
    ModOrdResp.NnfField = ModOrder->NnfField;       
    ModOrdResp.filler = ModOrder->filler;
    ModOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    ModOrdResp.tap_hdr.swapBytes(); 
    
    if (ModOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|ErrorCode %d|Token %ld", FD, int(dOrderNo),ModOrdResp.ErrorCode,(Token + FOOFFSET));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
        ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
        SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
        
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 1; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
        ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
        if (ModOrdResp.ErrorCode == ERR_INVALID_USER_ID)
        {
            pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    int OrderNumber = ModOrdResp.OrderNumber;
    
     // Mod to order Store  ------------------------------------------------------------------------------------------------- Start
    Order_Store_NSEFO[OrderNumber].TraderId = ModOrdResp.TraderId;
    Order_Store_NSEFO[OrderNumber].BookType = ModOrdResp.BookType;
    memcpy(&Order_Store_NSEFO[OrderNumber].AccountNumber,&ModOrdResp.AccountNumber,sizeof(Order_Store_NSEFO[OrderNumber].AccountNumber));
    memcpy(&Order_Store_NSEFO[OrderNumber].BuySellIndicator,&ModOrdResp.BuySellIndicator,sizeof(Order_Store_NSEFO[OrderNumber].BuySellIndicator));
    Order_Store_NSEFO[OrderNumber].DisclosedVolume = ModOrdResp.DisclosedVolume;
    Order_Store_NSEFO[OrderNumber].Volume = ModOrdResp.Volume;
    Order_Store_NSEFO[OrderNumber].Price = ModOrdResp.Price;
    memcpy(&Order_Store_NSEFO[OrderNumber].OrderFlags,& ModOrdResp.OrderFlags, sizeof(Order_Store_NSEFO[OrderNumber].OrderFlags));
    Order_Store_NSEFO[OrderNumber].BranchId = ModOrdResp.BranchId;
    Order_Store_NSEFO[OrderNumber].UserId = ModOrdResp.UserId;        
    memcpy(&Order_Store_NSEFO[OrderNumber].BrokerId,&ModOrdResp.BrokerId,sizeof(Order_Store_NSEFO[OrderNumber].BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&Order_Store_NSEFO[OrderNumber].Settlor,&ModOrdResp.Settlor,sizeof(Order_Store_NSEFO[OrderNumber].Settlor));
    Order_Store_NSEFO[OrderNumber].ProClientIndicator = ModOrdResp.ProClientIndicator;
    Order_Store_NSEFO[OrderNumber].NnfField = ModOrdResp.NnfField;   
    Order_Store_NSEFO[OrderNumber].OrderNumber = ModOrdResp.OrderNumber;    
    
    // Mod to order Store  ------------------------------------------------------------------------------------------------- END    
     //SwapDouble((char*) &ModOrdResp.OrderNumber);
    memcpy(&bookdetails.OrderNo,&ModOrdResp.OrderNumber,sizeof(bookdetails.OrderNo));
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    //SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.IsDQ = IsDQ;
    /*Sneha - E*/
    //std::cout << " bookdetails.lQty  " << bookdetails.lQty <<  "   " <<  __bswap_32(AddOrder->Volume) <<  std::endl;
    
    snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|IOC %d|DQ %d|DQty %d",FD,OrderNumber,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty);
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl; 
    long datareturn = Modtoorderbook(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET) /* Please replace token number here*/,IsIOC,IsDQ,IsSL,__bswap_constant_32(ModOrdResp.LastModified));    
    //SwapDouble((char*) &ModOrdResp.OrderNumber);
    int i = 0;
   
     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    //std::cout << "MS_OM_REQUEST_TR Mod :: Order Number "  <<  ModOrdResp.OrderNumber <<  "  Bytes Sent " << i << std::endl;   
    //TokenItr itSymbol = pNSECMContract->find(std::string(std::string(ModOrder->sec_info.Symbol) + "|" + ModOrder->sec_info.Series));
    datareturn = Matching((__bswap_32(ModOrder->TokenNo) - FOOFFSET)/*1 Please replace token number here*/,FD,IsIOC,IsDQ,pConnInfo);
}

int ValidateCanReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series)
{
    int errCode = 0;
    bool orderFound = false;
    
    errCode = ValidateUser(iUserID, FD);
    if (errCode != 0)
    {
      return errCode;
    }
    
    if (_nSegMode ==  SEG_NSECM)
    {
       char Symbol[10 +1 ] = {0};
       strncpy(Symbol, symbol, 10);
       Trim(Symbol);
    
       char lcSeries[2 + 1] = {0};
       strncpy(lcSeries, series, 2);
 
        std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
        TokenItr itSymbol = pNSECMContract->find(lszSymbol);
        
        if (itSymbol == pNSECMContract->end())
        {
            errCode = ERR_INVALID_SYMBOL;
            return errCode;
        }
        else
        {
            Token = itSymbol->second;
        }
    }
    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, NULL);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), NULL);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[Token].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[Token].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
             }   
         }
     }
    
    if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}

int CanOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder,int FD, CONNINFO* pConnInfo)
{
    ORDER_BOOK_DTLS bookdetails;
    NSECM::MS_OE_RESPONSE_TR CanOrdResp;
    long Token = 0;
    /*char Symbol[10 + 1] = {0};
    strncpy(Symbol, ModOrder->sec_info.Symbol, 10);
    Trim(Symbol);
    char lcSeries[2 + 1] = {0};
    strncpy(lcSeries, ModOrder->sec_info.Series, 2);
    std::string lszSymbol = std::string(Symbol) + "|" + lcSeries;
    TokenItr itSymbol = pNSECMContract->find(lszSymbol);*/
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    CanOrdResp.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series);
         
     memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = ModOrder->TraderId;
    //CanOrdResp.ErrorCode = 0;
    CanOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.TimeStamp1 = __bswap_64(CanOrdResp.TimeStamp1); /*sneha*/
    CanOrdResp.TimeStamp2 = '1'; /*sneha*/
    CanOrdResp.BookType = ModOrder->BookType;
    memcpy(&CanOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemain = ModOrder->DisclosedVolume;
    CanOrdResp.TotalVolumeRemain = ModOrder->Volume;
    CanOrdResp.Volume = ModOrder->Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = ModOrder->Price;
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    CanOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&CanOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.UserId = ModOrder->UserId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    CanOrdResp.Suspended = ModOrder->Suspended;       
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClient = ModOrder->ProClientIndicator;
    CanOrdResp.SettlementPeriod =  __bswap_16(1);       
    memcpy(&CanOrdResp.sec_info,&ModOrder->sec_info,sizeof(CanOrdResp.sec_info));
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.TransactionId = ModOrder->TransactionId;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
   
    CanOrdResp.tap_hdr.swapBytes(); 
    
    if (CanOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|ErrorCode %d|Symbol %s|Series %s|Token %ld",FD,int(dOrderNo),CanOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<int(dOrderNo)<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
        SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR),pConnInfo);
         
         memset (&LogData, 0, sizeof(LogData));
         LogData.MyFd = 1; /*1 = Order response*/
         memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
         Inqptr_MeToLog_Global->enqueue(LogData);
    
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
        if (CanOrdResp.ErrorCode == ERR_INVALID_USER_ID)
        {
            pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
        
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.OrderNo = CanOrdResp.OrderNumber;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    //std::cout << " bookdetails.lQty  " << bookdetails.lQty <<  "   " <<  __bswap_32(AddOrder->Volume) <<  std::endl;
    
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld",FD, int (bookdetails.OrderNo));
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<bookdetails.OrderNo<<std::endl;
    long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),Token /*1 Please replace token number here*/, __bswap_constant_32(CanOrdResp.LastModified));    
    //SwapDouble((char*) &CanOrdResp.OrderNumber);    
    int i = 0;
    
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    //std::cout << "MS_OM_REQUEST_TR Can :: Order Number "  <<  CanOrdResp.OrderNumber <<  "  Bytes Sent " << i << std::endl; 
}

int CanOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder,int FD, CONNINFO* pConnInfo)
{
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR CanOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));

    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    CanOrdResp.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->TraderId), FD,dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,NULL,NULL);
        
    memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = ModOrder->TraderId;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    //CanOrdResp.ErrorCode = 0;       
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1); /*sneha*/
    CanOrdResp.Timestamp2 = '1';
    CanOrdResp.BookType = ModOrder->BookType;
    memcpy(&CanOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    CanOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    CanOrdResp.Volume = ModOrder->Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = ModOrder->Price;
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    CanOrdResp.LastModified = __bswap_32(getEpochTime());
    memcpy(&CanOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.UserId = ModOrder->UserId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    
    //OrderResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    CanOrdResp.TokenNo = ModOrder->TokenNo;
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.filler = ModOrder->filler;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    CanOrdResp.tap_hdr.swapBytes(); 
    
    if (CanOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|ErrorCode %d|Token %ld",FD,int(dOrderNo),CanOrdResp.ErrorCode,(Token + FOOFFSET));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
         SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    
         memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 1; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
         CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
         if (CanOrdResp.ErrorCode == ERR_INVALID_USER_ID)
         {
             pConnInfo->status = DISCONNECTED;
         }
        return 0;
    }
    
    bookdetails.OrderNo =  CanOrdResp.OrderNumber;
    SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    //std::cout << " bookdetails.lQty  " << bookdetails.lQty <<  "   " <<  __bswap_32(AddOrder->Volume) <<  std::endl;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld",FD, int (bookdetails.OrderNo));
    Logger::getLogger().log(DEBUG, logBuf);
    //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<bookdetails.OrderNo<<std::endl;
    long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET)/*1 Please replace token number here*/,__bswap_constant_32(CanOrdResp.LastModified) );    
   // SwapDouble((char*) &CanOrdResp.OrderNumber);    
    int i =0;
   
     i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    //std::cout << "MS_OM_REQUEST_TR Can :: Order Number "  <<  CanOrdResp.OrderNumber <<  "  Bytes Sent " << i << std::endl; 
}

/*Validates order data received. Output will decide if order is to be accepted or rejected*/
int ValidateMLAddReq(NSEFO::MS_SPD_OE_REQUEST* Req, int Fd, int noOfLegs)
{
    int errCode = 0, i = 0;
    bool orderFound = false;

    /*check if user has signed ON*/
    errCode = ValidateUser(__bswap_32(Req->TraderId1), Fd);
    if (errCode != 0)
    {
       return errCode;
    }
   
    /*Reject if underlying asset is different for any 2 legs*/
    if (memcmp(Req->SecurityInformation1.Symbol, Req->leg2.SecurityInformation2.Symbol, 10) != 0)
    {
        errCode = e$invalid_contract_comb;
        return errCode;
    }
    if (noOfLegs == 3)   
    {
        if (memcmp(Req->leg2.SecurityInformation2.Symbol, Req->leg3.SecurityInformation3.Symbol, 10) != 0)
        {
           errCode = e$invalid_contract_comb;
           return errCode;
        }
    }
    
    /*Reject if any two contracts are same*/
    for (i = 0; i < (noOfLegs-1); i++) 
    {
         for (int j = i+1; j< noOfLegs; j++)
         {
                if (MLOrderInfo[i].token == MLOrderInfo[j].token)
                {
                     errCode = e$invalid_contract_comb;
                     return errCode;
                }
         }
     }
        
   /*Check if Token is subscribed. If yes the fill lot size in MLOrderIno else reject order*/
    bool found = true;
    int loc = 0;
    for (i = 0; i < noOfLegs; i++) 
    {
        found = binarySearch(TokenStore, TokenCount, MLOrderInfo[i].token, &loc);
        if (found == false)
        {
           errCode = ERR_SECURITY_NOT_AVAILABLE;
           return errCode;
        }
        else
        {
           MLOrderInfo[i].lotSize = contractInfoGlobal[loc].BoardLotQuantity;
        }
    }
       
    return errCode;
}

/*checks if each leg of the ML is executable based on its price*/
bool validateOrderPriceExecutable(int noOfLegs)
{
   for (int i = 0; i < noOfLegs;i++)
   {
     if(MLOrderInfo[i].buySell == 1)
    {
           snprintf(logBuf, 500,"Buy Leg|Leg price %d|Top sell price %d|Token %d", MLOrderInfo[i].orderPrice, ME_OrderBook.OrderBook[MLOrderInfo[i].token - FOOFFSET].Sell[0].lPrice, MLOrderInfo[i].token);
           Logger::getLogger().log(DEBUG, logBuf);
           if(MLOrderInfo[i].orderPrice < ME_OrderBook.OrderBook[MLOrderInfo[i].token - FOOFFSET].Sell[0].lPrice)
           {
               return false;
           }    
     } 
    else
    {
           snprintf(logBuf, 500,"Sell Leg|Leg price %d|Top Buy price %d", MLOrderInfo[i].orderPrice, ME_OrderBook.OrderBook[MLOrderInfo[i].token - FOOFFSET].Buy[0].lPrice);
           Logger::getLogger().log(DEBUG, logBuf);
          if(MLOrderInfo[i].orderPrice > ME_OrderBook.OrderBook[MLOrderInfo[i].token - FOOFFSET].Buy[0].lPrice)
           {
               return false;
           }           
     }    
  }
  return true; 
}

/*checks if order is executable in terms of its legs order qty ratio.*/
/*If yes then  fill qty to be excuted in execQty of MLOrderInfo else cancel order*/
bool validateOrderQtyExecutable(int noOfLegs)
{
     int i = 0, j = 0, gcd = 0;
     int32_t Token = 0;
     /*Find GCD of order legs qty*/
     if (noOfLegs == 2)
     {
          gcd = _gcd(MLOrderInfo[0].orderQty, MLOrderInfo[1].orderQty);
     }
     else
     {
          gcd = _gcd((_gcd(MLOrderInfo[0].orderQty, MLOrderInfo[1].orderQty)), MLOrderInfo[2].orderQty);
     }
     
     for (i=0; i< noOfLegs; i++)
     {
        Token = MLOrderInfo[i].token - FOOFFSET;
       /*Find available Qty on opposite side - S*/
       if (MLOrderInfo[i].buySell == 1)
       {
           for (j=0; j< ME_OrderBook.OrderBook[Token].SellRecords; j++)        
          {
              if ((MLOrderInfo[i].avlblOppQty < MLOrderInfo[i].orderQty) &&
                   !(MLOrderInfo[i].orderPrice < ME_OrderBook.OrderBook[Token].Sell[j].lPrice))
             {
                 MLOrderInfo[i].avlblOppQty = MLOrderInfo[i].avlblOppQty + ME_OrderBook.OrderBook[Token].Sell[j].lQty;
              }
              else
              {
                break;
              }
          }
       }
       else
       {
           for (j=0; j< ME_OrderBook.OrderBook[Token].BuyRecords; j++)        
          {
              if ((MLOrderInfo[i].avlblOppQty < MLOrderInfo[i].orderQty) &&
                   !(MLOrderInfo[i].orderPrice > ME_OrderBook.OrderBook[Token].Buy[j].lPrice))
             {
                 MLOrderInfo[i].avlblOppQty = MLOrderInfo[i].avlblOppQty + ME_OrderBook.OrderBook[Token].Buy[j].lQty;
              }
              else
              {
                break;
              }
          }
       }
       /*Find available Qty on opposite side - E*/
       
       /*Find ratio*/
       MLOrderInfo[i].ratio = MLOrderInfo[i].orderQty/gcd;
     }
    
     /*temp code*/
     for (i=0; i< noOfLegs; i++)
     {
       snprintf(logBuf, 500, "AvlOppQty %d| ratio %d| leg %d", MLOrderInfo[i].avlblOppQty, MLOrderInfo[i].ratio, i+1);
       Logger::getLogger().log(DEBUG, logBuf);    
     }
     
     
     /*Check if order can be executed in ratio*/    
     int multiple = 0, qty = 0;
     bool qtygrt = false, qtyeql = false, exit = false;
     while (exit == false)           
     {                           
       multiple++;
                                 
       for (i=0; i< noOfLegs; i++)
       {                         
         qty = MLOrderInfo[i].lotSize * multiple * MLOrderInfo[i].ratio;
         snprintf(logBuf, 500, "OrderQty %d| qty %d| multiple %d|Leg %d", MLOrderInfo[i].orderQty, qty, multiple, i+1);
         Logger::getLogger().log(DEBUG, logBuf);
         if (qty > MLOrderInfo[i].orderQty || qty > MLOrderInfo[i].avlblOppQty)
         {  
             exit = true;
             qtygrt = true; 
             Logger::getLogger().log(DEBUG, "exit & qtygrt true");
         }
         if (qty == MLOrderInfo[i].orderQty || qty == MLOrderInfo[i].avlblOppQty)
         {  
             exit = true;
             qtyeql = true;
             Logger::getLogger().log(DEBUG, "exit & qtyeql true");
         }
       }
     }
      
     snprintf(logBuf, 500, "qtygrt %d| multiple %d", qtygrt, multiple);
      Logger::getLogger().log(DEBUG, logBuf);
      if (qtygrt == true && multiple == 1)
     {
        //Send cancellation. This is match not found case
        return false;
     }
     else
     {
       if (qtygrt == true)
       {
          multiple--;
       } 
       for (i=0; i< noOfLegs; i++)
       {
         MLOrderInfo[i].execQty = MLOrderInfo[i].lotSize * multiple * MLOrderInfo[i].ratio; 
         MLOrderInfo[i].multiplier = multiple; 
       }
     }
  
 return true; 
}

/*Send ML order cancellation if qty or price validation fails. This IOC order cancellation*/
int SendMLOrderCancellation(NSEFO::MS_SPD_OE_REQUEST*  MLOrderResp, int FD, int noOfLegs, CONNINFO* pConnInfo)
{
        double OrderNo = MLOrderResp->OrderNUmber1;
        SwapDouble((char*) &OrderNo);
        int SeqNo = GlobalSeqNo++;
        MLOrderResp->tap_hdr.iSeqNo = __bswap_32(SeqNo);
        //MLOrderResp.OrderNUmber1 = __bswap_64(OrderNumber);
        MLOrderResp->EntryDateTime1= __bswap_32(getEpochTime());
        MLOrderResp->LastModified1 = MLOrderResp->EntryDateTime1;
        MLOrderResp->MktReplay = MLOrderResp->EntryDateTime1;
        MLOrderResp->msg_hdr.TimeStamp1 = __bswap_64(getCurrentTimeInNano());
        MLOrderResp->msg_hdr.LogTime = __bswap_32(1);
        MLOrderResp->msg_hdr.ErrorCode = __bswap_16(e$fok_order_cancelled);
        if (noOfLegs == 2)
        { 
            MLOrderResp->msg_hdr.TransactionCode = __bswap_16(NSEFO_2LEG_CAN_CNF);
        }
        else
        {
            MLOrderResp->msg_hdr.TransactionCode = __bswap_16(NSEFO_3LEG_CAN_CNF);
        }
        
        int  i = SendToClient(FD , (char *)MLOrderResp , sizeof(NSEFO::MS_SPD_OE_REQUEST), pConnInfo);
     
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML UnsolCAN ORDER|Order# %ld", FD, int(OrderNo));
        Logger::getLogger().log(DEBUG, logBuf);
         
        /*Send order cancel to MsgDnld thread*/
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 3; /*3 = ML Order*/
        memcpy (LogData.msgBuffer, (void*)MLOrderResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);  
}

/*Send cancellation for single leg of ML order after partial fill*/
int SendMLOrderLegCancellation(NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int i, int FD,CONNINFO* pConnInfo)
{
     NSEFO::MS_OE_RESPONSE_TR CanOrdResp;
     
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.ErrorCode = __bswap_16(e$fok_order_cancelled);       
    CanOrdResp.Timestamp1 =  __bswap_64(getCurrentTimeInNano());
    CanOrdResp.Timestamp2 = '1';
    CanOrdResp.TraderId = MLOrderResp->TraderId1;
    CanOrdResp.BookType = MLOrderResp->BookType1;
    memcpy(&CanOrdResp.AccountNumber,&(MLOrderResp->AccountNUmber1),sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.OrderFlags,& (MLOrderResp->OrderFlags1), sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = MLOrderResp->BranchId1;
    CanOrdResp.UserId = MLOrderResp->TraderId1;
    memcpy(&CanOrdResp.BrokerId,&(MLOrderResp->BrokerId1),sizeof(CanOrdResp.BrokerId)) ;      
    memcpy(&CanOrdResp.Settlor,&(MLOrderResp->Settlor1),sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = MLOrderResp->ProClient1;
     memcpy(&CanOrdResp.filler, &(MLOrderResp->filler1to16), sizeof(CanOrdResp.filler));
    CanOrdResp.NnfField = MLOrderResp->NnfField;
    CanOrdResp.OrderNumber = MLOrderResp->OrderNUmber1;
    CanOrdResp.DisclosedVolume = 0;
    CanOrdResp.DisclosedVolumeRemaining = 0;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.BuySellIndicator = __bswap_16(MLOrderInfo[i].buySell);
    CanOrdResp.TotalVolumeRemaining = __bswap_32(MLOrderInfo[i].orderRemainingQty);
    CanOrdResp.Volume = __bswap_32(MLOrderInfo[i].orderQty);
    CanOrdResp.Price = __bswap_32(MLOrderInfo[i].orderPrice);
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    CanOrdResp.LastModified = __bswap_32(getEpochTime());
    CanOrdResp.TokenNo = __bswap_32(MLOrderInfo[i].token);

    CanOrdResp.tap_hdr.swapBytes(); 
 
    int  byteSent = SendToClient(FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR), pConnInfo);

    /*Send Cancel to MsgDnld thread*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ML Leg UnsolCan|Order # %ld|Leg %d|Remaining Qty %d", 
                     FD, int (CanOrdResp.OrderNumber), i+1, MLOrderInfo[i].orderRemainingQty);
    Logger::getLogger().log(DEBUG, logBuf);
}

/*Accept or reject ML order by performing various validation. If order accepted send it for matching*/
int AddMLOrder(NSEFO::MS_SPD_OE_REQUEST* MLOrderReq,int FD, CONNINFO* pConnInfo)
{
    int noOfLegs = 0;
    NSEFO::MS_SPD_OE_REQUEST MLOrderResp;
    memcpy(&MLOrderResp, MLOrderReq, sizeof(NSEFO::MS_SPD_OE_REQUEST));
      
    /*MLOrderInfo is like order store*/
    memset(&MLOrderInfo[0], 0, sizeof (NSEFO::MultiLegOrderInfo));
    memset(&MLOrderInfo[1], 0, sizeof (NSEFO::MultiLegOrderInfo));
    memset(&MLOrderInfo[2], 0, sizeof (NSEFO::MultiLegOrderInfo));
      
    MLOrderInfo[0].token = __bswap_32(MLOrderReq->Token1);
    MLOrderInfo[0].orderPrice = __bswap_32(MLOrderReq->Price1);
    MLOrderInfo[0].orderQty = __bswap_32(MLOrderReq->Volume1);
    MLOrderInfo[0].orderRemainingQty = __bswap_32(MLOrderReq->Volume1);
    MLOrderInfo[0].buySell = __bswap_16(MLOrderReq->BuySell1);
     
    MLOrderInfo[1].token = __bswap_32(MLOrderReq->leg2.Token2);
    MLOrderInfo[1].orderPrice = __bswap_32(MLOrderReq->leg2.Price2);
    MLOrderInfo[1].orderQty = __bswap_32(MLOrderReq->leg2.Volume2);
    MLOrderInfo[1].orderRemainingQty = __bswap_32(MLOrderReq->leg2.Volume2);
    MLOrderInfo[1].buySell = __bswap_16(MLOrderReq->leg2.BuySell2);
        
    int32_t transCode = __bswap_16(MLOrderReq->msg_hdr.TransactionCode);
    if (transCode == NSEFO_2LEG_ADD_REQ)
    {
        noOfLegs = 2;
    }
    else
    {
         noOfLegs= 3;
         MLOrderInfo[2].token = __bswap_32(MLOrderReq->leg3.Token3);
         MLOrderInfo[2].orderPrice = __bswap_32(MLOrderReq->leg3.Price3);
         MLOrderInfo[2].orderQty = __bswap_32(MLOrderReq->leg3.Volume3);
         MLOrderInfo[2].orderRemainingQty = __bswap_32(MLOrderReq->leg3.Volume3);
         MLOrderInfo[2].buySell = __bswap_16(MLOrderReq->leg3.BuySell3);
    }
    
    /*Validate order data*/
    int16_t errCode = ValidateMLAddReq(MLOrderReq, FD, noOfLegs);
    
    int64_t OrderNumber =  ME_OrderNumber++;   
    int32_t SeqNo = GlobalSeqNo++;
    MLOrderResp.tap_hdr.iSeqNo = __bswap_32(SeqNo);
    MLOrderResp.OrderNUmber1 = OrderNumber;
    SwapDouble((char*) &MLOrderResp.OrderNUmber1);  
    MLOrderResp.EntryDateTime1= __bswap_32(getEpochTime());
    MLOrderResp.LastModified1 = MLOrderResp.EntryDateTime1;
    MLOrderResp.MktReplay = MLOrderResp.EntryDateTime1;
    MLOrderResp.msg_hdr.ErrorCode = __bswap_16(errCode);
    MLOrderResp.msg_hdr.TimeStamp1 = __bswap_64(getCurrentTimeInNano());
    MLOrderResp.msg_hdr.LogTime = __bswap_32(1);

    if (transCode == NSEFO_2LEG_ADD_REQ)
    {
        if (errCode == 0)
        {
           MLOrderResp.msg_hdr.TransactionCode = __bswap_16(NSEFO_2LEG_ADD_CNF);
        }
        else
        {
          MLOrderResp.msg_hdr.TransactionCode = __bswap_16(NSEFO_2LEG_ADD_REJ);
        }
        snprintf(logBuf, 500, "Thread_ME|FD %d|2L ADD ORDER|Order# %ld|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                         "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                         FD,OrderNumber, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
                         __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
    }
    else
    {
         if (errCode == 0)
         {
             MLOrderResp.msg_hdr.TransactionCode = __bswap_16(NSEFO_3LEG_ADD_CNF);
         }
         else
         {
             MLOrderResp.msg_hdr.TransactionCode = __bswap_16(NSEFO_3LEG_ADD_REJ);
         }
         snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD ORDER|Order# %ld|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                        "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                         FD,OrderNumber,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                         __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                        __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
    }

    int  i = SendToClient(FD , (char *)&MLOrderResp , sizeof(NSEFO::MS_SPD_OE_REQUEST),pConnInfo);
   
    /*Send order to MsgDnld thread*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 3; /*3 = ML Order*/
    memcpy (LogData.msgBuffer, (void*)&MLOrderResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
        
    if (errCode != 0)
    {
        if (errCode == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    /*Validate whether all the legs are executable based on their price*/
    bool retVal = validateOrderPriceExecutable(noOfLegs);
    if (retVal == false)
    {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML ADD ORDER|Order# %ld|Order price not executable|Sending cancellation", FD,OrderNumber);
        Logger::getLogger().log(DEBUG, logBuf);
        
        SendMLOrderCancellation(&MLOrderResp, FD, noOfLegs, pConnInfo);
        return 0;
    }
    
    /*Validate whether all the legs are executable based on their qty*/
    retVal = validateOrderQtyExecutable(noOfLegs);
    if (retVal == false)
    {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML ADD ORDER|Order# %ld|Order qty not executable|Sending cancellation", FD,OrderNumber);
        Logger::getLogger().log(DEBUG, logBuf);
        
        SendMLOrderCancellation(&MLOrderResp, FD, noOfLegs, pConnInfo);
        return 0;
    }
    
    /*Execute order*/
    MatchMLOrder(noOfLegs, &MLOrderResp, FD, pConnInfo);
}

/*Execute order based on the execQty calculated in MLOrderInfo. Cancel partially filled legs*/
int MatchMLOrder(int noOfLegs, NSEFO::MS_SPD_OE_REQUEST* MLOrderResp, int FD, CONNINFO* pConnInfo)
{
  int i = 0, tradeQty = 0, tradePrice = 0, TTQ = 0;
  int64_t MyTime;
  int32_t epochTime, Token = 0;
  long orderNo = 0;
  double  MLOrderNo = MLOrderResp->OrderNUmber1;
  SwapDouble((char*)&MLOrderNo);
  NSEFO::TRADE_CONFIRMATION_TR SendTradeConf;
  CONNINFO* OrderConnInfo = NULL;
  int OrderFD = 0, iUserID;
  for (i=0; i< noOfLegs; i++)
  {
     TTQ = 0;
     Token = MLOrderInfo[i].token - FOOFFSET;
     while (MLOrderInfo[i].execQty > 0)
     {
         MyTime = getCurrentTimeInNano();  
        epochTime = getEpochTime();
         if(MLOrderInfo[i].buySell == 1)
        {
           if(MLOrderInfo[i].orderPrice >= ME_OrderBook.OrderBook[Token].Sell[0].lPrice)
           {
              if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[Token].Sell[0].lQty)
              {
                tradeQty = MLOrderInfo[i].execQty;
              }
              else
              {
                tradeQty = ME_OrderBook.OrderBook[Token].Sell[0].lQty;
              }                                                                                                                                                                                                                                                                                                                                                            
              tradePrice = ME_OrderBook.OrderBook[Token].Sell[0].lPrice;
           } 
           else
           {
             break;
           }
           TTQ = TTQ + tradeQty;
           ME_OrderBook.OrderBook[Token].TradeNo++;
           ME_OrderBook.OrderBook[Token].Sell[0].TTQ =  ME_OrderBook.OrderBook[Token].Sell[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[Token].Sell[0].lQty = ME_OrderBook.OrderBook[Token].Sell[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
           
           /*OrderBook sell order trade*/
           memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,Order_Store_NSEFO[orderNo].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
           SendTradeConf.DisclosedVolume = Order_Store_NSEFO[orderNo].DisclosedVolume;
           SendTradeConf.Price = Order_Store_NSEFO[orderNo].Price;
           SendTradeConf.Token = Order_Store_NSEFO[orderNo].TokenNo;
           SendTradeConf.TraderId = Order_Store_NSEFO[orderNo].TraderId;
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(2);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp = MyTime;
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.ActivityTime  = __bswap_32(epochTime);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
          
          // int sellFD = ME_OrderBook.OrderBook[Token].Sell[0].FD;
           iUserID = __bswap_32(SendTradeConf.TraderId);
          bool bConnInfo = getConnInfo (iUserID, &OrderFD, &OrderConnInfo);
           
           if (bConnInfo == true)
           {
               SendToClient(OrderFD , (char *)&SendTradeConf , sizeof(SendTradeConf), OrderConnInfo);
           }
           
           snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Qty %d|Price %ld",OrderFD,orderNo,tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
           
          /*Fill OrderId in Broadcast Msg. OrderID for ML = 0*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0; 
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
          
           if(ME_OrderBook.OrderBook[Token].Sell[0].lQty == 0)
           {   
                ORDER_BOOK_DTLS BookDetails;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Sell[0],sizeof(BookDetails));
                Filltoorderbook(&BookDetails, 2, Token); // 1 Buy , 2 Sell
           }   
        }  
        else
        { 
          if(MLOrderInfo[i].orderPrice <= ME_OrderBook.OrderBook[Token].Buy[0].lPrice)
          {
                if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[Token].Buy[0].lQty)
               {
                  tradeQty = MLOrderInfo[i].execQty;
               }
               else
               {
                  tradeQty = ME_OrderBook.OrderBook[Token].Buy[0].lQty;
               }
               tradePrice =  ME_OrderBook.OrderBook[Token].Buy[0].lPrice;
           }     
          else
          {
            break;
          }
           TTQ = TTQ + tradeQty;
           ME_OrderBook.OrderBook[Token].TradeNo++;
           ME_OrderBook.OrderBook[Token].Buy[0].TTQ = ME_OrderBook.OrderBook[Token].Buy[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[Token].Buy[0].lQty = ME_OrderBook.OrderBook[Token].Buy[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
         
           /*Book buy order trade*/
          memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,Order_Store_NSEFO[orderNo].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
           SendTradeConf.DisclosedVolume = Order_Store_NSEFO[orderNo].DisclosedVolume;
           SendTradeConf.Price = Order_Store_NSEFO[orderNo].Price;
           SendTradeConf.Token = Order_Store_NSEFO[orderNo].TokenNo;
           SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
           SendTradeConf.TraderId = Order_Store_NSEFO[orderNo].TraderId;
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(1);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp = MyTime;
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.ActivityTime  = __bswap_32(epochTime);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
          
           iUserID = __bswap_32(SendTradeConf.TraderId);
           bool bConnInfo = getConnInfo (iUserID, &OrderFD, &OrderConnInfo);
           
           if (bConnInfo == true)
           {
               SendToClient(OrderFD , (char *)&SendTradeConf , sizeof(SendTradeConf), OrderConnInfo);
           }
           
          snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Qty %d|Price %ld",OrderFD,orderNo,tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
                 
          /*Fill OrderId in Broadcast Msg*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0 ;
          
            if(ME_OrderBook.OrderBook[Token].Buy[0].lQty == 0)
           {   
                ORDER_BOOK_DTLS BookDetails;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Buy[0],sizeof(BookDetails));
                Filltoorderbook(&BookDetails,1, Token); // 1 Buy , 2 Sell
           }   
         }      
       
          /*Leg trade*/
          NSEFO::TRADE_CONFIRMATION_TR SendLegTradeConf = {0}; // 20222
          memcpy(&SendLegTradeConf.AccountNumber,MLOrderResp->AccountNUmber1, sizeof(SendLegTradeConf.AccountNumber));
          SendLegTradeConf.Price = __bswap_32(MLOrderInfo[i].orderPrice);
          SendLegTradeConf.Token = __bswap_32(MLOrderInfo[i].token);
          SendLegTradeConf.BookType = MLOrderResp->BookType1;
          SendLegTradeConf.DisclosedVolume = 0;
          SendLegTradeConf.DisclosedVolumeRemaining = 0;
          SendLegTradeConf.VolumeFilledToday = __bswap_32(TTQ);
          SendLegTradeConf.BuySellIndicator = __bswap_16(MLOrderInfo[i].buySell);
          SendLegTradeConf.Timestamp = MyTime;
          SendLegTradeConf.Timestamp1 =  __bswap_64(MyTime);
          SendLegTradeConf.Timestamp2 = '1'; 
          SendLegTradeConf.ActivityTime  = __bswap_32(epochTime);
          SwapDouble((char*) &SendLegTradeConf.Timestamp);
          SendLegTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
          SendLegTradeConf.FillQuantity = __bswap_32(tradeQty);
          SendLegTradeConf.RemainingVolume = __bswap_32(MLOrderInfo[i].orderRemainingQty);
          SendLegTradeConf.FillPrice = __bswap_32(tradePrice);
          SendLegTradeConf.ResponseOrderNumber = MLOrderResp->OrderNUmber1;
          SendLegTradeConf.TraderId = MLOrderResp->TraderId1;
          SendLegTradeConf.TransactionCode = __bswap_16(20222);
          SendLegTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
           int resp = SendToClient( FD, (char *)&SendLegTradeConf , sizeof(SendLegTradeConf),pConnInfo);
           
           snprintf (logBuf, 500, "Thread_ME|FD %d|ML(%d) Trade|Leg %d|Order# %ld|Qty %d|Price %ld",
                              FD,MLOrderInfo[i].buySell, i+1, int(MLOrderNo),tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
           
          /*Send OrderBookTrade to MsgDnld Thread*/
           memset (&LogData, 0, sizeof(LogData));
          LogData.MyFd = 2; /*2 = Trade response*/
          memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
          Inqptr_MeToLog_Global->enqueue(LogData);
          
          /*Send MLTrade to MsgDnld Thread*/
           memset (&LogData, 0, sizeof(LogData));
          LogData.MyFd = 2; /*2 = Trade response*/
          memcpy (LogData.msgBuffer, (void*)&SendLegTradeConf, sizeof(LogData.msgBuffer));
          Inqptr_MeToLog_Global->enqueue(LogData);
         
          
           /*Send Trade for Broadcast*/
          FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
          FillData.stBcastMsg.stTrdMsg.nToken = __bswap_32(Order_Store_NSEFO[orderNo].TokenNo);
          FillData.stBcastMsg.stTrdMsg.nTradePrice = tradePrice;
          FillData.stBcastMsg.stTrdMsg.nTradeQty = tradeQty;
          FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
          Inqptr_METoBroadcast_Global->enqueue(FillData);
    }

    /*Cancel leg if partially filled*/   
    if (MLOrderInfo[i].orderRemainingQty > 0)
    {
        SendMLOrderLegCancellation(MLOrderResp, i, FD, pConnInfo);
    }
  }
}

long Modtoorderbook(ORDER_BOOK_DTLS *Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime) // 1 Buy , 2 Sell
{
    //std::cout << "Mod Called "  << std::endl;
    
    //ORDER_BOOK_DTLS swpbookdtls;
    if(BuySellSide == 1)
    {
        
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].BuyRecords) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
                 
                 memcpy(&ME_OrderBook.OrderBook[Token].Buy[j],Mybookdetails , sizeof(ME_OrderBook.OrderBook[Token].Buy[j]));
                 //ME_OrderBook.OrderBook[Token].Buy[j].lPrice = Mybookdetails->lPrice;
                 //ME_OrderBook.OrderBook[Token].Buy[j].lQty = Mybookdetails->lQty;
                 ME_OrderBook.OrderBook[Token].Buy[j].SeqNo = GlobalSeqNo++;
//                 //std::cout <<  " Mod Done " <<  ME_OrderBook.OrderBook[Token].Buy[j].lPrice 
//                         << std::endl;
             }   
         }
          
            
            SortBuySideBook(Token);
            
    }  
    else
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].SellRecords) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
                 
                 memcpy(&ME_OrderBook.OrderBook[Token].Sell[j],Mybookdetails , sizeof(ME_OrderBook.OrderBook[Token].Sell[j]));
//                 ME_OrderBook.OrderBook[Token].Sell[j].lPrice = Mybookdetails->lPrice;
//                 ME_OrderBook.OrderBook[Token].Sell[j].lQty = Mybookdetails->lQty;
                 ME_OrderBook.OrderBook[Token].Sell[j].SeqNo = GlobalSeqNo++;
//                 //std::cout <<  " Mod Done " <<  ME_OrderBook.OrderBook[Token].Sell[j].lPrice 
//                         << std::endl;
             }   
         }
          
            
            SortSellSideBook(Token);

 
            
    }    
   // Enqueue Broadcast Packet 
    AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
    AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'M';
    if(BuySellSide == 1)
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
    }
    else    
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;   
    if (_nSegMode == SEG_NSEFO){
       AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
    Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    // End Enqueue Broadcast Packet      
    return 0; 
}


long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token, int32_t epochTime) // 1 Buy , 2 Sell
{
    
    if(BuySellSide == 1)
    {
        //std::cout << "Can Called  Buy "  << std::endl;
        
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
                ME_OrderBook.OrderBook[Token].Buy[j].lPrice = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].DQty = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].IsDQ =0 ;
                ME_OrderBook.OrderBook[Token].Buy[j].IsIOC = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].OpenQty = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].OrderNo = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].SeqNo = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].TTQ = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].lQty = 0;
//                // std::cout <<  " Can Done "  
//                         << std::endl;
                SortBuySideBook(Token); 
                ME_OrderBook.OrderBook[Token].BuyRecords = ME_OrderBook.OrderBook[Token].BuyRecords - 1; 
             }   
         }
        
          
            
            
            //return 0;
    }  
    else
    {
        //std::cout << "Can Called Sell Order Number " <<  Mybookdetails->OrderNo << std::endl;
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
                ME_OrderBook.OrderBook[Token].Sell[j].lPrice = 999999999999;
                ME_OrderBook.OrderBook[Token].Sell[j].DQty = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].IsDQ =0;
                ME_OrderBook.OrderBook[Token].Sell[j].IsIOC = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].OpenQty = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].OrderNo = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].SeqNo = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].TTQ = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].lQty = 0;
                 
//                 //std::cout <<  " Can Done "  
//                         << std::endl;
                SortSellSideBook(Token); 
                ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords - 1; 
             }   
         }
        
            //return 0;
    }    
    // Enqueue Broadcast Packet 
    AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
    AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'X';
    if(BuySellSide == 1)
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
    }
    else    
    {
        AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
    AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
    if (_nSegMode == SEG_NSEFO){
       AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
    }
    AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
    Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    // End Enqueue Broadcast Packet             
    
}


long Filltoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token) // 1 Buy , 2 Sell
{
    
    if(BuySellSide == 1)
    {
        //std::cout << "Fill Called  Buy "  << std::endl;
        
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
                ME_OrderBook.OrderBook[Token].Buy[j].lPrice = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].DQty = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].IsDQ =0 ;
                ME_OrderBook.OrderBook[Token].Buy[j].IsIOC = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].OpenQty = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].OrderNo = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].SeqNo = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].TTQ = 0;
                ME_OrderBook.OrderBook[Token].Buy[j].lQty = 0;
//                 //std::cout <<  " Fill Adjusted Done "  
//                         << std::endl;
                SortBuySideBook(Token); 
                ME_OrderBook.OrderBook[Token].BuyRecords = ME_OrderBook.OrderBook[Token].BuyRecords - 1; 
             }   
         }
        
          
            
            
            return 0;
    }  
    else
    {
        //std::cout << "Fill Called Sell "  << std::endl;
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[Token].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[Token].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
                ME_OrderBook.OrderBook[Token].Sell[j].lPrice = 999999999999;
                ME_OrderBook.OrderBook[Token].Sell[j].DQty = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].IsDQ =0;
                ME_OrderBook.OrderBook[Token].Sell[j].IsIOC = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].OpenQty = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].OrderNo = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].SeqNo = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].TTQ = 0;
                ME_OrderBook.OrderBook[Token].Sell[j].lQty = 0;
                 
//                 //std::cout <<  " Fill Adjusted Done "  
//                         << std::endl;
                SortSellSideBook(Token); 
                ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords - 1; 
             }   
         }
        
          
            
            
            return 0;
    }    
}

int Matching(long Token, int FD,int IsIOC,int IsDQ,CONNINFO* pConnInfo)
{
    //std::cout << "Matching Called " << std::endl;
    /*Sneha - multiple connection changes:15/07/16 - S*/
    /*int buyFD = ME_OrderBook.OrderBook[Token].Buy[0].FD;
    int sellFD = ME_OrderBook.OrderBook[Token].Sell[0].FD; 
    CONNINFO*  buyConnInfo = ME_OrderBook.OrderBook[Token].Buy[0].connInfo;
    CONNINFO* sellConnInfo = ME_OrderBook.OrderBook[Token].Sell[0].connInfo; */
    int buyFD, sellFD, userId;
    CONNINFO *buyConnInfo = NULL, *sellConnInfo= NULL;
    bool bConnInfo= true;
    long orderNo = 0;
    /*Sneha - multiple connection changes:15/07/16 - E*/
    if(ME_OrderBook.OrderBook[Token].Sell[0].lPrice <=  ME_OrderBook.OrderBook[Token].Buy[0].lPrice )   // Bid is greater than or equal to ask
    {
        int loop = 1;
        int TradeQty;
        long TradePrice;
        int resp; 
        //int MyTime = GlobalSeqNo++;              
        int64_t MyTime = getCurrentTimeInNano();  
        int32_t epochTime = getEpochTime();
        
        while(loop > 0)
        {
            /*Sneha - DQty changes - S*/
            long buyQty = ME_OrderBook.OrderBook[Token].Buy[0].lQty; 
            long sellQty = ME_OrderBook.OrderBook[Token].Sell[0].lQty;
            if (ME_OrderBook.OrderBook[Token].Sell[0].IsDQ && 
                (ME_OrderBook.OrderBook[Token].Sell[0].DQty < sellQty))
            {
               sellQty = ME_OrderBook.OrderBook[Token].Sell[0].DQty;
            }
            if (ME_OrderBook.OrderBook[Token].Buy[0].IsDQ &&
                (ME_OrderBook.OrderBook[Token].Buy[0].DQty < buyQty))
            {
                buyQty = ME_OrderBook.OrderBook[Token].Buy[0].DQty;
            }
            
            if(sellQty > buyQty) // to determine the Trade Qty
            {
                 TradeQty = buyQty;
            }
            else
            {
                TradeQty = sellQty; 
            }
              /*Sneha - DQty changes - E*/   
            if(ME_OrderBook.OrderBook[Token].Buy[0].SeqNo < ME_OrderBook.OrderBook[Token].Sell[0].SeqNo)
            {
                TradePrice = ME_OrderBook.OrderBook[Token].Buy[0].lPrice;
            }    
            else
            {
               TradePrice = ME_OrderBook.OrderBook[Token].Sell[0].lPrice;    
            }    
                 
                // ---- Sell Handling 
                 ME_OrderBook.OrderBook[Token].TradeNo = ME_OrderBook.OrderBook[Token].TradeNo + 1;
                 ME_OrderBook.OrderBook[Token].Sell[0].TTQ = ME_OrderBook.OrderBook[Token].Sell[0].TTQ + TradeQty;
                 ME_OrderBook.OrderBook[Token].Sell[0].lQty = ME_OrderBook.OrderBook[Token].Sell[0].lQty - TradeQty;
                 
                 switch(_nSegMode)
                 {
                   case SEG_NSECM: //Order_Store_NSECM
                   {
                     NSECM::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
//                     memcpy(&SendTradeConf.AccountNumber,OrderInfofortrade[ME_OrderBook.OrderBook[Token].Sell[0].SeqNo].AccountNumber,
//                       sizeof(SendTradeConf.AccountNumber));
                     orderNo = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,&(Order_Store_NSECM[orderNo].AccountNumber),
                       sizeof(SendTradeConf.AccountNumber));
                     //SendTradeConf.BookType = OrderInfofortrade[ME_OrderBook.OrderBook[Token].Sell[0].SeqNo].BookType;
                     SendTradeConf.BookType = Order_Store_NSECM[orderNo].BookType;
                     SendTradeConf.DisclosedVolume = __bswap_32(Order_Store_NSECM[orderNo].DisclosedVolume);
                     SendTradeConf.Price = __bswap_32(Order_Store_NSECM[orderNo].Price);
                     memcpy(&SendTradeConf.sec_info, &Order_Store_NSECM[orderNo].sec_info,sizeof(SendTradeConf.sec_info));
                     SendTradeConf.BookType = __bswap_16(Order_Store_NSECM[orderNo].BookType);
                     
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].TTQ);
                      //memcpy(&SendTradeConf,&OrderInfofortrade[ME_OrderBook.OrderBook[Token].Sell[0].SeqNo],sizeof(SendTradeConf));
                      SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);     
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.ActivityTime  = epochTime;
                      //SwapDouble((char*) &SendTradeConf.Timestamp1);   
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.ActivityTime  = __bswap_32(SendTradeConf.ActivityTime);
                      SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      
                      SendTradeConf.UserId = Order_Store_NSECM[orderNo].TraderId; 
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
                                          

                      /*Sneha - multiple connection changes:15/07/16 - S*/
                      //resp = write( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      userId = __bswap_32(SendTradeConf.UserId);
                      bConnInfo= getConnInfo(userId, &sellFD, &sellConnInfo);
                      if (bConnInfo == true)
                      {
                          resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                      }
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Qty %d|Price %ld",sellFD,orderNo,TradeQty,TradePrice);
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<sellFD<<"|Sell Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      /*Sneha - E*/     
    
                      
//
//                      std::cout << " Trade Sent -- Order Number" << ME_OrderBook.OrderBook[Token].Sell[0].OrderNo 
//                              << "  Trade No  "   <<   ME_OrderBook.OrderBook[Token].TradeNo  
//                              <<  " Fill Price " << ME_OrderBook.OrderBook[Token].Sell[0].lPrice 
//                              <<  " Fill Qty " << TradeQty
//                              << std::endl; 
                      
                      
                     // ---- Sell Handling ------------------------------------



                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[Token].Buy[0].lQty = ME_OrderBook.OrderBook[Token].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[Token].Buy[0].TTQ = ME_OrderBook.OrderBook[Token].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,Order_Store_NSECM[orderNo].AccountNumber,sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = Order_Store_NSECM[orderNo].BookType;
                     SendTradeConf.DisclosedVolume = __bswap_32(Order_Store_NSECM[orderNo].DisclosedVolume);
                     SendTradeConf.Price = __bswap_32(Order_Store_NSECM[orderNo].Price);
                     memcpy(&SendTradeConf.sec_info, &Order_Store_NSECM[orderNo].sec_info,sizeof(SendTradeConf.sec_info));
                     //std::cout<<"Matching|Symbol-Series ="<<SendTradeConf.sec_info.Symbol<<"-"<<SendTradeConf.sec_info.Series<<std::endl;
                     SendTradeConf.BookType = __bswap_16(Order_Store_NSECM[orderNo].BookType);
                      //memcpy(&SendTradeConf,&OrderInfofortrade[ME_OrderBook.OrderBook[Token].Buy[0].SeqNo],sizeof(SendTradeConf));
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].TTQ);

                      SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.ActivityTime  = epochTime;
                      //SwapDouble((char*) &SendTradeConf.Timestamp1);   
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.ActivityTime  = __bswap_32(SendTradeConf.ActivityTime);

                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].lQty);

                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16 - S*/
                      SendTradeConf.UserId = Order_Store_NSECM[orderNo].TraderId; 
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf));  
                      userId = __bswap_32(SendTradeConf.UserId);
                      bConnInfo= getConnInfo(userId, &buyFD, &buyConnInfo);
                      if (bConnInfo == true)
                      {
                          resp = SendToClient( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf), buyConnInfo);
                      }
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Qty  %d|Price %ld",buyFD,orderNo,TradeQty,TradePrice);
                     Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                     
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      /*Sneha - E*/
                        // Enqueue Broadcast Packet 
                        //FillData.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                        FillData.stBcastMsg.stTrdMsg.header.nSeqNo= GlobalBrodcastSeqNo++;
                        FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                        FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                        FillData.stBcastMsg.stTrdMsg.nToken = Token;
                        FillData.stBcastMsg.stTrdMsg.nTradePrice = TradePrice;
                        FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeQty;
                        FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
                        Inqptr_METoBroadcast_Global->enqueue(FillData);
                        // End Enqueue Broadcast Packet                         
                      
                   }
                   break;
                   case SEG_NSEFO: //Order_Store_NSEFO
                   {
                     NSEFO::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,Order_Store_NSEFO[orderNo].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
                     SendTradeConf.DisclosedVolume = Order_Store_NSEFO[orderNo].DisclosedVolume;
                     SendTradeConf.Price = Order_Store_NSEFO[orderNo].Price;
                     SendTradeConf.Token = Order_Store_NSEFO[orderNo].TokenNo;
                     //SendTradeConf.BookType = __bswap_16(Order_Store_NSEFO[orderNo].BookType);
                     
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].TTQ);
                      //memcpy(&SendTradeConf,&OrderInfofortrade[ME_OrderBook.OrderBook[Token].Sell[0].SeqNo],sizeof(SendTradeConf));
                      SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);     
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.ActivityTime  = epochTime;
                      //SwapDouble((char*) &SendTradeConf.Timestamp1);   
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.ActivityTime  = __bswap_32(SendTradeConf.ActivityTime);
                      SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Sell[0].lQty);


                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));

                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                       /*Sneha - multiple connection changes:15/07/16 - S*/
                       SendTradeConf.TraderId = Order_Store_NSEFO[orderNo].TraderId;
                       //resp = write( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      userId = __bswap_32(SendTradeConf.TraderId);
                      bConnInfo= getConnInfo(userId, &sellFD, &sellConnInfo);
                      if (bConnInfo == true)
                      {
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                      }
                       snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Qty %d|Price %ld",sellFD,orderNo,TradeQty,TradePrice);
                       Logger::getLogger().log(DEBUG, logBuf);
                       //std::cout<<"FD "<<sellFD<<"|Sell Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                        
                       memset (&LogData, 0, sizeof(LogData));
                       LogData.MyFd = 2; /*2 = Trade response*/
                       memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                       Inqptr_MeToLog_Global->enqueue(LogData);
                       /*Sneha - E*/ 
                     // ---- Sell Handling ------------------------------------



                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[Token].Buy[0].lQty = ME_OrderBook.OrderBook[Token].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[Token].Buy[0].TTQ = ME_OrderBook.OrderBook[Token].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,Order_Store_NSEFO[orderNo].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
                     SendTradeConf.DisclosedVolume = Order_Store_NSEFO[orderNo].DisclosedVolume;
                     SendTradeConf.Price = Order_Store_NSEFO[orderNo].Price;
                     SendTradeConf.Token = Order_Store_NSEFO[orderNo].TokenNo;
                     SendTradeConf.BookType = Order_Store_NSEFO[orderNo].BookType;
                      //memcpy(&SendTradeConf,&OrderInfofortrade[ME_OrderBook.OrderBook[Token].Buy[0].SeqNo],sizeof(SendTradeConf));
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].TTQ);

                      SendTradeConf.FillNumber =  __bswap_32(ME_OrderBook.OrderBook[Token].TradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.ActivityTime  = epochTime;
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.ActivityTime  = __bswap_32(SendTradeConf.ActivityTime);

                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[Token].Buy[0].lQty);

                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16*/
                      SendTradeConf.TraderId = Order_Store_NSEFO[orderNo].TraderId;
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf));  
                      userId = __bswap_32(SendTradeConf.TraderId);
                      bConnInfo= getConnInfo(userId, &buyFD, &buyConnInfo);
                      if (bConnInfo == true)
                      {
                           resp = SendToClient( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf),buyConnInfo);
                      }
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Qty %d|Price %ld",buyFD,orderNo,TradeQty,TradePrice);
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
               
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      /*Sneha - E*/ 
                        // Enqueue Broadcast Packet 
                        //FillData.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                        FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                        FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[Token].Buy[0].OrderNo;
                        FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[Token].Sell[0].OrderNo;
                        FillData.stBcastMsg.stTrdMsg.nToken = __bswap_32(Order_Store_NSEFO[orderNo].TokenNo);
                        FillData.stBcastMsg.stTrdMsg.nTradePrice = TradePrice;
                        FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeQty;
                        FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
                        Inqptr_METoBroadcast_Global->enqueue(FillData);
                        // End Enqueue Broadcast Packet                         
                      
                   }
                   break;
                   default:
                     break;
                 }
                   
                 
                // ---- Buy Handling ----------------------------------------------------
            if(ME_OrderBook.OrderBook[Token].Sell[0].lQty == 0)
            {   
                /*Sneha - DQty changes - S*/
                ORDER_BOOK_DTLS BookDetails;
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Sell[0],sizeof(BookDetails));
                Filltoorderbook(&BookDetails,2,Token); // 1 Buy , 2 Sell
                //ME_OrderBook.OrderBook[Token].Sell[0].lPrice = 999999999999;                    
                //ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords - 1; 
                //SortSellSideBook(Token);
                //std::cout << "ME_OrderBook.OrderBook[Token].Sell[0].lQty == 0" << std::endl;
            }   
            if(ME_OrderBook.OrderBook[Token].Buy[0].lQty == 0 )
            {
                /*Sneha - multiple connection changes:15/07/16 - S*/
                ORDER_BOOK_DTLS BookDetails;
              
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Buy[0],sizeof(BookDetails));
                Filltoorderbook(&BookDetails,1,Token); // 1 Buy , 2 Sell
                  /*Sneha - DQty changes - E*/
                //ME_OrderBook.OrderBook[Token].Sell[0].lPrice = 999999999999;                    
                //ME_OrderBook.OrderBook[Token].SellRecords = ME_OrderBook.OrderBook[Token].SellRecords - 1; 
                //SortSellSideBook(Token);
               
                /*Sneha - multiple connection changes:15/07/16 - E*/
            }    
        } 
        Matching(Token,FD,IsIOC,IsDQ,pConnInfo);
    } 
    else
    {
        ORDER_BOOK_DTLS BookDetails;
        if(ME_OrderBook.OrderBook[Token].Sell[0].IsIOC == 1)
        {
            //long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token) // 1 Buy , 2 Sell
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Sell[0],sizeof(BookDetails));
            //std::cout <<  "Sell IOC Check " << std::endl;
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/  
                SendOrderCancellation_NSECM(BookDetails.OrderNo, Token,0, FD,pConnInfo);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                SendOrderCancellation_NSEFO(BookDetails.OrderNo, Token,0, FD,pConnInfo);
              }
              break;
              
            }        
            Cantoorderbook(&BookDetails, 2, Token, getEpochTime()); // 1 Buy , 2 Sell            
            
        }   
        if(ME_OrderBook.OrderBook[Token].Buy[0].IsIOC == 1)
        {
            //std::cout <<  "Buy IOC Check " << std::endl;
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[Token].Buy[0],sizeof(BookDetails));
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                SendOrderCancellation_NSECM(BookDetails.OrderNo, Token,0, FD,pConnInfo);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                SendOrderCancellation_NSEFO(BookDetails.OrderNo, Token,0, FD,pConnInfo);
              }
              break;
              
            }              
            Cantoorderbook(&BookDetails, 1, Token, getEpochTime()); // 1 Buy , 2 Sell
            
        }  
        
//        if(ME_OrderBook.OrderBook[Token].Sell[0].IsIOC == 1 && ME_OrderBook.OrderBook[Token].Buy[0].lPrice == 0 )
//        {
//            std::cout << " IOC is True and Buy Price = 0" << std::endl;
//            memcpy(&ME_OrderBook.OrderBook[Token].Sell[0],&BookDetails,sizeof(ME_OrderBook.OrderBook[Token].Sell[0]));                      
//            switch(_nSegMode)
//            {
//              case SEG_NSECM:
//              {
//                SendOrderCancellation_NSECM(BookDetails.OrderNo, Token,0,FD);
//              }
//              break;
//              case SEG_NSEFO:
//              {
//                SendOrderCancellation_NSEFO(BookDetails.OrderNo, Token,0,FD);
//              }
//              break;
//              
//            }        
//            Cantoorderbook(&BookDetails, 2, Token); // 1 Buy , 2 Sell   
//            
//        }    
        
    }    
    
 
}

int SendOrderBook(long Token, int ExchSeg)
{
    
}        



