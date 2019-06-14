/* 
 * File:   main.cpp
 * Author: muditsharma
 *
 * Created on March 1, 2016, 5:00 PM
 */

#include <cstdlib>
#include <thread>
#include <unordered_map>
#include "spsc_atomic1.h"
#include "ConfigReader.h"
#include "All_Structures.h"
#include"BrodcastStruct.h" 
//#include "Thread_Broadcast.h"
//#include "BookBuilder.h"
#include "Thread_ME.h"
#include "Thread_TCP.h"
#include "Thread_MsgDnld.h"
#include "Thread_TCPRecovery.h"
#include "Thread_PFS.h"
#include "Thread_MarketBook.h"
#include"Thread_Broadcast.h"
#include <licence.h>
#include <bits/unordered_map.h>
#include<csignal>
#include <signal.h>
//#include "con"

struct timespec Time1;

typedef std::unordered_map<std::string, int32_t>NSECMToken;
typedef NSECMToken::iterator TokenItr;

int16_t gStartPFSFlag = 0;
char logBufMain[250];
//void StartBroadcast(ProducerConsumerQueue<BROADCAST_DATA>* qptr, const char* lszIpAddress,const char* lszBroadcast_IpAddress, int lnBroadcast_PortNumber);
//void StartBroadcast(ProducerConsumerQueue<BROADCAST_DATA>* qptr);

int FileDigesterCM (const char* pStrFileName, NSECMToken& nTokenStore)
{
  assert(pStrFileName);
  
  int nRet = 0;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    char buf[100] = {0};
    //snprintf(logBufMain, 250, "Error while opening file: %s", pStrFileName);
    std::cout << " Error while opening " << pStrFileName << std::endl;
     //Logger::getLogger().log(DEBUG, logBufMain);
    return nRet;
  }  

  int lnCount = 0;
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
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
      
      std::string lszKey = (lstOMSScripInfo.cSymbol);
      lszKey += "|";
      lszKey += lstOMSScripInfo.cSeries;
      
      std::pair<TokenItr, bool>lcRetVal;      
      lcRetVal = nTokenStore.insert(NSECMToken::value_type(lszKey, lstOMSScripInfo.nToken));
      if (false == lcRetVal.second)
      {
        return -1;
      }      
      
      if(pLine != NULL)
      {free(pLine);}
    } //if(getline(&pLine,&sizeLine,fp)
    else
    {
      //cout << errno << " returned while reading line." << endl; 
    } 
  } //while(!feof(fp)) 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  return 1;
} 

bool LoadContractFile (const char* pStrFileName, CONTRACTINFO* pCntrctCDInfo, int32_t* arr, int iNoOfToken)
{
  assert(pStrFileName);
  
  bool bRet = true;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
       std::cout << " Error while opening " << pStrFileName << std::endl;
        return false;
  }  

  int lnCount = 0;
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    int tokenLoc = 0;
     int32_t token = 0;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
           while (fieldCnt <= 41 && tokenLoc != -1)
           {
                fieldValue = strsep(&pLine, "|");

                if (fieldValue != NULL)
                {
                      switch(fieldCnt)
                      {
                        case (int)cd_contractfile::Token:
                                token = atoi (fieldValue);
                                
                                if ((bRet = binarySearch(arr, iNoOfToken, token, &tokenLoc)) == true)
                                {
                                   pCntrctCDInfo[tokenLoc].Token = token;
                                   
                                }
                                else
                                {
                                   tokenLoc = -1;
                                }
                          break;
                                                    
                        case (int)cd_contractfile::MinimumLotQuantity:
                                pCntrctCDInfo[tokenLoc].MinimumLotQuantity =   atoi(fieldValue);
                          break; 
                          
                        case (int)cd_contractfile::BoardLotQuantity:
                                pCntrctCDInfo[tokenLoc].BoardLotQuantity = atoi(fieldValue);
                          break; 
                          
                        default:
                          break;
                     }//switch
                }
                else
                {
                  std::cout<<"Error while reading field number:"<<fieldCnt<<std::endl;
                  return false;
                }
                fieldCnt++;
           }
    } //if(getline(&pLine,&sizeLine,fp)
    else
    {
        std::cout << errno << " returned while reading line." << std::endl; 
    } 
  } //while(!feof(fp)) 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  return true;
} 

bool LoadCDContractFile (const char* pStrFileName, CD_CONTRACTINFO* pCntrctInfo, int32_t* arr, int iNoOfToken)
{
  assert(pStrFileName);
  
  bool bRet = true;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
       std::cout << " Error while opening " << pStrFileName << std::endl;
        return false;
  }  

  int lnCount = 0;
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    int tokenLoc = 0;
     int32_t token = 0;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
           while (fieldCnt <= 52 && tokenLoc != -1)
           {
                fieldValue = strsep(&pLine, "|");

                if (fieldValue != NULL)
                {
                      switch(fieldCnt)
                      {
                        case Token:
                                token = atoi (fieldValue);
                                
                                if ((bRet = binarySearch(arr, iNoOfToken, token, &tokenLoc)) == true)
                                {
                                   pCntrctInfo[tokenLoc].Token = token;
                                   
                                }
                                else
                                {
                                   tokenLoc = -1;
                                }
                          break;
                                                    
                          case MinimumLotQuantity:
                                pCntrctInfo[tokenLoc].MinimumLotQuantity =   atoi(fieldValue);
                          break; 
                          
                          case BoardLotQuantity:
                                  pCntrctInfo[tokenLoc].BoardLotQuantity = atoi(fieldValue);
                            break; 
                          case CD_Multiplier:
                                pCntrctInfo[tokenLoc].Multiplier = atoi(fieldValue);
                          break; 
                          
                        default:
                          break;
                     }//switch
                }
                else
                {
                  std::cout<<"Error while reading field number:"<<fieldCnt<<std::endl;
                  return false;
                }
                fieldCnt++;
           }
    } //if(getline(&pLine,&sizeLine,fp)
    else
    {
        std::cout << errno << " returned while reading line." << std::endl; 
    } 
  } //while(!feof(fp)) 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  return true;
} 


bool  LoadSubscriptionFile (const char* pStrFileName, int32_t* arr, int& iNoOfToken, int maxTokens, int _nMode)
{
  assert(pStrFileName);
  
  bool bRet = true;
  
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pStrFileName << std::endl;
    bRet = false;
    return bRet;
  }  
  
  
  size_t sizeLine = 10;
  int32_t token = 0;
 bool found = true;
 iNoOfToken = 0;

  while(!feof(fp))
  {
     char* pLine = NULL;
     token = 0;
     if(getdelim(&pLine, &sizeLine, 44,  fp) != -1)
    {
        token = atoi(pLine);
      
        if (token != 0)
        { 
            if (_nMode == SEG_NSECM && token>34999 )
            {
                std::cout << "Invalid Token[" << token << "] encountered in CM Subscription file :" << pStrFileName << std::endl;
                continue;
            }
            else if (_nMode == SEG_NSEFO && token<=34999 )
            {
                std::cout << "Invalid Token[" << token << "] encountered in FO Subscription file :" << pStrFileName << std::endl;
                continue;
            }
            if (_nMode == SEG_NSECD && token>10000 )
            {
                std::cout << "Invalid Token[" << token << "] encountered in CD Subscription file :" << pStrFileName << std::endl;
                continue;
            }
            if (iNoOfToken == 0)
            {
                arr[iNoOfToken] = token;
            }
            else if (token > arr[iNoOfToken-1])
            {
                arr[iNoOfToken] = token;
            }
            else
            {
                found = binarySearch(arr, iNoOfToken, token, NULL);
                if (found == false)
                {
                   int i = iNoOfToken -1;
                   while (token<arr[i] && i>= 0)
                   {
                      arr[i+1] = arr[i];
                      i--;
                   }
                   arr[i+1] = token;
                }
                else
                {
                    iNoOfToken--;   
                }
            }
            //arr[iNoOfToken] = token;
            iNoOfToken++;
            if (iNoOfToken > maxTokens)
            {
                std::cout<<"Tokens in token subscription file exceed config limit"<<std::endl;
                return false;
            }
   
        }
    } 
    else
    {
         std::cout<<"Reading Token subscription file|Error "<<errno<<std::endl;
         bRet = false;
    }
    if (pLine != NULL)
    {
        free(pLine);
    }
  } 

  if (fp != NULL)
  {
      fclose(fp);
  }
  
  return bRet;
} 


 bool   LoadDealerFile(const char* pStrFileName, dealerInfoMap& dealerInfomap)
{
  assert(pStrFileName);
  
  bool bRet = true;
  
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    //snprintf(logBufMain,250, "Error while opening file %s", pStrFileName);
    //Logger::getLogger().log(DEBUG, logBufMain);	
    std::cout << " Error while opening " << pStrFileName << std::endl;
    bRet = false;
    return bRet;
  }  
  
  
  size_t sizeLine = 30;
  int32_t dealerID = 0;
  int index = 0;
  while(!feof(fp))
  {
     char* pLine = NULL;
     if(getline(&pLine, &sizeLine, fp) != -1)
    {
           char* ipTok = strtok(pLine, "|");
           if (ipTok != NULL)
           {
              char* dealerTok= strtok(NULL, "|");
              if (dealerTok != NULL)
              {
                char* COLTok = strtok(NULL, "|");
                if (COLTok != NULL)
                {
                    IP_STATUS*  pIPSts = new (IP_STATUS);
                    memcpy(pIPSts->IP, ipTok, sizeof(pIPSts->IP));
                    pIPSts->status = LOGGED_OFF;
                    pIPSts->COL = atoi(COLTok);
                    if (pIPSts->COL == 1)
                    {
                        pIPSts->dealerOrdIndex = index;
                        index++;
                    }
                    else
                    {
                      pIPSts->dealerOrdIndex = -1;
                    }
                    
                    dealerID = atoi(dealerTok);
                    auto it = dealerInfomap.find(dealerID);
                    if(it != dealerInfomap.end())
                    {
                      std::cout<<"Dealer::"<<dealerID<<" with multiple entry|EXIT" <<std::endl;
                      bRet = false;
                      exit(1);   
                    }
                    else
                    {
                      dealerInfomap.insert(std::pair<int32_t, IP_STATUS*>(dealerID, pIPSts));
                    }
                }
              }
           }
     } 
   /* else
    {
         std::cout<<"Reading Dealer file|Error "<<errno<<std::endl;
         bRet = false;
    }*/
    if (pLine != NULL)
    {
        free(pLine);
    }
  } 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  
  return bRet;
} 

/*void signalHandler (int signum)
{
    std::cout<<"Signal received:"<<signum<<std::endl;
    terminate = true;
}*/

int main(int argc, char** argv) 
{
  {
    std::string errorMessage;

    SET_SERVER_INFO("10.250.34.205", 9900 , "");

    if ( ! VALIDATE_CLIENT( std::string("inhousematching"), 10000, errorMessage ))
    {
         std::cout<<"\n Licence Validation Failed. Error:"<<errorMessage<<std::endl;
         return 2;
    }
  }
 
  //signal(SIGUSR1, signalHandler);

    ConfigReader lszConfigReader(CONFIG_FILE_NAME);
    
    lszConfigReader.dump();   
    
    int _nMode = 0;
    short MLmode = 1;
    std::string _strContractFilePath;
    
    _nMode = atoi(lszConfigReader.getProperty("ME_EXCHG_SEG").c_str());
    _strContractFilePath = lszConfigReader.getProperty("NSE_CONTRACT_FILE_PATH");
    
    std::string lszIpAddress = lszConfigReader.getProperty("IP_ADDRESS");
    int lnPortNumber = atoi(lszConfigReader.getProperty("PORT").c_str());
    std::string lszIpAddress_init = lszConfigReader.getProperty("GR_IP_ADDRESS");
    int lnPortNumber_init = atoi(lszConfigReader.getProperty("GR_PORT").c_str());
    std::string lszBroadcast_IpAddress = lszConfigReader.getProperty("BROADCAST_IP");
    int lnBroadcast_PortNumber = atoi(lszConfigReader.getProperty("BROADCAST_PORT").c_str());
    std::string lszTCPRcvry_IpAddress = lszConfigReader.getProperty("TCPRECOVERY_IP");
    int lszTCPRcvry_PortNumber = atoi(lszConfigReader.getProperty("TCPRECOVERY_PORT").c_str());
    int iSendBuff = atoi(lszConfigReader.getProperty("SENDBUFFER_SIZE").c_str());
    int iRecvBuff = atoi(lszConfigReader.getProperty("RECVBUFFER_SIZE").c_str());
    int iMaxWriteAttempt = atoi(lszConfigReader.getProperty("MAX_WRITE_ATTEMPT").c_str());
    int iEpochBase = atoi(lszConfigReader.getProperty("EPOCH_BASE").c_str());
    int iMECore = atoi(lszConfigReader.getProperty("MATCHING_ENGINE_CORE").c_str());
    int iTCPCore = atoi(lszConfigReader.getProperty("TCP_CORE").c_str());
    int iMsgDnldCore = atoi(lszConfigReader.getProperty("MESSAGE_DOWNLOAD_CORE").c_str());
    int iBrdcstCore = atoi(lszConfigReader.getProperty("BROADCAST_CORE").c_str());
    int iTCPRcvryCore = atoi(lszConfigReader.getProperty("TCP_RECOVERY_CORE").c_str());
    std::string lszTokenFile = lszConfigReader.getProperty("TOKEN_FILE_PATH");
    std::string lszDealerFile = lszConfigReader.getProperty("DEALER_FILE_PATH");
    int iLoggerCore = atoi(lszConfigReader.getProperty("LOGGER_CORE").c_str());
    int iMaxToken = atoi(lszConfigReader.getProperty("NO_OF_TOKENS").c_str());
   // int iMaxOrders = atoi(lszConfigReader.getProperty("NO_OF_ORDERS").c_str());
    int32_t iTTLOpt = atoi(lszConfigReader.getProperty("TTL_OPTION").c_str());
    int32_t iTTLVal = atoi(lszConfigReader.getProperty("TTL_VALUE").c_str());
    std::string brdcst =  lszConfigReader.getProperty("ENABLE_BROADCAST");
    std::string ffBrdcst =  lszConfigReader.getProperty("ENABLE_FASTFEED");
    short StreamID = atoi(lszConfigReader.getProperty("STREAM_ID").c_str());
    MLmode = atoi(lszConfigReader.getProperty("ML_MODE").c_str());
    std::string PFS ="N";
    PFS =  lszConfigReader.getProperty("ENABLE_PFS");
    std::string PFSConfig = lszConfigReader.getProperty("PFS_CONFIG_FILEPATH");
    std::string BBConfig = lszConfigReader.getProperty("BB_CONFIG_FILEPATH");
    int Simulator_Mode = 1;//default Normal Mode
    Simulator_Mode =  atoi(lszConfigReader.getProperty("SIMULATOR_MODE").c_str());
    if(Simulator_Mode == 0)
    {
      Simulator_Mode = 1;
    }
    
    int32_t iInitialBookSize = atoi(lszConfigReader.getProperty("INITIAL_BOOKSIZE").c_str());
    int16_t sBookSizeThresholdPercentage = atoi(lszConfigReader.getProperty("BOOKSIZE_THRESHOLD_PERCENTAGE").c_str());
    if(sBookSizeThresholdPercentage < 1 || sBookSizeThresholdPercentage > 100)
    {
      std::cout<<"BOOKSIZE_THRESHOLD_PERCENTAGE should between 1 to 100"<<std::endl;
      sleep(2);
      return 0;
    }
    std::string lszFcast_IpAddress = lszConfigReader.getProperty("FCAST_IP");
    int lnFcast_PortNumber = atoi(lszConfigReader.getProperty("FCAST_PORT").c_str());    
    
    if(StreamID < 1 || StreamID > 9)
    {
      std::cout<<"Invalid Stream ID.Exiting.";
      sleep(5);
      return 0;
    }
    bool enableValidateMsg = false;
    std::string sValidateMsg = "N";
    sValidateMsg =  lszConfigReader.getProperty("VALIDATE_MSG");
    if ("Y" == sValidateMsg || "y" == sValidateMsg)
    {
       enableValidateMsg = true;
    }
    
    bool enablePFS = false;
    if ("Y" == PFS || "y" == PFS)
    {
       enablePFS = true;
    }
    
    bool enableBrdcst = true;
    if ("N" == brdcst || "n" == brdcst)
    {
       enableBrdcst = false;
    }
    
    
    bool enableFFBrdcst = false;
    if ("Y" == ffBrdcst || "y" == ffBrdcst)
    {
       enableFFBrdcst = true;
    }    
    
    clock_gettime(CLOCK_MONOTONIC,&Time1);
    NSECMToken lcNSECMTokenStore;
    
   int32_t*  TokenArr = new int32_t[iMaxToken];
   memset(TokenArr, 0, sizeof(TokenArr));
    int TokenCount = 0;

    /*Load Security.txt*/
    if(SEG_NSECM == _nMode)
    {
      //lcNSECMTokenStore.bucket(35000);        
      FileDigesterCM(_strContractFilePath.c_str(), lcNSECMTokenStore);
    }
    
   /*Load Tokens subscribed*/     
    bool retVal;
    if ((retVal = LoadSubscriptionFile(lszTokenFile.c_str(),  TokenArr, TokenCount, iMaxToken, _nMode)) == false)
    {
         sleep(5);
         return 0;
    }
    else
    {
        std::cout<<"Tokens Subscribed";
        for (int i = 0; i< TokenCount; i++)
        {
            std::cout<<"|"<<TokenArr[i];
        }
        std::cout<<std::endl;
    }
    
    /*Load dealer-IP info*/
    dealerInfoMap  dealerInfomap;
    
     if ((retVal = LoadDealerFile(lszDealerFile.c_str(), dealerInfomap)) == false)
    {
         sleep(5);
         return 0;
    }
   else
    {
        std::cout<<"Dealer IP set:"<<std::endl;
        for (dealerInfoItr itr = dealerInfomap.begin(); itr != dealerInfomap.end(); itr++)
        {
            std::cout<<"Dealer:"<<itr->first<<"|IP:"<<(itr->second)->IP<<"|COL:"<<(itr->second)->COL<<std::endl;
        }
    }
    
    /*Load contract.txt*/
    CONTRACTINFO* pCntrctInfo = NULL;
    if(_nMode == SEG_NSEFO)
    {
      pCntrctInfo = new CONTRACTINFO[TokenCount];
      if ((retVal = LoadContractFile(_strContractFilePath.c_str(), pCntrctInfo, TokenArr, TokenCount)) == false)
      {
         sleep(5);
         return 0;
      }
      else
      {
         std::cout<<"Contract Info"<<std::endl;
         for (int i = 0; i< TokenCount; i++)
         {
              std::cout<<pCntrctInfo[i].Token<<"|"<<pCntrctInfo[i].MinimumLotQuantity<<"|"<<pCntrctInfo[i].BoardLotQuantity<<std::endl;
         }
      }
    }
    
    /*Load cd contract.txt*/
    CD_CONTRACTINFO* pCDCntrctInfo = NULL;
    if(_nMode == SEG_NSECD)
    {
      pCDCntrctInfo = new CD_CONTRACTINFO[TokenCount];
      if ((retVal = LoadCDContractFile(_strContractFilePath.c_str(), pCDCntrctInfo, TokenArr, TokenCount)) == false)
      {
         sleep(5);
         return 0;
      }
      else
      {
         std::cout<<"CD Contract Info"<<std::endl;
         for (int i = 0; i< TokenCount; i++)
         {
           if(pCDCntrctInfo[i].Token == TokenArr[i])
           {
             std::cout<<pCDCntrctInfo[i].Token<<"|"<<pCDCntrctInfo[i].MinimumLotQuantity<<"|"<<pCDCntrctInfo[i].BoardLotQuantity<<"|"<<pCDCntrctInfo[i].Multiplier<<std::endl;
           }
           else
           {
             std::cout<<TokenArr[i]<<" Not found in contract file"<<std::endl;
           }
         }
      }
    }
    
    //Set Log Level
    Logger::getLogger().setLevel(ERROR);
    
    if (_nMode == SEG_NSECM)
    {
       Logger::getLogger().setDump(".matchingengineCM", iLoggerCore);
    }
    else if (_nMode == SEG_NSEFO)
    {
      Logger::getLogger().setDump(".matchingengineFO", iLoggerCore);
    }
    else if (_nMode == SEG_NSECD)
    {
      Logger::getLogger().setDump(".matchingengineCD", iLoggerCore);
    }
    snprintf(logBufMain,250,"Matching Engine Version: 1.0.0.0.12");
    Logger::getLogger().log(DEBUG, logBufMain);	
    
    ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_TCPServerToMe = new ProducerConsumerQueue<DATA_RECEIVED>(1000);
    ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MeToTCPServer = new ProducerConsumerQueue<DATA_RECEIVED>(1000);
    /*GR changes starts*/
    ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_Init_TCPServerToMe = new ProducerConsumerQueue<DATA_RECEIVED>(1000);
    ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_Init_MeToTCPServer = new ProducerConsumerQueue<DATA_RECEIVED>(1000);
    /*GR changes ends*/
    ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MeToLog = new ProducerConsumerQueue<DATA_RECEIVED>(1000);
    ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_PFStoME = new ProducerConsumerQueue<GENERIC_ORD_MSG>(1000);
    ProducerConsumerQueue<GENERIC_ORD_MSG> *Inqptr_MBtoME = new ProducerConsumerQueue<GENERIC_ORD_MSG>(1000);
    ProducerConsumerQueue<BROADCAST_DATA> *Inqptr_METoBroadcast = NULL;
    if (true == enableBrdcst){
      Inqptr_METoBroadcast = new ProducerConsumerQueue<BROADCAST_DATA>(1000);
    }
      
    std::thread thread_Log(StartLog,std::ref(Inqptr_MeToLog), _nMode,iMsgDnldCore);
    
//    std::thread thread_Broadcast;
//    std::thread thread_TCPRcvry;
//    if (true == enableBrdcst){
//      thread_Broadcast = std::thread(StartBroadcast,std::ref(Inqptr_METoBroadcast),lszIpAddress.c_str() ,lszBroadcast_IpAddress.c_str(),lnBroadcast_PortNumber, iBrdcstCore,iTTLOpt, iTTLVal, StreamID, enableFFBrdcst, lszFcast_IpAddress.c_str(), lnFcast_PortNumber,TokenCount, _nMode); 
//      thread_TCPRcvry = std::thread(StartTCPRecovery, lszTCPRcvry_IpAddress.c_str(), lszTCPRcvry_PortNumber,iTCPRcvryCore, StreamID, iMaxWriteAttempt);
//    }    
    
    std::thread thread_ME(StartME,std::ref(Inqptr_TCPServerToMe),std::ref(Inqptr_MeToTCPServer),std::ref(Inqptr_METoBroadcast),std::ref(Inqptr_PFStoME),std::ref(Inqptr_MBtoME), &lcNSECMTokenStore, _nMode, std::ref(Inqptr_MeToLog), iMaxWriteAttempt, iEpochBase,iMECore, TokenArr, TokenCount, &dealerInfomap, pCntrctInfo, pCDCntrctInfo, enableBrdcst,StreamID, MLmode, enablePFS, Simulator_Mode, lszIpAddress.c_str(), lnPortNumber,iInitialBookSize,sBookSizeThresholdPercentage,enableValidateMsg);
    std::thread thread_TCP(StartTCP,std::ref(Inqptr_TCPServerToMe),std::ref(Inqptr_MeToTCPServer), _nMode,lszIpAddress_init.c_str(), lnPortNumber_init, lszIpAddress.c_str(), lnPortNumber, iSendBuff,iRecvBuff, iTCPCore, &dealerInfomap);       
    
    std::thread thread_Broadcast;
    
    std::thread thread_TCPRcvry;
    if (true == enableBrdcst){
      thread_Broadcast = std::thread(StartBroadcast,std::ref(Inqptr_METoBroadcast),lszIpAddress.c_str() ,lszBroadcast_IpAddress.c_str(),lnBroadcast_PortNumber, iBrdcstCore,iTTLOpt, iTTLVal, StreamID, enableFFBrdcst, lszFcast_IpAddress.c_str(), lnFcast_PortNumber,TokenCount, _nMode); 
      thread_TCPRcvry = std::thread(StartTCPRecovery, lszTCPRcvry_IpAddress.c_str(), lszTCPRcvry_PortNumber,iTCPRcvryCore, StreamID, iMaxWriteAttempt);
    }
    
    if (true == enablePFS){
      std::thread thread_PFS;
      thread_PFS = std::thread(Start_PFS,std::ref(Inqptr_PFStoME),PFSConfig); 
      thread_PFS.join();
      // PFSConfig.ini, 
    }

    if(Simulator_Mode == 3)
    {
      std::thread thread_MarketBook;
      thread_MarketBook = std::thread(Start_MarketBook,std::ref(Inqptr_MBtoME),BBConfig); 
      thread_MarketBook.join();
    }
    
    snprintf(logBufMain, 250, "Matching Engine Started");    
    Logger::getLogger().log(DEBUG, logBufMain);	
    //std::cout << "Matching Engine Started" << std::endl;
     
   
   thread_TCP.join();
    return 0;
}

