
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
#include <string.h>
#include <signal.h>
#include "perf.h"  
#include <new>
bool TimerStarted1 = INIT_PERF_TIMER(7);
extern int16_t gStartPFSFlag;
extern int16_t gPFSWithClient;

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


#define STORESIZE 40000
//#define STORESIZE 200000
#define FOOFFSET 35000
#define ORDERNO 1000000000000000

typedef std::unordered_map<std::string, int32_t> NSECMToken;
typedef NSECMToken::iterator TokenItr;

//typedef StoreOrderID::iterator OrderIdItr;
//ORDER_BOOK_MAIN ME_OrderBook;
ORDER_BOOK_ME  ME_OrderBook;
ORDER_BOOK_ME ME_Passive_OrderBook;
ORDER_BOOK_ME_MB  ME_MB_OrderBook;
TOKEN_INDEX_MAPPING TokenIndexMapping;

/*NSECM_STORE_TRIM* Order_Store_NSECM;
NSEFO_STORE_TRIM* Order_Store_NSEFO;*/
TokenOrderCnt** dealerOrdArr; 
TokenOrderCnt** dealerOrdArrNonTrim;
//NSEFO::TRADE_CONFIRMATION_TR OrderInfofortrade[100000];
//long ME_OrderNumber;
int64_t ME_OrderNumber;
int32_t gMETradeNo;
long GlobalIncrement; // Only for sending incremental timestamp1 - Jiffy
long GlobalSeqNo; // For incrementing order / mod seq
long GlobalBrodcastSeqNo;
int16_t gBookSizeThresholdPer;
//int32_t gInitialBooksize;
NSECMToken *pNSECMContract;
int32_t*  TokenStore;
int TokenCount;
dealerInfoMap* dealerInfoMapGlobal;
CONTRACTINFO* contractInfoGlobal;
CD_CONTRACTINFO* cdContractInfoGlobal;
bool bEnableBrdcst;
bool bEnableValMsg;
short gMLmode;

BROADCAST_DATA FillData;
BROADCAST_DATA AddModCan;
ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast_Global;
ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MeToLog_Global;
DATA_RECEIVED LogData;
DATA_RECEIVED MEtoTCPData;
int _nSegMode;
int iMaxWriteAttempt_Global;
int iEpochBase_Global;
char logBuf[500];

int64_t beforeMatching = 0;
int64_t afterMatching = 0;

int64_t beforeMatchingBookBuilder = 0;
int64_t afterMatchingBookBuilder = 0;

int gSimulatorMode=1;
/*To handle MB Trade*/
short MBTradeHandlingSeqNo=0;
double OrderNumberMBTrade = 0;
/*To handle MB Trade ends*/
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

int32_t tradeNoBase(short streamID)
{
  int contd;
  int32_t baseOrder=streamID;
  contd=(streamID>=10||streamID<1)?contd=0:contd=1;                  //1,2 are reserved and % digit no are generated for streamID>=80
  if(contd)
  {                                                   
    baseOrder = baseOrder * 125000000;
    if(8 == streamID||9 == streamID)
    {
      baseOrder = baseOrder / 10;
    }
    return baseOrder;
  }
  else
  {
    snprintf(logBuf, 500, "Invalid StreamID. Allowed:1-9");
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
}
int64_t NewOrderNoBase(short streamID)
{
  int contd;
  int64_t baseOrder=streamID;
  contd=(streamID>=10||streamID<1)?contd=0:contd=1;                  
  if(contd)
  {                                                   
    baseOrder = baseOrder * 100000000000000 + 1000000000000000;
    
    return baseOrder;
  }
  else
  {
    snprintf(logBuf, 500, "Invalid StreamID. Allowed:1-9");
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
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

void fillDataBuy(int start,int end,ORDER_BOOK_DTLS Buy[])
{
  for(int j = start; j < end; j++)
  {
    Buy[j].lPrice = 0;
    Buy[j].DQty = 0;
    Buy[j].IsDQ =0 ;
    Buy[j].IsIOC = 0;
    Buy[j].OpenQty = 0;
    Buy[j].OrderNo = 0;
    Buy[j].SeqNo = 0;
    Buy[j].TTQ = 0;
    Buy[j].lQty = 0;
  }
}

  
void fillDataSell(int start,int end,ORDER_BOOK_DTLS Sell[])
{
  for(int j = start; j < end; j++)
  {
    Sell[j].lPrice = 2147483647;
    Sell[j].DQty = 0;
    Sell[j].IsDQ =0;
    Sell[j].IsIOC = 0;
    Sell[j].OpenQty = 0;
    Sell[j].OrderNo = 0;
    Sell[j].SeqNo = 0;
    Sell[j].TTQ = 0;
    Sell[j].lQty = 0;
  }
}

/*For Market Book Builder*/
void fillMBDataBuy(int start,int end,ORDER_BOOK_DTLS_MB Buy[])
{
  for(int j = start; j < end; j++)
  {
    Buy[j].lPrice = 0;
    Buy[j].IsDQ =0 ;
    Buy[j].IsIOC = 0;
    Buy[j].OrderNo = 0;
    Buy[j].SeqNo = 0;
    Buy[j].lQty = 0;
  }
}

  
void fillMBDataSell(int start,int end,ORDER_BOOK_DTLS_MB Sell[])
{
  for(int j = start; j < end; j++)
  {
    Sell[j].lPrice = 2147483647;
    Sell[j].IsDQ =0;
    Sell[j].IsIOC = 0;
    Sell[j].OrderNo = 0;
    Sell[j].SeqNo = 0;
    Sell[j].lQty = 0;
  }
}

/*For Market Book Builder end*/


//std::ref(Inqptr_TCPServerToMe),std::ref(Inqptr_MeToTCPServer),std::ref(Inqptr_METoBroadcast)
void StartME(ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_TCPServerToMe,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer,
             ProducerConsumerQueue<BROADCAST_DATA>* Inqptr_METoBroadcast, 
             ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_PFStoME,
             ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_MBtoME,
             std::unordered_map<std::string, int32_t>* pcNSECMTokenStore, int _nMode,
             ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToLog, int iMaxWriteAttempt, int iEpochBase, int iMECore,
             int32_t* Tokenarr, int TokenCnt, dealerInfoMap* dealerInfomap, CONTRACTINFO* pCntrctInfo, CD_CONTRACTINFO* pCDCntrctInfo, bool enableBrdcst,short StreamID, short MLmode, bool enablePFS,int simulatorMode,const char* pcIpaddress, int nPortNumber,int iInitialBookSize, int16_t sBookSizeThreshold,bool enableValMsg)
{
    TaskSetCPU(iMECore);
    gSimulatorMode = simulatorMode;
    bEnableValMsg = enableValMsg;
         
    snprintf(logBuf,500, "Thread_ME|Matching Thread started");
    Logger::getLogger().log(DEBUG, logBuf);
    
    snprintf(logBuf,500, "Thread_ME|NOOFTOKENS: %d", TokenCnt);
    Logger::getLogger().log(DEBUG, logBuf);

    snprintf(logBuf,500, "Thread_ME|INITIAL BOOKSIZE: %d", iInitialBookSize);
    Logger::getLogger().log(DEBUG, logBuf);
        
    snprintf(logBuf,500, "Thread_ME|FOOFFSET: %d", FOOFFSET);
    Logger::getLogger().log(DEBUG, logBuf);
    
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
    cdContractInfoGlobal = pCDCntrctInfo;
    bEnableBrdcst = enableBrdcst;
    ME_OrderNumber = NewOrderNoBase(StreamID);
    
    GlobalIncrement = 1;
    GlobalBrodcastSeqNo = 1;
    GlobalSeqNo = 1;
    noOfLegs = 0;
    _nSegMode = _nMode;
    gMETradeNo = tradeNoBase(StreamID);
    gMLmode = MLmode;
    
    gBookSizeThresholdPer = sBookSizeThreshold;

    // Set Broadcast Structures
    FillData.stBcastMsg.stTrdMsg.header.wStremID = 1 ;
    FillData.stBcastMsg.stTrdMsg.cMsgType = 'T';
    FillData.wPacketType = 2;  // Add - Mod - Can
    AddModCan.stBcastMsg.stGegenricOrdMsg.header.wStremID=1 ;  
    AddModCan.wPacketType = 1;
   // Done Broadcast Structures
    
   /*Declare and initialize order book*/    
    ME_OrderBook.OrderBook = new ORDER_BOOK_TOKEN[TokenCount];
    for(int i=0;i< TokenCount;i++)
    {
        ME_OrderBook.OrderBook[i].Token = TokenStore[i];
        ME_OrderBook.OrderBook[i].BuyRecords = 0;
        ME_OrderBook.OrderBook[i].SellRecords = 0;
        ME_OrderBook.OrderBook[i].TradeNo = 0;
        ME_OrderBook.OrderBook[i].BuySeqNo =0;
        ME_OrderBook.OrderBook[i].SellSeqNo=0;
        ME_OrderBook.OrderBook[i].SellBookFull = false;
        ME_OrderBook.OrderBook[i].BuyBookFull = false;
        ME_OrderBook.OrderBook[i].BuyBookSize = iInitialBookSize;
        ME_OrderBook.OrderBook[i].SellBookSize = iInitialBookSize;
        
        
        ME_OrderBook.OrderBook[i].Buy = new ORDER_BOOK_DTLS[iInitialBookSize];

        ME_OrderBook.OrderBook[i].Sell = new ORDER_BOOK_DTLS[iInitialBookSize];  
        
        for(int j=0; j < iInitialBookSize; j++)
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
            
            
            ME_OrderBook.OrderBook[i].Sell[j].lPrice = 2147483647;
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


    /*Declare and initialize MB order book*/ 
    if(gSimulatorMode==3)
    {
       
      ME_MB_OrderBook.OrderBook = new ORDER_BOOK_TOKEN_MB[TokenCount];
      for(int i=0;i< TokenCount;i++)
      {
          ME_MB_OrderBook.OrderBook[i].Token = TokenStore[i];
          ME_MB_OrderBook.OrderBook[i].BuyRecords = 0;
          ME_MB_OrderBook.OrderBook[i].SellRecords = 0;
          ME_MB_OrderBook.OrderBook[i].TradeNo = 0;
          ME_MB_OrderBook.OrderBook[i].BuySeqNo =0;
          ME_MB_OrderBook.OrderBook[i].SellSeqNo=0;
          ME_OrderBook.OrderBook[i].SellBookFull = false;
          ME_OrderBook.OrderBook[i].BuyBookFull = false;
          ME_MB_OrderBook.OrderBook[i].Buy = new ORDER_BOOK_DTLS_MB[iInitialBookSize];   
          ME_MB_OrderBook.OrderBook[i].Sell = new ORDER_BOOK_DTLS_MB[iInitialBookSize];
          
          for(int j=0; j < iInitialBookSize; j++)
          {
              ME_MB_OrderBook.OrderBook[i].Buy[j].lPrice = 0;
              ME_MB_OrderBook.OrderBook[i].Buy[j].IsDQ =0 ;
              ME_MB_OrderBook.OrderBook[i].Buy[j].IsIOC = 0;
              ME_MB_OrderBook.OrderBook[i].Buy[j].OrderNo = 0;
              ME_MB_OrderBook.OrderBook[i].Buy[j].SeqNo = 0;
              ME_MB_OrderBook.OrderBook[i].Buy[j].lQty = 0;


              ME_MB_OrderBook.OrderBook[i].Sell[j].lPrice = 2147483647;
              ME_MB_OrderBook.OrderBook[i].Sell[j].IsDQ =0;
              ME_MB_OrderBook.OrderBook[i].Sell[j].IsIOC = 0;
              ME_MB_OrderBook.OrderBook[i].Sell[j].OrderNo = 0;
              ME_MB_OrderBook.OrderBook[i].Sell[j].SeqNo = 0;
              ME_MB_OrderBook.OrderBook[i].Sell[j].lQty = 0;
          }    
      }    
    }
    
    /*Declare and initialize Passive order book*/    
    ME_Passive_OrderBook.OrderBook = new ORDER_BOOK_TOKEN[TokenCount];
    for(int i=0;i< TokenCount;i++)
    {
        ME_Passive_OrderBook.OrderBook[i].Token = TokenStore[i];
        ME_Passive_OrderBook.OrderBook[i].BuyRecords = 0;
        ME_Passive_OrderBook.OrderBook[i].SellRecords = 0;
        ME_Passive_OrderBook.OrderBook[i].TradeNo = 0;
        ME_Passive_OrderBook.OrderBook[i].BuySeqNo =0;
        ME_Passive_OrderBook.OrderBook[i].SellSeqNo=0;
        ME_OrderBook.OrderBook[i].SellBookFull = false;
        ME_OrderBook.OrderBook[i].BuyBookFull = false;
        ME_Passive_OrderBook.OrderBook[i].BuyBookSize = iInitialBookSize;
        ME_Passive_OrderBook.OrderBook[i].SellBookSize = iInitialBookSize;
           
        ME_Passive_OrderBook.OrderBook[i].Buy = new ORDER_BOOK_DTLS[iInitialBookSize];   
        ME_Passive_OrderBook.OrderBook[i].Sell = new ORDER_BOOK_DTLS[iInitialBookSize]; 
        
        for(int j=0; j < iInitialBookSize; j++)
        {
            ME_Passive_OrderBook.OrderBook[i].Buy[j].lPrice = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].TriggerPrice = 2147483647;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].DQty = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].IsDQ =0 ;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].IsIOC = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].OpenQty = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].OrderNo = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].SeqNo = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].TTQ = 0;
            ME_Passive_OrderBook.OrderBook[i].Buy[j].lQty = 0;
            
            
            ME_Passive_OrderBook.OrderBook[i].Sell[j].lPrice = 2147483647;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].TriggerPrice = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].DQty = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].IsDQ =0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].IsIOC = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].OpenQty = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].OrderNo = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].SeqNo = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].TTQ = 0;
            ME_Passive_OrderBook.OrderBook[i].Sell[j].lQty = 0;
            
        }    
    }    
    
    
    int milisec = 2; // length of time to sleep, in miliseconds
    struct timespec req = {0};
   
   /*declare dealerOrder array & initialize it with tokens*/
    
    
    int dealerCount  = dealerInfoMapGlobal->size();
   dealerOrdArr = new TokenOrderCnt* [dealerCount];
    for (int i = 0; i < dealerCount; i++)
    {
          dealerOrdArr[i] = new TokenOrderCnt[TokenCount];
    }
    for (int j = 0; j < TokenCount; j++)
    {
        for (int i = 0; i < dealerCount; i++)
        {
          dealerOrdArr[i][j].token = TokenStore[j];
        }
    }

   dealerOrdArrNonTrim = new TokenOrderCnt* [dealerCount];
    for (int i = 0; i < dealerCount; i++)
    {
          dealerOrdArrNonTrim[i] = new TokenOrderCnt[TokenCount];
    }
    for (int j = 0; j < TokenCount; j++)
    {
        for (int i = 0; i < dealerCount; i++)
        {
          dealerOrdArrNonTrim[i][j].token = TokenStore[j];
        }
    }
   
  //PFS Change start NK
  // Any preprocessing or initialization
   GENERIC_ORD_MSG tbtMsgOut;
   bool handlePFS = false;
   if (enablePFS && Inqptr_PFStoME!=NULL)
   {
     handlePFS = true;
   }
   gStartPFSFlag = 1;

  //PFS Change ends NK
   GENERIC_ORD_MSG MBMsgOut;

   while(1)
    {
        for(int j=0; j < 10 ; j++) 
        {

            if(Inqptr_TCPServerToMe->dequeue(RcvData))
            {
            
                snprintf(logBuf, 500, "Thread_ME|FD %d|Tap SeqNo %d", RcvData.MyFd, __bswap_32(((NSECM::ME_MESSAGE_HEADER *)RcvData.msgBuffer)->iSeqNo));
                Logger::getLogger().log(DEBUG, logBuf);
               ProcessTranscodes(&RcvData,std::ref(Inqptr_MeToTCPServer));
               if ((RcvData.ptrConnInfo) != NULL){
                   (RcvData.ptrConnInfo)->recordCnt--;
                   if (DISCONNECTED == (RcvData.ptrConnInfo)->status){
                       MEtoTCPData.MyFd = RcvData.MyFd;
                       MEtoTCPData.Transcode = DISCONNECT_CLIENT;
                       MEtoTCPData.ptrConnInfo = (RcvData.ptrConnInfo);
                       Inqptr_MeToTCPServer->enqueue(MEtoTCPData);
                   }
               }
               memset(&RcvData, 0, sizeof(RcvData));
             }
            if(gSimulatorMode==3)
            {
              if(Inqptr_MBtoME->dequeue(MBMsgOut))
              {
                handleMBMsg(&MBMsgOut);
              }
            }
            //PFS Change start NK
            // Handle TBT MSG
            if(handlePFS==true)
            {
              if(Inqptr_PFStoME->dequeue(tbtMsgOut))
              {
//                #ifdef __PFS_LOG__
              snprintf(logBuf, 400, "Thread_ME|OUT:SeqNo:%d|MsgType:%c|OrderId:%ld|OrderType:%c|Price:%d|Qty:%d", tbtMsgOut.header.nSeqNo,tbtMsgOut.cMsgType,(int64_t)tbtMsgOut.dblOrdID,tbtMsgOut.cOrdType,tbtMsgOut.nPrice,tbtMsgOut.nQty);
              Logger::getLogger().log(DEBUG, logBuf);    
//              #endif
//                std::cout<<"OUT::"<<tbtMsgOut.header.nSeqNo<<"|"<<tbtMsgOut.cMsgType<<"|"<<(int64_t)tbtMsgOut.dblOrdID<<"|"<<tbtMsgOut.cOrdType<<"|"<<tbtMsgOut.nPrice<<"|"<<tbtMsgOut.nToken<<std::endl;
                handlePFSMsg(&tbtMsgOut);
                
              }
            }
            //PFS Change ends NK
        }            
         req.tv_sec = 0;
         req.tv_nsec = milisec * 1000000L;
    }        
}  

int SendOrderCancellation_NSECM(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst)
{
     NSECM::MS_OE_RESPONSE_TR CanOrdResp;
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    if (COL == 1){
      CanOrdResp.ErrorCode = 0;
    }
    else{
      CanOrdResp.ErrorCode = __bswap_16(e$fok_order_cancelled);
    }
    CanOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.TimeStamp1 = __bswap_64(CanOrdResp.TimeStamp1); 
    CanOrdResp.Timestamp  = CanOrdResp.TimeStamp1;
    CanOrdResp.TimeStamp2 = '1'; 
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    orderBook->LastModified = orderBook->LastModified + 1;
    CanOrdResp.LastModified = __bswap_32(orderBook->LastModified);
    CanOrdResp.SettlementPeriod =  __bswap_16(1);
    CanOrdResp.TraderId = __bswap_32(orderBook->TraderId);
    CanOrdResp.BookType = __bswap_16(orderBook->BookType);
    CanOrdResp.BuySellIndicator = __bswap_16(orderBook->BuySellIndicator);
    CanOrdResp.DisclosedVolume = __bswap_32(orderBook->DQty);
    CanOrdResp.DisclosedVolumeRemain = __bswap_32(orderBook->DQty);
    CanOrdResp.TotalVolumeRemain = __bswap_32(orderBook->lQty);
    CanOrdResp.Volume = __bswap_32(orderBook->Volume);
    CanOrdResp.Price = __bswap_32(orderBook->lPrice);
    
    CanOrdResp.AlgoCategory = __bswap_16(orderBook->AlgoCategory);
    CanOrdResp.AlgoId = __bswap_32(orderBook->AlgoId);
    memcpy(&CanOrdResp.PAN,&(orderBook->PAN),sizeof(CanOrdResp.PAN));
    
    CanOrdResp.TransactionId=__bswap_32(orderBook->nsecm_nsefo_nsecd.NSECM.TransactionId);
    CanOrdResp.BranchId = __bswap_16(orderBook->BranchId);
    CanOrdResp.UserId = __bswap_32(orderBook->UserId); 
    CanOrdResp.Suspended = orderBook->nsecm_nsefo_nsecd.NSECM.Suspended;       
    CanOrdResp.ProClient = __bswap_16(orderBook->ProClientIndicator);
    CanOrdResp.NnfField = orderBook->NnfField;
    memcpy(&CanOrdResp.sec_info,&(orderBook->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(CanOrdResp.sec_info));
    memcpy(&CanOrdResp.AccountNumber,&(orderBook->AccountNumber),sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.OrderFlags,&(orderBook->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(CanOrdResp.OrderFlags));
    memcpy(&CanOrdResp.Settlor,&(orderBook->Settlor),sizeof(CanOrdResp.Settlor));
    memcpy(&CanOrdResp.BrokerId,&(orderBook->BrokerId),sizeof(CanOrdResp.BrokerId));
    SwapDouble((char*) &CanOrdResp.NnfField);  
    CanOrdResp.OrderNumber = orderBook->OrderNo;
    CanOrdResp.tap_hdr.swapBytes(); 
    SwapDouble((char*) &CanOrdResp.OrderNumber);  

    int i = 0;
    if (COL != 1)
    {
       i = SendToClient(FD, (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
    }
    
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1;
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d|COL %d|Error code %d|LMT %d", 
                     FD, long(orderBook->OrderNo), i, COL, __bswap_16(CanOrdResp.ErrorCode), __bswap_32(CanOrdResp.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    
     // Enqueue Broadcast Packet 
    if (true == bEnableBrdcst && sendBrdcst == true && gSimulatorMode==1)
    {
            AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
            AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'X';
            if(1 == orderBook->BuySellIndicator)
            {
                AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
            }
            else    
            {
                AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
            }
            AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = orderBook->OrderNo;
            AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = orderBook->lPrice;
            AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = orderBook->lQty;
            AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
            AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = long(getEpochTime);
            Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet   
}


/*NK COL Starts*/
int SendOrderCancellationNonTrim_NSECM(ORDER_BOOK_DTLS *orderBook, long Token,int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst)
{
     NSECM::MS_OE_RESPONSE_SL CanOrdResp;
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_SL);
    CanOrdResp.msg_hdr.TransactionCode = __bswap_16(2075);
    CanOrdResp.msg_hdr.LogTime = __bswap_16(1);
    CanOrdResp.msg_hdr.TraderId =  __bswap_32(orderBook->UserId); 
    if (COL == 1){
      CanOrdResp.msg_hdr.ErrorCode = 0;
    }
    else{
      CanOrdResp.msg_hdr.ErrorCode = __bswap_16(e$fok_order_cancelled);
    }
    CanOrdResp.msg_hdr.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.msg_hdr.Timestamp1 = __bswap_64(CanOrdResp.msg_hdr.Timestamp1); 
    CanOrdResp.msg_hdr.Timestamp  = CanOrdResp.msg_hdr.Timestamp1;
    CanOrdResp.msg_hdr.TimeStamp2[7] = '1'; 
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    orderBook->LastModified = orderBook->LastModified + 1;
    CanOrdResp.LastModified = __bswap_32(orderBook->LastModified);
    CanOrdResp.SettlementPeriod =  __bswap_16(1);
    CanOrdResp.TraderId = __bswap_32(orderBook->TraderId);
    CanOrdResp.BookType = __bswap_16(orderBook->BookType);
    CanOrdResp.BuySellIndicator = __bswap_16(orderBook->BuySellIndicator);
    CanOrdResp.DisclosedVolume = __bswap_32(orderBook->DQty);
    CanOrdResp.DisclosedVolumeRemaining = __bswap_32(orderBook->DQty);
    CanOrdResp.TotalVolumeRemaining = __bswap_32(orderBook->lQty);
    CanOrdResp.Volume = __bswap_32(orderBook->Volume);
    CanOrdResp.Price = __bswap_32(orderBook->lPrice);
    
    CanOrdResp.AlgoCategory = __bswap_16(orderBook->AlgoCategory);
    CanOrdResp.AlgoId = __bswap_32(orderBook->AlgoId);
    memcpy(&CanOrdResp.PAN,&(orderBook->PAN),sizeof(CanOrdResp.PAN));
    
    CanOrdResp.TransactionId=__bswap_32(orderBook->nsecm_nsefo_nsecd.NSECM.TransactionId);
    CanOrdResp.BranchId = __bswap_16(orderBook->BranchId);
    
    CanOrdResp.Suspended = orderBook->nsecm_nsefo_nsecd.NSECM.Suspended;       
    CanOrdResp.ProClientIndicator = __bswap_16(orderBook->ProClientIndicator);
    CanOrdResp.NnfField = orderBook->NnfField;
    memcpy(&CanOrdResp.sec_info,&(orderBook->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(CanOrdResp.sec_info));
    memcpy(&CanOrdResp.AccountNumber,&(orderBook->AccountNumber),sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.st_order_flags,&(orderBook->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(CanOrdResp.st_order_flags));
    memcpy(&CanOrdResp.Settlor,&(orderBook->Settlor),sizeof(CanOrdResp.Settlor));
    memcpy(&CanOrdResp.BrokerId,&(orderBook->BrokerId),sizeof(CanOrdResp.BrokerId));
    SwapDouble((char*) &CanOrdResp.NnfField);  
    CanOrdResp.OrderNumber = orderBook->OrderNo;
    CanOrdResp.tap_hdr.swapBytes(); 
    SwapDouble((char*) &CanOrdResp.OrderNumber);  

    int i=0;
    
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 4;
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d|COL %d|Error code %d|LMT %d", 
                     FD, long(orderBook->OrderNo), i, COL, __bswap_16(CanOrdResp.msg_hdr.ErrorCode), __bswap_32(CanOrdResp.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);

}

int SendOrderCancellationNonTrim_NSEFO(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst)
{
    NSEFO::MS_OE_RESPONSE_SL CanOrdResp;
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_SL);
    CanOrdResp.msg_hdr.TransactionCode = __bswap_16(2075);
    CanOrdResp.msg_hdr.LogTime = __bswap_16(1);
    CanOrdResp.msg_hdr.TraderId =  __bswap_32(orderBook->UserId); 
    
    if (COL == 1){
      CanOrdResp.msg_hdr.ErrorCode = 0;
    }
    else{
      CanOrdResp.msg_hdr.ErrorCode = __bswap_16(e$fok_order_cancelled);
    }
    CanOrdResp.msg_hdr.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.msg_hdr.TimeStamp1 = __bswap_64(CanOrdResp.msg_hdr.TimeStamp1); 
    CanOrdResp.msg_hdr.Timestamp  = CanOrdResp.msg_hdr.TimeStamp1;
    CanOrdResp.msg_hdr.TimeStamp2[7] = '1';
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    orderBook->LastModified = orderBook->LastModified + 1;
    CanOrdResp.LastModified = __bswap_32(orderBook->LastModified);
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.TraderId = __bswap_32(orderBook->TraderId);
    CanOrdResp.BookType = __bswap_16(orderBook->BookType);
    CanOrdResp.BuySellIndicator = __bswap_16(orderBook->BuySellIndicator);
    CanOrdResp.DisclosedVolume = __bswap_32(orderBook->DQty);
    CanOrdResp.DisclosedVolumeRemaining = __bswap_32(orderBook->DQty);
    CanOrdResp.TotalVolumeRemaining = __bswap_32(orderBook->lQty);
    CanOrdResp.Volume = __bswap_32(orderBook->Volume);
    CanOrdResp.Price = __bswap_32(orderBook->lPrice);
    
    CanOrdResp.AlgoCategory = __bswap_16(orderBook->AlgoCategory);
    CanOrdResp.AlgoId = __bswap_32(orderBook->AlgoId);
    memcpy(&CanOrdResp.PAN,&(orderBook->PAN),sizeof(CanOrdResp.PAN));
    
    CanOrdResp.BranchId = __bswap_16(orderBook->BranchId);
        
    CanOrdResp.ProClientIndicator = __bswap_16(orderBook->ProClientIndicator);
    CanOrdResp.TokenNo = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSEFO.TokenNo);
     snprintf(logBuf, 500, "book filler = %d", orderBook->nsecm_nsefo_nsecd.NSEFO.filler);
    Logger::getLogger().log(DEBUG, logBuf);
    CanOrdResp.filler = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSEFO.filler);
    
    CanOrdResp.NnfField = orderBook->NnfField;   
    memcpy(&CanOrdResp.AccountNumber,&(orderBook->AccountNumber),sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BrokerId,&(orderBook->BrokerId),sizeof(CanOrdResp.BrokerId)) ;      
    memcpy(&CanOrdResp.Settlor,&(orderBook->Settlor),sizeof(CanOrdResp.Settlor));
    memcpy(&CanOrdResp.st_order_flags,&(orderBook->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(CanOrdResp.st_order_flags));
    SwapDouble((char*) &CanOrdResp.NnfField);
    CanOrdResp.OrderNumber = orderBook->OrderNo;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    CanOrdResp.tap_hdr.swapBytes(); 
  
    int i=0;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 5; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d|COL %d| Error code %d|LMT %d", 
                     FD, long(orderBook->OrderNo), i, COL, __bswap_16(CanOrdResp.msg_hdr.ErrorCode), __bswap_32(CanOrdResp.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
      
}
/*NK COL Ends*/

int SendOrderCancellation_NSEFO(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst)
{
    NSEFO::MS_OE_RESPONSE_TR CanOrdResp;
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
     if (COL == 1){
      CanOrdResp.ErrorCode = 0;
    }
    else{
      CanOrdResp.ErrorCode = __bswap_16(e$fok_order_cancelled);
    }
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1);
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1;
    CanOrdResp.Timestamp2 = '1'; /*Sneha*/
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    orderBook->LastModified = orderBook->LastModified + 1;
    CanOrdResp.LastModified = __bswap_32(orderBook->LastModified);
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.TraderId = __bswap_32(orderBook->TraderId);
    CanOrdResp.BookType = __bswap_16(orderBook->BookType);
    CanOrdResp.BuySellIndicator = __bswap_16(orderBook->BuySellIndicator);
    CanOrdResp.DisclosedVolume = __bswap_32(orderBook->DQty);
    CanOrdResp.DisclosedVolumeRemaining = __bswap_32(orderBook->DQty);
    CanOrdResp.TotalVolumeRemaining = __bswap_32(orderBook->lQty);
    CanOrdResp.Volume = __bswap_32(orderBook->Volume);
    CanOrdResp.Price = __bswap_32(orderBook->lPrice);
    
    CanOrdResp.AlgoCategory = __bswap_16(orderBook->AlgoCategory);
    CanOrdResp.AlgoId = __bswap_32(orderBook->AlgoId);
    memcpy(&CanOrdResp.PAN,&(orderBook->PAN),sizeof(CanOrdResp.PAN));
    
    CanOrdResp.BranchId = __bswap_16(orderBook->BranchId);
    CanOrdResp.UserId = __bswap_32(orderBook->UserId);        
    CanOrdResp.ProClientIndicator = __bswap_16(orderBook->ProClientIndicator);
    CanOrdResp.TokenNo = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSEFO.TokenNo);
     snprintf(logBuf, 500, "book filler = %d", orderBook->nsecm_nsefo_nsecd.NSEFO.filler);
    Logger::getLogger().log(DEBUG, logBuf);
    CanOrdResp.filler = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSEFO.filler);
    CanOrdResp.NnfField = orderBook->NnfField;   
     memcpy(&CanOrdResp.AccountNumber,&(orderBook->AccountNumber),sizeof(CanOrdResp.AccountNumber));
     memcpy(&CanOrdResp.BrokerId,&(orderBook->BrokerId),sizeof(CanOrdResp.BrokerId)) ;      
     memcpy(&CanOrdResp.Settlor,&(orderBook->Settlor),sizeof(CanOrdResp.Settlor));
     memcpy(&CanOrdResp.OrderFlags,&(orderBook->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(CanOrdResp.OrderFlags));
    SwapDouble((char*) &CanOrdResp.NnfField);
    CanOrdResp.OrderNumber = orderBook->OrderNo;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    CanOrdResp.tap_hdr.swapBytes(); 
  
    int i = 0;
    if (COL != 1)
    {
       i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    }
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d|COL %d| Error code %d|LMT %d", 
                     FD, long(orderBook->OrderNo), i, COL, __bswap_16(CanOrdResp.ErrorCode), __bswap_32(CanOrdResp.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    
    // Enqueue Broadcast Packet 
    if (true == bEnableBrdcst && sendBrdcst == true && gSimulatorMode==1)
    {
          AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
          AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'X';
          if(1 == orderBook->BuySellIndicator)
          {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
          }
          else    
          {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = orderBook->OrderNo;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = orderBook->lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = orderBook->lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET;    
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = long(getEpochTime);
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet  
}


int SendOrderCancellation_NSECD(ORDER_BOOK_DTLS *orderBook, long Token, int FD,CONNINFO* pConnInfo, int COL, bool sendBrdcst)
{
    NSECD::MS_OE_RESPONSE_TR CanOrdResp;
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECD::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
     if (COL == 1){
      CanOrdResp.ErrorCode = 0;
    }
    else{
      CanOrdResp.ErrorCode = __bswap_16(e$fok_order_cancelled);
    }
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1);
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1; 
    CanOrdResp.Timestamp2 = '1'; /*Sneha*/
    CanOrdResp.EntryDateTime = __bswap_32(getEpochTime());
    orderBook->LastModified = orderBook->LastModified + 1;
    CanOrdResp.LastModified = __bswap_32(orderBook->LastModified);
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.TraderId = __bswap_32(orderBook->TraderId);
    CanOrdResp.BookType = __bswap_16(orderBook->BookType);
    CanOrdResp.BuySellIndicator = __bswap_16(orderBook->BuySellIndicator);
    CanOrdResp.DisclosedVolume = __bswap_32(orderBook->DQty);
    CanOrdResp.DisclosedVolumeRemaining = __bswap_32(orderBook->DQty);
    CanOrdResp.TotalVolumeRemaining = __bswap_32(orderBook->lQty);
    CanOrdResp.Volume = __bswap_32(orderBook->Volume);
    CanOrdResp.Price = __bswap_32(orderBook->lPrice);
    
    CanOrdResp.AlgoCategory = __bswap_16(orderBook->AlgoCategory);
    CanOrdResp.AlgoId = __bswap_32(orderBook->AlgoId);
    memcpy(&CanOrdResp.PAN,&(orderBook->PAN),sizeof(CanOrdResp.PAN));
    
    CanOrdResp.BranchId = __bswap_16(orderBook->BranchId);
    CanOrdResp.UserId = __bswap_32(orderBook->UserId);        
    CanOrdResp.ProClientIndicator = __bswap_16(orderBook->ProClientIndicator);
    CanOrdResp.TokenNo = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSECD.TokenNo);
     snprintf(logBuf, 500, "book filler = %d", orderBook->nsecm_nsefo_nsecd.NSECD.filler);
    Logger::getLogger().log(DEBUG, logBuf);
    CanOrdResp.filler = __bswap_32(orderBook->nsecm_nsefo_nsecd.NSECD.filler);
    CanOrdResp.NnfField = orderBook->NnfField;   
     memcpy(&CanOrdResp.AccountNumber,&(orderBook->AccountNumber),sizeof(CanOrdResp.AccountNumber));
     memcpy(&CanOrdResp.BrokerId,&(orderBook->BrokerId),sizeof(CanOrdResp.BrokerId)) ;      
     memcpy(&CanOrdResp.Settlor,&(orderBook->Settlor),sizeof(CanOrdResp.Settlor));
     memcpy(&CanOrdResp.OrderFlags,&(orderBook->nsecm_nsefo_nsecd.NSECD.OrderFlags), sizeof(CanOrdResp.OrderFlags));
    SwapDouble((char*) &CanOrdResp.NnfField);
    CanOrdResp.OrderNumber = orderBook->OrderNo;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    CanOrdResp.tap_hdr.swapBytes(); 
  
    int i = 0;
    if (COL != 1)
    {
       i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECD::MS_OE_RESPONSE_TR),pConnInfo);
    }
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    snprintf(logBuf, 500, "Thread_ME|FD %d|Unsol Can|Order # %ld|Bytes Sent %d|COL %d| Error code %d|LMT %d", 
                     FD, long(orderBook->OrderNo), i, COL, __bswap_16(CanOrdResp.ErrorCode), __bswap_32(CanOrdResp.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    
    // Enqueue Broadcast Packet 
    if (true == bEnableBrdcst && sendBrdcst == true && gSimulatorMode==1)
    {
          AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
          AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'X';
          if(1 == orderBook->BuySellIndicator)
          {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
          }
          else    
          {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = orderBook->OrderNo;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = orderBook->lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = orderBook->lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = long(getEpochTime);
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet  
}


int ValidateAddReq(int32_t iUserID, int Fd, double dOrderNo, int iOrderSide, long&  Token, char* symbol, char*series, int& dlrIndex, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, Fd, dlrIndex);
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    else if (_nSegMode == SEG_NSECD)
    {
        found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }

//     if (iOrderSide == 1 && ME_OrderBook.OrderBook[tokIndex].BuyRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
//    else if (iOrderSide == 2 && ME_OrderBook.OrderBook[tokIndex].SellRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
    
    if (iOrderSide == 1 && ME_OrderBook.OrderBook[tokIndex].BuyRecords >= (ME_OrderBook.OrderBook[tokIndex].BuyBookSize - 1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    else if (iOrderSide == 2 && ME_OrderBook.OrderBook[tokIndex].SellRecords >= (ME_OrderBook.OrderBook[tokIndex].SellBookSize - 1))
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    
    return errCode;
}

int ValidateAddReqNonTrim(int32_t iUserID, int Fd, double dOrderNo, int iOrderSide, long&  Token, char* symbol, char*series, int& dlrIndex, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, Fd, dlrIndex);
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }

//     if (iOrderSide == 1 && ME_Passive_OrderBook.OrderBook[tokIndex].BuyRecords >= BOOKSIZE_PASSIVE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
//    else if (iOrderSide == 2 && ME_Passive_OrderBook.OrderBook[tokIndex].SellRecords >= BOOKSIZE_PASSIVE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
    if (iOrderSide == 1 && ME_Passive_OrderBook.OrderBook[tokIndex].BuyRecords >= (ME_Passive_OrderBook.OrderBook[tokIndex].BuyBookSize -1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    else if (iOrderSide == 2 && ME_Passive_OrderBook.OrderBook[tokIndex].SellRecords >= (ME_Passive_OrderBook.OrderBook[tokIndex].SellBookSize -1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    
    return errCode;
}
/*Market book handling functions begin*/


int handleMBMsg(GENERIC_ORD_MSG* GenOrd)
{
  //switch()
  int16_t IsIOC=0,IsDQ=0;
  int64_t recvTimeStamp=0;
  char cMsgType = GenOrd->cMsgType;

  int64_t tBeforeAllProcessing = 0;
  GET_PERF_TIME(tBeforeAllProcessing);  

  switch(cMsgType)
  {
    case 'I':
    case 'T':
       HandleMBTrade(GenOrd, IsIOC, IsDQ, recvTimeStamp);
      break;
    case 'N':
      AddMBOrderTrim(GenOrd, IsIOC, IsDQ, recvTimeStamp);
      break;
    case 'M':
      ModMBOrderTrim(GenOrd, IsIOC, IsDQ, recvTimeStamp);
      break;
    case 'X':
      CanMBOrderTrim(GenOrd,recvTimeStamp);
      break;
    default:
    {
      
    }
  }  
   
  int64_t tAfterAllProcessing = 0;
  GET_PERF_TIME(tAfterAllProcessing);

  tAfterAllProcessing -= tBeforeAllProcessing;
  snprintf(logBuf, 500, "Thread_ME|MB_LATENCY|ORDER|tAfterAllProcessing %ld|SeqNo %d",(int64_t)tAfterAllProcessing,GenOrd->header.nSeqNo);
  Logger::getLogger().log(DEBUG, logBuf);
    
}

int ValidateAddMBReq(int iOrderSide, int32_t&  Token, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;
    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        Token = Token - FOOFFSET; 
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }

//     if (iOrderSide == 1 && ME_MB_OrderBook.OrderBook[tokIndex].BuyRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
//    else if (iOrderSide == 2 && ME_MB_OrderBook.OrderBook[tokIndex].SellRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
    
    if (iOrderSide == 1 && ME_MB_OrderBook.OrderBook[tokIndex].BuyRecords >= (ME_MB_OrderBook.OrderBook[tokIndex].BuyBookSize - 1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    else if (iOrderSide == 2 && ME_MB_OrderBook.OrderBook[tokIndex].SellRecords >= (ME_MB_OrderBook.OrderBook[tokIndex].SellBookSize - 1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    
    return errCode;
}
int HandleMBTrade(GENERIC_ORD_MSG *TradeMBOrder, int IsIOC, int IsDQ, int64_t recvTime)
{
  
  ORDER_BOOK_DTLS_MB bookdetails;
  long datareturn;
  int tokenIndex = 0;
  CONNINFO* pConnInfo=NULL;
  int FD=0;
  
  int Token = TradeMBOrder->nToken;
  double dOrderNo = TradeMBOrder->dblOrdID;
  if(TradeMBOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  std::cout<<"OrderId::"<<(int64_t)dOrderNo<<std::endl;
  int ErrorCode = ValidateModMBReq(dOrderNo, bookdetails.BuySellIndicator, Token, tokenIndex);
  
  

  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|MB_TrdaeMBOrderTrim|SeqNo %d|ErrorCode %d",TradeMBOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  
  if(MBTradeHandlingSeqNo==0)
  {
    OrderNumberMBTrade = TradeMBOrder->dblOrdID;
    bookdetails.OrderNo = TradeMBOrder->dblOrdID;
    bookdetails.lQty = TradeMBOrder->nQty;
    
    datareturn = TradeQuantityModificationMB(&bookdetails,bookdetails.BuySellIndicator,Token ,IsIOC, tokenIndex);
    
    MBTradeHandlingSeqNo = 1;
  }
  else
  {
    
    bookdetails.OrderNo = TradeMBOrder->dblOrdID;
    
    bookdetails.lQty = TradeMBOrder->nQty;
    
    datareturn = TradeQuantityModificationMB(&bookdetails,bookdetails.BuySellIndicator,Token ,IsIOC, tokenIndex);
    MBTradeHandlingSeqNo=2;
    
  }
  
  
  
  int32_t epochTime = getEpochTime();
  if (true == bEnableBrdcst &&( TradeMBOrder->cMsgType=='I' || MBTradeHandlingSeqNo==2))
  {
    FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
    /*IOC tick: set order no zero for IOC orders*/
    if(TradeMBOrder->cMsgType=='I')
    {
      if(TradeMBOrder->cOrdType=='B')
      {
        FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = TradeMBOrder->dblOrdID;
      }
      else
      {
        FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0;
      }
      
      if(TradeMBOrder->cOrdType=='S')
      {
        FillData.stBcastMsg.stTrdMsg.dblSellOrdID = TradeMBOrder->dblOrdID;
      }
      else
      {
        FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0;
      }
    }
    
    if(TradeMBOrder->cMsgType=='T')
    {
      FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = OrderNumberMBTrade;
      
      FillData.stBcastMsg.stTrdMsg.dblSellOrdID = TradeMBOrder->dblOrdID;
    } 
   
    FillData.stBcastMsg.stTrdMsg.nToken = TradeMBOrder->nToken ;
    FillData.stBcastMsg.stTrdMsg.nTradePrice = TradeMBOrder->nPrice;
    FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeMBOrder->nQty;
    FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
    Inqptr_METoBroadcast_Global->enqueue(FillData);
    
    MBTradeHandlingSeqNo=0;
    datareturn = MatchingBookBuilder(TradeMBOrder->nToken,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
  }
  
}

long TradeQuantityModificationMB(ORDER_BOOK_DTLS_MB *Mybookdetails, int16_t BuySellSide, int32_t Token,int IsIOC, int tokenIndex) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iOldOrdLocn = -1;
    int32_t iNewOrdLocn = -1;
    int32_t iLogRecs = 0;
    GET_PERF_TIME(t1);    
    long ret = 0;
    
    if(BuySellSide == 1)
    {
      
        iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords;
        
        
        
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)  //Search for the OrderNo
        {   
          
             if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
               
              GET_PERF_TIME(t2);          
              iOldOrdLocn = j;
                int64_t BuyOrderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo;
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty > Mybookdetails->lQty && ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty != Mybookdetails->lQty)
                {
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty - Mybookdetails->lQty;
                  int BuyQtyRem = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty;
                  snprintf(logBuf, 200, "Thread_ME|Order Partially Traded|OrderNo: %ld|RemQty: %d",BuyOrderNo,BuyQtyRem);
                  Logger::getLogger().log(DEBUG, logBuf);
                }
                else
                {
                  
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ =0 ;
                  IsIOC = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = 0;
                  
                  snprintf(logBuf, 200, "Thread_ME|Order Fully Traded|OrderNo: %ld|RemQty: 0",BuyOrderNo);
                  Logger::getLogger().log(DEBUG, logBuf);
                  memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j])*(ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords-j-1));
                  GET_PERF_TIME(t3);
                  ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords = ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords - 1; 
              
                }
                //memcpy for sending book to sendCancellation(), if required.
                memcpy(Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j]));
            
              }
        }
       GET_PERF_TIME(t4);
    }
    else {
      
      iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords;
        if ((Mybookdetails->lPrice > ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice) && 1 == Mybookdetails->IsIOC) {
           ret = 5; /*5 means cancel IOC */
        }           
        
        for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords) ; j++)
        {   
             if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
               GET_PERF_TIME(t2);        
                iOldOrdLocn = j;
                int64_t SellOrderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo;
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty > Mybookdetails->lQty && ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty != Mybookdetails->lQty)
                {
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty - Mybookdetails->lQty;
                  int SellQtyRem = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty;
                  
                  snprintf(logBuf, 200, "Thread_ME|Order Partially Traded|OrderNo: %ld|RemQty: %d",SellOrderNo,SellQtyRem);
                  Logger::getLogger().log(DEBUG, logBuf);
                }
                else
                {
                  
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = 2147483647;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ =0;
                  IsIOC = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo = 0;
                  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = 0;
                  
                  snprintf(logBuf, 200, "Thread_ME|Order Fully Traded|OrderNo: %ld|RemQty: 0",SellOrderNo);
                  Logger::getLogger().log(DEBUG, logBuf);
                  
                  memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j])*(ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords-j-1));
                  GET_PERF_TIME(t3);
                  ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords = ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords - 1; 
                }
                memcpy (Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]));
          
            
            break;
          }
        }
        
        GET_PERF_TIME(t4);
    }
    
    
    // End Enqueue Broadcast Packet      
    snprintf(logBuf, 200, "Thread_ME|TradeHandletoorderbook|Recs %6d|Search=%6ld|PositionSearch=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret; 
}


int AddMBOrderTrim(GENERIC_ORD_MSG *AddMBOrder, int IsIOC, int IsDQ, int64_t recvTime)
{
//  int32_t OrderNumber = ME_OrderNumber++;
  ORDER_BOOK_DTLS_MB bookdetails;
  int tokenIndex = 0;
  long datareturn;
  CONNINFO* pConnInfo=NULL;
  int FD=0;
  
  if(AddMBOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  int ErrorCode = ValidateAddMBReq(bookdetails.BuySellIndicator, AddMBOrder->nToken, tokenIndex);
  
  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|MB_AddMBOrderTrim|SeqNo %d|ErrorCode %d",AddMBOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo =  (int64_t)AddMBOrder->dblOrdID;
  bookdetails.lPrice = AddMBOrder->nPrice;
  bookdetails.lQty = AddMBOrder->nQty;
  
  bookdetails.IsIOC = IsIOC;
  bookdetails.IsDQ = IsDQ;
  snprintf(logBuf, 500, "Thread_ME|ADD ORDER MB |Order# %ld|COrd# %ld|IOC %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)AddMBOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC,  bookdetails.lQty, bookdetails.lPrice,AddMBOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
    
  Logger::getLogger().log(DEBUG, logBuf);
   
  // search TokenIdx
  int64_t beforeOrderBook = 0;
  int64_t afterOrderBook = 0;
  GET_PERF_TIME(beforeOrderBook);
  datareturn = AddMBOrdertoorderbook(&bookdetails, bookdetails.BuySellIndicator , IsIOC, IsDQ, tokenIndex);
  GET_PERF_TIME(afterOrderBook);
  afterOrderBook -=beforeOrderBook;

  
   

  GET_PERF_TIME(beforeMatchingBookBuilder);
  datareturn = MatchingBookBuilder(AddMBOrder->nToken,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
  GET_PERF_TIME(afterMatchingBookBuilder);
  afterMatchingBookBuilder -=beforeMatchingBookBuilder;
  snprintf(logBuf, 500, "Thread_ME|MB_AddMatchingBookBuilder|ORDER|afterOrderBook %ld , afterMatchingBookBuilder %ld",(int64_t)afterOrderBook,(int64_t)afterMatchingBookBuilder);
  Logger::getLogger().log(DEBUG, logBuf);
}

int ValidateModMBReq(double dOrderNo, int iOrderSide, int32_t& Token, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;

    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        Token = Token - FOOFFSET;
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokIndex].BuyRecords ) ; j++)
        {   
          
             if(ME_MB_OrderBook.OrderBook[tokIndex].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_MB_OrderBook.OrderBook[tokIndex].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokIndex].SellRecords ) ; j++)
        {   
           
             if(ME_MB_OrderBook.OrderBook[tokIndex].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_MB_OrderBook.OrderBook[tokIndex].Sell[j].lQty == 0) 
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
long ModMBOrdertoorderbook(ORDER_BOOK_DTLS_MB *Mybookdetails, int16_t BuySellSide, int32_t Token,int IsIOC,int IsDQ, int tokenIndex) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iOldOrdLocn = -1;
    int32_t iNewOrdLocn = -1;
    int32_t iLogRecs = 0;
    GET_PERF_TIME(t1);    
    long ret = 0;
    
    if(BuySellSide == 1)
    {
      
      
        iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords;
        if ((Mybookdetails->lPrice < ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice) && 1 == Mybookdetails->IsIOC) {
            ret = 5; /* 5  means cancel IOC Order*/
        }
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "ModtoMBorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "ModtoMBorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }    
        #endif
        
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)  //Search for the OrderNo
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "ModtoMBorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
               
            GET_PERF_TIME(t2);          
            iOldOrdLocn = j;
                ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = Mybookdetails->lPrice;
                ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = Mybookdetails->lQty;
                ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = Mybookdetails->IsIOC;
                ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = GlobalSeqNo++;
                //memcpy for sending book to sendCancellation(), if required.
                memcpy(Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "ModtoMBorderbook|Buy|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice >= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Buy|4|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Buy|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
             }
                else
                {
            break; // Added for TC
         }
              }

              for(; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Buy|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Buy|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn+1]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
    }  
    else
    {
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Buy|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
                GET_PERF_TIME(t3);
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
            }
            
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "ModtoMBorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }
        #endif
        GET_PERF_TIME(t4);
    }
    else {
      
        iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords;
        if ((Mybookdetails->lPrice > ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice) && 1 == Mybookdetails->IsIOC) {
           ret = 5; /*5 means cancel IOC */
        }           
          
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "ModtoMBorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "ModtoMBorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords) ; j++)
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "ModtoMBorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty);          
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
               GET_PERF_TIME(t2);        
                iOldOrdLocn = j;
                //memcpy(&ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j],Mybookdetails , sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]));
                ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = Mybookdetails->lPrice;
                ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = Mybookdetails->lQty;
                ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = Mybookdetails->IsIOC;
                ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = GlobalSeqNo++;
                memcpy (Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "ModtoMBorderbook|Sell|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            
            if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice <= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Sell|4|j %d|NextPrice[% d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Sell|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                break; // TC
             }   
         }

              for(; j<ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Sell|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
    }    
                else
    /*IOC tick: Do not brdcst for IOC orders*/
    {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Sell|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn+1]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            else
          {
              //for(; j<ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords-1; j--)
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "ModtoMBorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
                }
                else    
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "ModtoMBorderbook|Sell|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "ModtoMBorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        GET_PERF_TIME(t4);
    }
    
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC)) {
        AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
        AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'M';
        if (BuySellSide == 1) {
            AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
        } else {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;   
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet      
    snprintf(logBuf, 200, "Thread_ME|ModtoMBorderbook|Recs %6d|Search=%6ld|PositionSearch=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret; 
}

int ModMBOrderTrim(GENERIC_ORD_MSG *ModMBOrder, int IsIOC, int IsDQ, int64_t recvTime)
{
  int64_t tAfterEnqueue = 0;
  GET_PERF_TIME(tAfterEnqueue);
  tAfterEnqueue -= recvTime;

  ORDER_BOOK_DTLS_MB bookdetails;
  int MyTime = GlobalSeqNo++;   
  int i = 0;
  long Token = 0;
  int tokenIndex = 0;
  
  double dOrderNo = ModMBOrder->dblOrdID;
  CONNINFO* pconnInfo=NULL;
  int FD=0;
  if(ModMBOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  int ErrorCode = ValidateModMBReq(dOrderNo, bookdetails.BuySellIndicator, ModMBOrder->nToken, tokenIndex);
  
  

  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|MB_ModMBOrderTrim|SeqNo %d|ErrorCode %d",ModMBOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo =  ModMBOrder->dblOrdID;
  bookdetails.lPrice = ModMBOrder->nPrice;
  bookdetails.lQty = ModMBOrder->nQty;

  bookdetails.IsIOC = 0;
  bookdetails.IsDQ = 0;
  if (1== IsIOC){
     bookdetails.IsIOC = 1;
  }
 
  snprintf(logBuf, 500, "Thread_ME|MOD ORDER MB |Order# %ld|COrd# %ld|IOC %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)ModMBOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC,  bookdetails.lQty, bookdetails.lPrice,ModMBOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
 
  Logger::getLogger().log(DEBUG, logBuf);

  int64_t tAfterLog = 0;
  GET_PERF_TIME(tAfterLog);
  tAfterLog -= recvTime;
  
  int64_t beforeOrderBook = 0;
  int64_t afterOrderBook = 0;
  GET_PERF_TIME(beforeOrderBook);
  long datareturn = ModMBOrdertoorderbook(&bookdetails,bookdetails.BuySellIndicator,ModMBOrder->nToken ,IsIOC,IsDQ, tokenIndex);
  GET_PERF_TIME(afterOrderBook);
  afterOrderBook -=beforeOrderBook;
  
  
  int64_t beforeMatchingBookBuilder = 0;
  int64_t afterMatchingBookBuilder = 0;
  GET_PERF_TIME(beforeMatchingBookBuilder);
  
  datareturn = MatchingBookBuilder(ModMBOrder->nToken,FD,IsIOC,IsDQ,pconnInfo, tokenIndex);
  
  GET_PERF_TIME(afterMatchingBookBuilder);
  
  snprintf(logBuf, 500, "Thread_ME|MB_MoDMatchingBookBuilder|ORDER|afterOrderBook %ld afterMatchingBookBuilder %ld BeforeMatchingBookBuilder %ld Diff %ld",(int64_t)afterOrderBook,afterMatchingBookBuilder,beforeMatchingBookBuilder,(int64_t)(afterMatchingBookBuilder -beforeMatchingBookBuilder));
  Logger::getLogger().log(DEBUG, logBuf);
  
  
}

long AddMBOrdertoorderbook(ORDER_BOOK_DTLS_MB * Mybookdetails, int16_t BuySellSide, int IsIOC, int IsDQ, int tokenIndx ) // 1 Buy , 2 Sell
{
    // Enqueue Broadcast Packet 
   /*IOC tick: added IOC!=1 check*/
    int64_t t1=0,t2=0,t3=0,t4=0;
    GET_PERF_TIME(t1);
    int32_t iNewOrdLocn = 0;
    int32_t iLogRecs = 0;
    
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC))
    {
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
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = TokenStore[tokenIndx];   
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = TokenStore[tokenIndx]; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
          
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet               
    
    if (BuySellSide == 1) {
      iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords;
        // Start Handling IOC Order -------------------------------------------------
           if(Mybookdetails->lPrice < ME_MB_OrderBook.OrderBook[tokenIndx].Sell[0].lPrice &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }    
        // End Handling IOC Order -------------------------------------------------        
      
               
          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC; 
          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty;
          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
          ME_MB_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_MB_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;

          ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          
          ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
          
          /*Dynamic Book size allocation*/
           if(ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords  > (ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS_MB *temp ;
              temp = ME_MB_OrderBook.OrderBook[tokenIndx].Buy;
              int Booksize = ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize;
              try
              {
                ME_MB_OrderBook.OrderBook[tokenIndx].Buy = new ORDER_BOOK_DTLS_MB[2*Booksize];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|AddMBtoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE(next order of this token will be rejected)",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookFull = true;
              }
              if(false == ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookFull)
              {
                fillMBDataBuy(0,Booksize*2,ME_MB_OrderBook.OrderBook[tokenIndx].Buy);
                memcpy(ME_MB_OrderBook.OrderBook[tokenIndx].Buy,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize = Booksize*2;
                snprintf(logBuf, 500, "Thread_ME|AddMBtoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_MB_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
           
           memcpy(Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1]), sizeof (ME_MB_OrderBook.OrderBook[tokenIndx].Buy[ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1]));
      
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "AddtoMBorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);

            for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "AddtoMBorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif
            
              for(int j=ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "AddtoMBorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                
                if(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }
            
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "AddtoMBorderbook|Buy|9|OldLcn %d|NewLcn %d", ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
            
              GET_PERF_TIME(t3);
//              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]));                
        
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "AddtoMBorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);;
            }
            #endif 

           GET_PERF_TIME(t4);
//           if (ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|AddtoMBorderbook|Token %d|BuyOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].BuyRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//           }
//            if (dealerIndx != -1)
//            {
//               dealerOrdArr[dealerIndx][tokenIndx].buyordercnt++;
//            }

     }  
    else {
        iLogRecs = ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords;
        
        // Start Handling IOC Order -------------------------------------------------
           if(((Mybookdetails->lPrice) > ME_MB_OrderBook.OrderBook[tokenIndx].Buy[0].lPrice) &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }           

          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC ; 
          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty;
          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
          ME_MB_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_MB_OrderBook.OrderBook[tokenIndx].SellSeqNo + 1;

          ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords = ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          
          /*Dynamic Book size allocation*/
           if(ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords  > (ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS_MB *temp ;
              temp = ME_MB_OrderBook.OrderBook[tokenIndx].Sell;
              int Booksize = ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize;
              try
              {
                ME_MB_OrderBook.OrderBook[tokenIndx].Sell = new ORDER_BOOK_DTLS_MB[Booksize*2];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|AddMBtoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE(next order of this token will be rejected)",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords,ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_MB_OrderBook.OrderBook[tokenIndx].SellBookFull = true;
              }
              
              if(false == ME_MB_OrderBook.OrderBook[tokenIndx].SellBookFull)
              {
                fillMBDataSell(0,Booksize*2,ME_MB_OrderBook.OrderBook[tokenIndx].Sell);

                memcpy(ME_MB_OrderBook.OrderBook[tokenIndx].Sell,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize = Booksize*2;

                snprintf(logBuf, 500, "Thread_ME|AddMBtoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords,ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_MB_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
          
        memcpy(Mybookdetails, &(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1]), sizeof (ME_MB_OrderBook.OrderBook[tokenIndx].Sell[ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1]));        
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "AddtoMBorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);
            for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "AddtoMBorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif  
          
              for(int j=ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "AddtoMBorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice > Mybookdetails->lPrice)                  
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }

              #ifdef __LOG_ORDER_BOOK__            
              snprintf(logBuf, 500, "AddtoMBorderbook|Sell|9|OldLcn %d|NewLcn %d", ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif

             GET_PERF_TIME(t3);
              //memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memmove(&(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memcpy(&(ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_MB_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]));                
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "AddtoMBorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            #endif
          GET_PERF_TIME(t4);
//          if (ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|AddtoMBorderbook|Token %d|SellOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_MB_OrderBook.OrderBook[tokenIndx].SellRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//            }
//            if (dealerIndx != -1)
//            {
//                dealerOrdArr[dealerIndx][tokenIndx].sellordercnt++;
//             }
          
     }    
    snprintf(logBuf, 200, "Thread_ME|AddtoMBorderbook|Recs %6d|Search=%6ld|Position Search=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
      
    return 0; 
}
int ValidateCanMBReq(double dOrderNo, int iOrderSide, int32_t& Token, int& tokenIndex)
{
  /*Check if Token is subscribed*/
  int errCode = 0;
  bool orderFound = false;
  bool found = true;
  
  if (_nSegMode == SEG_NSECM)
  {
    found = binarySearch(TokenStore, TokenCount, Token, &tokenIndex);
  }
  else if (_nSegMode == SEG_NSEFO)
  {
    Token = Token - FOOFFSET;
      found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokenIndex);
  }
  if (found == false)
  {
      errCode = ERR_SECURITY_NOT_AVAILABLE;
      return errCode;
  }

   if(iOrderSide == 1)
  {
  
      for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokenIndex].BuyRecords ) ; j++)
      {   
           if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == dOrderNo)
           {
               orderFound = true;
               /*filled order*/
               if (ME_MB_OrderBook.OrderBook[tokenIndex].Buy[j].lQty == 0) 
               {
                   errCode = ERR_MOD_CAN_REJECT;
               }
               
           }   
       }
  }  
  else
  {
       for(int j = 0 ; j < (ME_MB_OrderBook.OrderBook[tokenIndex].SellRecords ) ; j++)
      {   
           if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo ==  dOrderNo)
           {
               orderFound = true;
               /*filled order*/
               if (ME_MB_OrderBook.OrderBook[tokenIndex].Sell[j].lQty == 0) 
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
long CanMBOrdertoorderbook(ORDER_BOOK_DTLS_MB * Mybookdetails, int16_t BuySellSide, int32_t Token,int32_t tokIndx)
{
  int64_t t1=0,t2=0,t3=0,t4=0;
  int32_t iLogRecs = 0;

  GET_PERF_TIME(t1);
  long lPrice = 0, lQty = 0;
  short IsIOC = 0;
  int16_t transCode = 0;
  if (BuySellSide == 1) {
    iLogRecs = ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords;
    #ifdef __LOG_ORDER_BOOK__  
    snprintf(logBuf, 500, "CantoMBorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
      Logger::getLogger().log(DEBUG, logBuf);
      for(int j=0; j<ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
  {
        snprintf(logBuf, 500, "CantoMBorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
      }  
    #endif    
      for (int j = 0; j < (ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords); j++) {

        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "CantoMBorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lQty);
        Logger::getLogger().log(DEBUG, logBuf);
        #endif
           if(ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo == Mybookdetails->OrderNo)
           {
              lPrice =  ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lPrice;
              lQty =  ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lQty;
              ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lPrice = 0;
              
              IsIOC = ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC;
              ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC = 0;
              
              ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo = 0;
              ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].SeqNo = 0;
              ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lQty = 0;
            
              GET_PERF_TIME(t2);
              //SortBuySideBook(tokIndx);
              memmove(&(ME_MB_OrderBook.OrderBook[tokIndx].Buy[j]), &(ME_MB_OrderBook.OrderBook[tokIndx].Buy[j+1]), sizeof(ME_MB_OrderBook.OrderBook[tokIndx].Buy[j])*(ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords-j-1));
              GET_PERF_TIME(t3);
              ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords = ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords - 1; 
              
              break; // TC
           }   
       }
      #ifdef __LOG_ORDER_BOOK__
      for(int j=0; j<ME_MB_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
      {
        snprintf(logBuf, 500, "CantoMBorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_MB_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
  }  
      #endif
  }
  else {
      iLogRecs = ME_MB_OrderBook.OrderBook[tokIndx].SellRecords;
      #ifdef __LOG_ORDER_BOOK__  
      snprintf(logBuf, 500, "CantoMBorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_MB_OrderBook.OrderBook[tokIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
      Logger::getLogger().log(DEBUG, logBuf);
      for(int j=0; j<ME_MB_OrderBook.OrderBook[tokIndx].SellRecords; j++)
  {
        snprintf(logBuf, 500, "CantoMBorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
      }  
      #endif      
      for (int j = 0; j < (ME_MB_OrderBook.OrderBook[tokIndx].SellRecords); j++) {

        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "CantoMBorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lQty);
        Logger::getLogger().log(DEBUG, logBuf);
        #endif
           if(ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo == Mybookdetails->OrderNo)
           {
              lPrice =  ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lPrice;
              lQty =  ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lQty;
              ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lPrice = 2147483647;
              IsIOC = ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC;
              ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC = 0;
              
              ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo = 0;
              ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].SeqNo = 0;
              ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lQty = 0;
              GET_PERF_TIME(t2);
              //SortSellSideBook(tokIndx);
              memmove(&(ME_MB_OrderBook.OrderBook[tokIndx].Sell[j]), &(ME_MB_OrderBook.OrderBook[tokIndx].Sell[j+1]), sizeof(ME_MB_OrderBook.OrderBook[tokIndx].Sell[j])*(ME_MB_OrderBook.OrderBook[tokIndx].SellRecords-j-1));
              GET_PERF_TIME(t3);
              ME_MB_OrderBook.OrderBook[tokIndx].SellRecords = ME_MB_OrderBook.OrderBook[tokIndx].SellRecords - 1;
              
              break; // TC
           }   
       }
      #ifdef __LOG_ORDER_BOOK__
      for(int j=0; j<ME_MB_OrderBook.OrderBook[tokIndx].SellRecords; j++)
      {
        snprintf(logBuf, 500, "CantoMBorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_MB_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
  }    
      #endif
  }
  // Enqueue Broadcast Packet 
  /*IOC tick: Do not brdcast for IOC orders*/
  if (true == bEnableBrdcst && (1 != IsIOC || (1== IsIOC && transCode != NSECM_ADD_REQ_TR)))
  {
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
        AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = lPrice;
        AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = lQty;
        AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
        if (_nSegMode == SEG_NSEFO){
           AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
        }
        AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
        Inqptr_METoBroadcast_Global->enqueue(AddModCan);
  }
  // End Enqueue Broadcast Packet    
  snprintf(logBuf, 200, "Thread_ME|CantoMBorderbook|Search=%ld|Sort=%ld", t2-t1,t3-t2);
  Logger::getLogger().log(DEBUG, logBuf);
}
int CanMBOrderTrim(GENERIC_ORD_MSG *CanMBOrder, int64_t recvTime)
{
  ORDER_BOOK_DTLS_MB bookdetails;
  int tokenIndex;
  double dOrderNo = CanMBOrder->dblOrdID;
  if(CanMBOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  
  int32_t ErrorCode = ValidateCanMBReq( dOrderNo, bookdetails.BuySellIndicator,CanMBOrder->nToken, tokenIndex);
 
  
  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|MB_CanMBOrderTrim|SeqNo %d|ErrorCode %d",CanMBOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo = CanMBOrder->dblOrdID;
  
  bookdetails.lPrice = CanMBOrder->nPrice;
  bookdetails.lQty = CanMBOrder->nQty;
  bookdetails.IsIOC=0;
  bookdetails.IsDQ=0;
 
  snprintf(logBuf, 500, "Thread_ME|CXL ORDER MB |Order# %d|COrd# %d|IOC %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)CanMBOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC, bookdetails.lQty, bookdetails.lPrice,CanMBOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
  Logger::getLogger().log(DEBUG, logBuf);
 
  long datareturn = CanMBOrdertoorderbook(&bookdetails,bookdetails.BuySellIndicator,CanMBOrder->nToken , tokenIndex);

 
}

/*Market book handling ends here*/

/*Add PFS ORDER TRIM STARTS (NK)*/

int handlePFSMsg(GENERIC_ORD_MSG* GenOrd)
{
  //switch()
  int16_t IsIOC=0,IsDQ=0;
  int64_t recvTimeStamp=0;
  char cMsgType = GenOrd->cMsgType;

  int64_t tBeforeAllProcessing = 0;
  GET_PERF_TIME(tBeforeAllProcessing);  

  switch(cMsgType)
  {
    case 'I':
      IsIOC = 1;
      GenOrd->cMsgType='N'; // Should not be required
    case 'N':
      AddPFSOrderTrim(GenOrd, IsIOC, IsDQ, recvTimeStamp);
      break;
    case 'M':
      ModPFSOrderTrim(GenOrd, IsIOC, IsDQ, recvTimeStamp);
      break;
    case 'X':
      CanPFSOrderTrim(GenOrd,recvTimeStamp);
      break;
    default:
    {
      
    }
  }  
   
  int64_t tAfterAllProcessing = 0;
  GET_PERF_TIME(tAfterAllProcessing);

  tAfterAllProcessing -= tBeforeAllProcessing;
  snprintf(logBuf, 500, "Thread_ME|PFS_LATENCY|ORDER|tAfterAllProcessing %ld|SeqNo %d",(int64_t)tAfterAllProcessing,GenOrd->header.nSeqNo);
  Logger::getLogger().log(DEBUG, logBuf);
    
}

int ValidateAddPFSReq(int iOrderSide, int32_t&  Token, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;
    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        Token = Token - FOOFFSET; 
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }

//     if (iOrderSide == 1 && ME_OrderBook.OrderBook[tokIndex].BuyRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
//    else if (iOrderSide == 2 && ME_OrderBook.OrderBook[tokIndex].SellRecords >= BOOKSIZE)
//    {
//       errCode = BOOK_SIZE_CROSSED;
//       return errCode; 
//    }
    
    if (iOrderSide == 1 && ME_OrderBook.OrderBook[tokIndex].BuyRecords >= (ME_OrderBook.OrderBook[tokIndex].BuyBookSize - 1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    else if (iOrderSide == 2 && ME_OrderBook.OrderBook[tokIndex].SellRecords >= (ME_OrderBook.OrderBook[tokIndex].SellBookSize - 1) )
    {
       errCode = MEMORY_NOT_AVAILABLE;
       return errCode; 
    }
    
    return errCode;
}


int AddPFSOrderTrim(GENERIC_ORD_MSG *AddPFSOrder, int IsIOC, int IsDQ, int64_t recvTime)
{
//  int32_t OrderNumber = ME_OrderNumber++;
  ORDER_BOOK_DTLS bookdetails;
  int tokenIndex = 0;
  long datareturn;
  CONNINFO* pConnInfo=NULL;
  int FD=0;
  
  if(AddPFSOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  int ErrorCode = ValidateAddPFSReq(bookdetails.BuySellIndicator, AddPFSOrder->nToken, tokenIndex);
  
  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|PFS_AddPFSOrderTrim|SeqNo %d|ErrorCode %d",AddPFSOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo =  (int32_t)AddPFSOrder->dblOrdID;
  bookdetails.lPrice = AddPFSOrder->nPrice;
  bookdetails.lQty = AddPFSOrder->nQty;
  
  bookdetails.IsIOC = IsIOC;
  bookdetails.IsDQ = IsDQ;

  // search TokenIdx
  int64_t beforeOrderBook = 0;
  int64_t afterOrderBook = 0;
  GET_PERF_TIME(beforeOrderBook);
  datareturn = AddPFSOrdertoorderbook(&bookdetails, bookdetails.BuySellIndicator , IsIOC, IsDQ, tokenIndex);
  GET_PERF_TIME(afterOrderBook);
  afterOrderBook -=beforeOrderBook;
  snprintf(logBuf, 500, "Thread_ME|PFS_AddOrderBookLATENCY|ORDER|afterOrderBook %ld",(int64_t)afterOrderBook);
  Logger::getLogger().log(DEBUG, logBuf);
  
  snprintf(logBuf, 500, "Thread_ME|ADD ORDER PFS |Order# %ld|COrd# %ld|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)AddPFSOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice,AddPFSOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
    
  Logger::getLogger().log(DEBUG, logBuf);
    

  GET_PERF_TIME(beforeMatching);
  datareturn = Matching(AddPFSOrder->nToken,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
  GET_PERF_TIME(afterMatching);
  afterMatching -=beforeMatching;
  snprintf(logBuf, 500, "Thread_ME|PFS_AddMatching|ORDER|afterMatching %ld",(int64_t)afterMatching);
  Logger::getLogger().log(DEBUG, logBuf);
}

int ValidateModPFSReq(double dOrderNo, int iOrderSide, int32_t& Token, int& tokIndex)
{
    int errCode = 0;
    bool orderFound = false;

    
    /*Check if Token is subscribed*/
    bool found = true;
    if (_nSegMode == SEG_NSECM)
    {
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        Token = Token - FOOFFSET;
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokIndex].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokIndex].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokIndex].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokIndex].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokIndex].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokIndex].Sell[j].lQty == 0) 
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
long ModPFSOrdertoorderbook(ORDER_BOOK_DTLS *Mybookdetails, int16_t BuySellSide, int32_t Token,int IsIOC,int IsDQ, int tokenIndex) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iOldOrdLocn = -1;
    int32_t iNewOrdLocn = -1;
    int32_t iLogRecs = 0;
    GET_PERF_TIME(t1);    
    long ret = 0;
    
    if(BuySellSide == 1)
    {
      
      
      iLogRecs = ME_OrderBook.OrderBook[tokenIndex].BuyRecords;
        if ((Mybookdetails->lPrice < ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice) && 1 == Mybookdetails->IsIOC) {
            ret = 5; /* 5  means cancel IOC Order*/
        }
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokenIndex].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }    
        #endif
        
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)  //Search for the OrderNo
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
               
            GET_PERF_TIME(t2);          
            iOldOrdLocn = j;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = Mybookdetails->lQty;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ = Mybookdetails->IsDQ;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].FD = Mybookdetails->FD;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].connInfo = Mybookdetails->connInfo;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].dealerID = Mybookdetails->dealerID;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].TraderId = Mybookdetails->TraderId;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].BookType = Mybookdetails->BookType;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].Volume = Mybookdetails->Volume;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].BranchId = Mybookdetails->BranchId;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].UserId = Mybookdetails->UserId;        
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                 if (_nSegMode == SEG_NSECM){
                     memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                     ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                }
                else {
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].LastModified = Mybookdetails->LastModified; 
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].TransactionCode = NSECM_MOD_REQ_TR; /*IOC tick*/
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = GlobalSeqNo++;
                //memcpy for sending book to sendCancellation(), if required.
                memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndex].Buy[j]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook|Buy|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice >= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|4|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
             }
                else
                {
            break; // Added for TC
         }
              }

              for(; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn]), &(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn+1]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
    }  
    else
    {
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
                GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
            }
            
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }
        #endif
        GET_PERF_TIME(t4);
    }
    else {
      
      iLogRecs = ME_OrderBook.OrderBook[tokenIndex].SellRecords;
        if ((Mybookdetails->lPrice > ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice) && 1 == Mybookdetails->IsIOC) {
           ret = 5; /*5 means cancel IOC */
        }           
          
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokenIndex].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].SellRecords) ; j++)
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty);          
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
               GET_PERF_TIME(t2);        
            iOldOrdLocn = j;
                //memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j],Mybookdetails , sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j]));
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = Mybookdetails->lQty;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ = Mybookdetails->IsDQ;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].FD = Mybookdetails->FD;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].connInfo = Mybookdetails->connInfo;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].dealerID = Mybookdetails->dealerID;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].TraderId = Mybookdetails->TraderId;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].BookType = Mybookdetails->BookType;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].Volume = Mybookdetails->Volume;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].BranchId = Mybookdetails->BranchId;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].UserId = Mybookdetails->UserId;        
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                 if (_nSegMode == SEG_NSECM){
                     ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;  
                     memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));                 
                }
                else {
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].LastModified = Mybookdetails->LastModified;  
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].TransactionCode = NSECM_MOD_REQ_TR;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = GlobalSeqNo++;
                memcpy (Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndex].Sell[j]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook|Sell|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            
            if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice <= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|4|j %d|NextPrice[% d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                break; // TC
             }   
         }

              for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
    }    
                else
    /*IOC tick: Do not brdcst for IOC orders*/
    {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn]), &(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn+1]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            else
          {
              //for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j--)
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
                }
                else    
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        GET_PERF_TIME(t4);
    }
    
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC)) {
        AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
        AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'M';
        if (BuySellSide == 1) {
            AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
        } else {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;   
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet      
    snprintf(logBuf, 200, "Thread_ME|Modtoorderbook|Recs %6d|Search=%6ld|PositionSearch=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret; 
}

int ModPFSOrderTrim(GENERIC_ORD_MSG *ModPFSOrder, int IsIOC, int IsDQ, int64_t recvTime)
{
  int64_t tAfterEnqueue = 0;
  GET_PERF_TIME(tAfterEnqueue);
  tAfterEnqueue -= recvTime;

  ORDER_BOOK_DTLS bookdetails;
  int MyTime = GlobalSeqNo++;   
  int i = 0;
  long Token = 0;
  int tokenIndex = 0;
  
  double dOrderNo = ModPFSOrder->dblOrdID;
  CONNINFO* pconnInfo=NULL;
  int FD=0;
  if(ModPFSOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  int ErrorCode = ValidateModPFSReq(dOrderNo, bookdetails.BuySellIndicator, ModPFSOrder->nToken, tokenIndex);
  
  

  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|PFS_ModPFSOrderTrim|SeqNo %d|ErrorCode %d",ModPFSOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo =  ModPFSOrder->dblOrdID;
  bookdetails.lPrice = ModPFSOrder->nPrice;
  bookdetails.lQty = ModPFSOrder->nQty;
  bookdetails.DQty = 0;
  bookdetails.IsIOC = 0;
  bookdetails.IsDQ = 0;
  if (1== IsIOC){
     bookdetails.IsIOC = 1;
  }
  if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
    bookdetails.IsDQ = 1;
  }

  int64_t tAfterLog = 0;
  GET_PERF_TIME(tAfterLog);
  tAfterLog -= recvTime;
  
  int64_t beforeOrderBook = 0;
  int64_t afterOrderBook = 0;
  GET_PERF_TIME(beforeOrderBook);
  long datareturn = ModPFSOrdertoorderbook(&bookdetails,bookdetails.BuySellIndicator,ModPFSOrder->nToken ,IsIOC,IsDQ, tokenIndex);
  GET_PERF_TIME(afterOrderBook);
  afterOrderBook -=beforeOrderBook;
  snprintf(logBuf, 500, "Thread_ME|PFS_ModOrderBookLATENCY|ORDER|afterOrderBook %ld",(int64_t)afterOrderBook);
  Logger::getLogger().log(DEBUG, logBuf);
  
  
//  snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER PFS|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d",
//     FD, OrderNumber,ModOrder->TransactionId,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, Token, __bswap_16(ModOrdResp.BuySellIndicator),
//     LMT); 
//   Logger::getLogger().log(DEBUG, logBuf);
  snprintf(logBuf, 500, "Thread_ME|MOD ORDER PFS |Order# %ld|COrd# %ld|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)ModPFSOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice,ModPFSOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
//  
//  snprintf(logBuf, 500, "Thread_ME|MOD ORDER PFS |Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d", 
//       ModPFSOrder->dblOrdID, bookdetails.OrderNo, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, Token, bookdetails.BuySellIndicator);
  
  Logger::getLogger().log(DEBUG, logBuf);
  int64_t beforeMatching = 0;
  int64_t afterMatching = 0;
  GET_PERF_TIME(beforeMatching);
  datareturn = Matching(ModPFSOrder->nToken,FD,IsIOC,IsDQ,pconnInfo, tokenIndex);
  GET_PERF_TIME(afterMatching);
  
  snprintf(logBuf, 500, "Thread_ME|PFS_MoDMatching|ORDER|afterMatching %ld BeforeMatching %ld Diff %ld",afterMatching,beforeMatching,(int64_t)(afterMatching -beforeMatching));
  Logger::getLogger().log(DEBUG, logBuf);
  
  
}

long AddPFSOrdertoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int16_t BuySellSide, int IsIOC, int IsDQ, int tokenIndx ) // 1 Buy , 2 Sell
{
    // Enqueue Broadcast Packet 
   /*IOC tick: added IOC!=1 check*/
    int64_t t1=0,t2=0,t3=0,t4=0;
    GET_PERF_TIME(t1);
    int32_t iNewOrdLocn = 0;
    int32_t iLogRecs = 0;
    
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC))
    {
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
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = TokenStore[tokenIndx];   
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = TokenStore[tokenIndx]; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
          
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet               
    
    if (BuySellSide == 1) {
      iLogRecs = ME_OrderBook.OrderBook[tokenIndx].BuyRecords;
        // Start Handling IOC Order -------------------------------------------------
           if(Mybookdetails->lPrice < ME_OrderBook.OrderBook[tokenIndx].Sell[0].lPrice &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }    
        // End Handling IOC Order -------------------------------------------------        
      
        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC; 
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
               ME_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
           }
            
           /*Sneha - multiple connection changes:15/07/16*/
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].Volume = Mybookdetails->Volume;

          if (_nSegMode == SEG_NSECM){
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info));
           }
          else {
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
//                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = TokenStore[tokenIndx];
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
           }
           ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].LastModified = Mybookdetails->LastModified; 
           ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].TransactionCode = NSECM_ADD_REQ_TR; /*IOC tick*/
           ME_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           
           /*Dynamic Book size allocation*/
           if(ME_OrderBook.OrderBook[tokenIndx].BuyRecords  > (ME_OrderBook.OrderBook[tokenIndx].BuyBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_OrderBook.OrderBook[tokenIndx].Buy;
              int Booksize = ME_OrderBook.OrderBook[tokenIndx].BuyBookSize;
              
              try
              {
                ME_OrderBook.OrderBook[tokenIndx].Buy = new ORDER_BOOK_DTLS[2*Booksize];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|AddPFStoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_OrderBook.OrderBook[tokenIndx].BuyBookFull = true;
              }
              if(false == ME_OrderBook.OrderBook[tokenIndx].BuyBookFull)
              {

                fillDataBuy(0,Booksize*2,ME_OrderBook.OrderBook[tokenIndx].Buy);
                memcpy(ME_OrderBook.OrderBook[tokenIndx].Buy,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;

                ME_OrderBook.OrderBook[tokenIndx].BuyBookSize = Booksize*2;
                snprintf(logBuf, 500, "Thread_ME|AddPFStoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
           
           memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1]), sizeof (ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1]));
      
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_OrderBook.OrderBook[tokenIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);

            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif
            
              for(int j=ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                
                if(ME_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }
            
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Addtoorderbook|Buy|9|OldLcn %d|NewLcn %d", ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
            
              GET_PERF_TIME(t3);
//              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memmove(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]));                
        
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);;
            }
            #endif 

           GET_PERF_TIME(t4);
//           if (ME_OrderBook.OrderBook[tokenIndx].BuyRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|BuyOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//           }
//            if (dealerIndx != -1)
//            {
//               dealerOrdArr[dealerIndx][tokenIndx].buyordercnt++;
//            }

     }  
    else {
        iLogRecs = ME_OrderBook.OrderBook[tokenIndx].SellRecords;
        
        // Start Handling IOC Order -------------------------------------------------
           if(((Mybookdetails->lPrice) > ME_OrderBook.OrderBook[tokenIndx].Buy[0].lPrice) &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }           

        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {    
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[tokenIndx].SellRecords = ME_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC ; 
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
              ME_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_OrderBook.OrderBook[tokenIndx].SellSeqNo + 1;
           }    
           /*Sneha - multiple connection changes:15/07/16*/
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TraderId = Mybookdetails->TraderId;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BookType = Mybookdetails->BookType;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].Volume = Mybookdetails->Volume;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BranchId = Mybookdetails->BranchId;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].UserId = Mybookdetails->UserId;        
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].ProClientIndicator = Mybookdetails->ProClientIndicator;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].NnfField = Mybookdetails->NnfField;  
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
//          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
//          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
//          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].FD = Mybookdetails->FD;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].connInfo = Mybookdetails->connInfo;
//          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].dealerID = Mybookdetails->dealerID;
            if (_nSegMode == SEG_NSECM){
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info));
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
           }
          else {
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags));
          }
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].LastModified = Mybookdetails->LastModified;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TransactionCode = NSECM_ADD_REQ_TR; /*IOC tick*/
          ME_OrderBook.OrderBook[tokenIndx].SellRecords = ME_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          
          /*Dynamic Book size allocation*/
           if(ME_OrderBook.OrderBook[tokenIndx].SellRecords  > (ME_OrderBook.OrderBook[tokenIndx].SellBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_OrderBook.OrderBook[tokenIndx].Sell;
              int Booksize = ME_OrderBook.OrderBook[tokenIndx].SellBookSize;
              try
              {
                ME_OrderBook.OrderBook[tokenIndx].Sell = new ORDER_BOOK_DTLS[Booksize*2];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|AddPFStoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords,ME_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_OrderBook.OrderBook[tokenIndx].SellBookFull = true;
              }
              if(false == ME_OrderBook.OrderBook[tokenIndx].SellBookFull)
              {
                fillDataSell(0,Booksize*2,ME_OrderBook.OrderBook[tokenIndx].Sell);

                memcpy(ME_OrderBook.OrderBook[tokenIndx].Sell,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_OrderBook.OrderBook[tokenIndx].SellBookSize = Booksize*2;

                snprintf(logBuf, 500, "Thread_ME|AddPFStoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords,ME_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
          
          
        memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords-1]), sizeof (ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords-1]));        
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_OrderBook.OrderBook[tokenIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif  
          
              for(int j=ME_OrderBook.OrderBook[tokenIndx].SellRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice > Mybookdetails->lPrice)                  
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }

              #ifdef __LOG_ORDER_BOOK__            
              snprintf(logBuf, 500, "Addtoorderbook|Sell|9|OldLcn %d|NewLcn %d", ME_OrderBook.OrderBook[tokenIndx].SellRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif

             GET_PERF_TIME(t3);
              //memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memmove(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]));                
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            #endif
          GET_PERF_TIME(t4);
//          if (ME_OrderBook.OrderBook[tokenIndx].SellRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|SellOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//            }
//            if (dealerIndx != -1)
//            {
//                dealerOrdArr[dealerIndx][tokenIndx].sellordercnt++;
//             }
          
     }    
    snprintf(logBuf, 200, "Thread_ME|Addtoorderbook|Recs %6d|Search=%6ld|Position Search=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
      
    return 0; 
}
int ValidateCanPFSReq(double dOrderNo, int iOrderSide, int32_t& Token, int& tokenIndex)
{
  /*Check if Token is subscribed*/
  int errCode = 0;
  bool orderFound = false;
  bool found = true;
  
  if (_nSegMode == SEG_NSECM)
  {
    found = binarySearch(TokenStore, TokenCount, Token, &tokenIndex);
  }
  else if (_nSegMode == SEG_NSEFO)
  {
    Token = Token - FOOFFSET;
      found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokenIndex);
  }
  if (found == false)
  {
      errCode = ERR_SECURITY_NOT_AVAILABLE;
      return errCode;
  }

   if(iOrderSide == 1)
  {
  
      for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].BuyRecords ) ; j++)
      {   
           if(ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == dOrderNo)
           {
               orderFound = true;
               /*filled order*/
               if (ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty == 0) 
               {
                   errCode = ERR_MOD_CAN_REJECT;
               }
               
           }   
       }
  }  
  else
  {
       for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].SellRecords ) ; j++)
      {   
           if(ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo ==  dOrderNo)
           {
               orderFound = true;
               /*filled order*/
               if (ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty == 0) 
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
long CanPFSOrdertoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int16_t BuySellSide, int32_t Token,int32_t tokIndx)
{
  int64_t t1=0,t2=0,t3=0,t4=0;
  int32_t iLogRecs = 0;

  GET_PERF_TIME(t1);
  long lPrice = 0, lQty = 0;
  short IsIOC = 0;
  int16_t transCode = 0;
  if (BuySellSide == 1) {
    iLogRecs = ME_OrderBook.OrderBook[tokIndx].BuyRecords;
    #ifdef __LOG_ORDER_BOOK__  
    snprintf(logBuf, 500, "Cantoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
      Logger::getLogger().log(DEBUG, logBuf);
      for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
  {
        snprintf(logBuf, 500, "Cantoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
      }  
    #endif    
      for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].BuyRecords); j++) {

        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Cantoorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty);
        Logger::getLogger().log(DEBUG, logBuf);
        #endif
           if(ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo == Mybookdetails->OrderNo)
           {
              lPrice =  ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice;
              lQty =  ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].DQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].IsDQ =0 ;
              IsIOC = ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].OpenQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].SeqNo = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].TTQ = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].LastModified = Mybookdetails->LastModified;
              transCode = ME_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode;
              ME_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode = NSECM_CAN_REQ_TR; /*IOC tick*/
              GET_PERF_TIME(t2);
              //SortBuySideBook(tokIndx);
              memmove(&(ME_OrderBook.OrderBook[tokIndx].Buy[j]), &(ME_OrderBook.OrderBook[tokIndx].Buy[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Buy[j])*(ME_OrderBook.OrderBook[tokIndx].BuyRecords-j-1));
              GET_PERF_TIME(t3);
              ME_OrderBook.OrderBook[tokIndx].BuyRecords = ME_OrderBook.OrderBook[tokIndx].BuyRecords - 1; 
              
              break; // TC
           }   
       }
      #ifdef __LOG_ORDER_BOOK__
      for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
      {
        snprintf(logBuf, 500, "Cantoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
  }  
      #endif
  }
  else {
      iLogRecs = ME_OrderBook.OrderBook[tokIndx].SellRecords;
      #ifdef __LOG_ORDER_BOOK__  
      snprintf(logBuf, 500, "Cantoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
      Logger::getLogger().log(DEBUG, logBuf);
      for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
  {
        snprintf(logBuf, 500, "Cantoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
      }  
      #endif      
      for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].SellRecords); j++) {

        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Cantoorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty);
        Logger::getLogger().log(DEBUG, logBuf);
        #endif
           if(ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo == Mybookdetails->OrderNo)
           {
              lPrice =  ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice;
              lQty =  ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice = 2147483647;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].DQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].IsDQ =0;
              IsIOC = ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].OpenQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].SeqNo = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].TTQ = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty = 0;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].LastModified = Mybookdetails->LastModified;
              transCode = ME_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode;
              ME_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode = NSECM_CAN_REQ_TR; /*IOC tick*/
              GET_PERF_TIME(t2);
              //SortSellSideBook(tokIndx);
              memmove(&(ME_OrderBook.OrderBook[tokIndx].Sell[j]), &(ME_OrderBook.OrderBook[tokIndx].Sell[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Sell[j])*(ME_OrderBook.OrderBook[tokIndx].SellRecords-j-1));
              GET_PERF_TIME(t3);
              ME_OrderBook.OrderBook[tokIndx].SellRecords = ME_OrderBook.OrderBook[tokIndx].SellRecords - 1;
              
              break; // TC
           }   
       }
      #ifdef __LOG_ORDER_BOOK__
      for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
      {
        snprintf(logBuf, 500, "Cantoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);;
  }    
      #endif
  }
  // Enqueue Broadcast Packet 
  /*IOC tick: Do not brdcast for IOC orders*/
  if (true == bEnableBrdcst && (1 != IsIOC || (1== IsIOC && transCode != NSECM_ADD_REQ_TR)))
  {
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
        AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = lPrice;
        AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = lQty;
        AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
        if (_nSegMode == SEG_NSEFO){
           AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
        }
        AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
        Inqptr_METoBroadcast_Global->enqueue(AddModCan);
  }
  // End Enqueue Broadcast Packet    
  snprintf(logBuf, 200, "Thread_ME|Cantoorderbook|Search=%ld|Sort=%ld", t2-t1,t3-t2);
  Logger::getLogger().log(DEBUG, logBuf);
}
int CanPFSOrderTrim(GENERIC_ORD_MSG *CanPFSOrder, int64_t recvTime)
{
  ORDER_BOOK_DTLS bookdetails;
  int tokenIndex;
  double dOrderNo = CanPFSOrder->dblOrdID;
  if(CanPFSOrder->cOrdType=='B')
  {
    bookdetails.BuySellIndicator=1;
  }
  else
  {
    bookdetails.BuySellIndicator=2;
  }
  
  int32_t ErrorCode = ValidateCanPFSReq( dOrderNo, bookdetails.BuySellIndicator,CanPFSOrder->nToken, tokenIndex);
 
  
  if(ErrorCode != 0)
  {
    snprintf(logBuf, 500, "Thread_ME|PFS_CanPFSOrderTrim|SeqNo %d|ErrorCode %d",CanPFSOrder->header.nSeqNo,ErrorCode);
    Logger::getLogger().log(DEBUG, logBuf);
    return 0;
  }
  bookdetails.OrderNo = CanPFSOrder->dblOrdID;
  
  bookdetails.lPrice = CanPFSOrder->nPrice;
  bookdetails.lQty = CanPFSOrder->nQty;
  bookdetails.IsIOC=0;
  bookdetails.IsDQ=0;
  bookdetails.DQty=0;
  long datareturn = CanPFSOrdertoorderbook(&bookdetails,bookdetails.BuySellIndicator,CanPFSOrder->nToken , tokenIndex);

 snprintf(logBuf, 500, "Thread_ME|CXL ORDER PFS |Order# %d|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %d|Side %d", 
       (int64_t)CanPFSOrder->dblOrdID, (int64_t)bookdetails.OrderNo, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice,CanPFSOrder->nToken+FOOFFSET, bookdetails.BuySellIndicator);
// std::cout<<logBuf<<std::endl; 
 Logger::getLogger().log(DEBUG, logBuf);
}

/*Add PFS ORDER TRIM ENDS (NK)*/



/*Non Trim SL ORDER STARTS (NK)*/
int AddOrderNonTrim(NSECM::MS_OE_REQUEST *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
//    int64_t tAfterEnqueue = getCurrentTimeInNano()-recvTime;
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    int MyTime = GlobalSeqNo++;
//    long OrderNumber = ME_OrderNumber++;
    int64_t OrderNumber = ME_OrderNumber++;
    
    
    
    ORDER_BOOK_DTLS bookdetails;
    long Token = 0;
    NSECM::MS_OE_RESPONSE_SL OrderResponse;
    int dealerIndex, tokenIndex;
    memcpy(&OrderResponse.tap_hdr,& AddOrder->msg_hdr, sizeof(OrderResponse.tap_hdr));
    memcpy(&OrderResponse.msg_hdr.AlphaChar,&AddOrder->msg_hdr.AlphaChar,sizeof(OrderResponse.msg_hdr.AlphaChar));
    OrderResponse.msg_hdr.ErrorCode = ValidateAddReqNonTrim(__bswap_32(AddOrder->msg_hdr.TraderId), FD,0, __bswap_16(AddOrder->BuySellIndicator), Token,(char *)& (AddOrder->sec_info.Symbol),(char *)&(AddOrder->sec_info.Series), dealerIndex, tokenIndex);
   
    if(OrderResponse.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.msg_hdr.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.msg_hdr.ErrorCode != 0)
      {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",FD, AddOrder->TransactionId, OrderResponse.msg_hdr.ErrorCode, AddOrder->sec_info.Symbol, AddOrder->sec_info.Series, Token, __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    OrderResponse.msg_hdr.MessageLength = sizeof(NSECM::MS_OE_RESPONSE_SL)-sizeof(NSECM::TAP_HEADER);
    OrderResponse.msg_hdr.TransactionCode = 2073;
    OrderResponse.msg_hdr.LogTime = 1;
    OrderResponse.msg_hdr.TraderId =  __bswap_32(AddOrder->msg_hdr.TraderId);
    //OrderResponse.ErrorCode = 0;
    OrderResponse.msg_hdr.Timestamp1 =  getCurrentTimeInNano();
    
    OrderResponse.msg_hdr.Timestamp1 = __bswap_64(OrderResponse.msg_hdr.Timestamp1); /*sneha*/
    OrderResponse.msg_hdr.Timestamp  = OrderResponse.msg_hdr.Timestamp1;
    OrderResponse.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemaining = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemaining = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    
    OrderResponse.TriggerPrice = AddOrder->TriggerPrice;
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
    memcpy(&OrderResponse.st_order_flags,& AddOrder->st_order_flags, sizeof(OrderResponse.st_order_flags));
    OrderResponse.BranchId = AddOrder->BranchId;
    OrderResponse.TraderId = AddOrder->TraderId;        
    memcpy(&OrderResponse.BrokerId,&AddOrder->BrokerId,sizeof(OrderResponse.BrokerId)) ;      
    OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&OrderResponse.Settlor,&AddOrder->Settlor,sizeof(OrderResponse.Settlor));
    OrderResponse.ProClientIndicator = AddOrder->ProClientIndicator;
    OrderResponse.SettlementPeriod =  __bswap_16(1); 
    
    memcpy(&OrderResponse.sec_info,&AddOrder->sec_info,sizeof(OrderResponse.sec_info));
    memcpy(&OrderResponse.NnfField,&AddOrder->NnfField,sizeof(OrderResponse.NnfField));       
    OrderResponse.TransactionId = AddOrder->TransactionId;
    OrderResponse.OrderNumber = OrderNumber ;
    OrderResponse.ReasonCode = 0;
    
    
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_SL));    
    
     if (OrderResponse.msg_hdr.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",FD, OrderNumber, AddOrder->TransactionId, OrderResponse.msg_hdr.ErrorCode, AddOrder->sec_info.Symbol, AddOrder->sec_info.Series, Token, __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Symbol "<<AddOrder->sec_info.Symbol<<"|Series "<<AddOrder->sec_info.Series<<std::endl;
//        OrderResponse.msg_hdr.ErrorCode = __bswap_16(OrderResponse.msg_hdr.ErrorCode);
        OrderResponse.msg_hdr.swapBytes();           
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_SL), pConnInfo);
        
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 4; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
        
        OrderResponse.msg_hdr.ErrorCode = __bswap_16(OrderResponse.msg_hdr.ErrorCode);
        
        if (OrderResponse.msg_hdr.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    

    OrderResponse.msg_hdr.swapBytes();           
   
    bookdetails.FD = FD; /*Sneha - multiple connection changes:15/07/16*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.IsSL = 0;
    bookdetails.SeqNo = __bswap_32(OrderResponse.tap_hdr.iSeqNo);
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.TriggerPrice = __bswap_32(OrderResponse.TriggerPrice);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(OrderResponse.TraderId);
     if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    if(IsSL == 1)
    {
      bookdetails.IsSL = 1;
    }
    /*Pan Card changes*/
    bookdetails.AlgoId = __bswap_32(OrderResponse.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(OrderResponse.AlgoCategory);
    memcpy(&bookdetails.PAN,&OrderResponse.PAN,sizeof(bookdetails.PAN));
    /*Pan card changes end*/
    bookdetails.TraderId = __bswap_32(OrderResponse.TraderId);
    bookdetails.BookType = __bswap_16(OrderResponse.BookType);
    bookdetails.BuySellIndicator = __bswap_16(OrderResponse.BuySellIndicator);
    bookdetails.Volume = __bswap_32(OrderResponse.Volume);
    bookdetails.BranchId = __bswap_16(OrderResponse.BranchId);
    bookdetails.UserId = __bswap_32(OrderResponse.TraderId);        
    bookdetails.ProClientIndicator = __bswap_16(OrderResponse.ProClientIndicator);
    bookdetails.nsecm_nsefo_nsecd.NSECM.TransactionId = __bswap_32(OrderResponse.TransactionId);
    bookdetails.nsecm_nsefo_nsecd.NSECM.Suspended = OrderResponse.Suspended;
    bookdetails.NnfField = OrderResponse.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.sec_info,&(AddOrder->sec_info),sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.sec_info));
    memcpy(&bookdetails.AccountNumber,&OrderResponse.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&OrderResponse.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&OrderResponse.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags,&OrderResponse.st_order_flags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags));
    bookdetails.LastModified = __bswap_32(OrderResponse.LastModified);
    
    
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    //OrderResponse.OrderNumber =  Ord_No;     
    OrderResponse.tap_hdr.sLength= __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_SL));    
    
    
    long datareturn;

    // ---- Store in Data info table for Trade        
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|IOC %d|SL %d|DQ %d|DQty %d|Qty %ld|TriggerPrice %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD, OrderNumber, AddOrder->TransactionId, bookdetails.IsIOC,bookdetails.IsSL, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.TriggerPrice, bookdetails.lPrice, Token, __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<"|Qty "<< bookdetails.lQty<< std::endl;
  
    datareturn = Addtoorderbook_NonTrim(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator),Token,IsIOC,IsDQ,IsSL, __bswap_constant_32(OrderResponse.LastModified), dealerIndex, tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
   
    
    int i = 0;
    i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_SL), pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
      
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 4; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
        //int SendOrderCancellation_NSECM(long OrderNumber, long Token,short Segment,int FD)
        /*IOC tick: changed sendBrdcst from true to false*/
        SendOrderCancellation_NSECM(&bookdetails,Token,FD,pConnInfo, 0, false);
    }    
  
    datareturn = Matching(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER NonTrim[SL Order]|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);
}

int SLOrderTriggeredResponseCM(ORDER_BOOK_DTLS *bookdetails,int tokenIndex)
{
    int64_t tSLTriggerStarts = 0;
    GET_PERF_TIME(tSLTriggerStarts);
    
  
  
    NSECM::MS_SL_TRIGGER SLTriggeredResponse={0}; // 20222
    int dlrIndex;
    
    
    SLTriggeredResponse.tap_hdr.iSeqNo = bookdetails->SeqNo;
    SLTriggeredResponse.tap_hdr.sLength =  sizeof(NSECM::MS_SL_TRIGGER);
    
    SLTriggeredResponse.msg_hdr.TransactionCode = 2212;
    SLTriggeredResponse.msg_hdr.LogTime = 1;
    SLTriggeredResponse.msg_hdr.TraderId = bookdetails->UserId;
    SLTriggeredResponse.msg_hdr.ErrorCode = 0;

    SLTriggeredResponse.msg_hdr.Timestamp1 =  getCurrentTimeInNano();
    SLTriggeredResponse.msg_hdr.Timestamp1 = __bswap_64(SLTriggeredResponse.msg_hdr.Timestamp1);
    SLTriggeredResponse.msg_hdr.Timestamp  = SLTriggeredResponse.msg_hdr.Timestamp1;
    SLTriggeredResponse.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    SLTriggeredResponse.ActivityTime  = __bswap_32(getEpochTime());
    //OrderResponse.ErrorCode = 0;
    
    
    
    SLTriggeredResponse.BookType = __bswap_16(1);
    bookdetails->BookType = 1;
    memcpy(&SLTriggeredResponse.AccountNumber,&bookdetails->AccountNumber,sizeof(SLTriggeredResponse.AccountNumber));
    SLTriggeredResponse.BuySellIndicator = __bswap_16(bookdetails->BuySellIndicator);
    SLTriggeredResponse.DisclosedVolume = __bswap_32(bookdetails->DQty);
    SLTriggeredResponse.DisclosedVolumeRemaining = __bswap_32(bookdetails->DQty);
    SLTriggeredResponse.TraderNumber = __bswap_32(bookdetails->UserId);
    SLTriggeredResponse.OriginalVolume = __bswap_32(bookdetails->Volume);
    SLTriggeredResponse.VolumeFilledToday = 0;
    SLTriggeredResponse.Price = __bswap_32(bookdetails->lPrice);
    /*Pan card changes*/
    memcpy(&SLTriggeredResponse.PAN,&bookdetails->PAN,sizeof(bookdetails->PAN));
    SLTriggeredResponse.AlgoCategory = __bswap_16(bookdetails->AlgoCategory);
    SLTriggeredResponse.AlgoId = __bswap_32(bookdetails->AlgoId);
    /*Pan card changes end*/
    memcpy(&SLTriggeredResponse.BrokerId,&bookdetails->BrokerId,sizeof(SLTriggeredResponse.BrokerId)) ;      
        
    
    SLTriggeredResponse.ProClient = __bswap_16(bookdetails->ProClientIndicator);
     
    
    memcpy(&SLTriggeredResponse.sec_info,&bookdetails->nsecm_nsefo_nsecd.NSECM.sec_info,sizeof(SLTriggeredResponse.sec_info));
     
    memcpy(&SLTriggeredResponse.OrderFlags,& bookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags, sizeof(SLTriggeredResponse.OrderFlags));
    
    int64_t OrderNumber = bookdetails->OrderNo ;
    SLTriggeredResponse.ResponseOrderNumber = bookdetails->OrderNo ;
    
    
    SLTriggeredResponse.tap_hdr.swapBytes();  
    SLTriggeredResponse.msg_hdr.swapBytes(); 
    
    int16_t BuySellSide = bookdetails->BuySellIndicator;
    
    int errCode = ValidateUser(bookdetails->UserId, bookdetails->FD, dlrIndex);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER [SL TRIGGERED]|Order# %ld|COrd# %d|IOC %d|SL %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %d|Side %d|LMT %d", 
      bookdetails->FD,(int64_t)SLTriggeredResponse.ResponseOrderNumber,bookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId, bookdetails->IsIOC,bookdetails->IsSL , bookdetails->IsDQ, bookdetails->DQty, bookdetails->lQty, bookdetails->lPrice,TokenStore[tokenIndex], __bswap_16(SLTriggeredResponse.BuySellIndicator),
      __bswap_32(SLTriggeredResponse.ActivityTime));
    Logger::getLogger().log(DEBUG, logBuf);
    
    
//    std::cout<<bookdetails->PAN<<"|"<<SLTriggeredResponse.PAN<<"|Cate::"<<bookdetails->AlgoCategory<<"|"<<__bswap_16(SLTriggeredResponse.AlgoCategory)<<"|Id::"<<bookdetails->AlgoId<<"|"<<__bswap_32(SLTriggeredResponse.AlgoId)<<"|Length::"<<__bswap_16(SLTriggeredResponse.tap_hdr.sLength)<<std::endl;
    long datareturn =Addtoorderbook(bookdetails,BuySellSide,TokenStore[tokenIndex],0,0,1,__bswap_32(SLTriggeredResponse.ActivityTime),dlrIndex,tokenIndex);
    
    
    
    datareturn = Cantoorderbook_NonTrim(bookdetails,BuySellSide,TokenStore[tokenIndex],__bswap_32(SLTriggeredResponse.ActivityTime), dlrIndex, tokenIndex);
    
    SwapDouble((char*) &SLTriggeredResponse.ResponseOrderNumber);    
    
    
    
    int i = 0;
    i = SendToClient( bookdetails->FD , (char *)&SLTriggeredResponse , sizeof(NSECM::MS_SL_TRIGGER), bookdetails->connInfo);
    
    
    
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 3; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&SLTriggeredResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    
    
    datareturn = Matching(TokenStore[tokenIndex],bookdetails->FD,0,0,bookdetails->connInfo, tokenIndex);
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= tSLTriggerStarts; 
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER NonTrim[SL TRIGGERED]|Order# %ld|Total=%ld", 
                     OrderNumber,  tTotal);
    Logger::getLogger().log(DEBUG, logBuf);
    
    
    
}
int SLOrderTriggeredResponseFO(ORDER_BOOK_DTLS *bookdetails,int tokenIndex)
{
    int64_t tSLTriggerStarts = 0;
    GET_PERF_TIME(tSLTriggerStarts);
    int dlrIndex = 0;
    
    
    NSEFO::MS_SL_TRIGGER SLTriggeredResponse={0};
    SLTriggeredResponse.tap_hdr.iSeqNo = bookdetails->SeqNo;
    SLTriggeredResponse.tap_hdr.sLength = sizeof(NSEFO::MS_SL_TRIGGER);
    //std::cout<<"size Trigger response::"<<sizeof(NSEFO::MS_SL_TRIGGER);    
    SLTriggeredResponse.msg_hdr.TransactionCode = 2212;
    SLTriggeredResponse.msg_hdr.LogTime = 1;
    SLTriggeredResponse.msg_hdr.TraderId = bookdetails->UserId;
    SLTriggeredResponse.msg_hdr.ErrorCode = 0;
    
    SLTriggeredResponse.msg_hdr.TimeStamp1 =  getCurrentTimeInNano();
    SLTriggeredResponse.msg_hdr.TimeStamp1 = __bswap_64(SLTriggeredResponse.msg_hdr.TimeStamp1);/*sneha*/
    SLTriggeredResponse.msg_hdr.Timestamp  = SLTriggeredResponse.msg_hdr.TimeStamp1;
    SLTriggeredResponse.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    SLTriggeredResponse.BookType = __bswap_16(1);
    bookdetails->BookType = 1;
    memcpy(&SLTriggeredResponse.AccountNumber,&bookdetails->AccountNumber,sizeof(SLTriggeredResponse.AccountNumber));
    SLTriggeredResponse.BuySellIndicator = __bswap_16(bookdetails->BuySellIndicator);
    SLTriggeredResponse.DisclosedVolume = __bswap_32(bookdetails->DQty);
    SLTriggeredResponse.DisclosedVolumeRemaining = __bswap_32(bookdetails->DQty);
    SLTriggeredResponse.TraderNumber = __bswap_32(bookdetails->UserId);
    SLTriggeredResponse.OriginalVolume = __bswap_32(bookdetails->Volume);
    SLTriggeredResponse.VolumeFilledToday = 0;
    SLTriggeredResponse.Price = __bswap_32(bookdetails->lPrice);
    
    /*Pan card changes*/
    memcpy(&SLTriggeredResponse.PAN,&bookdetails->PAN,sizeof(bookdetails->PAN));
    SLTriggeredResponse.AlgoCategory = __bswap_16(bookdetails->AlgoCategory);
    SLTriggeredResponse.AlgoId = __bswap_32(bookdetails->AlgoId);
    /*Pan card changes end*/
    
    SLTriggeredResponse.ActivityTime = __bswap_32(getEpochTime());
    memcpy(&SLTriggeredResponse.OrderFlags,& bookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags, sizeof(SLTriggeredResponse.OrderFlags));
    
    memcpy(&SLTriggeredResponse.BrokerId,&bookdetails->BrokerId,sizeof(SLTriggeredResponse.BrokerId)) ;      
    //SLTriggeredResponse.Suspended = AddOrder->Suspended;       

    
    //SLTriggeredResponse.SettlementPeriod =  __bswap_16(1);       
    //memcpy(&SLTriggeredResponse.sec_info,&AddOrder->sec_info,sizeof(SLTriggeredResponse.sec_info));
    
    
    int64_t OrderNumber = bookdetails->OrderNo ;
    SLTriggeredResponse.ResponseOrderNumber = bookdetails->OrderNo;
   
    
    SLTriggeredResponse.tap_hdr.swapBytes();
    SLTriggeredResponse.msg_hdr.swapBytes();       
    
    
    int BuySellSide = bookdetails->BuySellIndicator;
    
    int errCode = ValidateUser(bookdetails->UserId, bookdetails->FD, dlrIndex);
    
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER [SL TRIGGERED]|Order# %f|COrd# %d|SL %d|Qty %d|Price %d| Token %d|Side %d|LMT %d", 
      bookdetails->FD,bookdetails->OrderNo,bookdetails->nsecm_nsefo_nsecd.NSEFO.filler,bookdetails->IsSL , bookdetails->lQty, bookdetails->lPrice,TokenStore[tokenIndex], __bswap_16(SLTriggeredResponse.BuySellIndicator),
      __bswap_32(SLTriggeredResponse.ActivityTime));
    Logger::getLogger().log(DEBUG, logBuf);
    
//    std::cout<<"Trigger Response::"<<__bswap_32(SLTriggeredResponse.Price)<<"|"<<SLTriggeredResponse.TriggerPrice<<"|"<<SLTriggeredResponse.Volume<<"|"<<TokenStore[tokenIndex]<<"|"<<bookdetails->OrderNo<<"|"<<bookdetails->IsSL<<"|"<<bookdetails->nsecm_nsefo_nsecd.NSEFO.filler<<std::endl;
    
    long datareturn =Addtoorderbook(bookdetails,BuySellSide,TokenStore[tokenIndex]-FOOFFSET,0,0,1,__bswap_32(SLTriggeredResponse.ActivityTime),dlrIndex,tokenIndex);
    
    datareturn = Cantoorderbook_NonTrim(bookdetails,BuySellSide,TokenStore[tokenIndex]-FOOFFSET,__bswap_32(SLTriggeredResponse.ActivityTime), dlrIndex, tokenIndex);
    
    SwapDouble((char*) &SLTriggeredResponse.ResponseOrderNumber);    
    
    SLTriggeredResponse.msg_hdr.MessageLength = __bswap_16(sizeof(NSEFO::MS_SL_TRIGGER)); 
    
    int i = 0;
    i = SendToClient( bookdetails->FD , (char *)&SLTriggeredResponse , sizeof(NSEFO::MS_SL_TRIGGER),bookdetails->connInfo);
    
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 4; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&SLTriggeredResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    datareturn = Matching(TokenStore[tokenIndex] - FOOFFSET,bookdetails->FD,0,0,bookdetails->connInfo, tokenIndex);
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= tSLTriggerStarts;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER NonTrim[SL TRIGGERED]|Order# %ld|Total=%ld", 
                     OrderNumber,  tTotal);
}


int AddOrderNonTrim(NSEFO::MS_OE_REQUEST *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
//    int64_t tAfterEnqueue = getCurrentTimeInNano()-recvTime;
 
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    int MyTime = GlobalSeqNo++;
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_SL OrderResponse;
    long Token = (__bswap_32(AddOrder->TokenNo) - FOOFFSET);    
    int dealerIndex, tokenIndex;
    memcpy(&OrderResponse.tap_hdr,& AddOrder->msg_hdr, sizeof(OrderResponse.tap_hdr));
    memcpy(&OrderResponse.msg_hdr.AlphaChar,&AddOrder->msg_hdr.AlphaChar,sizeof(OrderResponse.msg_hdr.AlphaChar));
    OrderResponse.msg_hdr.ErrorCode = ValidateAddReqNonTrim(__bswap_32(AddOrder->msg_hdr.TraderId), FD, 0, __bswap_16(AddOrder->BuySellIndicator), Token, NULL, NULL, dealerIndex, tokenIndex);
    
    if(OrderResponse.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.msg_hdr.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.msg_hdr.ErrorCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, AddOrder->filler, OrderResponse.msg_hdr.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    
    int64_t OrderNumber = ME_OrderNumber++;
    
    OrderResponse.msg_hdr.MessageLength = sizeof(NSEFO::MS_OE_RESPONSE_SL)-sizeof(NSEFO::TAP_HEADER);
    OrderResponse.msg_hdr.TransactionCode = 2073;
    OrderResponse.msg_hdr.LogTime = 1;
    OrderResponse.msg_hdr.TraderId = __bswap_32(AddOrder->msg_hdr.TraderId);
    //OrderResponse.ErrorCode = 0;
    
    OrderResponse.msg_hdr.TimeStamp1 =  getCurrentTimeInNano();
    OrderResponse.msg_hdr.TimeStamp1 = __bswap_64(OrderResponse.msg_hdr.TimeStamp1);/*sneha*/
    OrderResponse.msg_hdr.Timestamp  = OrderResponse.msg_hdr.TimeStamp1 ;
    OrderResponse.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    OrderResponse.TraderId = AddOrder->TraderId;
    
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemaining = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemaining = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    
    /*Pan card changes*/
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    /*Pan card changes ends*/
    
    OrderResponse.TriggerPrice =  AddOrder->TriggerPrice;
    
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
    memcpy(&OrderResponse.st_order_flags,& AddOrder->st_order_flags, sizeof(OrderResponse.st_order_flags));
    OrderResponse.BranchId = AddOrder->BranchId;
           
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
    OrderResponse.ReasonCode = 0;
    
//    OrderResponse.tap_hdr.swapBytes();
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_SL));    
    
    if (OrderResponse.msg_hdr.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, OrderNumber, AddOrder->filler, OrderResponse.msg_hdr.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Token "<<(Token+FOOFFSET)<<std::endl;
//        OrderResponse.msg_hdr.ErrorCode = __bswap_16(OrderResponse.msg_hdr.ErrorCode);
        OrderResponse.msg_hdr.swapBytes();
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_SL), pConnInfo);
        
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 5; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
        OrderResponse.msg_hdr.ErrorCode = __bswap_16(OrderResponse.msg_hdr.ErrorCode);
        if (OrderResponse.msg_hdr.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    OrderResponse.msg_hdr.swapBytes();
    
    bookdetails.FD = FD; 
    bookdetails.connInfo = pConnInfo;
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.IsSL = 0;
    bookdetails.SeqNo = __bswap_32(OrderResponse.tap_hdr.iSeqNo);
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.TriggerPrice = __bswap_32(OrderResponse.TriggerPrice);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    if(IsSL == 1)
    {
      bookdetails.IsSL = 1;
    }
    
    /*Pan card changes*/    
    memcpy(&bookdetails.PAN,&OrderResponse.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(OrderResponse.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(OrderResponse.AlgoCategory);
    /*Pan card changes end*/
    
    bookdetails.dealerID = __bswap_32(OrderResponse.TraderId);
    bookdetails.TraderId = __bswap_32(OrderResponse.msg_hdr.TraderId);
    bookdetails.BookType = __bswap_16(OrderResponse.BookType);
    bookdetails.BuySellIndicator = __bswap_16(OrderResponse.BuySellIndicator);
    bookdetails.Volume = __bswap_32(OrderResponse.Volume);
    bookdetails.BranchId = __bswap_16(OrderResponse.BranchId);
    bookdetails.UserId = __bswap_32(OrderResponse.msg_hdr.TraderId);        
    bookdetails.ProClientIndicator = __bswap_16(OrderResponse.ProClientIndicator);
    bookdetails.nsecm_nsefo_nsecd.NSEFO.TokenNo = __bswap_32(OrderResponse.TokenNo);
    bookdetails.nsecm_nsefo_nsecd.NSEFO.filler = __bswap_32(OrderResponse.filler);
    
    bookdetails.NnfField = OrderResponse.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.AccountNumber,&OrderResponse.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&OrderResponse.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&OrderResponse.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags,&OrderResponse.st_order_flags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags));    
    bookdetails.LastModified = __bswap_32(OrderResponse.LastModified);
    
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|IOC %d|SL %d|DQ %d|DQty %d|Qty %ld|TriggerPrice %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD,OrderNumber,AddOrder->filler, bookdetails.IsIOC,bookdetails.IsSL , bookdetails.IsDQ, bookdetails.DQty, bookdetails.lQty, bookdetails.TriggerPrice, bookdetails.lPrice,(Token+FOOFFSET), __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    
    
    long datareturn = Addtoorderbook_NonTrim(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator), (__bswap_32(AddOrder->TokenNo) - FOOFFSET) /*1 Please replace token number here*/,IsIOC,IsDQ,IsSL , __bswap_constant_32(OrderResponse.LastModified), dealerIndex, tokenIndex);
   
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    // ---- Store in Data info table for Trade
    long ArrayIndex = (long)OrderResponse.OrderNumber;

      
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_SL));    
    
    int i = 0;
    
     i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_SL),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;    
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 5; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;    
    
    //std::cout << "MS_OE_RESPONSE_SL_TR :: Order Number "  <<  ME_OrderNumber <<  "  Bytes Sent " << i << std::endl;    
    /*Sneha*/
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
      /*IOC tick: changed sendBrdcst from true to false*/
        SendOrderCancellation_NSEFO(&bookdetails,(__bswap_32(AddOrder->TokenNo) - FOOFFSET),FD,pConnInfo, 0, false);
    } 
    datareturn = Matching((__bswap_32(AddOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;    
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER NonTrim[SL Order]|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}




long Addtoorderbook_NonTrim(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime, int dealerIndx, int tokenIndx ) // 1 Buy , 2 Sell
{
    // Enqueue Broadcast Packet 
   /*IOC tick: added IOC!=1 check*/
  
    int64_t t1=0,t2=0,t3=0,t4=0;
    GET_PERF_TIME(t1);
    int32_t iNewOrdLocn = 0;
    int32_t iLogRecs = 0;
 
    if (BuySellSide == 1) {
      iLogRecs = ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords;
        // Start Handling IOC Order -------------------------------------------------
           if(Mybookdetails->lPrice < ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[0].lPrice &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }    
        // End Handling IOC Order -------------------------------------------------        
      
        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].IsDQ = 1;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].DQty = Mybookdetails->DQty;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
                ME_Passive_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_Passive_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
                //ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC; 
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].IsSL = Mybookdetails->IsSL;
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].TriggerPrice = Mybookdetails->TriggerPrice;
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty;
               ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
               ME_Passive_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_Passive_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
           }

           /*Sneha - multiple connection changes:15/07/16*/
      ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].TTQ = 0;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].TraderId = Mybookdetails->TraderId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].BookType = Mybookdetails->BookType;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].Volume = Mybookdetails->Volume;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].DQty = Mybookdetails->DQty;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].BranchId = Mybookdetails->BranchId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].UserId = Mybookdetails->UserId;        
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].ProClientIndicator = Mybookdetails->ProClientIndicator;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].NnfField = Mybookdetails->NnfField; 
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].FD = Mybookdetails->FD;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].connInfo = Mybookdetails->connInfo;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].dealerID = Mybookdetails->dealerID;
          /*Pan card changes*/
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].AlgoId = Mybookdetails->AlgoId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].AlgoCategory = Mybookdetails->AlgoCategory ;
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
          /*Pan card changes end*/
          
          if (_nSegMode == SEG_NSECM){
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info));
           }
          else {
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
           }
           ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].LastModified = Mybookdetails->LastModified; 
           ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords].TransactionCode = NSECM_ADD_REQ; /*IOC tick*/
           
           
           
           ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           
           /*Dynamic Book size allocation*/
           if(ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords  > (ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_Passive_OrderBook.OrderBook[tokenIndx].Buy;
              int Booksize = ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize;
              try
              {
                ME_Passive_OrderBook.OrderBook[tokenIndx].Buy = new ORDER_BOOK_DTLS[Booksize*2];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook Non Trim|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookFull = true;
              }
              if(false == ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookFull )
              {
                fillDataBuy(0,Booksize*2,ME_Passive_OrderBook.OrderBook[tokenIndx].Buy);

                memcpy(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize = Booksize*2;

                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook Non Trim|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_Passive_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
           
           
        memcpy(Mybookdetails, &(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1]), sizeof (ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1]));
      
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Buy|1|BuyRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);

            for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Buy|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif
            
              for(int j=ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Buy|8|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j-1, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j-1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                
                if(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j-1].TriggerPrice > Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }
            
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Buy|9|OldLcn %d|NewLcn %d", ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
            
              GET_PERF_TIME(t3);
//              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]));                
        
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Buy|After|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);;
            }
            #endif 

           GET_PERF_TIME(t4);
//           if (ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook_NonTrim|Token %d|BuyOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//           }
            if (dealerIndx != -1)
            {
               dealerOrdArrNonTrim[dealerIndx][tokenIndx].buyordercnt++;
            }

     }  
    else {
      
        iLogRecs = ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords;
        
        // Start Handling IOC Order -------------------------------------------------
           if(((Mybookdetails->lPrice) > ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[0].lPrice) &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }           

        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {    
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].IsDQ = 1;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
                ME_Passive_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_Passive_OrderBook.OrderBook[tokenIndx].SellSeqNo + 1;
                //ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords = ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC ; 
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].IsSL = Mybookdetails->IsSL;
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].TriggerPrice = Mybookdetails->TriggerPrice;
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty;
              ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
              ME_Passive_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_Passive_OrderBook.OrderBook[tokenIndx].SellSeqNo + 1;
           }    
           /*Sneha - multiple connection changes:15/07/16*/
        ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].TTQ = 0;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].TraderId = Mybookdetails->TraderId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].BookType = Mybookdetails->BookType;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].Volume = Mybookdetails->Volume;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].BranchId = Mybookdetails->BranchId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].UserId = Mybookdetails->UserId;        
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].ProClientIndicator = Mybookdetails->ProClientIndicator;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].NnfField = Mybookdetails->NnfField;  
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].FD = Mybookdetails->FD;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].connInfo = Mybookdetails->connInfo;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].dealerID = Mybookdetails->dealerID;
          
          /*Pan card changes*/
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].AlgoId = Mybookdetails->AlgoId;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].AlgoCategory = Mybookdetails->AlgoCategory ;
          memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
          /*Pan card changes end*/
          
          if (_nSegMode == SEG_NSECM){
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info));
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
           }
          else {
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags));
          }
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].LastModified = Mybookdetails->LastModified;
          ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords].TransactionCode = NSECM_ADD_REQ_TR; /*IOC tick*/
          ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords = ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          
          
          /*Dynamic Book size allocation*/
           if(ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords  > (ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_Passive_OrderBook.OrderBook[tokenIndx].Sell;
              int Booksize = ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize;
              try
              {
                ME_Passive_OrderBook.OrderBook[tokenIndx].Sell = new ORDER_BOOK_DTLS[Booksize*2];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook Non Trim|Token %d|SellOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords,ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookFull = true;
              }
              if(false == ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookFull)
              {
                fillDataSell(0,Booksize*2,ME_Passive_OrderBook.OrderBook[tokenIndx].Sell);

                memcpy(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize = Booksize*2;

                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook_Non Trim|Token %d|SellOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords,ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_Passive_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              
              }               
           }
           /*Dynamic Book size allocation end*/
          
          memcpy(Mybookdetails, &(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1]), sizeof (ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1]));        
          GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Sell|1|SellRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);
            for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Sell|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif  
          
              for(int j=ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Sell|8|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j-1, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j-1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j-1].TriggerPrice < Mybookdetails->TriggerPrice)                  
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }

              #ifdef __LOG_ORDER_BOOK__            
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Sell|9|OldLcn %d|NewLcn %d", ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif

             GET_PERF_TIME(t3);
              //memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]));                
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook_NonTrim|Sell|After|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            #endif
          GET_PERF_TIME(t4);
//          if (ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook_NonTrim|Token %d|SellOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//            }
            if (dealerIndx != -1)
            {
              dealerOrdArrNonTrim[dealerIndx][tokenIndx].sellordercnt++;
              
            }
     }    
    snprintf(logBuf, 200, "Thread_ME|Addtoorderbook_NonTrim|Recs %6d|Search=%6ld|Position Search=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
      
    return 0; 
}


int ModOrderNonTrim(NSECM::MS_OE_REQUEST *ModOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pconnInfo, int64_t recvTime) {
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    ORDER_BOOK_DTLS bookdetails;
    int MyTime = GlobalSeqNo++;   
    NSECM::MS_OE_RESPONSE_SL ModOrdResp;
    int i = 0;
    long Token = 0;
    int tokenIndex = 0, dealerIndex = -1;
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo); 
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->msg_hdr, sizeof(ModOrdResp.tap_hdr));
    memcpy(&ModOrdResp.msg_hdr.AlphaChar,& ModOrder->msg_hdr.AlphaChar, sizeof(ModOrdResp.msg_hdr.AlphaChar));
    ModOrdResp.msg_hdr.ErrorCode = ValidateModReqNonTrim(__bswap_32(ModOrder->msg_hdr.TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token, ModOrder->sec_info.Symbol, ModOrder->sec_info.Series, tokenIndex, dealerIndex, LMT);
    
    if(ModOrdResp.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      ModOrdResp.msg_hdr.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(ModOrdResp.msg_hdr.ErrorCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",
           FD,(int64_t)dOrderNo, ModOrder->TransactionId,ModOrdResp.msg_hdr.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
         Logger::getLogger().log(DEBUG, logBuf);
         return 0;
      }
    }
    
    ModOrdResp.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_SL));
    
    ModOrdResp.msg_hdr.MessageLength = sizeof(NSECM::MS_OE_RESPONSE_SL)-sizeof(NSECM::TAP_HEADER);
    ModOrdResp.msg_hdr.TransactionCode = 2074;
    ModOrdResp.msg_hdr.LogTime = 1;
    ModOrdResp.msg_hdr.TraderId = __bswap_32(ModOrder->msg_hdr.TraderId);
    
    ModOrdResp.msg_hdr.Timestamp1 =  getCurrentTimeInNano();
    ModOrdResp.msg_hdr.Timestamp1 = ModOrdResp.msg_hdr.Timestamp1; /*Sneha*/
    ModOrdResp.msg_hdr.Timestamp  = ModOrdResp.msg_hdr.Timestamp1;
    ModOrdResp.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    ModOrdResp.BookType = ModOrder->BookType;
    memcpy(&ModOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(ModOrdResp.AccountNumber));
    memcpy(&ModOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(ModOrdResp.BuySellIndicator));
    ModOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    ModOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    ModOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    ModOrdResp.Volume = ModOrder->Volume;
    ModOrdResp.VolumeFilledToday = 0;
    ModOrdResp.Price = ModOrder->Price;
    
    memcpy(&ModOrdResp.PAN,&ModOrder->PAN,sizeof(ModOrdResp.PAN));
    ModOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    ModOrdResp.AlgoId = ModOrder->AlgoId;
    
    
    ModOrdResp.TriggerPrice = ModOrder->TriggerPrice;
    ModOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    memcpy(&ModOrdResp.st_order_flags,& ModOrder->st_order_flags, sizeof(ModOrdResp.st_order_flags));
    ModOrdResp.BranchId = ModOrder->BranchId;
    ModOrdResp.TraderId = ModOrder->TraderId;        
    memcpy(&ModOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(ModOrdResp.BrokerId)) ;      
    ModOrdResp.Suspended = ModOrder->Suspended;       
    memcpy(&ModOrdResp.Settlor,&ModOrder->Settlor,sizeof(ModOrdResp.Settlor));
    ModOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    ModOrdResp.SettlementPeriod =  __bswap_16(1);       
    memcpy(&ModOrdResp.sec_info,&ModOrder->sec_info,sizeof(ModOrdResp.sec_info));
    ModOrdResp.NnfField = ModOrder->NnfField;       
    ModOrdResp.TransactionId = ModOrder->TransactionId;
    ModOrdResp.OrderNumber = ModOrder->OrderNumber;
    ModOrdResp.ReasonCode = 0;
    //std::cout << "ModOrdResp.TransactionId  " << ModOrdResp.TransactionId << std::endl;
    
     
    
    if (ModOrdResp.msg_hdr.ErrorCode != 0)
    {
         snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",
           FD,(int64_t)dOrderNo, ModOrder->TransactionId,ModOrdResp.msg_hdr.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
         Logger::getLogger().log(DEBUG, logBuf);
         //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
//         ModOrdResp.msg_hdr.ErrorCode = __bswap_16(ModOrdResp.msg_hdr.ErrorCode);
         ModOrdResp.msg_hdr.swapBytes();
         ModOrdResp.LastModified = __bswap_32 (LMT);
         i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pconnInfo);
         
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 4; /*4 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
         ModOrdResp.msg_hdr.ErrorCode = __bswap_16(ModOrdResp.msg_hdr.ErrorCode);
         
         if (ModOrdResp.msg_hdr.ErrorCode == ERR_INVALID_USER_ID)
         {
             pconnInfo->status = DISCONNECTED;
         }
         return 0;
    }
    
    ModOrdResp.msg_hdr.swapBytes();
    
    LMT = LMT + 1;
    
    ModOrdResp.LastModified = __bswap_32 (LMT);
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    int64_t OrderNumber = ModOrdResp.OrderNumber;
    
    bookdetails.OrderNo =  ModOrdResp.OrderNumber;
    //SwapDouble((char*) &bookdetails.OrderNo);
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.TriggerPrice = __bswap_32(ModOrdResp.TriggerPrice);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.connInfo = pconnInfo;
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(ModOrdResp.TraderId);
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.IsSL = 0;
    if (1== IsIOC){
       bookdetails.IsIOC = 1;
    }
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    if (1== IsSL){
       bookdetails.IsSL = 1;
    }
    bookdetails.TraderId = __bswap_32(ModOrdResp.TraderId);
    bookdetails.BookType = __bswap_16(ModOrdResp.BookType);
    bookdetails.Volume = __bswap_32(ModOrdResp.Volume);
    bookdetails.BranchId = __bswap_16(ModOrdResp.BranchId);
    bookdetails.UserId = __bswap_32(ModOrdResp.msg_hdr.TraderId);        
    bookdetails.ProClientIndicator = __bswap_16(ModOrdResp.ProClientIndicator);
    bookdetails.nsecm_nsefo_nsecd.NSECM.Suspended = ModOrdResp.Suspended;
    bookdetails.NnfField = ModOrdResp.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.AccountNumber,&ModOrdResp.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&ModOrdResp.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&ModOrdResp.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags,&ModOrdResp.st_order_flags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags));
    bookdetails.LastModified = LMT;
   /*Sneha - E*/
        

    
   snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|IOC %d|SL %d|DQ %d|DQty %d|Qty %ld|TriggerPrice %ld|Price %ld| Token %ld|Side %d|LMT %d",
     FD, OrderNumber,ModOrder->TransactionId,bookdetails.IsIOC,bookdetails.IsSL,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.TriggerPrice, bookdetails.lPrice, Token, __bswap_16(ModOrdResp.BuySellIndicator),
     LMT); 
   Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
   //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
   long datareturn = Modtoorderbook_NonTrim(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),Token ,IsIOC,IsDQ,IsSL ,__bswap_constant_32(ModOrdResp.LastModified), tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;

     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pconnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 4; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    
    
    datareturn = Matching(Token,FD,IsIOC,IsDQ,pconnInfo, tokenIndex);
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|MOD ORDER NonTrim[SL Order]|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);
}

int ValidateModReqNonTrim(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int& tokIndex, int& dlrIndex, int32_t& LMT)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, FD, dlrIndex);
    
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokIndex].BuyRecords ) ; j++)
        {   
             if(ME_Passive_OrderBook.OrderBook[tokIndex].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_Passive_OrderBook.OrderBook[tokIndex].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 if (LMT != ME_Passive_OrderBook.OrderBook[tokIndex].Buy[j].LastModified)
                 {
                     errCode = OE_ORD_CANNOT_MODIFY;
                     snprintf (logBuf, 500, "Thread_ME|ValidateModReq NonTrim|Incorrect LMT|RecvdLMT %d|BookLMT %d", 
                       LMT,  ME_Passive_OrderBook.OrderBook[tokIndex].Buy[j].LastModified);
                     Logger::getLogger().log(DEBUG, logBuf); 
                 }
                 LMT = ME_Passive_OrderBook.OrderBook[tokIndex].Buy[j].LastModified;
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokIndex].SellRecords ) ; j++)
        {   
             if(ME_Passive_OrderBook.OrderBook[tokIndex].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_Passive_OrderBook.OrderBook[tokIndex].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 if (LMT != ME_Passive_OrderBook.OrderBook[tokIndex].Sell[j].LastModified) 
                 {
                     errCode = OE_ORD_CANNOT_MODIFY;
                     snprintf (logBuf, 500, "Thread_ME|ValidateModReq NonTrim|Incorrect LMT|RecvdLMT %d|BookLMT %d", 
                       LMT,  ME_Passive_OrderBook.OrderBook[tokIndex].Sell[j].LastModified);
                     Logger::getLogger().log(DEBUG, logBuf); 
                 }
                 LMT = ME_Passive_OrderBook.OrderBook[tokIndex].Sell[j].LastModified;
             }   
         }
     }
    
     if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}

int ModOrderNonTrim(NSEFO::MS_OE_REQUEST *ModOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_SL ModOrdResp;
    
    int MyTime = GlobalSeqNo++; 
     int tokenIndex = 0, dealerIndex = -1;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    int32_t LMT= __bswap_32(ModOrder->LastModified);
    
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->msg_hdr, sizeof(ModOrdResp.tap_hdr));
    memcpy(&ModOrdResp.msg_hdr.AlphaChar,&ModOrder->msg_hdr.AlphaChar,sizeof(ModOrder->msg_hdr.AlphaChar));
    
    ModOrdResp.msg_hdr.ErrorCode = ValidateModReqNonTrim(__bswap_32(ModOrder->msg_hdr.TraderId), FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token,NULL,NULL, tokenIndex, dealerIndex, LMT);
    
    if(ModOrdResp.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      ModOrdResp.msg_hdr.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(ModOrdResp.msg_hdr.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.msg_hdr.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    
    //memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.msg_hdr.TransactionCode = 2074;
    ModOrdResp.msg_hdr.LogTime = 1;
    ModOrdResp.msg_hdr.TraderId = __bswap_32(ModOrder->msg_hdr.TraderId);
    ModOrdResp.msg_hdr.MessageLength = sizeof(NSEFO::MS_OE_RESPONSE_SL)-sizeof(NSEFO::TAP_HEADER);
    ModOrdResp.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_SL));
    //ModOrdResp.ErrorCode = 0;       
    ModOrdResp.msg_hdr.TimeStamp1 =  getCurrentTimeInNano();
    ModOrdResp.msg_hdr.TimeStamp1 = __bswap_64(ModOrdResp.msg_hdr.TimeStamp1); /*sneha*/
    ModOrdResp.msg_hdr.Timestamp  = ModOrdResp.msg_hdr.TimeStamp1;
    ModOrdResp.msg_hdr.TimeStamp2[7] = 1; 
    ModOrdResp.BookType = ModOrder->BookType;
    memcpy(&ModOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(ModOrdResp.AccountNumber));
    memcpy(&ModOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(ModOrdResp.BuySellIndicator));
    ModOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    ModOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    ModOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    ModOrdResp.Volume = ModOrder->Volume;
    ModOrdResp.VolumeFilledToday = 0;
    ModOrdResp.Price = ModOrder->Price;
    
    memcpy(&ModOrdResp.PAN,&ModOrder->PAN,sizeof(ModOrdResp.PAN));
    ModOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    ModOrdResp.AlgoId = ModOrder->AlgoId;
    
    ModOrdResp.TriggerPrice = ModOrder->TriggerPrice;
    ModOrdResp.EntryDateTime =ModOrder->EntryDateTime;
    
    memcpy(&ModOrdResp.st_order_flags,& ModOrder->st_order_flags, sizeof(ModOrdResp.st_order_flags));
    ModOrdResp.BranchId = ModOrder->BranchId;
    ModOrdResp.TraderId = ModOrder->TraderId;        
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
    
    ModOrdResp.ReasonCode = 0;
    
     
    
    if (ModOrdResp.msg_hdr.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.msg_hdr.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
//        ModOrdResp.msg_hdr.ErrorCode = __bswap_16(ModOrdResp.msg_hdr.ErrorCode);
        ModOrdResp.msg_hdr.swapBytes();
        ModOrdResp.LastModified = __bswap_32(LMT);
        SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
        
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 5; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
        ModOrdResp.msg_hdr.ErrorCode = __bswap_16(ModOrdResp.msg_hdr.ErrorCode);
        if (ModOrdResp.msg_hdr.ErrorCode == ERR_INVALID_USER_ID)
        {
            pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    ModOrdResp.msg_hdr.swapBytes();
    LMT = LMT + 1;
    ModOrdResp.LastModified = __bswap_32(LMT);
    SwapDouble((char*) &ModOrdResp.OrderNumber);
//    int OrderNumber = ModOrdResp.OrderNumber;
    int64_t OrderNumber = ModOrdResp.OrderNumber;
     
    memcpy(&bookdetails.OrderNo,&ModOrdResp.OrderNumber,sizeof(bookdetails.OrderNo));
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    //SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.TriggerPrice = __bswap_32(ModOrdResp.TriggerPrice);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.connInfo = pConnInfo;
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(ModOrdResp.TraderId);
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.IsSL = 0;
    if (1 == IsIOC){
      bookdetails.IsIOC = 1;
    }
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    if (1 == IsSL){
      bookdetails.IsSL = 1;
    }
    bookdetails.TraderId = __bswap_32(ModOrdResp.TraderId);
    bookdetails.BookType = __bswap_16(ModOrdResp.BookType);
    bookdetails.Volume = __bswap_32(ModOrdResp.Volume);
    bookdetails.BranchId = __bswap_16(ModOrdResp.BranchId);
    bookdetails.UserId = __bswap_32(ModOrdResp.TraderId);        
    bookdetails.ProClientIndicator = __bswap_16(ModOrdResp.ProClientIndicator);
    bookdetails.NnfField = ModOrdResp.NnfField;  
    SwapDouble((char*) &bookdetails.NnfField);
    memcpy(&bookdetails.BrokerId,&ModOrdResp.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&ModOrdResp.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.AccountNumber,&ModOrdResp.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags,& ModOrdResp.st_order_flags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags));
    bookdetails.LastModified = LMT;
    /*Sneha - E*/
    
    snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|IOC %d|SL %d|DQ %d|DQty %d|Qty %ld|TriggerPrice %ld|Price %ld| Token %ld|Side %d|LMT %d",
    FD,OrderNumber,ModOrder->filler,bookdetails.IsIOC,bookdetails.IsSL,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.TriggerPrice,  bookdetails.lPrice, (Token + FOOFFSET), __bswap_16(ModOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    
    long datareturn = Modtoorderbook_NonTrim(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET) ,IsIOC,IsDQ,IsSL,__bswap_constant_32(ModOrdResp.LastModified), tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    
    int i = 0;
   
     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 5; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
   

    datareturn = Matching((__bswap_32(ModOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|MOD ORDER NonTrim[SL Order]|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}

int ValidateCanReqNonTrim(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int& dealerIndx, int& tokenIndx, int& LMT)
{
    int errCode = 0;
    bool orderFound = false;
    
    errCode = ValidateUser(iUserID, FD, dealerIndx);
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokenIndx);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokenIndx);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords ) ; j++)
        {   
             if(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 LMT = ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].LastModified;
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords ) ; j++)
        {   
             if(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 LMT = ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].LastModified;
             }   
         }
     }
    
    if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}



int CanOrderNonTrim(NSECM::MS_OE_REQUEST *ModOrder, int FD, CONNINFO* pConnInfo, int64_t recvTime) {
    ORDER_BOOK_DTLS bookdetails;
    NSECM::MS_OE_RESPONSE_SL CanOrdResp;
    long Token = 0;
    
    int dealerIndex, tokenIndex;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    memcpy(&CanOrdResp.tap_hdr,& ModOrder->msg_hdr, sizeof(CanOrdResp.tap_hdr));
    memcpy(&CanOrdResp.msg_hdr.AlphaChar,& ModOrder->msg_hdr.AlphaChar, sizeof(CanOrdResp.msg_hdr.AlphaChar));
    
    int16_t BookType= __bswap_16(ModOrder->BookType);
//    std::cout<<BookType<<std::endl;
    if(BookType == 3)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateCanReqNonTrim(__bswap_32(ModOrder->msg_hdr.TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series, dealerIndex, tokenIndex,LMT);
    }
    else if(BookType == 1)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->msg_hdr.TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series, dealerIndex, tokenIndex,LMT);
    }     
    CanOrdResp.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_SL));

    if(CanOrdResp.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(CanOrdResp.msg_hdr.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.msg_hdr.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    
    CanOrdResp.msg_hdr.TransactionCode =2075;
    CanOrdResp.msg_hdr.LogTime = 1;
    CanOrdResp.msg_hdr.TraderId = __bswap_32(ModOrder->msg_hdr.TraderId);
    CanOrdResp.msg_hdr.MessageLength = sizeof(NSECM::MS_OE_RESPONSE_SL) - sizeof(NSECM::TAP_HEADER);
 
    CanOrdResp.msg_hdr.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.msg_hdr.Timestamp1 =  __bswap_64(CanOrdResp.msg_hdr.Timestamp1); /*sneha*/
    CanOrdResp.msg_hdr.Timestamp = CanOrdResp.msg_hdr.Timestamp1 ;
    CanOrdResp.msg_hdr.TimeStamp2[7] = 1; /*sneha*/
    CanOrdResp.BookType = ModOrder->BookType;
    memcpy(&CanOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    CanOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    CanOrdResp.Volume = ModOrder->Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = ModOrder->Price;
    
    memcpy(&CanOrdResp.PAN,&ModOrder->PAN,sizeof(CanOrdResp.PAN));
    CanOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    CanOrdResp.AlgoId = ModOrder->AlgoId;
    
    
    CanOrdResp.TriggerPrice = ModOrder->TriggerPrice;
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
     memcpy(&CanOrdResp.st_order_flags,& ModOrder->st_order_flags, sizeof(CanOrdResp.st_order_flags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.TraderId = ModOrder->TraderId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    CanOrdResp.Suspended = ModOrder->Suspended;       
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    CanOrdResp.SettlementPeriod =  __bswap_16(1);       
    memcpy(&CanOrdResp.sec_info,&ModOrder->sec_info,sizeof(CanOrdResp.sec_info));
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.TransactionId = ModOrder->TransactionId;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
   
    CanOrdResp.ReasonCode = 0;
    
    
    if (CanOrdResp.msg_hdr.ErrorCode != 0)
    {
      
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT",
          FD, (int64_t)dOrderNo, ModOrder->TransactionId,CanOrdResp.msg_hdr.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<int(dOrderNo)<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
//        CanOrdResp.msg_hdr.ErrorCode = __bswap_16(CanOrdResp.msg_hdr.ErrorCode);
        CanOrdResp.msg_hdr.swapBytes(); 
        CanOrdResp.LastModified = __bswap_32(LMT);
        SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_SL),pConnInfo);
         
         memset (&LogData, 0, sizeof(LogData));
         LogData.MyFd = 4; /*1 = Order response*/
         memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
         Inqptr_MeToLog_Global->enqueue(LogData);
    
        CanOrdResp.msg_hdr.ErrorCode = __bswap_16(CanOrdResp.msg_hdr.ErrorCode);
        if (CanOrdResp.msg_hdr.ErrorCode == ERR_INVALID_USER_ID)
        {
            pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    CanOrdResp.msg_hdr.swapBytes(); 
    LMT = LMT + 1;
    CanOrdResp.LastModified = __bswap_32(LMT);    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.OrderNo = CanOrdResp.OrderNumber;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.TriggerPrice = __bswap_32(CanOrdResp.TriggerPrice);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.dealerID = __bswap_32(CanOrdResp.TraderId);
    bookdetails.LastModified = LMT;
    int64_t OrderNumber = bookdetails.OrderNo;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|Qty %ld|TriggerPrice %ld|Price %ld|Token %ld|Side %d|LMT %d",
    FD, OrderNumber, ModOrder->TransactionId,bookdetails.lQty, bookdetails.TriggerPrice,bookdetails.lPrice, Token, __bswap_16(CanOrdResp.BuySellIndicator), LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    
    
    long datareturn;
    if(BookType == 1)
    {
     
      datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),Token , __bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
    }
    else
    {
      datareturn = Cantoorderbook_NonTrim(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),Token , __bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
    }
    int i = 0;
    
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_SL),pConnInfo);
    /*Sneha*/
    
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 4; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);

    SwapDouble((char*) &CanOrdResp.OrderNumber);   
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    snprintf(logBuf, 500, "Thread_ME|LATENCY|CAN ORDER NonTrim[SL Order]|Order# %ld|Total=%ld", OrderNumber, tTotal-recvTime);
    Logger::getLogger().log(DEBUG, logBuf);
}

int CanOrderNonTrim(NSEFO::MS_OE_REQUEST *ModOrder, int FD, CONNINFO* pConnInfo, int64_t recvTime) {
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_SL CanOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));
     int dealerIndex, tokenIndex;
     
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    memcpy(&CanOrdResp.tap_hdr,& ModOrder->msg_hdr, sizeof(CanOrdResp.tap_hdr));
    memcpy(&CanOrdResp.msg_hdr.AlphaChar,& ModOrder->msg_hdr.AlphaChar, sizeof(CanOrdResp.msg_hdr.AlphaChar));
    
    int16_t BookType= __bswap_16(ModOrder->BookType);
    
    if(BookType == 3)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateCanReqNonTrim(__bswap_32(ModOrder->msg_hdr.TraderId), FD,dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,NULL,NULL, dealerIndex, tokenIndex,LMT);
    }
    else if(BookType == 1)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->msg_hdr.TraderId), FD,dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,NULL,NULL, dealerIndex, tokenIndex,LMT);
    }
    
    if(CanOrdResp.msg_hdr.ErrorCode == 0 && bEnableValMsg)
    {
      CanOrdResp.msg_hdr.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(CanOrdResp.msg_hdr.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.msg_hdr.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    CanOrdResp.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_SL));
//    std::cout<<"size cancel response::"<<sizeof(NSEFO::MS_OE_RESPONSE_SL);
    CanOrdResp.msg_hdr.TransactionCode = 2075;
    CanOrdResp.msg_hdr.LogTime = 1;
    CanOrdResp.msg_hdr.TraderId = __bswap_32(ModOrder->TraderId);
    CanOrdResp.msg_hdr.MessageLength = sizeof(NSEFO::MS_OE_RESPONSE_SL) - sizeof(NSEFO::TAP_HEADER);
    //CanOrdResp.ErrorCode = 0;       
    CanOrdResp.msg_hdr.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.msg_hdr.TimeStamp1 = __bswap_64(CanOrdResp.msg_hdr.TimeStamp1); /*sneha*/
    CanOrdResp.msg_hdr.Timestamp  = CanOrdResp.msg_hdr.TimeStamp1;
    CanOrdResp.msg_hdr.TimeStamp2[7] = 1;
    CanOrdResp.BookType = ModOrder->BookType;
    memcpy(&CanOrdResp.AccountNumber,&ModOrder->AccountNumber,sizeof(CanOrdResp.AccountNumber));
    memcpy(&CanOrdResp.BuySellIndicator,&ModOrder->BuySellIndicator,sizeof(CanOrdResp.BuySellIndicator));
    CanOrdResp.DisclosedVolume = ModOrder->DisclosedVolume;
    CanOrdResp.DisclosedVolumeRemaining = ModOrder->DisclosedVolume;
    CanOrdResp.TotalVolumeRemaining = ModOrder->Volume;
    CanOrdResp.Volume = ModOrder->Volume;
    CanOrdResp.VolumeFilledToday = 0;
    CanOrdResp.Price = ModOrder->Price;
    
    memcpy(&CanOrdResp.PAN,&ModOrder->PAN,sizeof(CanOrdResp.PAN));
    CanOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    CanOrdResp.AlgoId = ModOrder->AlgoId;
    
    
    CanOrdResp.TriggerPrice = ModOrder->TriggerPrice;
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    memcpy(&CanOrdResp.st_order_flags,& ModOrder->st_order_flags, sizeof(CanOrdResp.st_order_flags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.TraderId = ModOrder->TraderId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    CanOrdResp.TokenNo = ModOrder->TokenNo;
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.filler = ModOrder->filler;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    CanOrdResp.ReasonCode = 0;
    
    
    
    if (CanOrdResp.msg_hdr.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.msg_hdr.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
//        CanOrdResp.msg_hdr.ErrorCode = __bswap_16(CanOrdResp.msg_hdr.ErrorCode);
        CanOrdResp.msg_hdr.swapBytes(); 
        CanOrdResp.LastModified = __bswap_32(LMT);
        SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_SL),pConnInfo);
    
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 5; /*1 = Order response*/
        memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);
    
         CanOrdResp.msg_hdr.ErrorCode = __bswap_16(CanOrdResp.msg_hdr.ErrorCode);
         if (CanOrdResp.msg_hdr.ErrorCode == ERR_INVALID_USER_ID)
         {
             pConnInfo->status = DISCONNECTED;
         }
        return 0;
    }
    CanOrdResp.msg_hdr.swapBytes(); 
    LMT = LMT + 1;
    CanOrdResp.LastModified = __bswap_32(LMT);
    bookdetails.OrderNo =  CanOrdResp.OrderNumber;
    SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.TriggerPrice = __bswap_32(CanOrdResp.TriggerPrice);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.dealerID = __bswap_32(CanOrdResp.TraderId);
    bookdetails.LastModified = LMT;
    
    int64_t OrderNumber = bookdetails.OrderNo;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER NonTrim[SL Order]|Order# %ld|COrd# %d|Qty %ld|TriggerPrice %ld|Price %ld|Token %ld|Side %d|LMT %d",
    FD,  OrderNumber, ModOrder->filler,bookdetails.lQty, bookdetails.TriggerPrice, bookdetails.lPrice, (Token + FOOFFSET), __bswap_16(CanOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    long datareturn;
    
    
    if(BookType == 1)
    {
      datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET),__bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
    }
    else
    {
      /*Cancel from active book*/
      datareturn  = Cantoorderbook_NonTrim(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET),__bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
    }
    int i =0;
   
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_SL),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 5; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    snprintf(logBuf, 500, "Thread_ME|LATENCY|CAN ORDER NonTrim[SL Order]|Order# %ld|Total=%ld", OrderNumber, tTotal-recvTime);
    Logger::getLogger().log(DEBUG, logBuf);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
}

long Modtoorderbook_NonTrim(ORDER_BOOK_DTLS *Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime, int tokenIndex) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iOldOrdLocn = -1;
    int32_t iNewOrdLocn = -1;
    int32_t iLogRecs = 0;
    GET_PERF_TIME(t1);    
    long ret = 0;
    if(BuySellSide == 1)
    {
      iLogRecs = ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords;
        if ((Mybookdetails->lPrice < ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice) && 1 == Mybookdetails->IsIOC) {
            ret = 5; /* 5  means cancel IOC Order*/
        }
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|1|BuyRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }    
        #endif
          
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)  //Search for the OrderNo
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|TriggerPrice %d|Qty %d|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
            GET_PERF_TIME(t2);          
            iOldOrdLocn = j;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = Mybookdetails->lPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice = Mybookdetails->TriggerPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = Mybookdetails->lQty;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].DQty = Mybookdetails->DQty;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ = Mybookdetails->IsDQ;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].FD = Mybookdetails->FD;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].connInfo = Mybookdetails->connInfo;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].dealerID = Mybookdetails->dealerID;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = Mybookdetails->IsIOC;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].IsSL = Mybookdetails->IsSL;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TraderId = Mybookdetails->TraderId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].BookType = Mybookdetails->BookType;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].Volume = Mybookdetails->Volume;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].BranchId = Mybookdetails->BranchId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].UserId = Mybookdetails->UserId;        
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                 
                /*Pan card changes*/
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].AlgoId = Mybookdetails->AlgoId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].AlgoCategory = Mybookdetails->AlgoCategory; 
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
                /*Pan card changes end*/
                
                if (_nSegMode == SEG_NSECM){
                     memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                     ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                }
                else {
                      memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].LastModified = Mybookdetails->LastModified; 
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TransactionCode = NSECM_MOD_REQ; /*IOC tick*/
                ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = GlobalSeqNo++;
                //memcpy for sending book to sendCancellation(), if required.
                memcpy(Mybookdetails, &(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice <= Mybookdetails->TriggerPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|4|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice, Mybookdetails->TriggerPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|5|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice < Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else
                {
                  break; // Added for TC
                }
              }

              for(; j<ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|6|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j+1].TriggerPrice == Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn]), &(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn+1]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
    }   
    else
    {
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|8|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j-1, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j-1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j-1].TriggerPrice > Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
                GET_PERF_TIME(t3);
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
            }
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Buy|After|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }
        #endif
        GET_PERF_TIME(t4);
    }
    else {
      iLogRecs = ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords;
        if ((Mybookdetails->lPrice > ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice) && 1 == Mybookdetails->IsIOC) {
           ret = 5; /*5 means cancel IOC */
        }           
          
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|1|SellRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords) ; j++)
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|TriggerPrice %d|Qty %d|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lQty);          
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
            GET_PERF_TIME(t2);        
            iOldOrdLocn = j;
                //memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j],Mybookdetails , sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j]));
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = Mybookdetails->lPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice = Mybookdetails->TriggerPrice;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = Mybookdetails->lQty;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].DQty = Mybookdetails->DQty;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ = Mybookdetails->IsDQ;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].FD = Mybookdetails->FD;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].connInfo = Mybookdetails->connInfo;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].dealerID = Mybookdetails->dealerID;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = Mybookdetails->IsIOC;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TraderId = Mybookdetails->TraderId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].BookType = Mybookdetails->BookType;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].Volume = Mybookdetails->Volume;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].BranchId = Mybookdetails->BranchId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].UserId = Mybookdetails->UserId;        
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                /*Pan card changes*/
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].AlgoId = Mybookdetails->AlgoId;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].AlgoCategory = Mybookdetails->AlgoCategory; 
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
                /*Pan card changes end*/
                if (_nSegMode == SEG_NSECM){
                     ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;  
                     memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));                 
                }
                else {
                      memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].LastModified = Mybookdetails->LastModified;  
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TransactionCode = NSECM_MOD_REQ;
                ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = GlobalSeqNo++;
                memcpy (Mybookdetails, &(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice >= Mybookdetails->TriggerPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|4|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|5|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice > Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else
                {
                break; // TC
                }   
              }

              for(; j<ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|6|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j+1, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j+1].TriggerPrice == Mybookdetails->TriggerPrice)
                {
                  continue;
                }    
                else
    /*IOC tick: Do not brdcst for IOC orders*/
                {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn]), &(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn+1]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            else
            {
              //for(; j<ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords-1; j--)
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|8|j %d|NextTriggerPrice[%d] %d|CurrTriggerPrice %d|", j, j-1, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j-1].TriggerPrice, Mybookdetails->TriggerPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j-1].TriggerPrice < Mybookdetails->TriggerPrice)
                {
                  continue;
                }
                else    
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn+1]), &(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), sizeof(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook_NonTrim|Sell|After|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        GET_PERF_TIME(t4);
    }
        
    snprintf(logBuf, 200, "Thread_ME|Modtoorderbook_NonTrim|Recs %6d|Search=%6ld|PositionSearch=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret; 
}


long Cantoorderbook_NonTrim(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token, int32_t epochTime, int dlrIndx, int tokIndx) // 1 Buy , 2 Sell , 1 SLTriggered ,0 SLNotTriggered
{
    
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iLogRecs = 0;
    long ret = 0;
    GET_PERF_TIME(t1);
    long lPrice = 0, lTriggerPrice=0,lQty = 0;
    short IsIOC = 0,IsSL = 0;
    int16_t transCode = 0;
    if (BuySellSide == 1) {
      iLogRecs = ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords;
      #ifdef __LOG_ORDER_BOOK__  
      snprintf(logBuf, 500, "Cantoorderbook NonTrim|Buy|1|BuyRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
    {
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Buy|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
      #endif    
        for (int j = 0; j < (ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords); j++) {

          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
                lPrice =  ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lPrice;
                lTriggerPrice = ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TriggerPrice;
                lQty =  ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lQty;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lPrice = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TriggerPrice = 2147483647;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].DQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].IsDQ =0 ;
                IsIOC = ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC;
                IsSL = ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].IsSL;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].IsSL = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OpenQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].SeqNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TTQ = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].LastModified = Mybookdetails->LastModified;
                transCode = ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode = NSECM_CAN_REQ; /*IOC tick*/
                GET_PERF_TIME(t2);
                //SortBuySideBook(tokIndx);
                memmove(&(ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j]), &(ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j+1]), sizeof(ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j])*(ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords-j-1));
                GET_PERF_TIME(t3);
                ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords = ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords - 1; 
                
                int lnBuyRec = ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lPrice = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].DQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsDQ = 0 ;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsIOC = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OpenQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OrderNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].SeqNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].TTQ = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].LastModified = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].TransactionCode = 0;
                
                if (dlrIndx != -1)
                {
                   dealerOrdArrNonTrim[dlrIndx][tokIndx].buyordercnt--;
                }
                ret = 1;
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Buy|After|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_Passive_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
    }  
        #endif
    }
    else {
        iLogRecs = ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords;
        #ifdef __LOG_ORDER_BOOK__  
        snprintf(logBuf, 500, "Cantoorderbook NonTrim|Sell|1|SellRecs %d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords, Mybookdetails->TriggerPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Sell|Before|j %2d|TriggerPrice %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
        #endif      
        for (int j = 0; j < (ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords); j++) {

          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
                lPrice =  ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lPrice;
                lTriggerPrice = ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TriggerPrice;
                lQty =  ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lQty;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lPrice = 2147483647;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TriggerPrice = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].DQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].IsDQ =0;
                IsIOC = ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC;
                IsSL = ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].IsSL;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].IsSL = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OpenQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].SeqNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TTQ = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].LastModified = Mybookdetails->LastModified;
                transCode = ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode = NSECM_CAN_REQ; /*IOC tick*/
                GET_PERF_TIME(t2);
                //SortSellSideBook(tokIndx);
                memmove(&(ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j]), &(ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j+1]), sizeof(ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j])*(ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords-j-1));
                GET_PERF_TIME(t3);
                ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords = ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords - 1;
                
                int lnSellRec = ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lPrice = 2147483647;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].DQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsDQ =0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsIOC = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OpenQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OrderNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].SeqNo = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].TTQ = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lQty = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].LastModified = 0;
                ME_Passive_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].TransactionCode = 0;
                
                 if (dlrIndx != -1)
                 {
                   dealerOrdArrNonTrim[dlrIndx][tokIndx].sellordercnt--;
                   
                 }
                ret = 1;
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_Passive_OrderBook.OrderBook[tokIndx].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Cantoorderbook NonTrim|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].TriggerPrice, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_Passive_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
    }    
        #endif
    }
    
    snprintf(logBuf, 200, "Thread_ME|Cantoorderbook_NonTrim|Search=%ld|Sort=%ld", t2-t1,t3-t2);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret;
}
/*Non Trim SL ORDER ENDS (NK)*/

/*Add Trade Functions Begins (NK)*/
int AddnTradeOrderTrim(NSECM::MS_OE_REQUEST_TR *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {

    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    int MyTime = GlobalSeqNo++;

    int64_t OrderNumber = ME_OrderNumber++;
    
    
    long Token = 0;
    NSECM::MS_OE_RESPONSE_TR OrderResponse;
    int dealerIndex, tokenIndex;
    
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD,0, __bswap_16(AddOrder->BuySellIndicator), Token,(char *)& (AddOrder->sec_info.Symbol),(char *)&(AddOrder->sec_info.Series), dealerIndex, tokenIndex);

    
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;

    OrderResponse.TimeStamp1 =  getCurrentTimeInNano();
    OrderResponse.TimeStamp1 = __bswap_64(OrderResponse.TimeStamp1); 
    OrderResponse.Timestamp  = OrderResponse.TimeStamp1;
    OrderResponse.TimeStamp2 = '1'; 
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemain = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemain = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
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
    OrderResponse.ReasonCode = 0;
    
    OrderResponse.tap_hdr.swapBytes();           
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
     if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",FD, OrderNumber, AddOrder->TransactionId, OrderResponse.ErrorCode, AddOrder->sec_info.Symbol, AddOrder->sec_info.Series, Token, __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Symbol "<<AddOrder->sec_info.Symbol<<"|Series "<<AddOrder->sec_info.Series<<std::endl;
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
        
        
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        
        if (OrderResponse.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }


    SwapDouble((char*) &OrderResponse.OrderNumber);    
         
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
    
 
    long datareturn;

    int32_t lPrice = __bswap_32(OrderResponse.Price);
    int32_t lQty = __bswap_32(OrderResponse.Volume);
    int32_t DQty = __bswap_32(OrderResponse.DisclosedVolume);
    
    // ---- Store in Data info table for Trade        
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|IOC %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD, OrderNumber, OrderResponse.TransactionId, OrderResponse.OrderFlags.IOC,  DQty , lQty, lPrice, Token, __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<"|Qty "<< bookdetails.lQty<< std::endl;
  
   
    int i = 0;
    i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
    
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;    
    
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue,  tTotal);
    Logger::getLogger().log(DEBUG, logBuf);  
    
    
    gMETradeNo = gMETradeNo + 1;
    
    NSECM::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
    
    memcpy(&SendTradeConf.AccountNumber,&AddOrder->AccountNumber,sizeof(SendTradeConf.AccountNumber));
    SendTradeConf.BookType =AddOrder->BookType;
    SendTradeConf.DisclosedVolume = AddOrder->DisclosedVolume;
    SendTradeConf.Price = AddOrder->Price;
    memcpy(&SendTradeConf.sec_info, &AddOrder->sec_info,sizeof(SendTradeConf.sec_info));
    SendTradeConf.VolumeFilledToday = AddOrder->Volume;
    SendTradeConf.BuySellIndicator = AddOrder->BuySellIndicator;
    SendTradeConf.FillPrice = AddOrder->Price;    
    SendTradeConf.Timestamp1 =  MyTime;
    SendTradeConf.Timestamp = MyTime;
    SendTradeConf.Timestamp2 = '1'; /*Sneha*/
    
    int32_t LastModified = __bswap_32(OrderResponse.LastModified) + 1;
    
    SendTradeConf.ActivityTime  = __bswap_32(LastModified);
    SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
    SwapDouble((char*) &SendTradeConf.Timestamp);
    SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
    SendTradeConf.FillQuantity = AddOrder->Volume;
    SendTradeConf.RemainingVolume = 0;
    SendTradeConf.TransactionCode = __bswap_16(20222);
    SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
    SendTradeConf.tap_hdr.iSeqNo =  0;
    SendTradeConf.UserId = AddOrder->UserId;        
    SendTradeConf.ResponseOrderNumber = OrderResponse.OrderNumber;
    

   i = SendToClient( FD , (char *)&SendTradeConf , sizeof(SendTradeConf),pConnInfo);
   
  snprintf (logBuf, 500, "Thread_ME|FD %d|Trade|Order# %ld|Trade# %d|Qty %d|Price %d|Token %ld|VFT %d|LMT %d",
       FD,OrderNumber,gMETradeNo,__bswap_32(SendTradeConf.FillQuantity),__bswap_32(SendTradeConf.FillPrice), (Token), __bswap_32(SendTradeConf.VolumeFilledToday),
       __bswap_32(SendTradeConf.ActivityTime));
   Logger::getLogger().log(DEBUG, logBuf);
    
    
}

int AddnTradeOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {

  int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    int MyTime = GlobalSeqNo++;
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR OrderResponse;
   long Token = (__bswap_32(AddOrder->TokenNo) - FOOFFSET);    
   int dealerIndex, tokenIndex;
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD, 0, __bswap_16(AddOrder->BuySellIndicator), Token, NULL, NULL, dealerIndex, tokenIndex);
    if(OrderResponse.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.ErrorCode != 0)
      {
          
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, AddOrder->filler, OrderResponse.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
//      std::cout<<"ErrorCode::"<<OrderResponse.ErrorCode<<std::endl;
    }
    //    long OrderNumber = ME_OrderNumber++;
    int64_t OrderNumber = ME_OrderNumber++;
    
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.Timestamp1 =  getCurrentTimeInNano();
    OrderResponse.Timestamp1 = __bswap_64(OrderResponse.Timestamp1);/*sneha*/
    OrderResponse.Timestamp  = OrderResponse.Timestamp1;
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
    
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
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
    OrderResponse.ReasonCode = 0;
    OrderResponse.tap_hdr.swapBytes();       
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, OrderNumber, AddOrder->filler, OrderResponse.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Token "<<(Token+FOOFFSET)<<std::endl;
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_TR), pConnInfo);
    
    
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        if (OrderResponse.ErrorCode  == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    int32_t lPrice = __bswap_32(OrderResponse.Price);
    int32_t lQty = __bswap_32(OrderResponse.Volume);
    int32_t DQty = __bswap_32(OrderResponse.DisclosedVolume);
    
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|IOC %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD,OrderNumber,AddOrder->filler, bookdetails.IsIOC, DQty, lQty, lPrice,(Token+FOOFFSET), __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
      
    SwapDouble((char*) &OrderResponse.OrderNumber);    
         
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    int i = 0;
    
     i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;    
    
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue,  tTotal);
    Logger::getLogger().log(DEBUG, logBuf);  
    
    gMETradeNo = gMETradeNo + 1;
    
    NSEFO::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
    
    memcpy(&SendTradeConf.AccountNumber,&AddOrder->AccountNumber,sizeof(SendTradeConf.AccountNumber));
    SendTradeConf.BookType = AddOrder->BookType;
    SendTradeConf.DisclosedVolume = AddOrder->DisclosedVolume;
    SendTradeConf.Price = AddOrder->Price;
    SendTradeConf.Token = AddOrder->TokenNo;
    SendTradeConf.VolumeFilledToday = AddOrder->Volume;
    SendTradeConf.BuySellIndicator =AddOrder->BuySellIndicator;
    SendTradeConf.FillPrice = AddOrder->Price;
    SendTradeConf.Timestamp1 =  MyTime;
    SendTradeConf.Timestamp = MyTime;
//    SendTradeConf.Timestamp2 = '1'; 
    SendTradeConf.Timestamp2[7] = 1; 
    
    int32_t LastModified = __bswap_32(OrderResponse.LastModified) + 1;
    SendTradeConf.ActivityTime  = __bswap_32(LastModified);
    
    SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
    SwapDouble((char*) &SendTradeConf.Timestamp);
    SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
    SendTradeConf.FillQuantity = AddOrder->Volume;
    SendTradeConf.RemainingVolume = 0;
    SendTradeConf.TransactionCode = __bswap_16(20222);
    SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
    SendTradeConf.tap_hdr.iSeqNo =  0;
    SendTradeConf.ResponseOrderNumber = OrderResponse.OrderNumber;
    SendTradeConf.TraderId = AddOrder->TraderId;
     
    i = SendToClient( FD , (char *)&SendTradeConf , sizeof(SendTradeConf),pConnInfo);
    
   snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
       FD,OrderNumber,gMETradeNo,__bswap_32(SendTradeConf.FillQuantity),__bswap_32(SendTradeConf.FillPrice), (Token+FOOFFSET), __bswap_32(SendTradeConf.VolumeFilledToday),
       __bswap_32(SendTradeConf.ActivityTime));
   Logger::getLogger().log(DEBUG, logBuf);
    
}
/*Add Trade Functions Ends (NK)*/



int AddOrderTrim(NSECM::MS_OE_REQUEST_TR *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
//    int64_t tAfterEnqueue = getCurrentTimeInNano()-recvTime;
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    int MyTime = GlobalSeqNo++;
//    long OrderNumber = ME_OrderNumber++;
    int64_t OrderNumber = ME_OrderNumber++;
     
    ORDER_BOOK_DTLS bookdetails;
    long Token = 0;
    NSECM::MS_OE_RESPONSE_TR OrderResponse;
    int dealerIndex, tokenIndex;
    
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD,0, __bswap_16(AddOrder->BuySellIndicator), Token,(char *)& (AddOrder->sec_info.Symbol),(char *)&(AddOrder->sec_info.Series), dealerIndex, tokenIndex);
    //std::cout<<"Token = "<<Token<<std::endl;;
    
    if(OrderResponse.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.ErrorCode != 0)
      {
          
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, AddOrder->TransactionId, OrderResponse.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
    }
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.TimeStamp1 =  getCurrentTimeInNano();
    OrderResponse.TimeStamp1 = __bswap_64(OrderResponse.TimeStamp1); /*sneha*/
    OrderResponse.Timestamp  = OrderResponse.TimeStamp1;
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
    
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
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
    OrderResponse.ReasonCode = 0;
    
    OrderResponse.tap_hdr.swapBytes();           
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
     if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",FD, OrderNumber, AddOrder->TransactionId, OrderResponse.ErrorCode, AddOrder->sec_info.Symbol, AddOrder->sec_info.Series, Token, __bswap_32(OrderResponse.LastModified));
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
    bookdetails.connInfo = pConnInfo;
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(OrderResponse.TraderId);
     if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    memcpy(&bookdetails.PAN,&OrderResponse.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(OrderResponse.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(OrderResponse.AlgoCategory);
  
    
    bookdetails.TraderId = __bswap_32(OrderResponse.TraderId);
    bookdetails.BookType = __bswap_16(OrderResponse.BookType);
    bookdetails.BuySellIndicator = __bswap_16(OrderResponse.BuySellIndicator);
    bookdetails.Volume = __bswap_32(OrderResponse.Volume);
    bookdetails.BranchId = __bswap_16(OrderResponse.BranchId);
    bookdetails.UserId = __bswap_32(OrderResponse.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(OrderResponse.ProClient);
    bookdetails.nsecm_nsefo_nsecd.NSECM.TransactionId = __bswap_32(OrderResponse.TransactionId);
    bookdetails.nsecm_nsefo_nsecd.NSECM.Suspended = OrderResponse.Suspended;
    bookdetails.NnfField = OrderResponse.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.sec_info,&(AddOrder->sec_info),sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.sec_info));
    memcpy(&bookdetails.AccountNumber,&OrderResponse.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&OrderResponse.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&OrderResponse.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags,&OrderResponse.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags));
    bookdetails.LastModified = __bswap_32(OrderResponse.LastModified);
    
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECM::MS_OE_RESPONSE_TR));    
    
 
    long datareturn;

    // ---- Store in Data info table for Trade        
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD, OrderNumber, AddOrder->TransactionId, bookdetails.IsIOC, bookdetails.IsDQ,  bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, Token, __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<"|Qty "<< bookdetails.lQty<< std::endl;
  
    datareturn = Addtoorderbook(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator),Token,IsIOC,IsDQ,IsSL, __bswap_constant_32(OrderResponse.LastModified), dealerIndex, tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
   
    int i = 0;
    i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSECM::MS_OE_RESPONSE_TR), pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
      
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
        SendOrderCancellation_NSECM(&bookdetails,Token,FD,pConnInfo, 0, false);
    }    
  
    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);
}

int AddOrderTrim(NSEFO::MS_OE_REQUEST_TR *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
//    int64_t tAfterEnqueue = getCurrentTimeInNano()-recvTime;
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    int MyTime = GlobalSeqNo++;
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR OrderResponse;
    long Token = (__bswap_32(AddOrder->TokenNo) - FOOFFSET);    
    int dealerIndex, tokenIndex;
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD, 0, __bswap_16(AddOrder->BuySellIndicator), Token, NULL, NULL, dealerIndex, tokenIndex);
    
    if(OrderResponse.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.ErrorCode != 0)
      {
         
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, AddOrder->filler, OrderResponse.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    //    long OrderNumber = ME_OrderNumber++;
    int64_t OrderNumber = ME_OrderNumber++;
    
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.Timestamp1 =  getCurrentTimeInNano();
    OrderResponse.Timestamp1 = __bswap_64(OrderResponse.Timestamp1);/*sneha*/
    OrderResponse.Timestamp  = OrderResponse.Timestamp1;
    OrderResponse.Timestamp2 = '1'; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemaining = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemaining = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
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
    OrderResponse.ReasonCode = 0;
    OrderResponse.tap_hdr.swapBytes();       
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, OrderNumber, AddOrder->filler, OrderResponse.ErrorCode, (Token+FOOFFSET), __bswap_32(OrderResponse.LastModified));
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
    bookdetails.connInfo = pConnInfo;
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.dealerID = __bswap_32(OrderResponse.TraderId);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
     if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    /*pan card changes*/
    memcpy(&bookdetails.PAN,&OrderResponse.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(OrderResponse.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(OrderResponse.AlgoCategory);
    /*pan card changes ends*/
    
    bookdetails.TraderId = __bswap_32(OrderResponse.TraderId);
    bookdetails.BookType = __bswap_16(OrderResponse.BookType);
    bookdetails.BuySellIndicator = __bswap_16(OrderResponse.BuySellIndicator);
    bookdetails.Volume = __bswap_32(OrderResponse.Volume);
    bookdetails.BranchId = __bswap_16(OrderResponse.BranchId);
    bookdetails.UserId = __bswap_32(OrderResponse.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(OrderResponse.ProClientIndicator);
    bookdetails.nsecm_nsefo_nsecd.NSEFO.TokenNo = __bswap_32(OrderResponse.TokenNo);
    bookdetails.nsecm_nsefo_nsecd.NSEFO.filler = __bswap_32(OrderResponse.filler);
    bookdetails.NnfField = OrderResponse.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.AccountNumber,&OrderResponse.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&OrderResponse.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&OrderResponse.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags,&OrderResponse.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags));    
    bookdetails.LastModified = __bswap_32(OrderResponse.LastModified);
    
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD,OrderNumber,AddOrder->filler, bookdetails.IsIOC,  bookdetails.IsDQ, bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice,(Token+FOOFFSET), __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
    long datareturn = Addtoorderbook(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator), (__bswap_32(AddOrder->TokenNo) - FOOFFSET) /*1 Please replace token number here*/,IsIOC,IsDQ,IsSL , __bswap_constant_32(OrderResponse.LastModified), dealerIndex, tokenIndex);
   
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    // ---- Store in Data info table for Trade
    long ArrayIndex = (long)OrderResponse.OrderNumber;
      
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    //OrderResponse.OrderNumber =  Ord_No;     
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSEFO::MS_OE_RESPONSE_TR));    
    
    int i = 0;
    
     i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;    
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;    
    
    //std::cout << "MS_OE_RESPONSE_TR :: Order Number "  <<  ME_OrderNumber <<  "  Bytes Sent " << i << std::endl;    
    /*Sneha*/
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
      /*IOC tick: changed sendBrdcst from true to false*/
        SendOrderCancellation_NSEFO(&bookdetails,(__bswap_32(AddOrder->TokenNo) - FOOFFSET),FD,pConnInfo, 0, false);
    } 
    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder((__bswap_32(AddOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching((__bswap_32(AddOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;    
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}


int AddOrderTrim(NSECD::MS_OE_REQUEST_TR *AddOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
//    int64_t tAfterEnqueue = getCurrentTimeInNano()-recvTime;
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    int MyTime = GlobalSeqNo++;
    ORDER_BOOK_DTLS bookdetails;
    NSECD::MS_OE_RESPONSE_TR OrderResponse;
    long Token = __bswap_32(AddOrder->TokenNo);    
    int dealerIndex, tokenIndex;
    OrderResponse.ErrorCode = ValidateAddReq(__bswap_32(AddOrder->TraderId), FD, 0, __bswap_16(AddOrder->BuySellIndicator), Token, NULL, NULL, dealerIndex, tokenIndex);
    
    if(OrderResponse.ErrorCode == 0 && bEnableValMsg)
    {
      OrderResponse.ErrorCode = ValidateChecksum((char*)AddOrder);
      if(OrderResponse.ErrorCode != 0)
      {
         
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, AddOrder->filler, OrderResponse.ErrorCode, Token, __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    //    long OrderNumber = ME_OrderNumber++;
    int64_t OrderNumber = ME_OrderNumber++;
    
    memcpy(&OrderResponse.tap_hdr,& AddOrder->tap_hdr, sizeof(OrderResponse.tap_hdr));
    OrderResponse.TransactionCode = __bswap_16(20073);
    OrderResponse.LogTime = __bswap_16(1);
    OrderResponse.TraderId = AddOrder->TraderId;
    //OrderResponse.ErrorCode = 0;
    OrderResponse.Timestamp1 =  getCurrentTimeInNano();
    OrderResponse.Timestamp1 = __bswap_64(OrderResponse.Timestamp1);/*sneha*/
    OrderResponse.Timestamp  = OrderResponse.Timestamp1;
    OrderResponse.Timestamp2 = '1'; /*sneha*/
    OrderResponse.BookType = AddOrder->BookType;
    memcpy(&OrderResponse.AccountNumber,&AddOrder->AccountNumber,sizeof(OrderResponse.AccountNumber));
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    memcpy(&OrderResponse.BuySellIndicator,&AddOrder->BuySellIndicator,sizeof(OrderResponse.BuySellIndicator));
    OrderResponse.DisclosedVolume = AddOrder->DisclosedVolume;
    OrderResponse.DisclosedVolumeRemaining = AddOrder->DisclosedVolume;
    OrderResponse.TotalVolumeRemaining = AddOrder->Volume;
    OrderResponse.Volume = AddOrder->Volume;
    OrderResponse.VolumeFilledToday = 0;
    OrderResponse.Price = AddOrder->Price;
    memcpy(&OrderResponse.PAN,&AddOrder->PAN,sizeof(OrderResponse.PAN));
    OrderResponse.AlgoCategory = AddOrder->AlgoCategory;
    OrderResponse.AlgoId = AddOrder->AlgoId;
    OrderResponse.EntryDateTime = __bswap_32(getEpochTime());
    OrderResponse.LastModified = OrderResponse.EntryDateTime;
    memcpy(&OrderResponse.OrderFlags,& AddOrder->OrderFlags, sizeof(OrderResponse.OrderFlags));
    OrderResponse.BranchId = AddOrder->BranchId;
    OrderResponse.UserId = AddOrder->UserId;        
    memcpy(&OrderResponse.BrokerId,&AddOrder->BrokerId,sizeof(OrderResponse.BrokerId)) ;      
    //OrderResponse.Suspended = AddOrder->Suspended;       
    memcpy(&OrderResponse.Settlor,&AddOrder->Settlor,sizeof(OrderResponse.Settlor));
    OrderResponse.ProClientIndicator = AddOrder->ProClientIndicator;
    
    OrderResponse.TokenNo = AddOrder->TokenNo;
    OrderResponse.NnfField = AddOrder->NnfField;       
    OrderResponse.filler = AddOrder->filler;
    OrderResponse.OrderNumber = OrderNumber;
    OrderResponse.ReasonCode = 0;
    OrderResponse.tap_hdr.swapBytes();       
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECD::MS_OE_RESPONSE_TR));    
    
    if (OrderResponse.ErrorCode != 0)
    {
        SwapDouble((char*) &OrderResponse.OrderNumber);    
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, OrderNumber, AddOrder->filler, OrderResponse.ErrorCode, Token, __bswap_32(OrderResponse.LastModified));
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|ErrorCode "<<OrderResponse.ErrorCode<<"|Token "<<(Token+FOOFFSET)<<std::endl;
        OrderResponse.ErrorCode = __bswap_16(OrderResponse.ErrorCode);
        SendToClient( FD , (char *)&OrderResponse , sizeof(NSECD::MS_OE_RESPONSE_TR), pConnInfo);
    
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
    bookdetails.connInfo = pConnInfo;
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    bookdetails.OrderNo =  OrderResponse.OrderNumber;
    bookdetails.lPrice = __bswap_32(OrderResponse.Price);
    bookdetails.lQty = __bswap_32(OrderResponse.Volume);
    bookdetails.dealerID = __bswap_32(OrderResponse.TraderId);
    bookdetails.DQty = __bswap_32(OrderResponse.DisclosedVolume);
    if(IsIOC == 1)
    {
        bookdetails.IsIOC = 1;
    } 
     if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    /*pan card changes*/
    memcpy(&bookdetails.PAN,&OrderResponse.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(OrderResponse.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(OrderResponse.AlgoCategory);
    /*pan card changes ends*/
    
    bookdetails.TraderId = __bswap_32(OrderResponse.TraderId);
    bookdetails.BookType = __bswap_16(OrderResponse.BookType);
    bookdetails.BuySellIndicator = __bswap_16(OrderResponse.BuySellIndicator);
    bookdetails.Volume = __bswap_32(OrderResponse.Volume);
    bookdetails.BranchId = __bswap_16(OrderResponse.BranchId);
    bookdetails.UserId = __bswap_32(OrderResponse.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(OrderResponse.ProClientIndicator);
    bookdetails.nsecm_nsefo_nsecd.NSECD.TokenNo = __bswap_32(OrderResponse.TokenNo);
    bookdetails.nsecm_nsefo_nsecd.NSECD.filler = __bswap_32(OrderResponse.filler);
    bookdetails.NnfField = OrderResponse.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.AccountNumber,&OrderResponse.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&OrderResponse.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&OrderResponse.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECD.OrderFlags,&OrderResponse.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECD.OrderFlags));    
    bookdetails.LastModified = __bswap_32(OrderResponse.LastModified);
    
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d", 
      FD,OrderNumber,AddOrder->filler, bookdetails.IsIOC,  bookdetails.IsDQ, bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, Token, __bswap_16(OrderResponse.BuySellIndicator),
      __bswap_32(OrderResponse.LastModified));
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    //std::cout<<"FD "<<FD<<"|ADD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
    long datareturn = Addtoorderbook(&bookdetails,__bswap_16(OrderResponse.BuySellIndicator), __bswap_32(AddOrder->TokenNo) /*1 Please replace token number here*/,IsIOC,IsDQ,IsSL , __bswap_constant_32(OrderResponse.LastModified), dealerIndex, tokenIndex);
   
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    // ---- Store in Data info table for Trade
    long ArrayIndex = (long)OrderResponse.OrderNumber;
      
    SwapDouble((char*) &OrderResponse.OrderNumber);    
    //OrderResponse.OrderNumber =  Ord_No;     
    
    OrderResponse.tap_hdr.sLength = __bswap_16(sizeof(NSECD::MS_OE_RESPONSE_TR));    
    
    int i = 0;
    
     i = SendToClient( FD , (char *)&OrderResponse , sizeof(NSECD::MS_OE_RESPONSE_TR),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;    
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&OrderResponse, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;    
    
    //std::cout << "MS_OE_RESPONSE_TR :: Order Number "  <<  ME_OrderNumber <<  "  Bytes Sent " << i << std::endl;    
    /*Sneha*/
    if(datareturn ==5 && bookdetails.IsIOC == 1 )
    {
      /*IOC tick: changed sendBrdcst from true to false*/
        SendOrderCancellation_NSECD(&bookdetails,(__bswap_32(AddOrder->TokenNo) ),FD,pConnInfo, 0, false);
    } 
    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder((__bswap_32(AddOrder->TokenNo)),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching((__bswap_32(AddOrder->TokenNo)),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;    
    snprintf(logBuf, 500, "Thread_ME|LATENCY|ADD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}

long Addtoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime, int dealerIndx, int tokenIndx) // 1 Buy , 2 Sell
{
    // Enqueue Broadcast Packet 
   /*IOC tick: added IOC!=1 check*/
    int64_t t1=0,t2=0,t3=0,t4=0;
    GET_PERF_TIME(t1);
    int32_t iNewOrdLocn = 0;
    int32_t iLogRecs = 0;
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC) && gSimulatorMode == 1)
    {
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
          if (_nSegMode == SEG_NSECD){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet               
    
    if (BuySellSide == 1) {
      iLogRecs = ME_OrderBook.OrderBook[tokenIndx].BuyRecords;
        // Start Handling IOC Order -------------------------------------------------
           if(Mybookdetails->lPrice < ME_OrderBook.OrderBook[tokenIndx].Sell[0].lPrice &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }    
        // End Handling IOC Order -------------------------------------------------        
      
        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].IsIOC = Mybookdetails->IsIOC; 
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].OrderNo = Mybookdetails->OrderNo;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lPrice = Mybookdetails->lPrice;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].TriggerPrice = Mybookdetails->TriggerPrice;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].lQty = Mybookdetails->lQty;
               ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].SeqNo = GlobalSeqNo++;
               ME_OrderBook.OrderBook[tokenIndx].BuySeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
           }

           /*Sneha - multiple connection changes:15/07/16*/
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].TTQ = 0;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].TraderId = Mybookdetails->TraderId;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].BookType = Mybookdetails->BookType;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].Volume = Mybookdetails->Volume;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].DQty = Mybookdetails->DQty;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].BranchId = Mybookdetails->BranchId;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].UserId = Mybookdetails->UserId;        
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].ProClientIndicator = Mybookdetails->ProClientIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].NnfField = Mybookdetails->NnfField; 
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].FD = Mybookdetails->FD;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].connInfo = Mybookdetails->connInfo;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].dealerID = Mybookdetails->dealerID;
          
          /*Pan card changes*/
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].AlgoId = Mybookdetails->AlgoId;
          ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].AlgoCategory = Mybookdetails->AlgoCategory ;
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
          /*Pan card changes end*/
          
          if (_nSegMode == SEG_NSECM){
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECM.sec_info));
           }
          else if(_nSegMode == SEG_NSEFO) {
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
           }
          else if(_nSegMode == SEG_NSECD) {
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECD.filler = Mybookdetails->nsecm_nsefo_nsecd.NSECD.filler;
                ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECD.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSECD.TokenNo;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECD.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECD.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].nsecm_nsefo_nsecd.NSECD.OrderFlags));
           }

           ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].LastModified = Mybookdetails->LastModified; 
           ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords].TransactionCode = NSECM_ADD_REQ_TR; /*IOC tick*/
           ME_OrderBook.OrderBook[tokenIndx].BuyRecords = ME_OrderBook.OrderBook[tokenIndx].BuyRecords + 1;
           
           #ifdef __LOG_ORDER_BOOK__
           snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|BuyOrderRecords %d|BookSize  %d",  
                       AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize);
           Logger::getLogger().log(DEBUG, logBuf);
           #endif
           
           /*Dynamic Book size allocation*/
           if(ME_OrderBook.OrderBook[tokenIndx].BuyRecords  > (ME_OrderBook.OrderBook[tokenIndx].BuyBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_OrderBook.OrderBook[tokenIndx].Buy;
              int Booksize = ME_OrderBook.OrderBook[tokenIndx].BuyBookSize;
              

              try
              {
                ME_OrderBook.OrderBook[tokenIndx].Buy = new ORDER_BOOK_DTLS[2*Booksize];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_OrderBook.OrderBook[tokenIndx].BuyBookFull = true;
                
              }
              if(false == ME_OrderBook.OrderBook[tokenIndx].BuyBookFull )
              {

                fillDataBuy(0,Booksize*2,ME_OrderBook.OrderBook[tokenIndx].Buy);
                memcpy(ME_OrderBook.OrderBook[tokenIndx].Buy,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;

                ME_OrderBook.OrderBook[tokenIndx].BuyBookSize = Booksize*2;
                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|BuyOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize/2,ME_OrderBook.OrderBook[tokenIndx].BuyBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
           
        memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1]), sizeof (ME_OrderBook.OrderBook[tokenIndx].Buy[ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1]));
      
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_OrderBook.OrderBook[tokenIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);

            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif
            
              for(int j=ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                
                if(ME_OrderBook.OrderBook[tokenIndx].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }
            
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Addtoorderbook|Buy|9|OldLcn %d|NewLcn %d", ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
            
              GET_PERF_TIME(t3);
//              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memmove(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].BuyRecords-1-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndx].Buy[iNewOrdLocn]));                
        
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].BuyRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);;
            }
            #endif 

           GET_PERF_TIME(t4);
//           if (ME_OrderBook.OrderBook[tokenIndx].BuyRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|BuyOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].BuyRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//           }
            if (dealerIndx != -1)
            {
            
               dealerOrdArr[dealerIndx][tokenIndx].buyordercnt++;
              
            }

     }  
    else {
        iLogRecs = ME_OrderBook.OrderBook[tokenIndx].SellRecords;
        
        // Start Handling IOC Order -------------------------------------------------
           if(((Mybookdetails->lPrice) > ME_OrderBook.OrderBook[tokenIndx].Buy[0].lPrice) &&  Mybookdetails->IsIOC == 1)
           {
               return 5; // 5 return means cancel IOC Order Immidiately- without adding in Order Book
           }           

        // Start Handling DQ Order -------------------------------------------------   
           if(Mybookdetails->IsDQ) /*Sneha*/
           {    
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsDQ = 1;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty; /*Sneha*/
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQRemaining = Mybookdetails->lQty - Mybookdetails->DQty ;                
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
                ME_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_OrderBook.OrderBook[tokenIndx].BuySeqNo + 1;
                //ME_OrderBook.OrderBook[tokenIndx].SellRecords = ME_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          // End Handling DQ Order -------------------------------------------------               
           }    
           else
           {    
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].IsIOC = Mybookdetails->IsIOC ; 
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].OrderNo = Mybookdetails->OrderNo;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lPrice = Mybookdetails->lPrice;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TriggerPrice = Mybookdetails->TriggerPrice;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].lQty = Mybookdetails->lQty;
              ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].SeqNo = GlobalSeqNo++;
              ME_OrderBook.OrderBook[tokenIndx].SellSeqNo = ME_OrderBook.OrderBook[tokenIndx].SellSeqNo + 1;
           }    
           /*Sneha - multiple connection changes:15/07/16*/
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TTQ=0;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TraderId = Mybookdetails->TraderId;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BookType = Mybookdetails->BookType;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BuySellIndicator = Mybookdetails->BuySellIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].Volume = Mybookdetails->Volume;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BranchId = Mybookdetails->BranchId;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].UserId = Mybookdetails->UserId;        
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].ProClientIndicator = Mybookdetails->ProClientIndicator;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].NnfField = Mybookdetails->NnfField;  
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].DQty = Mybookdetails->DQty;
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].FD = Mybookdetails->FD;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].connInfo = Mybookdetails->connInfo;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].dealerID = Mybookdetails->dealerID;
          
          /*Pan card changes*/
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].AlgoId = Mybookdetails->AlgoId;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].AlgoCategory = Mybookdetails->AlgoCategory ;
          memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
          /*Pan card changes end*/
          
            if (_nSegMode == SEG_NSECM){
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.TransactionId = Mybookdetails->nsecm_nsefo_nsecd.NSECM.TransactionId;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.sec_info,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSECM.sec_info));
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
           }
          else {
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.filler = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.filler;
                ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.TokenNo = Mybookdetails->nsecm_nsefo_nsecd.NSEFO.TokenNo;
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags));
          }
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].LastModified = Mybookdetails->LastModified;
          ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords].TransactionCode = NSECM_ADD_REQ_TR; /*IOC tick*/
          ME_OrderBook.OrderBook[tokenIndx].SellRecords = ME_OrderBook.OrderBook[tokenIndx].SellRecords + 1;
          
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|SellOrderRecords %d|BookSize %d",  
                       AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords,ME_OrderBook.OrderBook[tokenIndx].SellBookSize);
              Logger::getLogger().log(DEBUG, logBuf);
          #endif 

          /*Dynamic Book size allocation*/
           if(ME_OrderBook.OrderBook[tokenIndx].SellRecords  > (ME_OrderBook.OrderBook[tokenIndx].SellBookSize*gBookSizeThresholdPer)/100)
           {
              ORDER_BOOK_DTLS *temp ;
              temp = ME_OrderBook.OrderBook[tokenIndx].Sell;
              int Booksize = ME_OrderBook.OrderBook[tokenIndx].SellBookSize;
              try
              {
                ME_OrderBook.OrderBook[tokenIndx].Sell = new ORDER_BOOK_DTLS[Booksize*2];
              }
              catch (std::bad_alloc& ba)
              {
                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d Failed|MEMORY NOT AVAILABLE",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords,ME_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
                temp = NULL;
                ME_OrderBook.OrderBook[tokenIndx].SellBookFull = true;
              }
              
              if(false == ME_OrderBook.OrderBook[tokenIndx].SellBookFull)
              {
                fillDataSell(0,Booksize*2,ME_OrderBook.OrderBook[tokenIndx].Sell);

                memcpy(ME_OrderBook.OrderBook[tokenIndx].Sell,temp,sizeof(ORDER_BOOK_DTLS)*Booksize);
                delete []temp;
                ME_OrderBook.OrderBook[tokenIndx].SellBookSize = Booksize*2;

                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|SellOrderRecords %d|BookSize increased from %d to %d",  
                         AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords,ME_OrderBook.OrderBook[tokenIndx].SellBookSize/2,ME_OrderBook.OrderBook[tokenIndx].SellBookSize);
                Logger::getLogger().log(DEBUG, logBuf);
              }
               
           }
           /*Dynamic Book size allocation end*/
          
          
        memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords-1]), sizeof (ME_OrderBook.OrderBook[tokenIndx].Sell[ME_OrderBook.OrderBook[tokenIndx].SellRecords-1]));        
        GET_PERF_TIME(t2);
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Addtoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|TokenIndex %d|", ME_OrderBook.OrderBook[tokenIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo, tokenIndx);
            Logger::getLogger().log(DEBUG, logBuf);
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }  
            #endif  
          
              for(int j=ME_OrderBook.OrderBook[tokenIndx].SellRecords-1; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Addtoorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndx].Sell[j-1].lPrice > Mybookdetails->lPrice)                  
                {
                  continue;
                }
                else
                {
                  iNewOrdLocn = j;
                  break;
                }
              }

              #ifdef __LOG_ORDER_BOOK__            
              snprintf(logBuf, 500, "Addtoorderbook|Sell|9|OldLcn %d|NewLcn %d", ME_OrderBook.OrderBook[tokenIndx].SellRecords-1, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif

             GET_PERF_TIME(t3);
              //memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memmove(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn])*(ME_OrderBook.OrderBook[tokenIndx].SellRecords-1-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndx].Sell[iNewOrdLocn]));                
            #ifdef __LOG_ORDER_BOOK__
            for(int j=0; j<ME_OrderBook.OrderBook[tokenIndx].SellRecords; j++)
            {
              snprintf(logBuf, 500, "Addtoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            #endif
          GET_PERF_TIME(t4);
//          if (ME_OrderBook.OrderBook[tokenIndx].SellRecords >= BOOKSIZE){
//                snprintf(logBuf, 500, "Thread_ME|Addtoorderbook|Token %d|SellOrderRecords %d",  
//                                                      AddModCan.stBcastMsg.stGegenricOrdMsg.nToken, ME_OrderBook.OrderBook[tokenIndx].SellRecords);
//               Logger::getLogger().log(DEBUG, logBuf);
//            }
            if (dealerIndx != -1)
            {
                dealerOrdArr[dealerIndx][tokenIndx].sellordercnt++;
             }
     }    
    snprintf(logBuf, 200, "Thread_ME|Addtoorderbook|Recs %6d|Search=%6ld|Position Search=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
      
    return 0; 
}

int  SendDnldData(int32_t TraderId, int64_t SeqNo, int Fd,CONNINFO*  pConnInfo)
{
    FILE* infile = NULL;
    char filename[20] = {"\0"}; 
    sprintf (filename, "%d", TraderId);
    strcat(filename, "Dnld");
    SeqNo = __bswap_64(SeqNo);
    snprintf(logBuf, 500, "Thread_ME|FD %d|MsgDnld Req|UserId %s|SeqNo %ld", Fd, filename, SeqNo);
    Logger::getLogger().log(DEBUG, logBuf);
   
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

int ValidateUser(int32_t iUserID, int fd, int& dealerIndex)
{
     int ret = 0;
     dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
      if (itr != dealerInfoMapGlobal->end())
     {
            IP_STATUS* pIPSsts = itr->second;
            dealerIndex = pIPSsts->dealerOrdIndex;
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

bool getConnInfo(int iUserID, dealerInfoItr& itr)
{
     bool found = false;
      itr = dealerInfoMapGlobal->find(iUserID);
     if (itr != dealerInfoMapGlobal->end())
     {
           if ((itr->second)->status == LOGGED_ON)
           {
                /*Do nothing*/
           }
           else
           {
                snprintf(logBuf, 500, "Thread_ME|FD %d|getConnInfo|Dealer %d NOT Logged ON", (itr->second)->FD, iUserID);
                Logger::getLogger().log(DEBUG, logBuf);
           }
           found = true;
     }
     else
     {
          snprintf(logBuf, 500, "Thread_ME|getConnInfo|Dealer %d not found", iUserID);
          Logger::getLogger().log(DEBUG, logBuf);
          found = false;
     }
    return found;
}



int ProcessTranscodes(DATA_RECEIVED * RcvData,ProducerConsumerQueue<_DATA_RECEIVED>* Inqptr_MeToTCPServer)
{
    DATA_RECEIVED SendData;
    //CUSTOM_HEADER definition moved to All_Structures.h
    CUSTOM_HEADER *tapHdr=(CUSTOM_HEADER *)RcvData->msgBuffer;  
    tapHdr->swapBytes();    
    int transcode = tapHdr->sTransCode;
    int32_t COLDealerId = tapHdr->iSeqNo;
    int IsIOC = 0;
    int IsDQ = 0;
    int IsSL = 0;
    int tempIOC = 0;
    int tempSL = 0;
    tapHdr->swapBytes(); 
    
    switch(transcode)
    { 
      
        case BOX_SIGN_ON_REQUEST_IN:
        {
          switch (_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_BOX_SIGN_ON_REQUEST_IN *pBoxSignOnRequest = (NSECM::MS_BOX_SIGN_ON_REQUEST_IN *)RcvData->msgBuffer;
              NSECM::MS_BOX_SIGN_ON_REQUEST_OUT BoxSignOnResponse;
              CONNINFO* connSts = RcvData->ptrConnInfo;
              int Fd = RcvData->MyFd;
              snprintf(logBuf, 500, "Thread_ME|FD %d|BOX IN request|IP %s|wBoxId %d|cBrokerId %s|cSessionKey %s", Fd,connSts->IP, __bswap_16(pBoxSignOnRequest->wBoxId), pBoxSignOnRequest->cBrokerId,pBoxSignOnRequest->cSessionKey);
              Logger::getLogger().log(DEBUG, logBuf);
              
              BoxSignOnResponse.msg_hdr.TransactionCode = BOX_SIGN_ON_REQUEST_OUT;
              BoxSignOnResponse.msg_hdr.ErrorCode = 0;
              BoxSignOnResponse.msg_hdr.sLength = sizeof(NSECM::MS_BOX_SIGN_ON_REQUEST_OUT);
              BoxSignOnResponse.wBoxId  = pBoxSignOnRequest->wBoxId;
              int bytesWritten = 0;
              
              
              BoxSignOnResponse.msg_hdr.swapBytes();     
              bytesWritten = SendToClient( Fd , (char *)&BoxSignOnResponse , sizeof(NSECM::MS_BOX_SIGN_ON_REQUEST_OUT), (RcvData->ptrConnInfo));
              BoxSignOnResponse.msg_hdr.swapBytes(); 

              if (BoxSignOnResponse.msg_hdr.ErrorCode != 0)
              {
                   connSts->status = DISCONNECTED;
              }
              snprintf(logBuf, 500, "Thread_ME|FD %d|Box_REQUEST NSEFO %d|Error Code %d|Bytes Sent  %d",Fd,BoxSignOnResponse.msg_hdr.TransactionCode,BoxSignOnResponse.msg_hdr.ErrorCode, bytesWritten);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_BOX_SIGN_ON_REQUEST_IN *pBoxSignOnRequest = (NSEFO::MS_BOX_SIGN_ON_REQUEST_IN *)RcvData->msgBuffer;
              NSEFO::MS_BOX_SIGN_ON_REQUEST_OUT BoxSignOnResponse;
         
              CONNINFO* connSts = RcvData->ptrConnInfo;
              int Fd = RcvData->MyFd;
              snprintf(logBuf, 500, "Thread_ME|FD %d|BOX IN request|IP %s|wBoxId %d|cBrokerId %s|cSessionKey %s", Fd,connSts->IP, __bswap_16(pBoxSignOnRequest->wBoxId), pBoxSignOnRequest->cBrokerId,pBoxSignOnRequest->cSessionKey);
              Logger::getLogger().log(DEBUG, logBuf);
  
              BoxSignOnResponse.msg_hdr.TransactionCode = BOX_SIGN_ON_REQUEST_OUT;
              BoxSignOnResponse.msg_hdr.ErrorCode = 0;
              BoxSignOnResponse.msg_hdr.sLength = sizeof(NSEFO::MS_BOX_SIGN_ON_REQUEST_OUT);
              BoxSignOnResponse.wBoxId  = pBoxSignOnRequest->wBoxId;
              
              int bytesWritten = 0;
              
              BoxSignOnResponse.msg_hdr.swapBytes(); 
              
              bytesWritten = SendToClient( Fd , (char *)&BoxSignOnResponse , sizeof(NSEFO::MS_BOX_SIGN_ON_REQUEST_OUT), (RcvData->ptrConnInfo));
              BoxSignOnResponse.msg_hdr.swapBytes(); 

              if (BoxSignOnResponse.msg_hdr.ErrorCode != 0)
              {
                   connSts->status = DISCONNECTED;
              }
              snprintf(logBuf, 500, "Thread_ME|FD %d|GR_REQUEST NSEFO %d|Error Code %d|Bytes Sent  %d",Fd,BoxSignOnResponse.msg_hdr.TransactionCode,BoxSignOnResponse.msg_hdr.ErrorCode, bytesWritten);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            break;
            case SEG_NSECD:
            {
              NSECD::MS_BOX_SIGN_ON_REQUEST_IN *pBoxSignOnRequest = (NSECD::MS_BOX_SIGN_ON_REQUEST_IN *)RcvData->msgBuffer;
              NSECD::MS_BOX_SIGN_ON_REQUEST_OUT BoxSignOnResponse;
         
              CONNINFO* connSts = RcvData->ptrConnInfo;
              int Fd = RcvData->MyFd;
              snprintf(logBuf, 500, "Thread_ME|FD %d|BOX IN request|IP %s|wBoxId %d|cBrokerId %s|cSessionKey %s", Fd,connSts->IP, __bswap_16(pBoxSignOnRequest->wBoxId), pBoxSignOnRequest->cBrokerId,pBoxSignOnRequest->cSessionKey);
              Logger::getLogger().log(DEBUG, logBuf);
  
              BoxSignOnResponse.msg_hdr.TransactionCode = BOX_SIGN_ON_REQUEST_OUT;
              BoxSignOnResponse.msg_hdr.ErrorCode = 0;
              BoxSignOnResponse.msg_hdr.sLength = sizeof(NSECD::MS_BOX_SIGN_ON_REQUEST_OUT);
              BoxSignOnResponse.wBoxId  = pBoxSignOnRequest->wBoxId;
              
              int bytesWritten = 0;
              
              BoxSignOnResponse.msg_hdr.swapBytes(); 
              
              bytesWritten = SendToClient( Fd , (char *)&BoxSignOnResponse , sizeof(NSECD::MS_BOX_SIGN_ON_REQUEST_OUT), (RcvData->ptrConnInfo));
              BoxSignOnResponse.msg_hdr.swapBytes(); 

              if (BoxSignOnResponse.msg_hdr.ErrorCode != 0)
              {
                   connSts->status = DISCONNECTED;
              }
              snprintf(logBuf, 500, "Thread_ME|FD %d|GR_REQUEST NSECD %d|Error Code %d|Bytes Sent  %d",Fd,BoxSignOnResponse.msg_hdr.TransactionCode,BoxSignOnResponse.msg_hdr.ErrorCode, bytesWritten);
              Logger::getLogger().log(DEBUG, logBuf);
            }
            break;
            default:
                break;
          }

        }
        break;
        case SIGN_ON_REQUEST_IN:
        {
            bool result = false;
            gPFSWithClient=1;
            
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
              
              case SEG_NSECD:
              {
                NSECD::MS_SIGNON_RESP SignonResp;
                NSECD::MS_SIGNON_REQ *SignonReq=(NSECD::MS_SIGNON_REQ *)RcvData->msgBuffer; 
                
                SignonResp.msg_hdr.TransactionCode = SIGN_ON_REQUEST_OUT;
                SignonResp.msg_hdr.ErrorCode = 0;
                SignonResp.msg_hdr.TimeStamp2[7] = 1;  
                SignonResp.msg_hdr.sLength = sizeof(NSECD::MS_SIGNON_RESP);
                int Fd = RcvData->MyFd;
                 /*Dealer Validation - S*/
                CONNINFO* connSts = RcvData->ptrConnInfo;
                int32_t iUserID = __bswap_32(SignonReq->UserId);
                dealerInfoItr itr = dealerInfoMapGlobal->find(iUserID);
                
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN ON request|UserID %d|IP %s", Fd, iUserID, connSts->IP);
                Logger::getLogger().log(DEBUG, logBuf);
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
                      SignonResp.msg_hdr.ErrorCode = ERR_USER_ALREADY_SIGNED_ON;
                    }
                  }
                  else
                  {
                       snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: IP do not match|%s|%s",Fd,connSts->IP,pIPSsts->IP);
                       Logger::getLogger().log(DEBUG, logBuf);
                       SignonResp.msg_hdr.ErrorCode = ERR_INVALID_USER_ID;
                  }
                }
                else
                {
                  snprintf(logBuf, 500, "Thread_ME|FD %d|ERROR: Dealer not found",Fd); 
                  Logger::getLogger().log(DEBUG, logBuf);
                  SignonResp.msg_hdr.ErrorCode = ERR_USER_NOT_FOUND;
                }
                /*Dealer Validation - E*/
                
                SignonResp.msg_hdr.swapBytes();     
                
                int bytesWritten = 0;

                bytesWritten = SendToClient( Fd , (char *)&SignonResp , sizeof(NSECD::MS_SIGNON_RESP), (RcvData->ptrConnInfo));
                SignonResp.msg_hdr.swapBytes();    
                if (SignonResp.msg_hdr.ErrorCode != 0)
                {
                  connSts->status = DISCONNECTED;
                }
                snprintf(logBuf, 500, "Thread_ME|FD %d|SIGN_ON_REQUEST_IN NSECD %d|Error Code %d|Bytes Sent %d",Fd,SignonResp.msg_hdr.TransactionCode,SignonResp.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                
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
              int bytesWritten = 0;
              System_Info.msg_hdr.AlphaChar[0] = 1;
              System_Info.msg_hdr.TimeStamp2[7]  = 1;
              System_Info.msg_hdr.TransactionCode = 1601;
              System_Info.msg_hdr.ErrorCode = 0;
              System_Info.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pSysReq->msg_hdr.TraderId), Fd, bytesWritten);
              System_Info.msg_hdr.sLength = sizeof(NSECM::MS_SYSTEM_INFO_DATA);
              System_Info.msg_hdr.swapBytes();
              
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
              int bytesWritten = 0;
              
              System_Info.msg_hdr.AlphaChar[0] = 1;
              System_Info.msg_hdr.TimeStamp2[7]  = 1;
              System_Info.msg_hdr.TransactionCode = 1601;
              System_Info.msg_hdr.ErrorCode = 0;
              System_Info.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pSysReq->msg_hdr.TraderId), Fd, bytesWritten);
              System_Info.msg_hdr.sLength = sizeof(NSEFO::MS_SYSTEM_INFO_DATA);
              System_Info.msg_hdr.swapBytes();
              
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
            
            case SEG_NSECD:
            {
              NSECD::MS_SYSTEM_INFO_DATA System_Info;//SYSTEM_INFORMATION_OUT(1601)
              NSECD::MS_SYSTEM_INFO_REQ* pSysReq = (NSECD::MS_SYSTEM_INFO_REQ*)RcvData->msgBuffer; 
              
              int Fd = RcvData->MyFd;
              int bytesWritten = 0;
              
              System_Info.msg_hdr.AlphaChar[0] = 1;
              System_Info.msg_hdr.TimeStamp2[7]  = 1;
              System_Info.msg_hdr.TransactionCode = 1601;
              System_Info.msg_hdr.ErrorCode = 0;
              System_Info.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pSysReq->msg_hdr.TraderId), Fd, bytesWritten);
              System_Info.msg_hdr.sLength = sizeof(NSECD::MS_SYSTEM_INFO_DATA);
              System_Info.msg_hdr.swapBytes();
              
              bytesWritten = SendToClient( Fd , (char *)&System_Info , sizeof(NSECD::MS_SYSTEM_INFO_DATA),(RcvData->ptrConnInfo));
              System_Info.msg_hdr.swapBytes();                 
              snprintf(logBuf, 500, "Thread_ME|FD %d|SYSTEM_INFORMATION_IN NSECD %d|Error Code %d|Bytes Sent %d",Fd,System_Info.msg_hdr.TransactionCode, System_Info.msg_hdr.ErrorCode, bytesWritten);
              Logger::getLogger().log(DEBUG, logBuf);
              if (System_Info.msg_hdr.ErrorCode != 0)
              {
                (RcvData->ptrConnInfo)->status = DISCONNECTED;
              }
              //std::cout<<"FD "<<Fd<<"|SYSTEM_INFORMATION_IN NSECD " <<System_Info.msg_hdr.TransactionCode<<"|Bytes Sent " << bytesWritten << std::endl;
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
                 int bytesWritten = 0;
                   
                 Update_Hdr.msg_hdr.AlphaChar[0] = 1;
                 Update_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                 Update_Hdr.msg_hdr.TransactionCode = UPDATE_LOCALDB_HEADER;
                 Update_Hdr.msg_hdr.ErrorCode = 0;
                 Update_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pUptDBReq->msg_hdr.TraderId), Fd, bytesWritten);
                 Update_Hdr.msg_hdr.sLength = sizeof(NSECM::UPDATE_LDB_HEADER);
                 Update_Hdr.msg_hdr.swapBytes();
                
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
                 int bytesWritten = 0;
                 
                 Exchg_Portf_Resp.msg_hdr.AlphaChar[0] = 1;
                 Exchg_Portf_Resp.msg_hdr.TimeStamp2[7]  = 1;
                 Exchg_Portf_Resp.msg_hdr.TransactionCode = EXCH_PORTF_OUT;
                 Exchg_Portf_Resp.msg_hdr.ErrorCode = 0;
                 Exchg_Portf_Resp.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pPortFolioReq->msg_hdr.TraderId),Fd, bytesWritten);
                 Exchg_Portf_Resp.msg_hdr.sLength= sizeof(NSEFO::EXCH_PORTFOLIO_RESP);

                 Exchg_Portf_Resp.MoreRecs = 'N';
                 Exchg_Portf_Resp.msg_hdr.swapBytes();
              
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
                int bytesWritten = 0;
                  
                Industry_Dload_index.msg_hdr.TimeStamp2[7]  = 1;
                Industry_Dload_index.msg_hdr.TransactionCode = INDUSTRY_INDEX_DLOAD_OUT;
                Industry_Dload_index.msg_hdr.ErrorCode = 0;
                Industry_Dload_index.msg_hdr.ErrorCode = ValidateUser(__bswap_32(pIndusDnldReq->msg_hdr.TraderId), Fd, bytesWritten);
                Industry_Dload_index.msg_hdr.sLength = sizeof(NSECM::MS_INDUSTRY_INDEX_DLOAD_RESP);
                Industry_Dload_index.NumberOfRecords =__bswap_16(1);
                strcpy(Industry_Dload_index.Index_Dload_Data[0].IndexName,"S & P NIFTY");
                Industry_Dload_index.Index_Dload_Data[0].IndexValue = 0;
                Industry_Dload_index.Index_Dload_Data[0].IndustryCode = 0;  
                Industry_Dload_index.msg_hdr.swapBytes();
               
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
                int bytesWritten = 0;
                
                Dload_Hdr.msg_hdr.AlphaChar[0] = 1;
                Dload_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                Dload_Hdr.msg_hdr.TransactionCode = DOWNLOAD_HEADER;
                Dload_Hdr.msg_hdr.ErrorCode = 0;
                Dload_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(MsgDnldRequest->msg_hdr.TraderId), Fd, bytesWritten);
                Dload_Hdr.msg_hdr.sLength = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_HEADER);
                Dload_Hdr.msg_hdr.swapBytes();
     
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
                int bytesWritten = 0;
                
                Dload_Hdr.msg_hdr.AlphaChar[0] = 1;
                Dload_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                Dload_Hdr.msg_hdr.TransactionCode = DOWNLOAD_HEADER;
                Dload_Hdr.msg_hdr.ErrorCode = 0;
                Dload_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(MsgDnldRequest->msg_hdr.TraderId), Fd, bytesWritten);
                Dload_Hdr.msg_hdr.sLength = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_HEADER);
                Dload_Hdr.msg_hdr.swapBytes();
                
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
                 //std::cout<<"SSendDnldData status|"<<(RcvData->ptrConnInfo)->status<<"|recordcount|"<<(RcvData->ptrConnInfo)->recordCnt<<std::endl;
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
              
              case SEG_NSECD:
              {
                NSECD::MS_MESSAGE_DOWNLOAD_HEADER Dload_Hdr; 
                NSECD::MS_MESSAGE_DOWNLOAD_REQ* MsgDnldRequest=(NSECD::MS_MESSAGE_DOWNLOAD_REQ *)RcvData->msgBuffer; 
                
                int Fd = RcvData->MyFd;
                int bytesWritten = 0;
                
                Dload_Hdr.msg_hdr.AlphaChar[0] = 1;
                Dload_Hdr.msg_hdr.TimeStamp2[7]  = 1;
                Dload_Hdr.msg_hdr.TransactionCode = DOWNLOAD_HEADER;
                Dload_Hdr.msg_hdr.ErrorCode = 0;
                Dload_Hdr.msg_hdr.ErrorCode = ValidateUser(__bswap_32(MsgDnldRequest->msg_hdr.TraderId), Fd, bytesWritten);
                Dload_Hdr.msg_hdr.sLength = sizeof(NSECD::MS_MESSAGE_DOWNLOAD_HEADER);
                Dload_Hdr.msg_hdr.swapBytes();
                
                bytesWritten = SendToClient( Fd , (char *)&Dload_Hdr , sizeof(NSECD::MS_MESSAGE_DOWNLOAD_HEADER),(RcvData->ptrConnInfo));
                Dload_Hdr.msg_hdr.swapBytes();      
                 snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_HEADER NSECD %d|Error Code %d|Bytes Sent %d", Fd, Dload_Hdr.msg_hdr.TransactionCode,Dload_Hdr.msg_hdr.ErrorCode, bytesWritten);
                 Logger::getLogger().log(DEBUG, logBuf);
                 if (Dload_Hdr.msg_hdr.ErrorCode != 0)
                 {
                     (RcvData->ptrConnInfo)->status = DISCONNECTED;
                 }
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_HEADER NSECD "<<Dload_Hdr.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl;  

                 /*Send Msg download data - S*/
                if ((RcvData->ptrConnInfo)->status == DISCONNECTED)
                {
                    return 0;
                }
                
                int32_t TraderId = __bswap_32(MsgDnldRequest->msg_hdr.TraderId);
                SendDnldData(TraderId, MsgDnldRequest->SeqNo, RcvData->MyFd,RcvData->ptrConnInfo);        
                 //std::cout<<"SSendDnldData status|"<<(RcvData->ptrConnInfo)->status<<"|recordcount|"<<(RcvData->ptrConnInfo)->recordCnt<<std::endl;
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
                Dload_Trail.msg_hdr.sLength = sizeof(NSECD::MS_MESSAGE_DOWNLOAD_TRAILER);
                Dload_Trail.msg_hdr.swapBytes();
                //int Fd = RcvData->MyFd;
               
                bytesWritten = SendToClient( Fd , (char *)&Dload_Trail , sizeof(NSECD::MS_MESSAGE_DOWNLOAD_TRAILER),(RcvData->ptrConnInfo));
                Dload_Trail.msg_hdr.swapBytes();
                snprintf(logBuf, 500, "Thread_ME|FD %d|MS_MESSAGE_DOWNLOAD_TRAILER NSECD %d|Error Code %d|Bytes Sent %d", Fd, Dload_Trail.msg_hdr.TransactionCode,Dload_Trail.msg_hdr.ErrorCode, bytesWritten);
                Logger::getLogger().log(DEBUG, logBuf);
                //std::cout<<"FD "<<Fd<<"|MS_MESSAGE_DOWNLOAD_TRAILER NSECD " <<Dload_Trail.msg_hdr.TransactionCode<<"|Bytes Sent "<<bytesWritten<<std::endl;
              }
              break;
              
              default:
              break;
            }
                         
        }
        break;
    case NSECM_ADD_REQ:
        {
          
         switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                NSECM::MS_OE_REQUEST *OrderRequest=(NSECM::MS_OE_REQUEST *)RcvData->msgBuffer;  
                //OrderRequest->tap_hdr.swapBytes();            
                int Fd = RcvData->MyFd;     
                CONNINFO* connSts = RcvData->ptrConnInfo;
                if(OrderRequest->st_order_flags.IOC == 1)
                {
                    IsIOC = 1;
                } 
            
                if(OrderRequest->st_order_flags.SL == 1)
                {
                    IsSL = 1;
                }             
                if(gSimulatorMode==2 || gSimulatorMode==3)
                {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support ADD SL|Invalid Transaction code received: %d",gSimulatorMode, transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
                } 
                AddOrderNonTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);

              }
              break;
              case SEG_NSEFO:
              {
                
                NSEFO::MS_OE_REQUEST *OrderRequest=(NSEFO::MS_OE_REQUEST *)RcvData->msgBuffer;  
                //OrderRequest->tap_hdr.swapBytes();            
                int Fd = RcvData->MyFd;     
                CONNINFO* connSts = RcvData->ptrConnInfo;
                
                if(OrderRequest->st_order_flags.IOC == 1)
                {
                    IsIOC = 1;
                } 
            
                if(OrderRequest->st_order_flags.SL == 1)
                {
                    IsSL = 1;
                }             
                if(gSimulatorMode==2 || gSimulatorMode==3)
                {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support ADD SL|Invalid Transaction code received: %d", gSimulatorMode,transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
                } 
                AddOrderNonTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
              }
              break;
              default:
              break;
            }
            
        }
        break;
    case NSECM_MOD_REQ:
        {
          switch(_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_OE_REQUEST *OrderRequest=(NSECM::MS_OE_REQUEST *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;   
               CONNINFO* connSts = RcvData->ptrConnInfo;
               
                if(OrderRequest->st_order_flags.IOC == 1)
                {
                    IsIOC = 1;
                } 
                
                if(OrderRequest->st_order_flags.SL == 1)
                {
                    IsSL = 1;
                }                 
            
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              if(gSimulatorMode==2 || gSimulatorMode==3)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support MOD SL|Invalid Transaction code received: %d",gSimulatorMode, transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              } 
              ModOrderNonTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_OE_REQUEST*OrderRequest=(NSEFO::MS_OE_REQUEST *)RcvData->msgBuffer;  
              
              int Fd = RcvData->MyFd;   
             CONNINFO* connSts = RcvData->ptrConnInfo;
             
                if(OrderRequest->st_order_flags.IOC == 1)
                {
                    IsIOC = 1;
                } 
                
                if(OrderRequest->st_order_flags.SL == 1)
                {
                    IsSL = 1;
                }                 
   
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              if(gSimulatorMode==2 || gSimulatorMode==3)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support MOD SL|Invalid Transaction code received: %d",gSimulatorMode, transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }            
              ModOrderNonTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
            }
            break;
            default:
            break;
          }
            
        }
        break;
    case NSECM_CAN_REQ:
        {
          switch(_nSegMode)
          {
            case SEG_NSECM:
            {
              NSECM::MS_OE_REQUEST *OrderRequest=(NSECM::MS_OE_REQUEST *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
               
              if(gSimulatorMode==2 || gSimulatorMode==3)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support CAN SL|Invalid Transaction code received: %d",gSimulatorMode, transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }            
              CanOrderNonTrim(OrderRequest, Fd, connSts, RcvData->recvTimeStamp);

            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_OE_REQUEST *OrderRequest=(NSEFO::MS_OE_REQUEST *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
              
              //AddOrderTrim(OrderRequest ,Fd);
              if(gSimulatorMode==2 || gSimulatorMode==3)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode %d doesn't support CAN SL|Invalid Transaction code received: %d", gSimulatorMode,transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }
              CanOrderNonTrim(OrderRequest, Fd, connSts, RcvData->recvTimeStamp);
            }
            break;
            default:
            break;
          }
            
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
                    
                if(gSimulatorMode==1 || gSimulatorMode==3)
                {
                  AddOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
                }
                else if(gSimulatorMode==2)
                {
                  AddnTradeOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
                }

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
 
                if(gSimulatorMode==1 || gSimulatorMode==3)
                {
                  AddOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
                }
                else if(gSimulatorMode==2)
                {
                  AddnTradeOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
                }
              }
              break;
              case SEG_NSECD:
              {
                NSECD::MS_OE_REQUEST_TR *OrderRequest=(NSECD::MS_OE_REQUEST_TR *)RcvData->msgBuffer;  
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
 
                AddOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
                
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
            
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support MOD|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }
              ModOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
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
   
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support MOD|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }          
              ModOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
            }
            break;
            case SEG_NSECD:
            {
              NSECD::MS_OM_REQUEST_TR *OrderRequest=(NSECD::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              
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
   
              SwapDouble((char*) &OrderRequest->OrderNumber);
              
              SwapDouble((char*) &OrderRequest->OrderNumber);
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support MOD|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }          
              ModOrderTrim(OrderRequest, Fd, IsIOC, IsDQ, IsSL, connSts, RcvData->recvTimeStamp);
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
               
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support CAN|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }           
              CanOrderTrim(OrderRequest, Fd, connSts, RcvData->recvTimeStamp);

            }
            break;
            case SEG_NSEFO:
            {
              NSEFO::MS_OM_REQUEST_TR *OrderRequest=(NSEFO::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
              
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support CAN|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }            
              CanOrderTrim(OrderRequest, Fd, connSts, RcvData->recvTimeStamp);
            }
            break;
            case SEG_NSECD:
            {
              NSECD::MS_OM_REQUEST_TR *OrderRequest=(NSECD::MS_OM_REQUEST_TR *)RcvData->msgBuffer;  
              //OrderRequest->tap_hdr.swapBytes();            
              int Fd = RcvData->MyFd;      
               CONNINFO* connSts = RcvData->ptrConnInfo;
              
              if(gSimulatorMode==2)
              {
                snprintf (logBuf, 500, "Thread_ME|ERROR| Simulator Mode 2 doesn't support CAN|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf);
                break;
              }            
              CanOrderTrim(OrderRequest, Fd, connSts, RcvData->recvTimeStamp);
            }
            break;
            default:
            break;
          }
            
        }
        break;
    case NSEFO_2LEG_ADD_REQ:
        {
          switch(_nSegMode)
          {
            case SEG_NSEFO:
            {
              NSEFO::MS_SPD_OE_REQUEST* pMLOrderReq = (NSEFO::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
              int Fd = RcvData->MyFd;      
              CONNINFO* connSts = RcvData->ptrConnInfo;
              if(gSimulatorMode==1)
              {
                AddMLOrder(pMLOrderReq, Fd, connSts);
              }
              else if(gSimulatorMode==2)
              {
                AddnTradeMLOrder(pMLOrderReq, Fd, connSts);
              }
            }
            break;
            case SEG_NSECD:
            {
              NSECD::MS_SPD_OE_REQUEST* pMLOrderReq = (NSECD::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
              int Fd = RcvData->MyFd;      
              CONNINFO* connSts = RcvData->ptrConnInfo;
              if(gSimulatorMode==1)
              {
                AddMLOrder(pMLOrderReq, Fd, connSts);
              }
              else if(gSimulatorMode==2)
              {
                AddnTradeMLOrder(pMLOrderReq, Fd, connSts);
              }
            }
            break;
          }
            

        }
        break;
    case NSEFO_3LEG_ADD_REQ:
        {
          switch(_nSegMode)
          {
            case SEG_NSEFO:
            {
              NSEFO::MS_SPD_OE_REQUEST* pMLOrderReq = (NSEFO::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
              int Fd = RcvData->MyFd;      
              CONNINFO* connSts = RcvData->ptrConnInfo;
              if(gSimulatorMode==1)
              {
                AddMLOrder(pMLOrderReq, Fd, connSts);
              }
              else if(gSimulatorMode==2)
              {
                AddnTradeMLOrder(pMLOrderReq, Fd, connSts);
              }
            }
            break;
            case SEG_NSECD:
            { 
              NSECD::MS_SPD_OE_REQUEST* pMLOrderReq = (NSECD::MS_SPD_OE_REQUEST*)(RcvData->msgBuffer);
              int Fd = RcvData->MyFd;      
              CONNINFO* connSts = RcvData->ptrConnInfo;
              if(gSimulatorMode==1)
              {
                AddMLOrder(pMLOrderReq, Fd, connSts);
              }
              else if(gSimulatorMode==2)
              {
                AddnTradeMLOrder(pMLOrderReq, Fd, connSts);
              }
            }
            break;
        }
      }
      break;
      case COL:
      {
        ProcessCOL(COLDealerId);  
      }
      break;
    case NSEFO_SPD_ADD_REQ:
    case NSEFO_SPD_MOD_REQ:
    case NSEFO_SPD_CAN_REQ:
    default:
        {
           if (EXCH_HEARTBEAT != transcode){
                snprintf (logBuf, 500, "Thread_ME|ERROR|Invalid Transaction code received: %d", transcode);
                Logger::getLogger().log(DEBUG, logBuf); 
           }
        }
        break;
    }
    return 0;
}

int SortBuySideBook(long tokenIndx)
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

    for( ; i < (ME_OrderBook.OrderBook[tokenIndx].BuyRecords) ; i++)        
    {
        for(int j=0; j< (ME_OrderBook.OrderBook[tokenIndx].BuyRecords ); j++)
        {    
           if(ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice < ME_OrderBook.OrderBook[tokenIndx].Buy[j+1].lPrice )
          {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[tokenIndx].Buy[j+1],sizeof(swpbookdtls));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[j+1],&ME_OrderBook.OrderBook[tokenIndx].Buy[j], sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[j]));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[j]));
           } 

            if((ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice == ME_OrderBook.OrderBook[tokenIndx].Buy[j+1].lPrice )&& (ME_OrderBook.OrderBook[tokenIndx].Buy[j].SeqNo > ME_OrderBook.OrderBook[tokenIndx].Buy[j+1].SeqNo ))
            {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[tokenIndx].Buy[j+1],sizeof(swpbookdtls));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[j+1],&ME_OrderBook.OrderBook[tokenIndx].Buy[j], sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[j]));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Buy[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[tokenIndx].Buy[j]));
            } 
        }    
    }  
    PrintBook(tokenIndx);
}

/*NK Passive begins*/

int SortBuySidePassiveBook(long tokenIndx)
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

    for( ; i < (ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords) ; i++)        
    {
        for(int j=0; j< (ME_Passive_OrderBook.OrderBook[tokenIndx].BuyRecords ); j++)
        {    
           if(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].TriggerPrice > ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1].TriggerPrice )
          {
                memcpy(&swpbookdtls,&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1],sizeof(swpbookdtls));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1],&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j], sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j]));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j],&swpbookdtls, sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j]));
           } 

            if((ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].TriggerPrice == ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1].TriggerPrice )&& (ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j].SeqNo > ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1].SeqNo ))
            {
                memcpy(&swpbookdtls,&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1],sizeof(swpbookdtls));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j+1],&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j], sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j]));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j],&swpbookdtls, sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Buy[j]));
            } 
        }    
    }  
    PrintPassiveBook(tokenIndx);
}

int SortSellSidePassiveBook(long tokenIndx)
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
   
    for( ; i < (ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords) ; i++)        
    {
        for(int j=0; j< (ME_Passive_OrderBook.OrderBook[tokenIndx].SellRecords); j++)
        {    
            if(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].TriggerPrice < ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1].TriggerPrice )
            {
                memcpy(&swpbookdtls,&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1],sizeof(swpbookdtls));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1],&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j], sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j]));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j],&swpbookdtls, sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j]));
            } 
            if((ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].TriggerPrice == ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1].TriggerPrice) && (ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j].SeqNo > ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1].SeqNo))
            {
                memcpy(&swpbookdtls,&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1],sizeof(swpbookdtls));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j+1],&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j], sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j]));
                memcpy(&ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j],&swpbookdtls, sizeof(ME_Passive_OrderBook.OrderBook[tokenIndx].Sell[j]));
            } 
        }    
    }  
    PrintPassiveBook(tokenIndx);
}



int PrintPassiveBook(long tokenIndx)
{       
    return 0;
    int BookDepth;
    if(ME_OrderBook.OrderBook[tokenIndx].BuyRecords > ME_OrderBook.OrderBook[tokenIndx].SellRecords)
    {
        BookDepth = ME_OrderBook.OrderBook[tokenIndx].BuyRecords;
    }   
    else
    {
        BookDepth = ME_OrderBook.OrderBook[tokenIndx].SellRecords;
    }    
            
    for(int j = 0 ; j < BookDepth ; j++)                
    {
            /*std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --" << "-- DQty --"   << "Buy Side Book" << std::endl;   
            std::cout << ME_OrderBook.OrderBook[tokenIndx].Buy[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].DQRemaining << "---- " 
                    << std::endl;  

            std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --"  << "-- DQty --"  << "Sell Side Book" << std::endl;           
            std::cout << ME_OrderBook.OrderBook[tokenIndx].Sell[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].DQRemaining << "---- " 
                    << std::endl;         */   
        
    } 
}
/*NK Passive Ends*/

int PrintBook(long tokenIndx)
{       
    return 0;
    int BookDepth;
    if(ME_OrderBook.OrderBook[tokenIndx].BuyRecords > ME_OrderBook.OrderBook[tokenIndx].SellRecords)
    {
        BookDepth = ME_OrderBook.OrderBook[tokenIndx].BuyRecords;
    }   
    else
    {
        BookDepth = ME_OrderBook.OrderBook[tokenIndx].SellRecords;
    }    
            
    for(int j = 0 ; j < BookDepth ; j++)                
    {
            /*std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --" << "-- DQty --"   << "Buy Side Book" << std::endl;   
            std::cout << ME_OrderBook.OrderBook[tokenIndx].Buy[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[tokenIndx].Buy[j].DQRemaining << "---- " 
                    << std::endl;  

            std::cout << "-- Seq No -- " << "-- Ord No -- " << "-- Price -- " << "-- Qty --"  << "-- DQty --"  << "Sell Side Book" << std::endl;           
            std::cout << ME_OrderBook.OrderBook[tokenIndx].Sell[j].SeqNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice << "---- "
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty << "---- " 
                    << ME_OrderBook.OrderBook[tokenIndx].Sell[j].DQRemaining << "---- " 
                    << std::endl;         */   
        
    } 
}

int SortSellSideBook(long tokenIndx)
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
    
    for( ; i < (ME_OrderBook.OrderBook[tokenIndx].SellRecords) ; i++)        
    {
        for(int j=0; j< (ME_OrderBook.OrderBook[tokenIndx].SellRecords); j++)
        {   
            if(ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice > ME_OrderBook.OrderBook[tokenIndx].Sell[j+1].lPrice )
            {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[tokenIndx].Sell[j+1],sizeof(swpbookdtls));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[j+1],&ME_OrderBook.OrderBook[tokenIndx].Sell[j], sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[j]));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[j]));
            } 
            if((ME_OrderBook.OrderBook[tokenIndx].Sell[j].lPrice == ME_OrderBook.OrderBook[tokenIndx].Sell[j+1].lPrice) && (ME_OrderBook.OrderBook[tokenIndx].Sell[j].SeqNo > ME_OrderBook.OrderBook[tokenIndx].Sell[j+1].SeqNo))
            {
                memcpy(&swpbookdtls,&ME_OrderBook.OrderBook[tokenIndx].Sell[j+1],sizeof(swpbookdtls));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[j+1],&ME_OrderBook.OrderBook[tokenIndx].Sell[j], sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[j]));
                memcpy(&ME_OrderBook.OrderBook[tokenIndx].Sell[j],&swpbookdtls, sizeof(ME_OrderBook.OrderBook[tokenIndx].Sell[j]));
            } 
        }    
    }  
    PrintBook(tokenIndx);
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


int ValidateModReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int& tokIndex, int& dlrIndex, int32_t& LMT)
{
    int errCode = 0;
    bool orderFound = false;

    errCode = ValidateUser(iUserID, FD, dlrIndex);
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokIndex);
    }
    else if (_nSegMode == SEG_NSECD)
    {
        found = binarySearch(TokenStore, TokenCount, Token, &tokIndex);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokIndex].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokIndex].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokIndex].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 if (LMT != ME_OrderBook.OrderBook[tokIndex].Buy[j].LastModified)
                 {
                     errCode = OE_ORD_CANNOT_MODIFY;
                     snprintf (logBuf, 500, "Thread_ME|ValidateModReq|Incorrect LMT|RecvdLMT %d|BookLMT %d", 
                       LMT,  ME_OrderBook.OrderBook[tokIndex].Buy[j].LastModified);
                     Logger::getLogger().log(DEBUG, logBuf); 
                 }
                 LMT = ME_OrderBook.OrderBook[tokIndex].Buy[j].LastModified;
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokIndex].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokIndex].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokIndex].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 if (LMT != ME_OrderBook.OrderBook[tokIndex].Sell[j].LastModified) 
                 {
                     errCode = OE_ORD_CANNOT_MODIFY;
                     snprintf (logBuf, 500, "Thread_ME|ValidateModReq|Incorrect LMT|RecvdLMT %d|BookLMT %d", 
                       LMT,  ME_OrderBook.OrderBook[tokIndex].Sell[j].LastModified);
                     Logger::getLogger().log(DEBUG, logBuf); 
                 }
                 LMT = ME_OrderBook.OrderBook[tokIndex].Sell[j].LastModified;
             }   
         }
     }
    
     if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}

int ModOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pconnInfo, int64_t recvTime) {
    int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    ORDER_BOOK_DTLS bookdetails;
    int MyTime = GlobalSeqNo++;   
    NSECM::MS_OE_RESPONSE_TR ModOrdResp;
    int i = 0;
    long Token = 0;
    int tokenIndex = 0, dealerIndex = -1;
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo); 
   
    ModOrdResp.ErrorCode = ValidateModReq(__bswap_32(ModOrder->TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token, ModOrder->sec_info.Symbol, ModOrder->sec_info.Series, tokenIndex, dealerIndex, LMT);
    if(ModOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      ModOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(ModOrdResp.ErrorCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",
           FD,(int64_t)dOrderNo, ModOrder->TransactionId,ModOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
         Logger::getLogger().log(DEBUG, logBuf);
         return 0;
      }
      
    }
    
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    ModOrdResp.TransactionCode = __bswap_16(20074);
    ModOrdResp.LogTime = __bswap_16(1);
    ModOrdResp.TraderId = ModOrder->TraderId;
    //ModOrdResp.ErrorCode = 0;
    ModOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    ModOrdResp.TimeStamp1 = __bswap_64(ModOrdResp.TimeStamp1); /*Sneha*/
    ModOrdResp.Timestamp  = ModOrdResp.TimeStamp1;
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
    memcpy(&ModOrdResp.PAN,&ModOrder->PAN,sizeof(ModOrdResp.PAN));
    ModOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    ModOrdResp.AlgoId = ModOrder->AlgoId;
    ModOrdResp.EntryDateTime = ModOrder->EntryDateTime;
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
    ModOrdResp.ReasonCode = 0;
    //std::cout << "ModOrdResp.TransactionId  " << ModOrdResp.TransactionId << std::endl;
    
    ModOrdResp.tap_hdr.swapBytes(); 
    
    if (ModOrdResp.ErrorCode != 0)
    {
         snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT %d",
           FD,(int64_t)dOrderNo, ModOrder->TransactionId,ModOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
         Logger::getLogger().log(DEBUG, logBuf);
         //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
         ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
         ModOrdResp.LastModified = __bswap_32 (LMT);
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
    LMT = LMT + 1;
    ModOrdResp.LastModified = __bswap_32 (LMT);
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    int64_t OrderNumber = ModOrdResp.OrderNumber;
    
    bookdetails.OrderNo =  ModOrdResp.OrderNumber;
    //SwapDouble((char*) &bookdetails.OrderNo);
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.connInfo = pconnInfo;
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(ModOrdResp.TraderId);
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    if (1== IsIOC){
       bookdetails.IsIOC = 1;
    }
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    /*pan card changes*/
    memcpy(&bookdetails.PAN,&ModOrdResp.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(ModOrdResp.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(ModOrdResp.AlgoCategory);
    /*pan card changes ends*/
    
    bookdetails.TraderId = __bswap_32(ModOrdResp.TraderId);
    bookdetails.BookType = __bswap_16(ModOrdResp.BookType);
    bookdetails.Volume = __bswap_32(ModOrdResp.Volume);
    bookdetails.BranchId = __bswap_16(ModOrdResp.BranchId);
    bookdetails.UserId = __bswap_32(ModOrdResp.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(ModOrdResp.ProClient);
    bookdetails.nsecm_nsefo_nsecd.NSECM.Suspended = ModOrdResp.Suspended;
    bookdetails.NnfField = ModOrdResp.NnfField;   
    SwapDouble((char*) &bookdetails.NnfField);    
    memcpy(&bookdetails.AccountNumber,&ModOrdResp.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.BrokerId,&ModOrdResp.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&ModOrdResp.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags,&ModOrdResp.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECM.OrderFlags));
    bookdetails.LastModified = LMT;
   /*Sneha - E*/
  snprintf(logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d",
     FD, OrderNumber,ModOrder->TransactionId,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, Token, __bswap_16(ModOrdResp.BuySellIndicator),
     LMT); 
   Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
   //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<OrderNumber<<"|IOC "<<bookdetails.IsIOC<<"|DQ "<<bookdetails.IsDQ<<"|DQty "<<bookdetails.DQty<<std::endl;
    
   long datareturn = Modtoorderbook(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),Token ,IsIOC,IsDQ,IsSL ,__bswap_constant_32(ModOrdResp.LastModified), tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;

     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pconnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
     if (5 == datareturn && 1 == bookdetails.IsIOC )
    {
        //SendOrderCancellation_NSECM(bookdetails.OrderNo, Token, 0, FD, pconnInfo, 0, false, 0);
        SendOrderCancellation_NSECM(&bookdetails, Token, FD, pconnInfo, 0, false);
        Cantoorderbook(&bookdetails, __bswap_16(ModOrdResp.BuySellIndicator), Token, getEpochTime(), dealerIndex, tokenIndex);
    }
    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder(Token,FD,IsIOC,IsDQ,pconnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching(Token,FD,IsIOC,IsDQ,pconnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|MOD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);
}

int ModOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
    
  int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR ModOrdResp;
    
    int MyTime = GlobalSeqNo++; 
     int tokenIndex = 0, dealerIndex = -1;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    int32_t LMT= __bswap_32(ModOrder->LastModified);
    ModOrdResp.ErrorCode = ValidateModReq(__bswap_32(ModOrder->TraderId), FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token,NULL,NULL, tokenIndex, dealerIndex, LMT);
    if(ModOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      ModOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(ModOrdResp.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.TransactionCode = __bswap_16(20074);
    ModOrdResp.LogTime = __bswap_16(1);
    ModOrdResp.TraderId = ModOrder->TraderId;
    ModOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    //ModOrdResp.ErrorCode = 0;       
    ModOrdResp.Timestamp1 =  getCurrentTimeInNano();
    ModOrdResp.Timestamp1 = __bswap_64(ModOrdResp.Timestamp1); /*sneha*/
    ModOrdResp.Timestamp  = ModOrdResp.Timestamp1;
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
    
    memcpy(&ModOrdResp.PAN,&ModOrder->PAN,sizeof(ModOrdResp.PAN));
    ModOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    ModOrdResp.AlgoId = ModOrder->AlgoId;
    
    ModOrdResp.EntryDateTime =ModOrder->EntryDateTime;
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
    
    ModOrdResp.ReasonCode = 0;
    
    ModOrdResp.tap_hdr.swapBytes(); 
    
    if (ModOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
        ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
        ModOrdResp.LastModified = __bswap_32(LMT);
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
    LMT = LMT + 1;
    ModOrdResp.LastModified = __bswap_32(LMT);
    SwapDouble((char*) &ModOrdResp.OrderNumber);

    int64_t OrderNumber = ModOrdResp.OrderNumber;
  
    memcpy(&bookdetails.OrderNo,&ModOrdResp.OrderNumber,sizeof(bookdetails.OrderNo));
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    //SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);
    /*Sneha - S*/
    bookdetails.FD = FD; 
    bookdetails.connInfo = pConnInfo;
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(ModOrdResp.TraderId);
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    if (1 == IsIOC){
      bookdetails.IsIOC = 1;
    }
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    /*pan card changes*/
    memcpy(&bookdetails.PAN,&ModOrdResp.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(ModOrdResp.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(ModOrdResp.AlgoCategory);
    /*pan card changes ends*/
    
    bookdetails.TraderId = __bswap_32(ModOrdResp.TraderId);
    bookdetails.BookType = __bswap_16(ModOrdResp.BookType);
    bookdetails.Volume = __bswap_32(ModOrdResp.Volume);
    bookdetails.BranchId = __bswap_16(ModOrdResp.BranchId);
    bookdetails.UserId = __bswap_32(ModOrdResp.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(ModOrdResp.ProClientIndicator);
    bookdetails.NnfField = ModOrdResp.NnfField;  
    SwapDouble((char*) &bookdetails.NnfField);
    memcpy(&bookdetails.BrokerId,&ModOrdResp.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&ModOrdResp.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.AccountNumber,&ModOrdResp.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags,& ModOrdResp.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSEFO.OrderFlags));
    bookdetails.LastModified = LMT;
    /*Sneha - E*/
    
    snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d",
    FD,OrderNumber,ModOrder->filler,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, (Token + FOOFFSET), __bswap_16(ModOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    
    long datareturn = Modtoorderbook(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET) ,IsIOC,IsDQ,IsSL,__bswap_constant_32(ModOrdResp.LastModified), tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    
    int i = 0;
   
     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    if (5 == datareturn && 1== bookdetails.IsIOC)
    {
        SendOrderCancellation_NSEFO(&bookdetails, Token, FD, pConnInfo, 0, false);
        Cantoorderbook(&bookdetails, __bswap_16(ModOrdResp.BuySellIndicator), Token, getEpochTime(), dealerIndex, tokenIndex);
    }

    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder((__bswap_32(ModOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching((__bswap_32(ModOrder->TokenNo) - FOOFFSET),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|MOD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}

int ModOrderTrim(NSECD::MS_OM_REQUEST_TR *ModOrder, int FD, int IsIOC, int IsDQ, int IsSL, CONNINFO* pConnInfo, int64_t recvTime) {
    
  int64_t tAfterEnqueue = 0;
    GET_PERF_TIME(tAfterEnqueue);
    tAfterEnqueue -= recvTime;
    
    ORDER_BOOK_DTLS bookdetails;
    NSECD::MS_OE_RESPONSE_TR ModOrdResp;
    
    int MyTime = GlobalSeqNo++; 
     int tokenIndex = 0, dealerIndex = -1;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo));
    int32_t LMT= __bswap_32(ModOrder->LastModified);
    ModOrdResp.ErrorCode = ValidateModReq(__bswap_32(ModOrder->TraderId), FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator), Token,NULL,NULL, tokenIndex, dealerIndex, LMT);
    if(ModOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      ModOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(ModOrdResp.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    memcpy(&ModOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(ModOrdResp.tap_hdr));
    ModOrdResp.TransactionCode = __bswap_16(20074);
    ModOrdResp.LogTime = __bswap_16(1);
    ModOrdResp.TraderId = ModOrder->TraderId;
    ModOrdResp.tap_hdr.sLength = sizeof(NSECD::MS_OE_RESPONSE_TR);
    //ModOrdResp.ErrorCode = 0;       
    ModOrdResp.Timestamp1 =  getCurrentTimeInNano();
    ModOrdResp.Timestamp1 = __bswap_64(ModOrdResp.Timestamp1); /*sneha*/
    ModOrdResp.Timestamp  = ModOrdResp.Timestamp1;
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
    
    memcpy(&ModOrdResp.PAN,&ModOrder->PAN,sizeof(ModOrdResp.PAN));
    ModOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    ModOrdResp.AlgoId = ModOrder->AlgoId;
    
    ModOrdResp.EntryDateTime =ModOrder->EntryDateTime;
    memcpy(&ModOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(ModOrdResp.OrderFlags));
    ModOrdResp.BranchId = ModOrder->BranchId;
    ModOrdResp.UserId = ModOrder->UserId;        
    memcpy(&ModOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(ModOrdResp.BrokerId)) ;      
           
    memcpy(&ModOrdResp.Settlor,&ModOrder->Settlor,sizeof(ModOrdResp.Settlor));
    ModOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    
    ModOrdResp.TokenNo = ModOrder->TokenNo;
    ModOrdResp.NnfField = ModOrder->NnfField;       
    ModOrdResp.filler = ModOrder->filler;
    ModOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    ModOrdResp.ReasonCode = 0;
    
    ModOrdResp.tap_hdr.swapBytes(); 
    
    if (ModOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d", 
          FD, int64_t(dOrderNo),ModOrder->filler,ModOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|MOD ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<ModOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
        ModOrdResp.ErrorCode = __bswap_16(ModOrdResp.ErrorCode);
        ModOrdResp.LastModified = __bswap_32(LMT);
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
    LMT = LMT + 1;
    ModOrdResp.LastModified = __bswap_32(LMT);
    SwapDouble((char*) &ModOrdResp.OrderNumber);

    int64_t OrderNumber = ModOrdResp.OrderNumber;

    memcpy(&bookdetails.OrderNo,&ModOrdResp.OrderNumber,sizeof(bookdetails.OrderNo));
    SwapDouble((char*) &ModOrdResp.OrderNumber);

    bookdetails.lPrice = __bswap_32(ModOrdResp.Price);
    bookdetails.lQty = __bswap_32(ModOrdResp.Volume);

    bookdetails.FD = FD; 
    bookdetails.connInfo = pConnInfo;
    bookdetails.DQty = __bswap_32(ModOrdResp.DisclosedVolume);
    bookdetails.dealerID = __bswap_32(ModOrdResp.TraderId);
    bookdetails.IsIOC = 0;
    bookdetails.IsDQ = 0;
    if (1 == IsIOC){
      bookdetails.IsIOC = 1;
    }
    if ((bookdetails.DQty > 0) && (bookdetails.DQty != bookdetails.lQty)){
      bookdetails.IsDQ = 1;
    }
    
    /*pan card changes*/
    memcpy(&bookdetails.PAN,&ModOrdResp.PAN,sizeof(bookdetails.PAN));
    bookdetails.AlgoId = __bswap_32(ModOrdResp.AlgoId);
    bookdetails.AlgoCategory = __bswap_16(ModOrdResp.AlgoCategory);
    /*pan card changes ends*/
    
    bookdetails.TraderId = __bswap_32(ModOrdResp.TraderId);
    bookdetails.BookType = __bswap_16(ModOrdResp.BookType);
    bookdetails.Volume = __bswap_32(ModOrdResp.Volume);
    bookdetails.BranchId = __bswap_16(ModOrdResp.BranchId);
    bookdetails.UserId = __bswap_32(ModOrdResp.UserId);        
    bookdetails.ProClientIndicator = __bswap_16(ModOrdResp.ProClientIndicator);
    bookdetails.NnfField = ModOrdResp.NnfField;  
    SwapDouble((char*) &bookdetails.NnfField);
    memcpy(&bookdetails.BrokerId,&ModOrdResp.BrokerId,sizeof(bookdetails.BrokerId)) ;      
    memcpy(&bookdetails.Settlor,&ModOrdResp.Settlor,sizeof(bookdetails.Settlor));
    memcpy(&bookdetails.AccountNumber,&ModOrdResp.AccountNumber,sizeof(bookdetails.AccountNumber));
    memcpy(&bookdetails.nsecm_nsefo_nsecd.NSECD.OrderFlags,& ModOrdResp.OrderFlags, sizeof(bookdetails.nsecm_nsefo_nsecd.NSECD.OrderFlags));
    bookdetails.LastModified = LMT;

    
    snprintf (logBuf, 500, "Thread_ME|FD %d|MOD ORDER|Order# %ld|COrd# %d|IOC %d|DQ %d|DQty %d|Qty %ld|Price %ld| Token %ld|Side %d|LMT %d",
    FD,OrderNumber,ModOrder->filler,bookdetails.IsIOC,bookdetails.IsDQ,bookdetails.DQty, bookdetails.lQty, bookdetails.lPrice, (Token + FOOFFSET), __bswap_16(ModOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    int64_t tAfterLog = 0;
    GET_PERF_TIME(tAfterLog);
    tAfterLog -= recvTime;
    
    long datareturn = Modtoorderbook(&bookdetails,__bswap_16(ModOrdResp.BuySellIndicator),__bswap_32(ModOrder->TokenNo) ,IsIOC,IsDQ,IsSL,__bswap_constant_32(ModOrdResp.LastModified), tokenIndex);
    int64_t tAfterOrdBook = 0;
    GET_PERF_TIME(tAfterOrdBook);
    tAfterOrdBook -= recvTime;
    
    int i = 0;
   
     i = SendToClient( FD , (char *)&ModOrdResp , sizeof(ModOrdResp),pConnInfo);
    int64_t tAfterSockWrite = 0;
    GET_PERF_TIME(tAfterSockWrite);
    tAfterSockWrite -= recvTime;
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&ModOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tAfterLogEnqueue = 0;
    GET_PERF_TIME(tAfterLogEnqueue);
    tAfterLogEnqueue -= recvTime;
    
    SwapDouble((char*) &ModOrdResp.OrderNumber);
    if (5 == datareturn && 1== bookdetails.IsIOC)
    {
        SendOrderCancellation_NSECD(&bookdetails, Token, FD, pConnInfo, 0, false);
        Cantoorderbook(&bookdetails, __bswap_16(ModOrdResp.BuySellIndicator), Token, getEpochTime(), dealerIndex, tokenIndex);
    }

    if(gSimulatorMode==3)
    {
      datareturn = MatchingBookBuilder(__bswap_32(ModOrder->TokenNo),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else 
    {
      datareturn = Matching(__bswap_32(ModOrder->TokenNo),FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    tTotal -= recvTime;
    snprintf(logBuf, 500, "Thread_ME|LATENCY|MOD ORDER|Order# %ld|Enqueue=%ld|Log=%ld|tAfterOrdBook=%d|Sock=%ld|LogEnqueue=%ld|Total=%ld", 
                     OrderNumber, tAfterEnqueue, tAfterLog-tAfterEnqueue, tAfterOrdBook-tAfterLog, tAfterSockWrite-tAfterOrdBook,  tAfterLogEnqueue-tAfterSockWrite, tTotal);
    Logger::getLogger().log(DEBUG, logBuf);    
}



int ValidateCanReq(int32_t iUserID, int FD, double dOrderNo, int iOrderSide, long& Token, char* symbol, char*series, int& dealerIndx, int& tokenIndx, int& LMT)
{
    int errCode = 0;
    bool orderFound = false;
    
    errCode = ValidateUser(iUserID, FD, dealerIndx);
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
         found = binarySearch(TokenStore, TokenCount, Token, &tokenIndx);
    }
    else if (_nSegMode == SEG_NSEFO)
    {
        found = binarySearch(TokenStore, TokenCount, (Token + FOOFFSET), &tokenIndx);
    }
    else if (_nSegMode == SEG_NSECD)
    {
        found = binarySearch(TokenStore, TokenCount, Token, &tokenIndx);
    }
    if (found == false)
    {
        errCode = ERR_SECURITY_NOT_AVAILABLE;
        return errCode;
    }
    
     if(iOrderSide == 1)
    {
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndx].BuyRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokenIndx].Buy[j].OrderNo == dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokenIndx].Buy[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 LMT = ME_OrderBook.OrderBook[tokenIndx].Buy[j].LastModified;
             }   
         }
    }  
    else
    {
         for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndx].SellRecords ) ; j++)
        {   
             if(ME_OrderBook.OrderBook[tokenIndx].Sell[j].OrderNo ==  dOrderNo)
             {
                 orderFound = true;
                 /*filled order*/
                 if (ME_OrderBook.OrderBook[tokenIndx].Sell[j].lQty == 0) 
                 {
                     errCode = ERR_MOD_CAN_REJECT;
                 }
                 LMT = ME_OrderBook.OrderBook[tokenIndx].Sell[j].LastModified;
             }   
         }
     }
    
    if (orderFound == false)
    {
        errCode = ORDER_NOT_FOUND;
    }
    
     return errCode;
}

int CanOrderTrim(NSECM::MS_OM_REQUEST_TR *ModOrder, int FD, CONNINFO* pConnInfo, int64_t recvTime) {
    ORDER_BOOK_DTLS bookdetails;
    NSECM::MS_OE_RESPONSE_TR CanOrdResp;
    long Token = 0;
    int dealerIndex, tokenIndex;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    CanOrdResp.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->TraderId),FD, dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series, dealerIndex, tokenIndex,LMT);
         
    if(CanOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      CanOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(CanOrdResp.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT",
          FD, (int64_t)dOrderNo, ModOrder->TransactionId,CanOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
     
    }
     memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.tap_hdr.sLength = sizeof(NSECM::MS_OE_RESPONSE_TR);
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = ModOrder->TraderId;
    //CanOrdResp.ErrorCode = 0;
    CanOrdResp.TimeStamp1 =  getCurrentTimeInNano();
    CanOrdResp.TimeStamp1 = __bswap_64(CanOrdResp.TimeStamp1); /*sneha*/
    CanOrdResp.Timestamp  = CanOrdResp.TimeStamp1;
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
    
    memcpy(&CanOrdResp.PAN,&ModOrder->PAN,sizeof(CanOrdResp.PAN));
    CanOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    CanOrdResp.AlgoId = ModOrder->AlgoId;
    
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
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
   
    CanOrdResp.ReasonCode = 0;
    CanOrdResp.tap_hdr.swapBytes(); 
    
    if (CanOrdResp.ErrorCode != 0)
    {
      
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Symbol %s|Series %s|Token %ld|LMT",
          FD, (int64_t)dOrderNo, ModOrder->TransactionId,CanOrdResp.ErrorCode,ModOrder->sec_info.Symbol,ModOrder->sec_info.Series,Token, LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<int(dOrderNo)<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Symbol "<<ModOrder->sec_info.Symbol<<"|Series "<<ModOrder->sec_info.Series<<std::endl;
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
        CanOrdResp.LastModified = __bswap_32(LMT);
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
    LMT = LMT + 1;
    CanOrdResp.LastModified = __bswap_32(LMT);    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.OrderNo = CanOrdResp.OrderNumber;
    SwapDouble((char*) &CanOrdResp.OrderNumber);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.dealerID = __bswap_32(CanOrdResp.TraderId);
    bookdetails.LastModified = LMT;
    int64_t OrderNumber = bookdetails.OrderNo;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|Qty %ld|Price %ld|Token %ld|Side %d|LMT %d",
    FD, OrderNumber, ModOrder->TransactionId,bookdetails.lQty, bookdetails.lPrice, Token, __bswap_16(CanOrdResp.BuySellIndicator), LMT);
    Logger::getLogger().log(DEBUG, logBuf);

    long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),Token , __bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);

    int i = 0;
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECM::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);   
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    snprintf(logBuf, 500, "Thread_ME|LATENCY|CAN ORDER|Order# %ld|Total=%ld", OrderNumber, tTotal-recvTime);
    Logger::getLogger().log(DEBUG, logBuf);
}

int CanOrderTrim(NSEFO::MS_OM_REQUEST_TR *ModOrder, int FD, CONNINFO* pConnInfo, int64_t recvTime) {
    ORDER_BOOK_DTLS bookdetails;
    NSEFO::MS_OE_RESPONSE_TR CanOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));
     int dealerIndex, tokenIndex;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) - FOOFFSET);
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    CanOrdResp.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->TraderId), FD,dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,NULL,NULL, dealerIndex, tokenIndex,LMT);
    if(CanOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      CanOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(CanOrdResp.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = ModOrder->TraderId;
    CanOrdResp.tap_hdr.sLength = sizeof(NSEFO::MS_OE_RESPONSE_TR);
    //CanOrdResp.ErrorCode = 0;       
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1); /*sneha*/
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1;
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
    
    memcpy(&CanOrdResp.PAN,&ModOrder->PAN,sizeof(CanOrdResp.PAN));
    CanOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    CanOrdResp.AlgoId = ModOrder->AlgoId;
    
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    memcpy(&CanOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.UserId = ModOrder->UserId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    CanOrdResp.TokenNo = ModOrder->TokenNo;
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.filler = ModOrder->filler;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    CanOrdResp.ReasonCode = 0;
    
    CanOrdResp.tap_hdr.swapBytes(); 
    
    if (CanOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Token "<<(Token + FOOFFSET)<<std::endl;
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
        CanOrdResp.LastModified = __bswap_32(LMT);
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
    LMT = LMT + 1;
    CanOrdResp.LastModified = __bswap_32(LMT);
    bookdetails.OrderNo =  CanOrdResp.OrderNumber;
    SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.dealerID = __bswap_32(CanOrdResp.TraderId);
    bookdetails.LastModified = LMT;
    
    int64_t OrderNumber = bookdetails.OrderNo;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|Qty %ld|Price %ld|Token %ld|Side %d|LMT %d",
    FD,  OrderNumber, ModOrder->filler,bookdetails.lQty, bookdetails.lPrice, (Token + FOOFFSET), __bswap_16(CanOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    
    long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) - FOOFFSET),__bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
   
    int i =0;
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSEFO::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    snprintf(logBuf, 500, "Thread_ME|LATENCY|CAN ORDER|Order# %ld|Total=%ld", OrderNumber, tTotal-recvTime);
    Logger::getLogger().log(DEBUG, logBuf);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
}

int CanOrderTrim(NSECD::MS_OM_REQUEST_TR *ModOrder, int FD, CONNINFO* pConnInfo, int64_t recvTime) {
    ORDER_BOOK_DTLS bookdetails;
    NSECD::MS_OE_RESPONSE_TR CanOrdResp;
    //memcpy(&ModOrdResp,ModOrder,sizeof(ModOrdResp));
     int dealerIndex, tokenIndex;
    double dOrderNo = ModOrder->OrderNumber;
    SwapDouble((char*) &dOrderNo);
    long Token = (__bswap_32(ModOrder->TokenNo) );
    int32_t LMT = __bswap_32(ModOrder->LastModified);
    CanOrdResp.ErrorCode = ValidateCanReq(__bswap_32(ModOrder->TraderId), FD,dOrderNo, __bswap_16(ModOrder->BuySellIndicator),Token,NULL,NULL, dealerIndex, tokenIndex,LMT);
    if(CanOrdResp.ErrorCode == 0 && bEnableValMsg)
    {
      CanOrdResp.ErrorCode = ValidateChecksum((char*)ModOrder);
      if(CanOrdResp.ErrorCode != 0)
      {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.ErrorCode,(Token + FOOFFSET), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        return 0;
      }
      
    }
    memcpy(&CanOrdResp.tap_hdr,& ModOrder->tap_hdr, sizeof(CanOrdResp.tap_hdr));
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.TraderId = ModOrder->TraderId;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECD::MS_OE_RESPONSE_TR);
    //CanOrdResp.ErrorCode = 0;       
    CanOrdResp.Timestamp1 =  getCurrentTimeInNano();
    CanOrdResp.Timestamp1 = __bswap_64(CanOrdResp.Timestamp1); /*sneha*/
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1;
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
    
    memcpy(&CanOrdResp.PAN,&ModOrder->PAN,sizeof(CanOrdResp.PAN));
    CanOrdResp.AlgoCategory = ModOrder->AlgoCategory;
    CanOrdResp.AlgoId = ModOrder->AlgoId;
    
    CanOrdResp.EntryDateTime = ModOrder->EntryDateTime;
    memcpy(&CanOrdResp.OrderFlags,& ModOrder->OrderFlags, sizeof(CanOrdResp.OrderFlags));
    CanOrdResp.BranchId = ModOrder->BranchId;
    CanOrdResp.UserId = ModOrder->UserId;        
    memcpy(&CanOrdResp.BrokerId,&ModOrder->BrokerId,sizeof(CanOrdResp.BrokerId)) ;      
    memcpy(&CanOrdResp.Settlor,&ModOrder->Settlor,sizeof(CanOrdResp.Settlor));
    CanOrdResp.ProClientIndicator = ModOrder->ProClientIndicator;
    CanOrdResp.TokenNo = ModOrder->TokenNo;
    CanOrdResp.NnfField = ModOrder->NnfField;       
    CanOrdResp.filler = ModOrder->filler;
    CanOrdResp.OrderNumber = ModOrder->OrderNumber;
    
    CanOrdResp.ReasonCode = 0;
    
    CanOrdResp.tap_hdr.swapBytes(); 
    
    if (CanOrdResp.ErrorCode != 0)
    {
        snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|ErrorCode %d|Token %ld|LMT %d",
          FD,int64_t(dOrderNo), ModOrder->filler,CanOrdResp.ErrorCode,(Token ), LMT);
        Logger::getLogger().log(DEBUG, logBuf);
        //std::cout<<"FD "<<FD<<"|CAN ORDER"<<"|Order# "<<dOrderNo<<"|ErrorCode "<<CanOrdResp.ErrorCode<<"|Token "<<(Token )<<std::endl;
        CanOrdResp.ErrorCode = __bswap_16(CanOrdResp.ErrorCode);
        CanOrdResp.LastModified = __bswap_32(LMT);
        SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECD::MS_OE_RESPONSE_TR),pConnInfo);
    
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
    LMT = LMT + 1;
    CanOrdResp.LastModified = __bswap_32(LMT);
    bookdetails.OrderNo =  CanOrdResp.OrderNumber;
    SwapDouble((char*) &bookdetails.OrderNo);
    bookdetails.lPrice = __bswap_32(CanOrdResp.Price);
    bookdetails.lQty = __bswap_32(CanOrdResp.Volume);
    bookdetails.FD = FD; /*Sneha*/
    bookdetails.connInfo = pConnInfo;
    bookdetails.dealerID = __bswap_32(CanOrdResp.TraderId);
    bookdetails.LastModified = LMT;
    
    int64_t OrderNumber = bookdetails.OrderNo;
    snprintf (logBuf, 500, "Thread_ME|FD %d|CAN ORDER|Order# %ld|COrd# %d|Qty %ld|Price %ld|Token %ld|Side %d|LMT %d",
    FD,  OrderNumber, ModOrder->filler,bookdetails.lQty, bookdetails.lPrice, (Token ), __bswap_16(CanOrdResp.BuySellIndicator),
      LMT);
    Logger::getLogger().log(DEBUG, logBuf);
    
    long datareturn = Cantoorderbook(&bookdetails,__bswap_16(CanOrdResp.BuySellIndicator),(__bswap_32(ModOrder->TokenNo) ),__bswap_constant_32(CanOrdResp.LastModified), dealerIndex, tokenIndex);
   
    int i =0;
    i = SendToClient( FD , (char *)&CanOrdResp , sizeof(NSECD::MS_OE_RESPONSE_TR),pConnInfo);
    /*Sneha*/
    memset (&LogData, 0, sizeof(LogData));
    LogData.MyFd = 1; /*1 = Order response*/
    memcpy (LogData.msgBuffer, (void*)&CanOrdResp, sizeof(LogData.msgBuffer));
    Inqptr_MeToLog_Global->enqueue(LogData);
    int64_t tTotal = 0;
    GET_PERF_TIME(tTotal);
    snprintf(logBuf, 500, "Thread_ME|LATENCY|CAN ORDER|Order# %ld|Total=%ld", OrderNumber, tTotal-recvTime);
    Logger::getLogger().log(DEBUG, logBuf);
    
    SwapDouble((char*) &CanOrdResp.OrderNumber);
}

/*Add and Trade for ML orders begins*/
int AddnTradeMLOrder(NSEFO::MS_SPD_OE_REQUEST* MLOrderReq,int FD, CONNINFO* pConnInfo)
{
    int noOfLegs = 0, dlrIndex, tokIndex;
    int32_t tradeQty,tradePrice;
    NSEFO::MS_SPD_OE_REQUEST MLOrderResp;
    memcpy(&MLOrderResp, MLOrderReq, sizeof(NSEFO::MS_SPD_OE_REQUEST));
   
    /*extract client order# for logging purpose*/
    int32_t clientOrd = 0;
    memcpy(&clientOrd, &(MLOrderReq->filler1to16), sizeof(int32_t));
        
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
    int errCode = ValidateMLAddReq(MLOrderReq, FD, noOfLegs);
    
    if(errCode == 0 && bEnableValMsg)
    {
      errCode = ValidateChecksum((char*)MLOrderReq);
      if(errCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD N TRADE|NEW ORDER GULPING|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                       "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                       FD, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
                       __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
        
        return 0;
      }
    }
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
    MLOrderResp.msg_hdr.Timestamp  = MLOrderResp.msg_hdr.TimeStamp1;
    MLOrderResp.msg_hdr.TimeStamp2[7] = 1;
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
      snprintf(logBuf, 500, "Thread_ME|FD %d|2L ADD N TRADE|NEW ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                       "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                       FD,OrderNumber, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
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
      snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD N TRADE|NEW ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                     "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                      FD,OrderNumber, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                      __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                     __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
     Logger::getLogger().log(DEBUG, logBuf);
    }

    int  i = SendToClient(FD , (char *)&MLOrderResp , sizeof(NSEFO::MS_SPD_OE_REQUEST),pConnInfo);
      
    if (errCode != 0)
    {
        if (errCode == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    NSEFO::TRADE_CONFIRMATION_TR SendTradeConf;
    for (i=0; i< noOfLegs; i++)
    {
      gMETradeNo = gMETradeNo + 1;
      
      memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
      
      memcpy(&SendTradeConf.AccountNumber,MLOrderReq->AccountNUmber1, sizeof(SendTradeConf.AccountNumber));
      SendTradeConf.BookType = MLOrderReq->BookType1;
      SendTradeConf.TraderId = MLOrderReq->TraderId1;
      
      if(i==0)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->DisclosedVol1;
        SendTradeConf.Price = MLOrderReq->Price1;
        SendTradeConf.Token = MLOrderReq->Token1;
        SendTradeConf.VolumeFilledToday = MLOrderReq->Volume1;
        SendTradeConf.BuySellIndicator = MLOrderReq->BuySell1;
        SendTradeConf.FillPrice = MLOrderReq->Price1;
        SendTradeConf.FillQuantity = MLOrderReq->Volume1;
        tradeQty = __bswap_32(MLOrderReq->Volume1);
        tradePrice = __bswap_32(MLOrderReq->Price1);
      }
      if(i==1)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->leg2.DisclosedVol2;
        SendTradeConf.Price = MLOrderReq->leg2.Price2;
        SendTradeConf.Token = MLOrderReq->leg2.Token2;
        SendTradeConf.VolumeFilledToday = MLOrderReq->leg2.Volume2;
        SendTradeConf.BuySellIndicator = MLOrderReq->leg2.BuySell2;
        SendTradeConf.FillPrice = MLOrderReq->leg2.Price2;
        SendTradeConf.FillQuantity = MLOrderReq->leg2.Volume2;
        tradeQty = __bswap_32(MLOrderReq->leg2.Volume2);
        tradePrice = __bswap_32(MLOrderReq->leg2.Price2);
      }
      if(i==2)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->leg3.DisclosedVol3;
        SendTradeConf.Price = MLOrderReq->leg3.Price3;
        SendTradeConf.Token = MLOrderReq->leg3.Token3;
        SendTradeConf.VolumeFilledToday = MLOrderReq->leg3.Volume3;
        SendTradeConf.BuySellIndicator = MLOrderReq->leg3.BuySell3;
        SendTradeConf.FillPrice = MLOrderReq->leg3.Price3;
        SendTradeConf.FillQuantity = MLOrderReq->leg3.Volume3;
        tradeQty = __bswap_32(MLOrderReq->leg3.Volume3);
        tradePrice = __bswap_32(MLOrderReq->leg3.Price3);
      }
      int64_t MyTime = getCurrentTimeInNano();  
      SendTradeConf.Timestamp1 = __bswap_64( MyTime);
      SendTradeConf.Timestamp   = SendTradeConf.Timestamp1;
      SendTradeConf.Timestamp2[7] = 1;
      int32_t LastModified = __bswap_32(MLOrderResp.LastModified1) + 1;
      SendTradeConf.ActivityTime  = __bswap_32(LastModified);
      SwapDouble((char*) &SendTradeConf.Timestamp);
      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
      
      SendTradeConf.RemainingVolume = 0;
      SendTradeConf.ResponseOrderNumber = MLOrderResp.OrderNUmber1;
      SendTradeConf.TransactionCode = __bswap_16(20222);
      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
      
      int j = SendToClient( FD , (char *)&SendTradeConf , sizeof(SendTradeConf),pConnInfo);
      
      snprintf (logBuf, 500, "Thread_ME|FD %d|ML ADD N Trade|TRADE|Order# %ld|Trade# %d|Qty %d|Price %ld",FD,OrderNumber, gMETradeNo, tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
    }
     
    
}

/*Add and Trade for ML order ends*/
/*Add and Trade for ML CD start*/
int AddnTradeMLOrder(NSECD::MS_SPD_OE_REQUEST* MLOrderReq,int FD, CONNINFO* pConnInfo)
{
    int noOfLegs = 0, dlrIndex, tokIndex;
    int32_t tradeQty,tradePrice;
    NSECD::MS_SPD_OE_REQUEST MLOrderResp;
    memcpy(&MLOrderResp, MLOrderReq, sizeof(NSECD::MS_SPD_OE_REQUEST));
   
    /*extract client order# for logging purpose*/
    int32_t clientOrd = 0;
    memcpy(&clientOrd, &(MLOrderReq->filler1to16), sizeof(int32_t));
        
    /*MLOrderInfo is like order store*/
    memset(&MLOrderInfo[0], 0, sizeof (NSECD::MultiLegOrderInfo));
    memset(&MLOrderInfo[1], 0, sizeof (NSECD::MultiLegOrderInfo));
    memset(&MLOrderInfo[2], 0, sizeof (NSECD::MultiLegOrderInfo));
      
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
    int errCode = ValidateMLAddReq(MLOrderReq, FD, noOfLegs);
    
    if(errCode == 0 && bEnableValMsg)
    {
      errCode = ValidateChecksum((char*)MLOrderReq);
      if(errCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ADD N TRADE|NEW ORDER GULPING|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                       "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                       FD, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
                       __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
        
        return 0;
      }
    }
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
    MLOrderResp.msg_hdr.Timestamp  = MLOrderResp.msg_hdr.TimeStamp1;
    MLOrderResp.msg_hdr.TimeStamp2[7] = 1;
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
      snprintf(logBuf, 500, "Thread_ME|FD %d|2L ADD N TRADE CD|NEW ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                       "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                       FD,OrderNumber, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
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
      snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD N TRADE CD|NEW ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                     "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                      FD,OrderNumber, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                      __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                     __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
     Logger::getLogger().log(DEBUG, logBuf);
    }

    int  i = SendToClient(FD , (char *)&MLOrderResp , sizeof(NSECD::MS_SPD_OE_REQUEST),pConnInfo);
      
    if (errCode != 0)
    {
        if (errCode == ERR_INVALID_USER_ID)
        {
           pConnInfo->status = DISCONNECTED;
        }
        return 0;
    }
    
    NSECD::TRADE_CONFIRMATION_TR SendTradeConf;
    for (i=0; i< noOfLegs; i++)
    {
      gMETradeNo = gMETradeNo + 1;
      
      memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
      
      memcpy(&SendTradeConf.AccountNumber,MLOrderReq->AccountNUmber1, sizeof(SendTradeConf.AccountNumber));
      SendTradeConf.BookType = MLOrderReq->BookType1;
      SendTradeConf.TraderId = MLOrderReq->TraderId1;
      
      if(i==0)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->DisclosedVol1;
        SendTradeConf.Price = MLOrderReq->Price1;
        SendTradeConf.Token = MLOrderReq->Token1;
        SendTradeConf.VolumeFilledToday = MLOrderReq->Volume1;
        SendTradeConf.BuySellIndicator = MLOrderReq->BuySell1;
        SendTradeConf.FillPrice = MLOrderReq->Price1;
        SendTradeConf.FillQuantity = MLOrderReq->Volume1;
        tradeQty = __bswap_32(MLOrderReq->Volume1);
        tradePrice = __bswap_32(MLOrderReq->Price1);
      }
      if(i==1)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->leg2.DisclosedVol2;
        SendTradeConf.Price = MLOrderReq->leg2.Price2;
        SendTradeConf.Token = MLOrderReq->leg2.Token2;
        SendTradeConf.VolumeFilledToday = MLOrderReq->leg2.Volume2;
        SendTradeConf.BuySellIndicator = MLOrderReq->leg2.BuySell2;
        SendTradeConf.FillPrice = MLOrderReq->leg2.Price2;
        SendTradeConf.FillQuantity = MLOrderReq->leg2.Volume2;
        tradeQty = __bswap_32(MLOrderReq->leg2.Volume2);
        tradePrice = __bswap_32(MLOrderReq->leg2.Price2);
      }
      if(i==2)
      {
        SendTradeConf.DisclosedVolume = MLOrderReq->leg3.DisclosedVol3;
        SendTradeConf.Price = MLOrderReq->leg3.Price3;
        SendTradeConf.Token = MLOrderReq->leg3.Token3;
        SendTradeConf.VolumeFilledToday = MLOrderReq->leg3.Volume3;
        SendTradeConf.BuySellIndicator = MLOrderReq->leg3.BuySell3;
        SendTradeConf.FillPrice = MLOrderReq->leg3.Price3;
        SendTradeConf.FillQuantity = MLOrderReq->leg3.Volume3;
        tradeQty = __bswap_32(MLOrderReq->leg3.Volume3);
        tradePrice = __bswap_32(MLOrderReq->leg3.Price3);
      }
      int64_t MyTime = getCurrentTimeInNano();  
      SendTradeConf.Timestamp1 = __bswap_64( MyTime);
      SendTradeConf.Timestamp   = SendTradeConf.Timestamp1;
      SendTradeConf.Timestamp2[7] = 1;
      int32_t LastModified = __bswap_32(MLOrderResp.LastModified1) + 1;
      SendTradeConf.ActivityTime  = __bswap_32(LastModified);
      SwapDouble((char*) &SendTradeConf.Timestamp);
      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
      
      SendTradeConf.RemainingVolume = 0;
      SendTradeConf.ResponseOrderNumber = MLOrderResp.OrderNUmber1;
      SendTradeConf.TransactionCode = __bswap_16(20222);
      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
      
      int j = SendToClient( FD , (char *)&SendTradeConf , sizeof(SendTradeConf),pConnInfo);
      
      snprintf (logBuf, 500, "Thread_ME|FD %d|ML ADD N Trade CD|TRADE|Order# %ld|Trade# %d|Qty %d|Price %ld",FD,OrderNumber, gMETradeNo, tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
    }
     
    
}
/*Add and Trade for ML CD start*/
/*CD Validates order data received. Output will decide if order is to be accepted or rejected start */
int ValidateMLAddReq(NSECD::MS_SPD_OE_REQUEST* Req, int Fd, int noOfLegs)
{
    int errCode = 0, i = 0, index;
    bool orderFound = false;

    /*check if user has signed ON*/
    errCode = ValidateUser(__bswap_32(Req->TraderId1), Fd, index);
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
        
   /*Check if Token is subscribed. If yes the fill lot size in MLOrderInfo else reject order*/
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
           MLOrderInfo[i].lotSize = cdContractInfoGlobal[loc].MinimumLotQuantity;
           MLOrderInfo[i].ordBookIndx = loc;
        }
    }
       
    return errCode;
}
/*CD Validates order data received. Output will decide if order is to be accepted or rejected end*/


/*Validates order data received. Output will decide if order is to be accepted or rejected*/
int ValidateMLAddReq(NSEFO::MS_SPD_OE_REQUEST* Req, int Fd, int noOfLegs)
{
    int errCode = 0, i = 0, index;
    bool orderFound = false;

    /*check if user has signed ON*/
    errCode = ValidateUser(__bswap_32(Req->TraderId1), Fd, index);
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
        
   /*Check if Token is subscribed. If yes the fill lot size in MLOrderInfo else reject order*/
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
           MLOrderInfo[i].ordBookIndx = loc;
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
           snprintf(logBuf, 500,"Buy Leg|Leg price %d|Top sell price %d|Token %d", MLOrderInfo[i].orderPrice, ME_OrderBook.OrderBook[MLOrderInfo[i].ordBookIndx].Sell[0].lPrice, MLOrderInfo[i].token);
           Logger::getLogger().log(DEBUG, logBuf);
           if(MLOrderInfo[i].orderPrice < ME_OrderBook.OrderBook[MLOrderInfo[i].ordBookIndx].Sell[0].lPrice)
           {
               return false;
           }    
     } 
    else
    {
           snprintf(logBuf, 500,"Sell Leg|Leg price %d|Top Buy price %d|Token %d", MLOrderInfo[i].orderPrice, ME_OrderBook.OrderBook[MLOrderInfo[i].ordBookIndx].Buy[0].lPrice, MLOrderInfo[i].token);
           Logger::getLogger().log(DEBUG, logBuf);
          if(MLOrderInfo[i].orderPrice > ME_OrderBook.OrderBook[MLOrderInfo[i].ordBookIndx].Buy[0].lPrice)
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
     int16_t tokenIndex = 0;
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
        tokenIndex = MLOrderInfo[i].ordBookIndx;
       /*Find available Qty on opposite side - S*/
       if (MLOrderInfo[i].buySell == 1)
       {
           for (j=0; j< ME_OrderBook.OrderBook[tokenIndex].SellRecords; j++)        
          {
              if ((MLOrderInfo[i].avlblOppQty < MLOrderInfo[i].orderQty) &&
                   !(MLOrderInfo[i].orderPrice < ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice))
             {
                 MLOrderInfo[i].avlblOppQty = MLOrderInfo[i].avlblOppQty + ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty;
              }
              else
              {
                break;
              }
          }
       }
       else
       {
           for (j=0; j< ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)        
          {
              if ((MLOrderInfo[i].avlblOppQty < MLOrderInfo[i].orderQty) &&
                   !(MLOrderInfo[i].orderPrice > ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice))
             {
                 MLOrderInfo[i].avlblOppQty = MLOrderInfo[i].avlblOppQty + ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty;
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
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1;
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
    CanOrdResp.LastModified = CanOrdResp.EntryDateTime;
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
/*CD Send ML order cancellation if qty or price validation fails. This IOC order cancellation*/
int SendMLOrderCancellation(NSECD::MS_SPD_OE_REQUEST*  MLOrderResp, int FD, int noOfLegs, CONNINFO* pConnInfo)
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
        
        int  i = SendToClient(FD , (char *)MLOrderResp , sizeof(NSECD::MS_SPD_OE_REQUEST), pConnInfo);
     
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML UnsolCAN ORDER|Order# %ld", FD, int(OrderNo));
        Logger::getLogger().log(DEBUG, logBuf);
         
        /*Send order cancel to MsgDnld thread*/
        memset (&LogData, 0, sizeof(LogData));
        LogData.MyFd = 3; /*3 = ML Order*/
        memcpy (LogData.msgBuffer, (void*)MLOrderResp, sizeof(LogData.msgBuffer));
        Inqptr_MeToLog_Global->enqueue(LogData);  
}

/* CD Send cancellation for single leg of ML order after partial fill*/
int SendMLOrderLegCancellation(NSECD::MS_SPD_OE_REQUEST* MLOrderResp, int i, int FD,CONNINFO* pConnInfo)
{
     NSECD::MS_OE_RESPONSE_TR CanOrdResp;
     
    CanOrdResp.tap_hdr.iSeqNo = GlobalSeqNo++;
    CanOrdResp.tap_hdr.sLength = sizeof(NSECD::MS_OE_RESPONSE_TR);
    
    CanOrdResp.TransactionCode = __bswap_16(20075);
    CanOrdResp.LogTime = __bswap_16(1);
    CanOrdResp.ErrorCode = __bswap_16(e$fok_order_cancelled);       
    CanOrdResp.Timestamp1 =  __bswap_64(getCurrentTimeInNano());
    CanOrdResp.Timestamp  = CanOrdResp.Timestamp1;
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
    CanOrdResp.LastModified = CanOrdResp.EntryDateTime;
    CanOrdResp.TokenNo = __bswap_32(MLOrderInfo[i].token);

    CanOrdResp.tap_hdr.swapBytes(); 
 
    int  byteSent = SendToClient(FD , (char *)&CanOrdResp , sizeof(NSECD::MS_OE_RESPONSE_TR), pConnInfo);

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
    int noOfLegs = 0, dlrIndex, tokIndex;
    NSEFO::MS_SPD_OE_REQUEST MLOrderResp;
    memcpy(&MLOrderResp, MLOrderReq, sizeof(NSEFO::MS_SPD_OE_REQUEST));
   
    /*extract client order# for logging purpose*/
    int32_t clientOrd = 0;
    memcpy(&clientOrd, &(MLOrderReq->filler1to16), sizeof(int32_t));
        
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
    
    if(errCode == 0 && bEnableValMsg)
    {
      errCode = ValidateChecksum((char*)MLOrderReq);
      if(errCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD ORDER|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                        "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                         FD, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                         __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                        __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
        
        return 0;
      }
    }
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
    MLOrderResp.msg_hdr.Timestamp  = MLOrderResp.msg_hdr.TimeStamp1;
    MLOrderResp.msg_hdr.TimeStamp2[7] = 1;
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
        snprintf(logBuf, 500, "Thread_ME|FD %d|2L ADD ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                         "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                         FD,OrderNumber, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
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
         snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD ORDER|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                        "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                         FD,OrderNumber, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
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
int AddMLOrder(NSECD::MS_SPD_OE_REQUEST* MLOrderReq,int FD, CONNINFO* pConnInfo)
{
    int noOfLegs = 0, dlrIndex, tokIndex;
    NSECD::MS_SPD_OE_REQUEST MLOrderResp;
    memcpy(&MLOrderResp, MLOrderReq, sizeof(NSECD::MS_SPD_OE_REQUEST));
   
    /*extract client order# for logging purpose*/
    int32_t clientOrd = 0;
    memcpy(&clientOrd, &(MLOrderReq->filler1to16), sizeof(int32_t));
        
    /*MLOrderInfo is like order store*/
    memset(&MLOrderInfo[0], 0, sizeof (NSECD::MultiLegOrderInfo));
    memset(&MLOrderInfo[1], 0, sizeof (NSECD::MultiLegOrderInfo));
    memset(&MLOrderInfo[2], 0, sizeof (NSECD::MultiLegOrderInfo));
      
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
    
    if(errCode == 0 && bEnableValMsg)
    {
      errCode = ValidateChecksum((char*)MLOrderReq);
      if(errCode != 0)
      {
        snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD ORDER CD|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                        "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                         FD, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                         __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                        __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
        
        return 0;
      }
    }
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
    MLOrderResp.msg_hdr.Timestamp  = MLOrderResp.msg_hdr.TimeStamp1;
    MLOrderResp.msg_hdr.TimeStamp2[7] = 1;
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
        snprintf(logBuf, 500, "Thread_ME|FD %d|2L ADD ORDER CD|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                         "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Error code %d", 
                         FD,OrderNumber, clientOrd,__bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1),
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
         snprintf(logBuf, 500, "Thread_ME|FD %d|3L ADD ORDER CD|Order# %ld|COrd# %d|Token1 %d|B/S %d |Qty1 %d|Price1 %d|"
                        "Token2 %d|B/S %d|Qty2 %d|Price2 %d|Token3 %d|B/S %d|Qty3 %d|Price3 %d|Error code %d", 
                         FD,OrderNumber, clientOrd, __bswap_32(MLOrderReq->Token1), __bswap_16(MLOrderReq->BuySell1),__bswap_32(MLOrderReq->Volume1), __bswap_32(MLOrderReq->Price1), 
                         __bswap_32(MLOrderReq->leg2.Token2), __bswap_16(MLOrderReq->leg2.BuySell2),__bswap_32(MLOrderReq->leg2.Volume2), __bswap_32(MLOrderReq->leg2.Price2), 
                        __bswap_32(MLOrderReq->leg3.Token3), __bswap_16(MLOrderReq->leg3.BuySell3),__bswap_32(MLOrderReq->leg3.Volume3), __bswap_32(MLOrderReq->leg3.Price3), errCode);
        Logger::getLogger().log(DEBUG, logBuf);
    }

    int  i = SendToClient(FD , (char *)&MLOrderResp , sizeof(NSECD::MS_SPD_OE_REQUEST),pConnInfo);
   
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
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML ADD ORDER CD|Order# %ld|Order price not executable|Sending cancellation", FD,OrderNumber);
        Logger::getLogger().log(DEBUG, logBuf);
        
        SendMLOrderCancellation(&MLOrderResp, FD, noOfLegs, pConnInfo);
        return 0;
    }
    
    /*Validate whether all the legs are executable based on their qty*/
    retVal = validateOrderQtyExecutable(noOfLegs);
    if (retVal == false)
    {
        snprintf(logBuf, 500, "Thread_ME|FD %d|ML ADD ORDER CD|Order# %ld|Order qty not executable|Sending cancellation", FD,OrderNumber);
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
  int32_t epochTime, Token = 0;;
  int16_t tokenIndex = -1;
  long orderNo = 0;
  double  MLOrderNo = MLOrderResp->OrderNUmber1;
  SwapDouble((char*)&MLOrderNo);
  NSEFO::TRADE_CONFIRMATION_TR SendTradeConf;
  dealerInfoItr dealerItr;
  int buyFD, sellFD;
  CONNINFO*  buyConnInfo = NULL;
  CONNINFO* sellConnInfo = NULL;
  bool status = true;
  for (i=0; i< noOfLegs; i++)
  {
     TTQ = 0;
     Token = MLOrderInfo[i].token - FOOFFSET;
     tokenIndex = MLOrderInfo[i].ordBookIndx;
     while (MLOrderInfo[i].execQty > 0)
     {
         MyTime = getCurrentTimeInNano();  
        epochTime = getEpochTime();
         if(MLOrderInfo[i].buySell == 1)
        {
           if(MLOrderInfo[i].orderPrice >= ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice)
           {
              if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty)
              {
                tradeQty = MLOrderInfo[i].execQty;
              }
              else
              {
                tradeQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty;
              }                                                                                                                                                                                                                                                                                                                                                            
              tradePrice = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice;
           } 
           else
           {
             break;
           }
           
           // For Fcast LTP S
          ME_OrderBook.OrderBook[tokenIndex].LTP = tradePrice;
           // For Fcast LTP E
           
           TTQ = TTQ + tradeQty;
           //ME_OrderBook.OrderBook[tokenIndex].TradeNo++;
           gMETradeNo = gMETradeNo + 1;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ =  ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
           
           /*OrderBook sell order trade*/
           memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
           SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
           SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
           
           /*Pan card changes*/
           SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
           SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
           memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
           /*Pan card changes end*/
           
           SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
           SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId);
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(2);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp  = SendTradeConf.Timestamp1;
//           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.Timestamp2[7] = 1;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
           SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
          
           bool sellfound = getConnInfo (ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, dealerItr);
           if (sellfound == true && (dealerItr->second)->status == LOGGED_ON){
                sellFD = (dealerItr->second)->FD;
                sellConnInfo = (dealerItr->second)->ptrConnInfo;
                SendToClient(sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf), sellConnInfo);
           }
                      
           snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld",sellFD,orderNo, gMETradeNo, tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
           
          /*Fill OrderId in Broadcast Msg. OrderID for ML = 0*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0; 
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
          
           if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty == 0)
           {   
                ORDER_BOOK_DTLS BookDetails;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (sellfound == true){
                     dealerIndex = (dealerItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails, 2, Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
           }   
        }  
        else
        { 
          if(MLOrderInfo[i].orderPrice <= ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice)
          {
                if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty)
               {
                  tradeQty = MLOrderInfo[i].execQty;
               }
               else
               {
                  tradeQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty;
               }
               tradePrice =  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice;
           }     
          else
          {
            break;
          }
          
          // For Fcast LTP S
          ME_OrderBook.OrderBook[tokenIndex].LTP = tradePrice;
          // For Fcast LTP E
          
           TTQ = TTQ + tradeQty;
           //ME_OrderBook.OrderBook[tokenIndex].TradeNo++;
           gMETradeNo = gMETradeNo + 1;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
         
           /*Book buy order trade*/
          memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
           SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
           SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
           
           /*Pan card changes*/
           SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
           SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
           memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
           /*Pan card changes end*/
           
           SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
           SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId);
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(1);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp  = SendTradeConf.Timestamp1;
//           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.Timestamp2[7] = 1;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
           SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
          
           bool buyfound = getConnInfo (ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, dealerItr);
           if (buyfound == true &&(dealerItr->second)->status == LOGGED_ON){
                 buyFD = (dealerItr->second)->FD;
                 buyConnInfo = (dealerItr->second)->ptrConnInfo;
                 SendToClient(buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf), buyConnInfo);
           }
                   
          snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld",buyFD,orderNo,gMETradeNo,tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
                 
          /*Fill OrderId in Broadcast Msg*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0 ;
          
          if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty == 0)
          {   
               ORDER_BOOK_DTLS BookDetails;
               memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));

               int dealerIndex = -1;
               if (buyfound == true){
                  dealerIndex =  (dealerItr->second)->dealerOrdIndex;
               }
               Filltoorderbook(&BookDetails,1, Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
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
//          SendLegTradeConf.Timestamp2 = '1'; 
          SendLegTradeConf.Timestamp2[7] = 1; 
          SendLegTradeConf.ActivityTime  = __bswap_32(epochTime);
          SwapDouble((char*) &SendLegTradeConf.Timestamp);
          SendLegTradeConf.FillNumber =  __bswap_32(gMETradeNo);
          SendLegTradeConf.FillQuantity = __bswap_32(tradeQty);
          SendLegTradeConf.RemainingVolume = __bswap_32(MLOrderInfo[i].orderRemainingQty);
          SendLegTradeConf.FillPrice = __bswap_32(tradePrice);
          SendLegTradeConf.ResponseOrderNumber = MLOrderResp->OrderNUmber1;
          SendLegTradeConf.TraderId = MLOrderResp->TraderId1;
          SendLegTradeConf.TransactionCode = __bswap_16(20222);
          SendLegTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
           int resp = SendToClient( FD, (char *)&SendLegTradeConf , sizeof(SendLegTradeConf),pConnInfo);
           
           snprintf (logBuf, 500, "Thread_ME|FD %d|ML(%d) Trade|Leg %d|Order# %ld|Trade# %d|Qty %d|Price %ld",
                              FD,MLOrderInfo[i].buySell, i+1, int64_t(MLOrderNo), gMETradeNo, tradeQty,tradePrice);
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
          if (true == bEnableBrdcst)
          {
              FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
              FillData.stBcastMsg.stTrdMsg.nToken = MLOrderInfo[i].token;
              FillData.stBcastMsg.stTrdMsg.nTradePrice = tradePrice;
              FillData.stBcastMsg.stTrdMsg.nTradeQty = tradeQty;
              FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
              Inqptr_METoBroadcast_Global->enqueue(FillData);
          }
    }
 
    if (1 == gMLmode)  
    {
        /*Cancel leg if partially filled*/   
        if (MLOrderInfo[i].orderRemainingQty > 0)
        {
            SendMLOrderLegCancellation(MLOrderResp, i, FD, pConnInfo);
        }
    }
  }
  
  if (1 != gMLmode)
  {
       for (i=0; i< noOfLegs; i++)
       {
           if (MLOrderInfo[i].orderRemainingQty > 0)
           {
               SendMLOrderLegCancellation(MLOrderResp, i, FD, pConnInfo);
           }
       }
  }
}

/*Execute order based on the execQty calculated in MLOrderInfo. Cancel partially filled legs*/
int MatchMLOrder(int noOfLegs, NSECD::MS_SPD_OE_REQUEST* MLOrderResp, int FD, CONNINFO* pConnInfo)
{
  int i = 0, tradeQty = 0, tradePrice = 0, TTQ = 0;
  int64_t MyTime;
  int32_t epochTime, Token = 0;;
  int16_t tokenIndex = -1;
  long orderNo = 0;
  double  MLOrderNo = MLOrderResp->OrderNUmber1;
  SwapDouble((char*)&MLOrderNo);
  NSECD::TRADE_CONFIRMATION_TR SendTradeConf;
  dealerInfoItr dealerItr;
  int buyFD, sellFD;
  CONNINFO*  buyConnInfo = NULL;
  CONNINFO* sellConnInfo = NULL;
  bool status = true;
  for (i=0; i< noOfLegs; i++)
  {
     TTQ = 0;
     Token = MLOrderInfo[i].token - FOOFFSET;
     tokenIndex = MLOrderInfo[i].ordBookIndx;
     while (MLOrderInfo[i].execQty > 0)
     {
         MyTime = getCurrentTimeInNano();  
        epochTime = getEpochTime();
         if(MLOrderInfo[i].buySell == 1)
        {
           if(MLOrderInfo[i].orderPrice >= ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice)
           {
              if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty)
              {
                tradeQty = MLOrderInfo[i].execQty;
              }
              else
              {
                tradeQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty;
              }                                                                                                                                                                                                                                                                                                                                                            
              tradePrice = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice;
           } 
           else
           {
             break;
           }
           
           // For Fcast LTP S
          ME_OrderBook.OrderBook[tokenIndex].LTP = tradePrice;
           // For Fcast LTP E
           
           TTQ = TTQ + tradeQty;
           //ME_OrderBook.OrderBook[tokenIndex].TradeNo++;
           gMETradeNo = gMETradeNo + 1;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ =  ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
           
           /*OrderBook sell order trade*/
           memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
           SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
           SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
           
           /*Pan card changes*/
           SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
           SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
           memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
           /*Pan card changes end*/
           
           SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSECD.TokenNo);
           SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId);
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(2);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp  = SendTradeConf.Timestamp1;
//           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.Timestamp2[7] = 1;
           ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
           SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
          
           bool sellfound = getConnInfo (ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, dealerItr);
           if (sellfound == true && (dealerItr->second)->status == LOGGED_ON){
                sellFD = (dealerItr->second)->FD;
                sellConnInfo = (dealerItr->second)->ptrConnInfo;
                SendToClient(sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf), sellConnInfo);
           }
                      
           snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld",sellFD,orderNo, gMETradeNo, tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
           
          /*Fill OrderId in Broadcast Msg. OrderID for ML = 0*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0; 
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
          
           if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty == 0)
           {   
                ORDER_BOOK_DTLS BookDetails;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (sellfound == true){
                     dealerIndex = (dealerItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails, 2, Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
           }   
        }  
        else
        { 
          if(MLOrderInfo[i].orderPrice <= ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice)
          {
                if (MLOrderInfo[i].execQty < ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty)
               {
                  tradeQty = MLOrderInfo[i].execQty;
               }
               else
               {
                  tradeQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty;
               }
               tradePrice =  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice;
           }     
          else
          {
            break;
          }
          
          // For Fcast LTP S
          ME_OrderBook.OrderBook[tokenIndex].LTP = tradePrice;
          // For Fcast LTP E
          
           TTQ = TTQ + tradeQty;
           //ME_OrderBook.OrderBook[tokenIndex].TradeNo++;
           gMETradeNo = gMETradeNo + 1;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + tradeQty;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - tradeQty;
           MLOrderInfo[i].orderRemainingQty = MLOrderInfo[i].orderRemainingQty - tradeQty;
           MLOrderInfo[i].execQty = MLOrderInfo[i].execQty - tradeQty;
         
           /*Book buy order trade*/
          memset(&SendTradeConf, 0, sizeof(SendTradeConf)); // 20222
           orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
           memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber, sizeof(SendTradeConf.AccountNumber));
           SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
           SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
           SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
           
           /*Pan card changes*/
           SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
           SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
           memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
           /*Pan card changes end*/
           
           SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSECD.TokenNo);
           SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId);
           SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);
           SendTradeConf.BuySellIndicator = __bswap_16(1);
           SendTradeConf.FillPrice = __bswap_32(tradePrice);     
           SendTradeConf.Timestamp1 = __bswap_64( MyTime);
           SendTradeConf.Timestamp  = SendTradeConf.Timestamp1;
//           SendTradeConf.Timestamp2 = '1'; 
           SendTradeConf.Timestamp2[7] = 1;
           ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
           SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
           SwapDouble((char*) &SendTradeConf.Timestamp);
           SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
           SendTradeConf.FillQuantity = __bswap_32(tradeQty);
           SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
           SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
           SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
           SendTradeConf.TransactionCode = __bswap_16(20222);
           SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
          
           bool buyfound = getConnInfo (ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, dealerItr);
           if (buyfound == true &&(dealerItr->second)->status == LOGGED_ON){
                 buyFD = (dealerItr->second)->FD;
                 buyConnInfo = (dealerItr->second)->ptrConnInfo;
                 SendToClient(buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf), buyConnInfo);
           }
                   
          snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld",buyFD,orderNo,gMETradeNo,tradeQty,tradePrice);
           Logger::getLogger().log(DEBUG, logBuf);
                 
          /*Fill OrderId in Broadcast Msg*/
          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0 ;
          
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty == 0)
           {   
                ORDER_BOOK_DTLS BookDetails;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));
                
                int dealerIndex = -1;
                if (buyfound == true){
                   dealerIndex =  (dealerItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails,1, Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
           }   
         }      
       
          /*Leg trade*/
          NSECD::TRADE_CONFIRMATION_TR SendLegTradeConf = {0}; // 20222
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
//          SendLegTradeConf.Timestamp2 = '1'; 
          SendLegTradeConf.Timestamp2[7] = 1; 
          SendLegTradeConf.ActivityTime  = __bswap_32(epochTime);
          SwapDouble((char*) &SendLegTradeConf.Timestamp);
          SendLegTradeConf.FillNumber =  __bswap_32(gMETradeNo);
          SendLegTradeConf.FillQuantity = __bswap_32(tradeQty);
          SendLegTradeConf.RemainingVolume = __bswap_32(MLOrderInfo[i].orderRemainingQty);
          SendLegTradeConf.FillPrice = __bswap_32(tradePrice);
          SendLegTradeConf.ResponseOrderNumber = MLOrderResp->OrderNUmber1;
          SendLegTradeConf.TraderId = MLOrderResp->TraderId1;
          SendLegTradeConf.TransactionCode = __bswap_16(20222);
          SendLegTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
           int resp = SendToClient( FD, (char *)&SendLegTradeConf , sizeof(SendLegTradeConf),pConnInfo);
           
           snprintf (logBuf, 500, "Thread_ME|FD %d|ML(%d) Trade|Leg %d|Order# %ld|Trade# %d|Qty %d|Price %ld",
                              FD,MLOrderInfo[i].buySell, i+1, int64_t(MLOrderNo), gMETradeNo, tradeQty,tradePrice);
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
          if (true == bEnableBrdcst)
          {
              FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
              FillData.stBcastMsg.stTrdMsg.nToken = MLOrderInfo[i].token;
              FillData.stBcastMsg.stTrdMsg.nTradePrice = tradePrice;
              FillData.stBcastMsg.stTrdMsg.nTradeQty = tradeQty;
              FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
              Inqptr_METoBroadcast_Global->enqueue(FillData);
          }
    }
 
    if (1 == gMLmode)  
    {
        /*Cancel leg if partially filled*/   
        if (MLOrderInfo[i].orderRemainingQty > 0)
        {
            SendMLOrderLegCancellation(MLOrderResp, i, FD, pConnInfo);
        }
    }
  }
  
  if (1 != gMLmode)
  {
       for (i=0; i< noOfLegs; i++)
       {
           if (MLOrderInfo[i].orderRemainingQty > 0)
           {
               SendMLOrderLegCancellation(MLOrderResp, i, FD, pConnInfo);
           }
       }
  }
}

long Modtoorderbook(ORDER_BOOK_DTLS *Mybookdetails, int BuySellSide, long Token,int IsIOC,int IsDQ,int IsSL, int32_t epochTime, int tokenIndex) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iOldOrdLocn = -1;
    int32_t iNewOrdLocn = -1;
    int32_t iLogRecs = 0;
    GET_PERF_TIME(t1);    
    long ret = 0;
    if(BuySellSide == 1)
    {
      iLogRecs = ME_OrderBook.OrderBook[tokenIndex].BuyRecords;
        if ((Mybookdetails->lPrice < ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice) && 1 == Mybookdetails->IsIOC) {
            ret = 5; /* 5  means cancel IOC Order*/
        }
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokenIndex].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }    
        #endif
          
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)  //Search for the OrderNo
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
            GET_PERF_TIME(t2);          
            iOldOrdLocn = j;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = Mybookdetails->lQty;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ = Mybookdetails->IsDQ;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].FD = Mybookdetails->FD;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].connInfo = Mybookdetails->connInfo;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].dealerID = Mybookdetails->dealerID;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].TraderId = Mybookdetails->TraderId;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].BookType = Mybookdetails->BookType;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].Volume = Mybookdetails->Volume;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].BranchId = Mybookdetails->BranchId;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].UserId = Mybookdetails->UserId;        
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                
                /*Pan card changes*/
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].AlgoCategory = Mybookdetails->AlgoCategory;  
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].AlgoId = Mybookdetails->AlgoId;  
                /*Pan card changes end*/
                
                if (_nSegMode == SEG_NSECM){
                     memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));
                     ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;
                }
                else if(_nSegMode == SEG_NSEFO) {
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                else if(_nSegMode == SEG_NSECD) {
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECD.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECD.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j].nsecm_nsefo_nsecd.NSECD.OrderFlags));
                }
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].LastModified = Mybookdetails->LastModified; 
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].TransactionCode = NSECM_MOD_REQ_TR; /*IOC tick*/
                ME_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = GlobalSeqNo++;
                //memcpy for sending book to sendCancellation(), if required.
                memcpy(Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndex].Buy[j]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook|Buy|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice >= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|4|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
             }
                else
                {
            break; // Added for TC
         }
              }

              for(; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn]), &(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn+1]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
    }  
    else
    {
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Buy|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Buy[j-1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Buy|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
                GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Buy[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Buy[iNewOrdLocn]));                
            }
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }
        #endif
        GET_PERF_TIME(t4);
    }
    else {
      iLogRecs = ME_OrderBook.OrderBook[tokenIndex].SellRecords;
        if ((Mybookdetails->lPrice > ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice) && 1 == Mybookdetails->IsIOC) {
           ret = 5; /*5 means cancel IOC */
        }           
          
        #ifdef __LOG_ORDER_BOOK__
        snprintf(logBuf, 500, "Modtoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokenIndex].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].SellRecords) ; j++)
        {   
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Modtoorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty);          
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
            GET_PERF_TIME(t2);        
            iOldOrdLocn = j;
                //memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j],Mybookdetails , sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j]));
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = Mybookdetails->lPrice;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = Mybookdetails->lQty;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].DQty = Mybookdetails->DQty;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ = Mybookdetails->IsDQ;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].FD = Mybookdetails->FD;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].connInfo = Mybookdetails->connInfo;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].dealerID = Mybookdetails->dealerID;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = Mybookdetails->IsIOC;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].TraderId = Mybookdetails->TraderId;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].BookType = Mybookdetails->BookType;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].Volume = Mybookdetails->Volume;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].BranchId = Mybookdetails->BranchId;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].UserId = Mybookdetails->UserId;        
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].ProClientIndicator = Mybookdetails->ProClientIndicator;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].NnfField = Mybookdetails->NnfField;  
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].BrokerId,&(Mybookdetails->BrokerId),sizeof(Mybookdetails->BrokerId)) ;      
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].Settlor,&(Mybookdetails->Settlor),sizeof(Mybookdetails->Settlor));
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].AccountNumber,&(Mybookdetails->AccountNumber),sizeof(Mybookdetails->AccountNumber));
                
                /*Pan card changes*/
                memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].PAN,&(Mybookdetails->PAN),sizeof(Mybookdetails->PAN));
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].AlgoCategory = Mybookdetails->AlgoCategory;  
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].AlgoId = Mybookdetails->AlgoId;  
                /*Pan card changes end*/ 
                
                if (_nSegMode == SEG_NSECM){
                     ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.Suspended = Mybookdetails->nsecm_nsefo_nsecd.NSECM.Suspended;  
                     memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECM.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECM.OrderFlags));                 
                }
                else if (_nSegMode == SEG_NSEFO){
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSEFO.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.OrderFlags));
                }
                else if (_nSegMode == SEG_NSECD){
                      memcpy(&ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECD.OrderFlags,&(Mybookdetails->nsecm_nsefo_nsecd.NSECD.OrderFlags), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSECD.OrderFlags));
                }
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].LastModified = Mybookdetails->LastModified;  
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].TransactionCode = NSECM_MOD_REQ_TR;
                ME_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = GlobalSeqNo++;
                memcpy (Mybookdetails, &(ME_OrderBook.OrderBook[tokenIndex].Sell[j]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[j]));
            #ifdef __LOG_ORDER_BOOK__
            snprintf(logBuf, 500, "Modtoorderbook|Sell|3|j %d", j);
            Logger::getLogger().log(DEBUG, logBuf);
            #endif
            if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice <= Mybookdetails->lPrice)
            {
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|4|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|5|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice < Mybookdetails->lPrice)
                {
                  continue;
                }
                else
                {
                break; // TC
             }   
         }

              for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j++)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|6|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j+1, ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j+1].lPrice == Mybookdetails->lPrice)
                {
                  continue;
    }    
                else
    /*IOC tick: Do not brdcst for IOC orders*/
    {
                  break;
                }
              }

              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|7|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn]), &(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn+1]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iNewOrdLocn-iOldOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            else
          {
              //for(; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords-1; j--)
              for(; j>0; j--)
              {
                #ifdef __LOG_ORDER_BOOK__
                snprintf(logBuf, 500, "Modtoorderbook|Sell|8|j %d|NextPrice[%d] %d|CurrPrice %d|", j, j-1, ME_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice, Mybookdetails->lPrice);
                Logger::getLogger().log(DEBUG, logBuf);
                #endif
                if(ME_OrderBook.OrderBook[tokenIndex].Sell[j-1].lPrice > Mybookdetails->lPrice)
                {
                  continue;
          }
          else    
          {
                  break;
                }
              }
              iNewOrdLocn = j;
              #ifdef __LOG_ORDER_BOOK__
              snprintf(logBuf, 500, "Modtoorderbook|Sell|9|j %d|OldLcn %d|NewLcn %d", j, iOldOrdLocn, iNewOrdLocn);                
              Logger::getLogger().log(DEBUG, logBuf);
              #endif
              GET_PERF_TIME(t3);
              memmove(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn+1]), &(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), sizeof(ME_OrderBook.OrderBook[tokenIndex].Sell[iOldOrdLocn])*(iOldOrdLocn-iNewOrdLocn));
              memcpy(&(ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]), Mybookdetails, sizeof (ME_OrderBook.OrderBook[tokenIndex].Sell[iNewOrdLocn]));                
            }
            break;
          }
        }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokenIndex].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Modtoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice, ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty, ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);
        }
        #endif
        GET_PERF_TIME(t4);
    }
    if (true == bEnableBrdcst && 1 != (Mybookdetails->IsIOC) && gSimulatorMode == 1) {
        AddModCan.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
        AddModCan.stBcastMsg.stGegenricOrdMsg.cMsgType = 'M';
        if (BuySellSide == 1) {
            AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'B';
        } else {
              AddModCan.stBcastMsg.stGegenricOrdMsg.cOrdType = 'S';
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.dblOrdID = Mybookdetails->OrderNo;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = Mybookdetails->lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = Mybookdetails->lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;   
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
          }
          if (_nSegMode == SEG_NSECD){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet      
    snprintf(logBuf, 200, "Thread_ME|Modtoorderbook|Recs %6d|Search=%6ld|PositionSearch=%6ld|Sort=%6ld|", iLogRecs, t2-t1,t3-t2, t4-t3);
    Logger::getLogger().log(DEBUG, logBuf);
    return ret; 
}


long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token, int32_t epochTime, int dlrIndx, int tokIndx) // 1 Buy , 2 Sell
{
    int64_t t1=0,t2=0,t3=0,t4=0;
    int32_t iLogRecs = 0;

    GET_PERF_TIME(t1);
    long lPrice = 0, lQty = 0;
    short IsIOC = 0;
    int16_t transCode = 0;
    
    if (BuySellSide == 1) {
      iLogRecs = ME_OrderBook.OrderBook[tokIndx].BuyRecords;
      #ifdef __LOG_ORDER_BOOK__  
      snprintf(logBuf, 500, "Cantoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
    {
          snprintf(logBuf, 500, "Cantoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
      #endif    
        for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].BuyRecords); j++) {

          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Cantoorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo == Mybookdetails->OrderNo)
             {
                lPrice =  ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice;
                lQty =  ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].IsDQ =0 ;
                IsIOC = ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].LastModified = Mybookdetails->LastModified;
                transCode = ME_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode = NSECM_CAN_REQ_TR; /*IOC tick*/
                
                GET_PERF_TIME(t2);
                //SortBuySideBook(tokIndx);
                memmove(&(ME_OrderBook.OrderBook[tokIndx].Buy[j]), &(ME_OrderBook.OrderBook[tokIndx].Buy[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Buy[j])*(ME_OrderBook.OrderBook[tokIndx].BuyRecords-j-1));
                GET_PERF_TIME(t3);
                
                ME_OrderBook.OrderBook[tokIndx].BuyRecords = ME_OrderBook.OrderBook[tokIndx].BuyRecords - 1; 
               
                int lnBuyRec = ME_OrderBook.OrderBook[tokIndx].BuyRecords;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lPrice = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsDQ =0 ;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].LastModified = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].TransactionCode = 0;
                
                if (dlrIndx != -1)
                {
                   dealerOrdArr[dlrIndx][tokIndx].buyordercnt--;
                   
                }
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Cantoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
    }  
        #endif
    }
    else {
        iLogRecs = ME_OrderBook.OrderBook[tokIndx].SellRecords;
        #ifdef __LOG_ORDER_BOOK__  
        snprintf(logBuf, 500, "Cantoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
    {
          snprintf(logBuf, 500, "Cantoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
        #endif      
        for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].SellRecords); j++) {

          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Cantoorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif
             if(ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo == Mybookdetails->OrderNo)
             {
                lPrice =  ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice;
                lQty =  ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice = 2147483647;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].IsDQ =0;
                IsIOC = ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].LastModified = Mybookdetails->LastModified;
                transCode = ME_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode = NSECM_CAN_REQ_TR; /*IOC tick*/
                GET_PERF_TIME(t2);
                //SortSellSideBook(tokIndx);
                memmove(&(ME_OrderBook.OrderBook[tokIndx].Sell[j]), &(ME_OrderBook.OrderBook[tokIndx].Sell[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Sell[j])*(ME_OrderBook.OrderBook[tokIndx].SellRecords-j-1));
                GET_PERF_TIME(t3);
                ME_OrderBook.OrderBook[tokIndx].SellRecords = ME_OrderBook.OrderBook[tokIndx].SellRecords - 1;
                
                int lnSellRec = ME_OrderBook.OrderBook[tokIndx].SellRecords;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lPrice = 2147483647;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsDQ =0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].LastModified = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].TransactionCode = 0;
                
                if (dlrIndx != -1)
                {
                   dealerOrdArr[dlrIndx][tokIndx].sellordercnt--;
                }
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Cantoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
    }    
        #endif
    }
    // Enqueue Broadcast Packet 
    /*IOC tick: Do not brdcast for IOC orders*/
    if (true == bEnableBrdcst && (1 != IsIOC || (1== IsIOC && transCode != NSECM_ADD_REQ_TR))  && gSimulatorMode == 1)
    {
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
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
          }
          if (_nSegMode == SEG_NSECD){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = epochTime;
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet    
    snprintf(logBuf, 200, "Thread_ME|Cantoorderbook|Search=%ld|Sort=%ld", t2-t1,t3-t2);
    Logger::getLogger().log(DEBUG, logBuf);
}


long Filltoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token, int dlrIndx, int tokIndx) // 1 Buy , 2 Sell
{
    long lQty = 0, lPrice = 0;
    short IsIOC = 0;
    int16_t transCode = 0;
    
    if(BuySellSide == 1)
    {
        #ifdef __LOG_ORDER_BOOK__  
        snprintf(logBuf, 500, "Filltoorderbook|Buy|1|BuyRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].BuyRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
    {
          snprintf(logBuf, 500, "Filltoorderbook|Buy|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
        #endif
        for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].BuyRecords); j++) {
          #ifdef __LOG_ORDER_BOOK__
          snprintf(logBuf, 500, "Filltoorderbook|Buy|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif

          if (ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo == Mybookdetails->OrderNo) {
                lQty = ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty;
                lPrice = ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice;
                IsIOC = ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC;
                transCode = ME_OrderBook.OrderBook[tokIndx].Buy[j].TransactionCode;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].IsDQ =0 ;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty = 0;

                //SortBuySideBook(tokIndx);
                memmove(&(ME_OrderBook.OrderBook[tokIndx].Buy[j]), &(ME_OrderBook.OrderBook[tokIndx].Buy[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Buy[j])*(ME_OrderBook.OrderBook[tokIndx].BuyRecords-j-1));
                ME_OrderBook.OrderBook[tokIndx].BuyRecords = ME_OrderBook.OrderBook[tokIndx].BuyRecords - 1;
                
                int lnBuyRec = ME_OrderBook.OrderBook[tokIndx].BuyRecords;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lPrice = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsDQ =0 ;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Buy[lnBuyRec].lQty = 0;
                if (dlrIndx != -1) {
                    dealerOrdArr[dlrIndx][tokIndx].buyordercnt--;
                }
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].BuyRecords; j++)
        {
          snprintf(logBuf, 500, "Filltoorderbook|Buy|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Buy[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Buy[j].lQty, ME_OrderBook.OrderBook[tokIndx].Buy[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }
        #endif
         //return 0;
    }  
    else {
        #ifdef __LOG_ORDER_BOOK__  
        snprintf(logBuf, 500, "Filltoorderbook|Sell|1|SellRecs %d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", ME_OrderBook.OrderBook[tokIndx].SellRecords, Mybookdetails->lPrice, Mybookdetails->lQty, Mybookdetails->OrderNo);
        Logger::getLogger().log(DEBUG, logBuf);
        for(int j=0; j < ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Filltoorderbook|Sell|Before|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
        #endif
        for (int j = 0; j < (ME_OrderBook.OrderBook[tokIndx].SellRecords); j++) {
          #ifdef __LOG_ORDER_BOOK__  
          snprintf(logBuf, 500, "Filltoorderbook|Sell|2|j %d|CurrOrdNo %0.0f|ReqOrdNo %0.0f|Price %d|Qty %d|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo, Mybookdetails->OrderNo, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty);
          Logger::getLogger().log(DEBUG, logBuf);
          #endif

          if (ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo == Mybookdetails->OrderNo) {
                lQty = ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty;
                lPrice = ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice;
                IsIOC = ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC;
                transCode = ME_OrderBook.OrderBook[tokIndx].Sell[j].TransactionCode;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice = 2147483647;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].IsDQ =0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty = 0;

                //SortSellSideBook(tokIndx);
                memmove(&(ME_OrderBook.OrderBook[tokIndx].Sell[j]), &(ME_OrderBook.OrderBook[tokIndx].Sell[j+1]), sizeof(ME_OrderBook.OrderBook[tokIndx].Sell[j])*(ME_OrderBook.OrderBook[tokIndx].SellRecords-j-1));                
                ME_OrderBook.OrderBook[tokIndx].SellRecords = ME_OrderBook.OrderBook[tokIndx].SellRecords - 1; 
                
                int lnSellRec = ME_OrderBook.OrderBook[tokIndx].SellRecords;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lPrice = 2147483647;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].DQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsDQ =0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].IsIOC = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OpenQty = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].OrderNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].SeqNo = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].TTQ = 0;
                ME_OrderBook.OrderBook[tokIndx].Sell[lnSellRec].lQty = 0;
                
                
                 if (dlrIndx != -1)
                {
                    dealerOrdArr[dlrIndx][tokIndx].sellordercnt--;
                 }
                break; // TC
             }   
         }
        #ifdef __LOG_ORDER_BOOK__  
        for(int j=0; j<ME_OrderBook.OrderBook[tokIndx].SellRecords; j++)
        {
          snprintf(logBuf, 500, "Filltoorderbook|Sell|After|j %2d|Price %6d|Qty %6d|CurrOrdNo %0.0f|", j, ME_OrderBook.OrderBook[tokIndx].Sell[j].lPrice, ME_OrderBook.OrderBook[tokIndx].Sell[j].lQty, ME_OrderBook.OrderBook[tokIndx].Sell[j].OrderNo);
          Logger::getLogger().log(DEBUG, logBuf);;
        }  
        #endif
       //  return 0;
    }

    /*IOC tick: Do not brdcast for IOC orders*/
    if (true == bEnableBrdcst && (1== IsIOC && transCode != NSECM_ADD_REQ_TR))
    {
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
          AddModCan.stBcastMsg.stGegenricOrdMsg.nPrice = lPrice;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nQty = lQty;
          AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token;    
          if (_nSegMode == SEG_NSEFO){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token + FOOFFSET; 
          }
          if (_nSegMode == SEG_NSECD){
             AddModCan.stBcastMsg.stGegenricOrdMsg.nToken = Token; 
          }
          AddModCan.stBcastMsg.stGegenricOrdMsg.lTimeStamp = getEpochTime();
          Inqptr_METoBroadcast_Global->enqueue(AddModCan);
    }
    // End Enqueue Broadcast Packet      
}

/*Market Book Builder starts*/

int MatchingBookBuilder(long Token, int FD,int IsIOC,int IsDQ,CONNINFO* pConnInfo, int tokenIndex)
{
    //std::cout << "Matching Called " << std::endl;
    /*Sneha - multiple connection changes:15/07/16 - S*/
    dealerInfoItr sellItr, buyItr;
    long orderNo = 0;
    /*Sneha - multiple connection changes:15/07/16 - E*/
    
    if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice <=  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice )   // Bid is greater than or equal to ask
    {

        int loop = 1;
        int TradeQty;
        long TradePrice;
        int resp; 
        //int MyTime = GlobalSeqNo++;              
        int buyFD = 0, sellFD = 0;
        CONNINFO*  buyConnInfo = NULL;
        CONNINFO* sellConnInfo = NULL;
        
        int64_t MyTime = getCurrentTimeInNano();  
        int32_t epochTime = getEpochTime();
       
        snprintf (logBuf, 500, "Thread_ME|Match found|Sell Order|Order# %ld|Qty %ld|Price %ld|Token %ld|Buy Order|Order# %ld|Qty %ld|Price %ld",
                           long(ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo),  ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty,  
                           ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice, ME_OrderBook.OrderBook[tokenIndex].Token,
                           long(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo),  ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lQty,  
                           ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
        Logger::getLogger().log(DEBUG, logBuf);
        
        bool buyfound=false;
        
        bool sellfound = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, sellItr);
        if (sellfound == true){
          sellFD = (sellItr->second)->FD; 
          sellConnInfo = (sellItr->second)->ptrConnInfo;  
        }
        
        while(loop > 0)
        {
            /*Sneha - DQty changes - S*/
            long buyQty = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lQty; 
            long sellQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty;
            if (ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsDQ && 
                (ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty < sellQty))
            {
               sellQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty;
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
            if(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].SeqNo < ME_OrderBook.OrderBook[tokenIndex].Sell[0].SeqNo)
            {
                TradePrice = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice;                
            }    
            else
            {
               TradePrice = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice;    
            }    
            
            ME_OrderBook.OrderBook[tokenIndex].LTP = TradePrice;    // FFeed Changes
            ME_MB_OrderBook.OrderBook[tokenIndex].LTP = TradePrice;    //Market Book builder FFeed Changes
            snprintf (logBuf, 200, "Thread_ME|LTP %d|Token %d", TradePrice, ME_OrderBook.OrderBook[tokenIndex].Token);
            Logger::getLogger().log(DEBUG, logBuf);
                 
                // ---- Sell Handling 
                 //ME_OrderBook.OrderBook[tokenIndex].TradeNo = ME_OrderBook.OrderBook[tokenIndex].TradeNo + 1;
                 gMETradeNo = gMETradeNo + 1;
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ + TradeQty;
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty - TradeQty;
                 
                 switch(_nSegMode)
                 {
                   case SEG_NSECM: //Order_Store_NSECM
                   {
                     NSECM::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber),
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     memcpy(&SendTradeConf.sec_info, &(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(SendTradeConf.sec_info));
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);   
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.UserId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId); 
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
                      
                                          
                      /*Sneha - multiple connection changes:15/07/16 - S*/
                     if (sellfound == true && (sellItr->second)->status == LOGGED_ON){
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                     }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                       sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday), __bswap_32 (SendTradeConf.ActivityTime));
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<sellFD<<"|Sell Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(sellFD >0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                      /*Sneha - E*/     
  
                      // ---- Buy Handling
                          
                      
                      orderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                     SendTradeConf.Price = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      

                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo = __bswap_32(0);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.RemainingVolume = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      
                      
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty  %d|Price %ld|Token %ld",
                     buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token);
                     Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                     if(buyFD > 0)
                     {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                                             
                      
                   }
                   break;
                   case SEG_NSEFO: //Order_Store_NSEFO
                   {
                     NSEFO::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);  
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                       /*Sneha - multiple connection changes:15/07/16 - S*/
                       SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId);
                       //resp = write( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (sellfound == true && (sellItr->second)->status == LOGGED_ON){
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                      }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                         sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET), __bswap_32(SendTradeConf.VolumeFilledToday),
                         __bswap_32(SendTradeConf.ActivityTime));
                     Logger::getLogger().log(DEBUG, logBuf);
                     if(sellFD > 0)
                     {
                       memset (&LogData, 0, sizeof(LogData));
                       LogData.MyFd = 2; /*2 = Trade response*/
                       memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                       Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                       /*Sneha - E*/ 
           
                      // ---- Buy Handling
                          
                      orderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      
                      
                      SendTradeConf.RemainingVolume = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_MB_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                  
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld",
                        buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET) );
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(buyFD > 0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                      
                   }
                   break;
                   default:
                     break;
                 }
                   
                 
                // ---- Buy Handling ----------------------------------------------------
            if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty == 0)
            {   
                /*Sneha - DQty changes - S*/
                ORDER_BOOK_DTLS BookDetails;
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (sellfound == true){
                     dealerIndex = (sellItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails,2,Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
            }   
                
        } 
        MatchingBookBuilder(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else if(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice <=  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice)
    {

        int loop = 1;
        int TradeQty;
        long TradePrice;
        int resp; 
        //int MyTime = GlobalSeqNo++;              
        int buyFD = 0, sellFD = 0;
        CONNINFO*  buyConnInfo = NULL;
        CONNINFO* sellConnInfo = NULL;
        
        int64_t MyTime = getCurrentTimeInNano();  
        int32_t epochTime = getEpochTime();
       
        snprintf (logBuf, 500, "Thread_ME|Match found|Sell Order|Order# %ld|Qty %ld|Price %ld|Token %ld|Buy Order|Order# %ld|Qty %ld|Price %ld",
                           long(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo),  ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lQty,  
                           ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice, ME_OrderBook.OrderBook[tokenIndex].Token,
                           long(ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo),  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty,  
                           ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
        Logger::getLogger().log(DEBUG, logBuf);
        
        bool buyfound = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, buyItr);
        if (buyfound == true){ 
           buyFD = (buyItr->second)->FD;
           buyConnInfo = (buyItr->second)->ptrConnInfo;
        }
        
        
        while(loop > 0)
        {
            /*Sneha - DQty changes - S*/
            long buyQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty; 
            long sellQty = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lQty;
        
            if (ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsDQ &&
                (ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty < buyQty))
            {
                buyQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty;
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
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].SeqNo < ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].SeqNo)
            {
                TradePrice = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice;                
            }    
            else
            {
               TradePrice = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice;    
            }    
            ME_MB_OrderBook.OrderBook[tokenIndex].LTP = TradePrice;
            ME_OrderBook.OrderBook[tokenIndex].LTP = TradePrice;    // FFeed Changes
            snprintf (logBuf, 200, "Thread_ME|LTP %d|Token %d", TradePrice, ME_OrderBook.OrderBook[tokenIndex].Token);
            Logger::getLogger().log(DEBUG, logBuf);
                 
                // ---- Sell Handling 
                 //ME_OrderBook.OrderBook[tokenIndex].TradeNo = ME_OrderBook.OrderBook[tokenIndex].TradeNo + 1;
                 gMETradeNo = gMETradeNo + 1;
                 
                 
                 switch(_nSegMode)
                 {
                   case SEG_NSECM: //Order_Store_NSECM
                   {
                     NSECM::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     SendTradeConf.Price = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);     
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.ResponseOrderNumber = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
                      
                                        
                     
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld",
                       sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token);
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<sellFD<<"|Sell Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(sellFD >0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                      /*Sneha - E*/     
  
                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber,sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                     memcpy(&SendTradeConf.sec_info, &ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSECM.sec_info,sizeof(SendTradeConf.sec_info));
                     //std::cout<<"Matching|Symbol-Series ="<<SendTradeConf.sec_info.Symbol<<"-"<<SendTradeConf.sec_info.Series<<std::endl;
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);

                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo = __bswap_32(0);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16 - S*/
                      SendTradeConf.UserId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId); 
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf)); 
                      
                      
                      if (buyfound == true && (buyItr->second)->status == LOGGED_ON){
                          resp = SendToClient(buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf), buyConnInfo);
                      }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty  %d|Price %ld|Token %ld|VFT %d|LMT %d",
                     buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday), __bswap_32(SendTradeConf.ActivityTime));
                     Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                     if(buyFD > 0)
                     {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                                               
                      
                   }
                   break;
                   case SEG_NSEFO: //Order_Store_NSEFO
                   {
                     NSEFO::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     SendTradeConf.Price = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);     
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.ResponseOrderNumber = ME_MB_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                     
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld",
                         sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET));
                     Logger::getLogger().log(DEBUG, logBuf);
                     if(sellFD > 0)
                     {
                       memset (&LogData, 0, sizeof(LogData));
                       LogData.MyFd = 2; /*2 = Trade response*/
                       memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                       Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                       /*Sneha - E*/ 
           
                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16*/
                      SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId);
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (buyfound == true && (buyItr->second)->status == LOGGED_ON){
                           resp = SendToClient( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf),buyConnInfo);
                      }
                  
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                        buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET), __bswap_32(SendTradeConf.VolumeFilledToday),
                        __bswap_32(SendTradeConf.ActivityTime));
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(buyFD > 0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                                               
                      
                   }
                   break;
                   default:
                     break;
                 }
                   
                 
                // ---- Buy Handling ----------------------------------------------------
             
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty == 0 )
            {
                /*Sneha - multiple connection changes:15/07/16 - S*/
                ORDER_BOOK_DTLS BookDetails;
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (buyfound == true){
                     dealerIndex = (buyItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails,1,Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
                  /*Sneha - DQty changes - E*/
                /*Sneha - multiple connection changes:15/07/16 - E*/
            }    
        } 
        MatchingBookBuilder(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    }
    else
    {
        ORDER_BOOK_DTLS BookDetails;
        if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsIOC == 1)
        {
            //long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token) // 1 Buy , 2 Sell
            bool sellStatus = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, sellItr);
             /*int sellFD = (sellItr->second)->FD; 
            CONNINFO* sellConnInfo = (sellItr->second)->ptrConnInfo;  */
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
            //std::cout <<  "Sell IOC Check " << std::endl;
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/  
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECM(&BookDetails, Token,FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSEFO(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
             }        
            int dealerIndex = -1;
            if (sellStatus == true){
                dealerIndex = (sellItr->second)->dealerOrdIndex;
            }
            Cantoorderbook(&BookDetails, 2, Token, getEpochTime(), dealerIndex, tokenIndex); // 1 Buy , 2 Sell            
            
        }   
        if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsIOC == 1)
        {
            //std::cout <<  "Buy IOC Check " << std::endl;
            bool buyStatus = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, buyItr);
            /*int buyFD = (buyItr->second)->FD;
            CONNINFO*  buyConnInfo = (buyItr->second)->ptrConnInfo;*/
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECM(&BookDetails, Token,FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSEFO(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
              
            }         
            int dealerIndex = -1;
            if (buyStatus == true){
                dealerIndex = (buyItr->second)->dealerOrdIndex;
            }
            Cantoorderbook(&BookDetails, 1, Token, getEpochTime(), dealerIndex, tokenIndex); // 1 Buy , 2 Sell
      }  
      
    }  
    /*Matching Engine Update for SL orders Starts NK*/
    
    for(int j = 0;j < ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords;j++)
    {
      if(ME_OrderBook.OrderBook[tokenIndex].LTP > 0 && ME_OrderBook.OrderBook[tokenIndex].LTP <= ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice)
      {
        
        /*Broadcast SL Order*/
        switch(_nSegMode)
        {
          case SEG_NSECM:
          {
            SLOrderTriggeredResponseCM(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j],tokenIndex);
          }
          break;
          case SEG_NSEFO:
          {
            SLOrderTriggeredResponseFO(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j],tokenIndex);
          }  
          break;
        }
        
      }
      else
      {
        break;
      }
    }
    for(int j = 0;j < ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords;j++)
    {
      if(ME_OrderBook.OrderBook[tokenIndex].LTP > 0 && ME_OrderBook.OrderBook[tokenIndex].LTP >= ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice)
      {
        switch(_nSegMode)
        {
          case SEG_NSECM:
          {
            SLOrderTriggeredResponseCM(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j],tokenIndex);
          }
          break;
          case SEG_NSEFO:
          {
            SLOrderTriggeredResponseFO(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j],tokenIndex);
          }  
          break;
        }
      }
      else
      {
        break;
      }
    }
    /*Matching Engine Update for SL orders Ends NK*/
}

/*Market Book Builder Ends*/

int Matching(long Token, int FD,int IsIOC,int IsDQ,CONNINFO* pConnInfo, int tokenIndex)
{
    //std::cout << "Matching Called " << std::endl;
    /*Sneha - multiple connection changes:15/07/16 - S*/
    dealerInfoItr sellItr, buyItr;
    long orderNo = 0;
    /*Sneha - multiple connection changes:15/07/16 - E*/
    
    if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice <=  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice )   // Bid is greater than or equal to ask
    {

        int loop = 1;
        int TradeQty;
        long TradePrice;
        int resp; 
        //int MyTime = GlobalSeqNo++;              
        int buyFD = 0, sellFD = 0;
        CONNINFO*  buyConnInfo = NULL;
        CONNINFO* sellConnInfo = NULL;
        
        int64_t MyTime = getCurrentTimeInNano();  
        int32_t epochTime = getEpochTime();
        
        snprintf (logBuf, 500, "Thread_ME|Match found|Sell Order|Order# %ld|Qty %ld|Price %ld|Token %ld|Buy Order|Order# %ld|Qty %ld|Price %ld",
                           long(ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo),  ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty,  
                           ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice, ME_OrderBook.OrderBook[tokenIndex].Token,
                           long(ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo),  ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty,  
                           ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
        Logger::getLogger().log(DEBUG, logBuf);
        
        bool buyfound = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, buyItr);
        if (buyfound == true){ 
           buyFD = (buyItr->second)->FD;
           buyConnInfo = (buyItr->second)->ptrConnInfo;
        }
        
        bool sellfound = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, sellItr);
        if (sellfound == true){
          sellFD = (sellItr->second)->FD; 
          sellConnInfo = (sellItr->second)->ptrConnInfo;  
        }
        
        while(loop > 0)
        {
            /*Sneha - DQty changes - S*/
            long buyQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty; 
            long sellQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty;
            if (ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsDQ && 
                (ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty < sellQty))
            {
               sellQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty;
            }
            if (ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsDQ &&
                (ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty < buyQty))
            {
                buyQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty;
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
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].SeqNo < ME_OrderBook.OrderBook[tokenIndex].Sell[0].SeqNo)
            {
                TradePrice = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice;                
            }    
            else
            {
               TradePrice = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice;    
            }    
            
            ME_OrderBook.OrderBook[tokenIndex].LTP = TradePrice;    // FFeed Changes
            snprintf (logBuf, 200, "Thread_ME|LTP %d|Token %d", TradePrice, ME_OrderBook.OrderBook[tokenIndex].Token);
            Logger::getLogger().log(DEBUG, logBuf);
                 
                // ---- Sell Handling 
                 //ME_OrderBook.OrderBook[tokenIndex].TradeNo = ME_OrderBook.OrderBook[tokenIndex].TradeNo + 1;
                 gMETradeNo = gMETradeNo + 1;
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ + TradeQty;
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty - TradeQty;
                 
                 switch(_nSegMode)
                 {
                   case SEG_NSECM: //Order_Store_NSECM
                   {
                     NSECM::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber),
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     
                     /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                     memcpy(&SendTradeConf.sec_info, &(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSECM.sec_info),sizeof(SendTradeConf.sec_info));
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);     
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.UserId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId); 
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);
                      
                                          
                      /*Sneha - multiple connection changes:15/07/16 - S*/
                     if (sellfound == true && (sellItr->second)->status == LOGGED_ON){
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                     }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                       sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday), __bswap_32 (SendTradeConf.ActivityTime));
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<sellFD<<"|Sell Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
//                      std::cout<<"Pan::"<<SendTradeConf.PAN<<"|"<<ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN<<"|Cat::"<<__bswap_16(SendTradeConf.AlgoCategory)<<"|"<<ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory<<"|Id:"<<ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId<<std::endl;
                      if(sellFD >0)
                      {
                        memset (&LogData, 0, sizeof(LogData));
                        LogData.MyFd = 2; /*2 = Trade response*/
                        memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                        Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                      /*Sneha - E*/     
  
                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber,sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                     memcpy(&SendTradeConf.sec_info, &ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSECM.sec_info,sizeof(SendTradeConf.sec_info));
                     //std::cout<<"Matching|Symbol-Series ="<<SendTradeConf.sec_info.Symbol<<"-"<<SendTradeConf.sec_info.Series<<std::endl;
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);

                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECM::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo = __bswap_32(0);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16 - S*/
                      SendTradeConf.UserId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId); 
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf)); 
                      
                      
                      if (buyfound == true && (buyItr->second)->status == LOGGED_ON){
                          resp = SendToClient(buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf), buyConnInfo);
                      }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty  %d|Price %ld|Token %ld|VFT %d|LMT %d",
                     buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday), __bswap_32(SendTradeConf.ActivityTime));
                     Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                     if(buyFD > 0)
                     {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                      /*Sneha - E*/
                        // Enqueue Broadcast Packet 
                        //FillData.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                      if (true == bEnableBrdcst ) {
                        FillData.stBcastMsg.stTrdMsg.header.nSeqNo= GlobalBrodcastSeqNo++;
                        /*IOC tick: set order no zero for IOC orders*/
                        if (1 == ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsIOC){
                           FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0;
                        }
                        else {
                          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                        }
                        if (1 == ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsIOC){
                          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0;
                        }
                        else {
                           FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                        }
                        FillData.stBcastMsg.stTrdMsg.nToken = Token;
                        FillData.stBcastMsg.stTrdMsg.nTradePrice = TradePrice;
                        FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeQty;
                        FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
                        Inqptr_METoBroadcast_Global->enqueue(FillData);
                      }
                       // End Enqueue Broadcast Packet                         
                      
                   }
                   break;
                   case SEG_NSEFO: //Order_Store_NSEFO
                   {
                     NSEFO::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                       /*Sneha - multiple connection changes:15/07/16 - S*/
                       SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId);
                       //resp = write( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (sellfound == true && (sellItr->second)->status == LOGGED_ON){
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                      }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                         sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET), __bswap_32(SendTradeConf.VolumeFilledToday),
                         __bswap_32(SendTradeConf.ActivityTime));
                     Logger::getLogger().log(DEBUG, logBuf);
                     if(sellFD > 0)
                     {
                       memset (&LogData, 0, sizeof(LogData));
                       LogData.MyFd = 2; /*2 = Trade response*/
                       memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                       Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                       /*Sneha - E*/ 
           
                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSEFO.TokenNo);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSEFO::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  __bswap_64(MyTime);
                      SendTradeConf.Timestamp = __bswap_64(MyTime);
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16*/
                      SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId);
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (buyfound == true && (buyItr->second)->status == LOGGED_ON){
                           resp = SendToClient( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf),buyConnInfo);
                      }
                  
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                        buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, (Token+FOOFFSET), __bswap_32(SendTradeConf.VolumeFilledToday),
                        __bswap_32(SendTradeConf.ActivityTime));
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(buyFD > 0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                      /*Sneha - E*/ 
                        // Enqueue Broadcast Packet 
                        //FillData.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                      if (true == bEnableBrdcst){
                        FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                        /*IOC tick: set order no zero for IOC orders*/
                        if (1 ==  ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsIOC){
                          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0;
                        }
                        else {
                          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                        }
                        if (1 == ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsIOC){
                          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0;
                        }
                        else {
                           FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                        }
                        FillData.stBcastMsg.stTrdMsg.nToken = ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSEFO.TokenNo;
                        FillData.stBcastMsg.stTrdMsg.nTradePrice = TradePrice;
                        FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeQty;
                        FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
                        Inqptr_METoBroadcast_Global->enqueue(FillData);
                      }
                        // End Enqueue Broadcast Packet                         
                      
                   }
                   break;
                   case SEG_NSECD: //Order_Store_NSECD
                   {
                     NSECD::TRADE_CONFIRMATION_TR SendTradeConf; // 20222
                     orderNo = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                     memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Sell[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSECD.TokenNo);
                     SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TTQ);
                     SendTradeConf.BuySellIndicator = __bswap_16(2);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Sell[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                       /*Sneha - multiple connection changes:15/07/16 - S*/
                       SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Sell[0].TraderId);
                       //resp = write( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (sellfound == true && (sellItr->second)->status == LOGGED_ON){
                           resp = SendToClient( sellFD , (char *)&SendTradeConf , sizeof(SendTradeConf),sellConnInfo);
                      }
                     snprintf (logBuf, 500, "Thread_ME|FD %d|Sell Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                         sellFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday),
                         __bswap_32(SendTradeConf.ActivityTime));
                     Logger::getLogger().log(DEBUG, logBuf);
                     if(sellFD > 0)
                     {
                       memset (&LogData, 0, sizeof(LogData));
                       LogData.MyFd = 2; /*2 = Trade response*/
                       memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                       Inqptr_MeToLog_Global->enqueue(LogData);
                     }
                       /*Sneha - E*/ 
           
                      // ---- Buy Handling
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty = ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty - TradeQty;    
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ = ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ + TradeQty;
                      orderNo = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      memcpy(&SendTradeConf.AccountNumber,ME_OrderBook.OrderBook[tokenIndex].Buy[0].AccountNumber,
                       sizeof(SendTradeConf.AccountNumber));
                     SendTradeConf.BookType = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].BookType);
                     SendTradeConf.DisclosedVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].DQty);
                     SendTradeConf.Price = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lPrice);
                     SendTradeConf.Token = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSECD.TokenNo);
                      SendTradeConf.BuySellIndicator = __bswap_16(1);
                      SendTradeConf.FillPrice = __bswap_32(TradePrice);
                      
                      /*Pan card changes*/
                      SendTradeConf.AlgoCategory = __bswap_16(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoCategory);
                      SendTradeConf.AlgoId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].AlgoId);
                      memcpy(&SendTradeConf.PAN,&(ME_OrderBook.OrderBook[tokenIndex].Buy[0].PAN),sizeof(SendTradeConf.PAN));
                     /*Pan card changes end*/
                      
                      SendTradeConf.VolumeFilledToday = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TTQ);
                      SendTradeConf.FillNumber =  __bswap_32(gMETradeNo);
                      SendTradeConf.tap_hdr.sLength =  __bswap_16(sizeof(NSECD::TRADE_CONFIRMATION_TR));
                      SendTradeConf.tap_hdr.iSeqNo =  0;
                      SendTradeConf.FillQuantity = __bswap_32(TradeQty);
                      SendTradeConf.Timestamp1 =  MyTime;
                      SendTradeConf.Timestamp = MyTime;
//                      SendTradeConf.Timestamp2 = '1'; /*Sneha*/
                      SendTradeConf.Timestamp2[7] = 1; /*Sneha*/
                      ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified = ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified + 1;
                      SendTradeConf.ActivityTime  = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].LastModified);
                      SendTradeConf.Timestamp1 = __bswap_64(SendTradeConf.Timestamp1); /*sneha*/
                      SwapDouble((char*) &SendTradeConf.Timestamp);
                      SendTradeConf.RemainingVolume = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty);
                      SendTradeConf.TransactionCode = __bswap_16(20222);
                      SendTradeConf.ResponseOrderNumber = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                      SwapDouble((char*) &SendTradeConf.ResponseOrderNumber);

                      /*Sneha - multiple connection changes:15/07/16*/
                      SendTradeConf.TraderId = __bswap_32(ME_OrderBook.OrderBook[tokenIndex].Buy[0].TraderId);
                      //resp = write( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf));
                      if (buyfound == true && (buyItr->second)->status == LOGGED_ON){
                           resp = SendToClient( buyFD , (char *)&SendTradeConf , sizeof(SendTradeConf),buyConnInfo);
                      }
                  
                      snprintf (logBuf, 500, "Thread_ME|FD %d|Buy Trade|Order# %ld|Trade# %d|Qty %d|Price %ld|Token %ld|VFT %d|LMT %d",
                        buyFD,orderNo,gMETradeNo,TradeQty,TradePrice, Token, __bswap_32(SendTradeConf.VolumeFilledToday),
                        __bswap_32(SendTradeConf.ActivityTime));
                      Logger::getLogger().log(DEBUG, logBuf);
                      //std::cout<<"FD "<<buyFD<<"|Buy Trade"<<"|Order# "<<orderNo<<"|Qty "<<TradeQty<<"|Price "<<TradePrice<<std::endl;
                      if(buyFD > 0)
                      {
                      memset (&LogData, 0, sizeof(LogData));
                      LogData.MyFd = 2; /*2 = Trade response*/
                      memcpy (LogData.msgBuffer, (void*)&SendTradeConf, sizeof(LogData.msgBuffer));
                      Inqptr_MeToLog_Global->enqueue(LogData);
                      }
                        // Enqueue Broadcast Packet 
                        //FillData.stBcastMsg.stGegenricOrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                      if (true == bEnableBrdcst){
                        FillData.stBcastMsg.stTrdMsg.header.nSeqNo = GlobalBrodcastSeqNo++;
                        /*IOC tick: set order no zero for IOC orders*/
                        if (1 ==  ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsIOC){
                          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = 0;
                        }
                        else {
                          FillData.stBcastMsg.stTrdMsg.dblBuyOrdID = ME_OrderBook.OrderBook[tokenIndex].Buy[0].OrderNo;
                        }
                        if (1 == ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsIOC){
                          FillData.stBcastMsg.stTrdMsg.dblSellOrdID = 0;
                        }
                        else {
                           FillData.stBcastMsg.stTrdMsg.dblSellOrdID = ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo;
                        }
                        FillData.stBcastMsg.stTrdMsg.nToken = ME_OrderBook.OrderBook[tokenIndex].Buy[0].nsecm_nsefo_nsecd.NSECD.TokenNo;
                        FillData.stBcastMsg.stTrdMsg.nTradePrice = TradePrice;
                        FillData.stBcastMsg.stTrdMsg.nTradeQty = TradeQty;
                        FillData.stBcastMsg.stTrdMsg.lTimestamp = epochTime;
                        Inqptr_METoBroadcast_Global->enqueue(FillData);
                      }
                        // End Enqueue Broadcast Packet                         
                      
                   }
                   break;
                   default:
                     break;
                 }
                   
                 
                // ---- Buy Handling ----------------------------------------------------
            if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty == 0)
            {   
                /*Sneha - DQty changes - S*/
                ORDER_BOOK_DTLS BookDetails;
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (sellfound == true){
                     dealerIndex = (sellItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails,2,Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
            }   
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].lQty == 0 )
            {
                /*Sneha - multiple connection changes:15/07/16 - S*/
                ORDER_BOOK_DTLS BookDetails;
                loop = 0;
                memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));
                int dealerIndex = -1;
                if (buyfound == true){
                     dealerIndex = (buyItr->second)->dealerOrdIndex;
                }
                Filltoorderbook(&BookDetails,1,Token, dealerIndex, tokenIndex); // 1 Buy , 2 Sell
                  /*Sneha - DQty changes - E*/
                /*Sneha - multiple connection changes:15/07/16 - E*/
            }    
        } 
        Matching(Token,FD,IsIOC,IsDQ,pConnInfo, tokenIndex);
    } 
    else
    {
        ORDER_BOOK_DTLS BookDetails;
        if(ME_OrderBook.OrderBook[tokenIndex].Sell[0].IsIOC == 1)
        {
            //long Cantoorderbook(ORDER_BOOK_DTLS * Mybookdetails, int BuySellSide, long Token) // 1 Buy , 2 Sell
            bool sellStatus = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Sell[0].dealerID, sellItr);
             /*int sellFD = (sellItr->second)->FD; 
            CONNINFO* sellConnInfo = (sellItr->second)->ptrConnInfo;  */
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Sell[0],sizeof(BookDetails));
            //std::cout <<  "Sell IOC Check " << std::endl;
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/  
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECM(&BookDetails, Token,FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSEFO(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSECD:
              {
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECD(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
             }        
            int dealerIndex = -1;
            if (sellStatus == true){
                dealerIndex = (sellItr->second)->dealerOrdIndex;
            }
            Cantoorderbook(&BookDetails, 2, Token, getEpochTime(), dealerIndex, tokenIndex); // 1 Buy , 2 Sell            
            
        }   
        if(ME_OrderBook.OrderBook[tokenIndex].Buy[0].IsIOC == 1)
        {
            //std::cout <<  "Buy IOC Check " << std::endl;
            bool buyStatus = getConnInfo(ME_OrderBook.OrderBook[tokenIndex].Buy[0].dealerID, buyItr);
            /*int buyFD = (buyItr->second)->FD;
            CONNINFO*  buyConnInfo = (buyItr->second)->ptrConnInfo;*/
            memcpy(&BookDetails,&ME_OrderBook.OrderBook[tokenIndex].Buy[0],sizeof(BookDetails));
            switch(_nSegMode)
            {
              case SEG_NSECM:
              {
                /*Sneha - multiple connection changes:15/07/16*/    
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECM(&BookDetails, Token,FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSEFO:
              {
                /*Sneha - multiple connection changes:15/07/16*/
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSEFO(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
              case SEG_NSECD:
              {
                if (pConnInfo!=NULL)
                  SendOrderCancellation_NSECD(&BookDetails, Token, FD,pConnInfo, 0, false);
              }
              break;
              
            }         
            int dealerIndex = -1;
            if (buyStatus == true){
                dealerIndex = (buyItr->second)->dealerOrdIndex;
            }
            Cantoorderbook(&BookDetails, 1, Token, getEpochTime(), dealerIndex, tokenIndex); // 1 Buy , 2 Sell
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
    /*Matching Engine Update for SL orders Starts NK*/
    
    for(int j = 0;j < ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords;j++)
    {
      if(ME_OrderBook.OrderBook[tokenIndex].LTP > 0 && ME_OrderBook.OrderBook[tokenIndex].LTP <= ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TriggerPrice)
      {
        
        /*Broadcast SL Order*/
        switch(_nSegMode)
        {
          case SEG_NSECM:
          {
            SLOrderTriggeredResponseCM(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j],tokenIndex);
          }
          break;
          case SEG_NSEFO:
          {
            SLOrderTriggeredResponseFO(&ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j],tokenIndex);
          }  
          break;
        }
        
      }
      else
      {
        break;
      }
    }
    for(int j = 0;j < ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords;j++)
    {
      if(ME_OrderBook.OrderBook[tokenIndex].LTP > 0 && ME_OrderBook.OrderBook[tokenIndex].LTP >= ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TriggerPrice)
      {
        switch(_nSegMode)
        {
          case SEG_NSECM:
          {
            SLOrderTriggeredResponseCM(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j],tokenIndex);
          }
          break;
          case SEG_NSEFO:
          {
            SLOrderTriggeredResponseFO(&ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j],tokenIndex);
          }  
          break;
        }
      }
      else
      {
        break;
      }
    }
    /*Matching Engine Update for SL orders Ends NK*/
}

int SendOrderBook(long Token, int ExchSeg)
{
    
}        

int ProcessCOL (int32_t COLDealerID)
{
  int count = 0; 
  dealerInfoItr itr = dealerInfoMapGlobal->find(COLDealerID);
  if (itr == dealerInfoMapGlobal->end())
  {
       snprintf (logBuf, 500, "Thread_ME|ProcessCOL|Dealer %d not found", COLDealerID);
       Logger::getLogger().log(DEBUG, logBuf); 
       return 0;
  }
   if ((itr->second)->COL != 1)
  {
       snprintf (logBuf, 500, "Thread_ME|ProcessCOL|Dealer %d|COL %d", COLDealerID, (itr->second)->COL);
       Logger::getLogger().log(DEBUG, logBuf); 
       return 0;
  }
  int dealerIndex = (itr->second)->dealerOrdIndex;
    
  for (int i = 0; i < TokenCount; i++)
  {

    int Token = dealerOrdArr[dealerIndex][i].token;

    if (_nSegMode == SEG_NSEFO){
      Token = Token - FOOFFSET;
    }
    int tokenIndex = i;

    if (dealerOrdArr[dealerIndex][i].buyordercnt > 0)
    {
         count  = 0;

          for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].BuyRecords ) ; j++)
         {   
            if(ME_OrderBook.OrderBook[tokenIndex].Buy[j].dealerID == COLDealerID)
            {
                /*Send for MessageDownload and  Broadcast*/
                if (_nSegMode == SEG_NSECM){
                    SendOrderCancellation_NSECM(&(ME_OrderBook.OrderBook[tokenIndex].Buy[j]), Token, -1, NULL, 1, true);
                }
                else if (_nSegMode == SEG_NSEFO){
                    SendOrderCancellation_NSEFO(&(ME_OrderBook.OrderBook[tokenIndex].Buy[j]), Token, -1, NULL, 1, true);
                }
                else if (_nSegMode == SEG_NSECD){
                    SendOrderCancellation_NSECD(&(ME_OrderBook.OrderBook[tokenIndex].Buy[j]), Token, -1, NULL, 1, true);
                }

               /*Cancel Order from book*/
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].DQty = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ =0 ;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].OpenQty = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].TTQ = 0;
               ME_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = 0;

               count++;
               if (count == dealerOrdArr[dealerIndex][i].buyordercnt)
               {
                 break;
               }
            }
          }
         SortBuySideBook(tokenIndex); 
         ME_OrderBook.OrderBook[tokenIndex].BuyRecords = ME_OrderBook.OrderBook[tokenIndex].BuyRecords - count;
         dealerOrdArr[dealerIndex][i].buyordercnt = dealerOrdArr[dealerIndex][i].buyordercnt - count;
    }   

    if (dealerOrdArr[dealerIndex][i].sellordercnt > 0)
    {

         count  = 0;
          for(int j = 0 ; j < (ME_OrderBook.OrderBook[tokenIndex].SellRecords ) ; j++)
         {    

            if(ME_OrderBook.OrderBook[tokenIndex].Sell[j].dealerID == COLDealerID)
            {
                /*Send for MessageDownload and  Broadcast*/
                if (_nSegMode == SEG_NSECM){
                    SendOrderCancellation_NSECM(&(ME_OrderBook.OrderBook[tokenIndex].Sell[j]), Token, -1, NULL, 1, true);
                }
                else if (_nSegMode == SEG_NSEFO){
                    SendOrderCancellation_NSEFO(&(ME_OrderBook.OrderBook[tokenIndex].Sell[j]), Token, -1, NULL, 1, true);
                }
                else if (_nSegMode == SEG_NSECD){
                    SendOrderCancellation_NSECD(&(ME_OrderBook.OrderBook[tokenIndex].Sell[j]), Token, -1, NULL, 1, true);
                }
                std::cout<< "Token::"<<ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.TokenNo<<"|"<<j<<"|"<<tokenIndex<<"|"<<
                 (int64_t)ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice<<std::endl;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = 2147483647;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].DQty = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ =0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].OpenQty = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].TTQ = 0;
               ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = 0;
               std::cout<< "Token::"<<ME_OrderBook.OrderBook[tokenIndex].Sell[j].nsecm_nsefo_nsecd.NSEFO.TokenNo<<"|"<<j<<"|"<<tokenIndex<<"|"<<
                 (int64_t)ME_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[j].lQty<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice<<std::endl;
               count++;
               if (count == dealerOrdArr[dealerIndex][i].sellordercnt)
               {
                 break;
               }
           }   
         }
         SortSellSideBook(tokenIndex); 
         std::cout<< "Token::"<<ME_OrderBook.OrderBook[tokenIndex].Sell[0].nsecm_nsefo_nsecd.NSEFO.TokenNo<<"|"<<tokenIndex<<"|"<<
                 (int64_t)ME_OrderBook.OrderBook[tokenIndex].Sell[0].OrderNo<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].lQty<<"|"<<
                 ME_OrderBook.OrderBook[tokenIndex].Sell[0].lPrice<<std::endl;
         ME_OrderBook.OrderBook[tokenIndex].SellRecords = ME_OrderBook.OrderBook[tokenIndex].SellRecords - count;
         dealerOrdArr[dealerIndex][i].sellordercnt = dealerOrdArr[dealerIndex][i].sellordercnt - count;
     }
  }

   /*COL Non Trim Orders NK*/
   for (int i = 0; i < TokenCount; i++)
  {
    int Token = dealerOrdArrNonTrim[dealerIndex][i].token;

    if (_nSegMode == SEG_NSEFO){
      Token = Token - FOOFFSET;
    }
    int tokenIndex = i;
    if (dealerOrdArrNonTrim[dealerIndex][i].buyordercnt > 0)
    {
         count  = 0;
          for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords ) ; j++)
         {   
            if(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].dealerID == COLDealerID)
            {

              if (_nSegMode == SEG_NSECM){
                    SendOrderCancellationNonTrim_NSECM(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j]), Token, -1, NULL, 1, true);
                }
              else if (_nSegMode == SEG_NSEFO){
                    SendOrderCancellationNonTrim_NSEFO(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j]), Token, -1, NULL, 1, true);
                }
               /*Cancel Order from book*/
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lPrice = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].DQty = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].IsDQ =0 ;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].IsIOC = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OpenQty = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].OrderNo = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].SeqNo = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].TTQ = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Buy[j].lQty = 0;

               count++;
               if (count == dealerOrdArrNonTrim[dealerIndex][i].buyordercnt)
               {
                 break;
               }
            }
          }
         SortBuySidePassiveBook(tokenIndex); 
         ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords = ME_Passive_OrderBook.OrderBook[tokenIndex].BuyRecords - count;
         dealerOrdArrNonTrim[dealerIndex][i].buyordercnt = dealerOrdArrNonTrim[dealerIndex][i].buyordercnt - count;
    }   

    if (dealerOrdArrNonTrim[dealerIndex][i].sellordercnt > 0)
    {

         count  = 0;
          for(int j = 0 ; j < (ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords ) ; j++)
         {    

            if(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].dealerID == COLDealerID)
            {
               if (_nSegMode == SEG_NSECM){
                    SendOrderCancellationNonTrim_NSECM(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j]), Token, -1, NULL, 1, true);
                }
                else if (_nSegMode == SEG_NSEFO){
                    SendOrderCancellationNonTrim_NSEFO(&(ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j]), Token, -1, NULL, 1, true);
                } 

               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lPrice = 2147483647;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].DQty = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].IsDQ =0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].IsIOC = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OpenQty = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].OrderNo = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].SeqNo = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].TTQ = 0;
               ME_Passive_OrderBook.OrderBook[tokenIndex].Sell[j].lQty = 0;
               count++;
               if (count == dealerOrdArrNonTrim[dealerIndex][i].sellordercnt)
               {
                 break;
               }
           }   
         }
         SortSellSidePassiveBook(tokenIndex); 

         ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords = ME_Passive_OrderBook.OrderBook[tokenIndex].SellRecords - count;
         dealerOrdArrNonTrim[dealerIndex][i].sellordercnt = dealerOrdArrNonTrim[dealerIndex][i].sellordercnt - count;

     }
  }
}


