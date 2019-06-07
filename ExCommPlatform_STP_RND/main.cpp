/* 
 * File:   main.cpp
 * Author: MuditSharma
 *
 * Created on March 7, 2017, 5:46 PM
 */

#include <cstdlib>
#include "ExComm.h" 
#include <memory.h>
#include <string.h>
#include <algorithm>
#include "ConfigReader.h"



using namespace std;

/*
 * 
 */
#define CONFIG_FILE_NAME		"config_ExCommPlatform.ini"
map<int,CONTRACTINFO> ContractInfoMap;


bool LoadContractFile (const char* pStrFileName,int *TokenArr)
{

  CONTRACTINFO cont_info;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pStrFileName << std::endl;
    return false;
  }  

  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  int tokenIdx=0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      while (fieldCnt <= 44 )
      {
        fieldValue = strsep(&pLine, "|");
//        std::cout<<fieldValue<<" token::" <<TokenArr[tokenIdx]<<std::endl;
        if(TokenArr[tokenIdx] != atoi (fieldValue) && fieldCnt==1)
        {
          break;
        }

        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case InstumentName:
              memcpy(cont_info.InstumentName,fieldValue,6);
              break;
            case Symbol:
              memcpy(cont_info.Symbol,fieldValue,9);
              break;
            case OptionType:
              memcpy(cont_info.OptionType,fieldValue,2);
              break;
            case Expiry:
              cont_info.ExpiryDate = atoi(fieldValue);
              break;
            case Strike:
              cont_info.StrikePrice = atoi(fieldValue);  
              break;
            case MinimumLotQuantity:
              cont_info.MinimumLotQuantity = atoi(fieldValue);
              break;
            case LowDPR:
              cont_info.LowDPR = atoi(fieldValue);
              break;
            case HighDPR:
              cont_info.HighDPR = atoi(fieldValue);
              ContractInfoMap.insert(make_pair(TokenArr[tokenIdx],cont_info));
              tokenIdx++;
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

bool LoadCDContractFile (const char* pStrFileName,int *TokenArr)
{
  
  CONTRACTINFO cont_info;
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pStrFileName << std::endl;
    return false;
  }  

  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  int tokenIdx=0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      while (fieldCnt <= 52 )
      {
        fieldValue = strsep(&pLine, "|");
//        std::cout<<fieldValue<<" token::" <<TokenArr[tokenIdx]<<std::endl;
        if(TokenArr[tokenIdx] != atoi (fieldValue) && fieldCnt==1)
        {
          break;
        }

        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case InstumentName:
              memcpy(cont_info.InstumentName,fieldValue,6);
              break;
            case Symbol:
              memcpy(cont_info.Symbol,fieldValue,9);
              break;
            case OptionType:
              memcpy(cont_info.OptionType,fieldValue,2);
              break;
            case Expiry:
              cont_info.ExpiryDate = atoi(fieldValue);
              break;
            case Strike:
              cont_info.StrikePrice = atoi(fieldValue);  
              break;
            case MinimumLotQuantity:
              cont_info.MinimumLotQuantity = atoi(fieldValue);
              break;
            case LowDPR:
              cont_info.LowDPR = atoi(fieldValue);
              break;
            case HighDPR:
              
              cont_info.HighDPR = atoi(fieldValue);
              break;
            case Cd_Mulitplier:
              cont_info.Multiplier = atoi(fieldValue);
              std::cout<<cont_info.Multiplier<<std::endl;
              ContractInfoMap.insert(make_pair(TokenArr[tokenIdx],cont_info));
              tokenIdx++;
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



int16_t sConnStatus = 0;
int parsePortfolio(char* pPortfolioFileName, int *tokenstore)
{ 
  FILE *fp = fopen(pPortfolioFileName, "r+");
  if(fp == NULL)
  {
       std::cout << " Error while opening " << pPortfolioFileName << std::endl;
        return false;
  }  

  
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  int tokenIdx=0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
 
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      while (fieldCnt <= 5 )
      {
        fieldValue = strsep(&pLine, ",");

        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case 3:
              tokenstore[tokenIdx] = atoi (fieldValue);
//                            std::cout<<tokenstore[tokenIdx]<<std::endl;
              tokenIdx++;
              break;
            case 4:
              tokenstore[tokenIdx] = atoi (fieldValue);
//                            std::cout<<tokenstore[tokenIdx]<<" ";
              tokenIdx++;
              break;
            case 5:
              tokenstore[tokenIdx] = atoi (fieldValue);
//                            std::cout<<tokenstore[tokenIdx]<<" ";
              tokenIdx++;
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
  return tokenIdx;
}

void EliminateRepeatitiveToken(int tokenArr[],int &max_token)
{
  int j;
  
  sort(tokenArr,tokenArr+max_token);

  //To eliminate repetitive elements in sorted array
  for(int i=0;i<max_token;++i)
  {
    for(j=i+1;j<max_token;)
    {
      if(tokenArr[i]==tokenArr[j])
      {
        for(int k=j;k<max_token-1;++k)
        {
          tokenArr[k]=tokenArr[k+1];
        }
        --max_token;
      }
      else
      {
        ++j;
      }
    }
  }
}
int parsePortfolio_Conrev(char* pPortfolioFileName,int *tokenstore,int &tokenIdx,storePortFolioInfo PFInfo[])
{ 
  FILE *fp = fopen(pPortfolioFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pPortfolioFileName << std::endl;
    return false;
  }  

  
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remoove the first line containing the file version 
  {
    free(pTemp);
  }
  int Idx=0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
 
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      while (fieldCnt <= 18 )
      {
        fieldValue = strsep(&pLine, ",");

        if (fieldValue != NULL)
        {
              switch(fieldCnt)
              {
                case 2:
                  PFInfo[Idx].ConRev = atoi (fieldValue);
                  break;
                case 3:
                  PFInfo[Idx].Token1 = atoi (fieldValue);
                  tokenstore[tokenIdx] = atoi (fieldValue);
                  tokenIdx++;
                  break;
                case 4:
                  PFInfo[Idx].Token2 = atoi (fieldValue);
                  tokenstore[tokenIdx] = atoi (fieldValue);
                  tokenIdx++;
                  break;
                case 5:
                  PFInfo[Idx].Token3 = atoi (fieldValue);
                  tokenstore[tokenIdx] = atoi (fieldValue);
                  tokenIdx++;
                  break;
                case 8:
                  PFInfo[Idx].Spread = atoi (fieldValue);
                  PFInfo[Idx].Spread= PFInfo[Idx].Spread*100;
                  break;
                case 18:
                  PFInfo[Idx].Strike = atoi (fieldValue);
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
      Idx++;
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
  return Idx;
}

int parsePortfolio_Box(char* pPortfolioFileName,int *tokenstore,int &tokenIdx,storePortFolioInfo_Box PFInfo[])
{ 
  FILE *fp = fopen(pPortfolioFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pPortfolioFileName << std::endl;
    return false;
  }  

  
  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remove the first line containing the file version 
  {
    free(pTemp);
  }
  int Idx=0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      while (fieldCnt <= 18 )
      {
        fieldValue = strsep(&pLine, ",");

        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case 2:
              PFInfo[Idx].ConRev = atoi (fieldValue);
              break;
            case 3:
              PFInfo[Idx].Token1 = atoi (fieldValue);
              tokenstore[tokenIdx] = atoi (fieldValue);
              tokenIdx++;
              break;
            case 4:
              PFInfo[Idx].Token2 = atoi (fieldValue);
              tokenstore[tokenIdx] = atoi (fieldValue);
              tokenIdx++;
              break;
            case 5:
              PFInfo[Idx].Token3 = atoi (fieldValue);
              tokenstore[tokenIdx] = atoi (fieldValue);
              tokenIdx++;
              break;
            case 6:
              PFInfo[Idx].Token4 = atoi (fieldValue);
              tokenstore[tokenIdx] = atoi (fieldValue);
              tokenIdx++;
              break;
            case 9:
              PFInfo[Idx].Spread = atoi (fieldValue);
              PFInfo[Idx].Spread= PFInfo[Idx].Spread*100;
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
      Idx++;
      
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
  return Idx;
}

int main(int argc, char** argv)
{
  
    ConfigReader lszConfigReader(CONFIG_FILE_NAME);
    lszConfigReader.dump(); 
    
    
    string strConnectionIP         = lszConfigReader.getProperty("IP_ADDRESS");
    uint32_t iConnectionPort       = atoi(lszConfigReader.getProperty("PORT").c_str());
    std::string strTMID            = lszConfigReader.getProperty("TMID");
    uint32_t iUserID               = atoi(lszConfigReader.getProperty("USERID").c_str());
    uint16_t sBranchId             = atoi(lszConfigReader.getProperty("BRANCHID").c_str());
    uint32_t iExchVer              = atoi(lszConfigReader.getProperty("EXCH_VER").c_str());
    int64_t llNNF                  = atoll(lszConfigReader.getProperty("NNF_ID").c_str());
    string strPassword             = lszConfigReader.getProperty("PASSWORD");
    char ML                        = lszConfigReader.getProperty("ML").c_str()[0];
    char Conrev_OrdersToExchForStratFeed  = lszConfigReader.getProperty("Conrev_OrdersToExchForStratFeed").c_str()[0];
    char Box_OrdersToExchForStratFeed  = lszConfigReader.getProperty("Box_OrdersToExchForStratFeed").c_str()[0];
    char SingleLeg_OrdersToExch  = lszConfigReader.getProperty("SingleLeg_OrdersToExch").c_str()[0];
    char SingleLeg_OrdersToExch_CD  = lszConfigReader.getProperty("SingleLeg_OrdersToExch_CD").c_str()[0];
//    std::cout<<strConnectionIP<<"|"<<iConnectionPort<<"|"<<strTMID<<"|"<<iUserID<<"|"<<sBranchId<<"|"<<iExchVer<<"|"<<llNNF<<"|"<<strPassword<<"|"<<ML<<"|"<<OrdersToExchForStratFeed<<endl;

    int32_t iSockId             = 0;
    int16_t iNoOfStreams        = 0;
    char chDataBuff[912000]     = {0};
    char chLogBuff[512]         = {0};
    int32_t iNoOfBytesRem       = 0; 
    int16_t iRetVal             = 0;

    cpu_set_t set;
    CPU_ZERO(&set);

    int32_t nCoreID = 7;
    CPU_SET(nCoreID, &set);

    if(sched_setaffinity(0,sizeof(cpu_set_t), &set) < 0)
    {
      printf("sched_setaffinity failed...\n");
    }
    else
    {
      printf("sched_setaffinity success, Pinned to Core %d...\n", nCoreID);
    }
    
    
    cout << chrono::high_resolution_clock::period::den << endl;      
    
    iRetVal = ConnectToNSEFOExch(strConnectionIP.c_str(), iConnectionPort, strTMID.c_str(), iUserID, sBranchId, iExchVer, strPassword.c_str(), iSockId, iNoOfStreams);
    if(iRetVal == 0)
    {
      sConnStatus = 1; //1:Connected
    }
    
    sleep(1);
    iNoOfBytesRem = 0;
    ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
    
    sprintf(chLogBuff, "|Bytes Recvd %d|ConnectToNSEFOExch|", iNoOfBytesRem);
    cout << chLogBuff << endl;

    ProcessExchResp(chDataBuff, iNoOfBytesRem);
//    ProcessLogonResp(chDataBuff);
    memset(chDataBuff, 0, 2048);
    
      
    std::cout << "Order Entry start" << std::endl;
    iNoOfBytesRem = 0; 
    for(int32_t i=0; i<10 ; i++)
    {
    ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
    if(iNoOfBytesRem > 0)
    {

      ProcessExchResp(chDataBuff, iNoOfBytesRem);
    }
    usleep(100);
    }

    if(Conrev_OrdersToExchForStratFeed=='Y')
    {
      if(argc != 3)
      {
        std::cout<<"Usage for Conrev OrdersToExchForStratFeed :: <binary> <delayinsec> <Portfolio>"<<std::endl;
        exit(1);
      }
      int max_token=0,PFIndex=0;
      int tokenStore[30000]={0};
      storePortFolioInfo PFinfo[15000];

      int portfolio_size = parsePortfolio_Conrev(argv[2],tokenStore,max_token,PFinfo);

      EliminateRepeatitiveToken(tokenStore,max_token);

      LoadContractFile("contract.txt",tokenStore);
      PortFolioFile_SendNSEFOAddReq_Recovery(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,0);
      for(int32_t i=0; i<7; i++) 
      {
        ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
        if(iNoOfBytesRem > 0)
        {
          ProcessExchResp(chDataBuff, iNoOfBytesRem);
        }
        usleep(100);
      }

      sleep(5);


      int SleepDuration = atoi(argv[1]);
      while(1)
      {
        PFIndex = PFIndex%portfolio_size;
        PortFolioFile_SendNSEFOAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);

        usleep(SleepDuration*1000000);
        PFIndex++;
        for(int32_t i=0; i<20; i++)
        {
          ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
          if(iNoOfBytesRem > 0)
          {
            ProcessExchResp(chDataBuff, iNoOfBytesRem);
          }
          usleep(100);
        }
      }
    }
    else if(SingleLeg_OrdersToExch=='Y')
    {
      if(argc != 3)
      {
        std::cout<<"Usage for Single OrdersToExchForStratFeed :: <binary> <delayinsec> <Portfolio>"<<std::endl;
        exit(1);
      }
      int max_token=0,PFIndex=0;
      int tokenStore[30000]={0};
      storePortFolioInfo PFinfo[15000];

      int portfolio_size = parsePortfolio_Conrev(argv[2],tokenStore,max_token,PFinfo);

      EliminateRepeatitiveToken(tokenStore,max_token);

      LoadContractFile("contract.txt",tokenStore);
      
      sleep(2);


      int SleepDuration = atoi(argv[1]);
      int iTempPrice = 2700000;
      while(1)
      {
        PFIndex = PFIndex%portfolio_size;
        SingleLeg_SendNSEFOAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);

        usleep(SleepDuration*1000000);
        PFIndex++;
        for(int32_t i=0; i<7; i++)
        {
          ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
          if(iNoOfBytesRem > 0)
          {
            ProcessExchResp(chDataBuff, iNoOfBytesRem);
          }
          usleep(100);
        }
      }
    }
    else if(SingleLeg_OrdersToExch_CD=='Y')
    {
      int ch;
      if(argc != 3)
      {
        std::cout<<"Usage for Single OrdersToExchForStratFeed CD:: <binary> <delayinsec> <Portfolio>"<<std::endl;
        exit(1);
      }
      int max_token=0,PFIndex=0;
      int tokenStore[30000]={0};
      storePortFolioInfo PFinfo[15000];

      int portfolio_size = parsePortfolio_Conrev(argv[2],tokenStore,max_token,PFinfo);

      EliminateRepeatitiveToken(tokenStore,max_token);

      LoadCDContractFile("cd_contract.txt",tokenStore);
      
      int SleepDuration = atoi(argv[1]);
      while(1)
      {
        PFIndex = PFIndex%portfolio_size;
        std::cout<<"1. New Order|2. Mod Order|3.Can Order"<<std::endl;
        cin >> ch;
        if(ch == 1)
        {
          SingleLeg_SendNSECDAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);
        }
        else if( ch == 2 )
        {
          SingleLeg_SendNSECDModReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);
        }
        else if( ch == 3 )
        {
          SingleLeg_SendNSECDCanReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);
        }
        
        usleep(SleepDuration*1000000);
        PFIndex++;
        for(int32_t i=0; i<7; i++)
        {
          ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
          if(iNoOfBytesRem > 0)
          {
            ProcessExchResp(chDataBuff, iNoOfBytesRem);
          }
          usleep(100);
        }
      }
    }
    else if(Box_OrdersToExchForStratFeed=='Y')
    {
      if(argc != 3)
      {
        std::cout<<"Usage for Box OrdersToExchForStratFeed :: <binary> <delayinsec> <Portfolio>"<<std::endl;
        exit(1);
      }
      int max_token=0,PFIndex=0;
      int tokenStore[30000]={0};
      storePortFolioInfo_Box PFinfo[15000];

      int portfolio_size = parsePortfolio_Box(argv[2],tokenStore,max_token,PFinfo);
      
      EliminateRepeatitiveToken(tokenStore,max_token);

      LoadContractFile("contract.txt",tokenStore);

      PortFolioFile_Box_SendNSEFOAddReq_Recovery(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,0);
      for(int32_t i=0; i<7; i++)
      {
        ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
        if(iNoOfBytesRem > 0)
        {
          ProcessExchResp(chDataBuff, iNoOfBytesRem);
        }
        usleep(100);
      }

      sleep(5);


      int SleepDuration = atoi(argv[1]);
      while(1)
      {
        PFIndex = PFIndex%portfolio_size;
        PortFolioFile_Box_SendNSEFOAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,PFinfo,PFIndex);

        usleep(SleepDuration*1000000);
        PFIndex++;
        for(int32_t i=0; i<12; i++)
        {
          ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
          if(iNoOfBytesRem > 0)
          {
            ProcessExchResp(chDataBuff, iNoOfBytesRem);
          }
          usleep(100);
        }

      }
    }
    else if(ML=='Y')
    {

      if(argc != 3)
      {
        std::cout<<"Usage for ML orders:: <binary> <delayinsec> <Portfolio>"<<std::endl;
        exit(1);
      }
      int tokenStore[30000]={0};
      int *tokenArr;
      int j;


      int max_token = parsePortfolio(argv[2],tokenStore);

//          std::cout<<"Max tokens::"<<max_token<<std::endl;
      tokenArr = new int[max_token];
      for(int i=0;i< max_token;i++ )
      {
        tokenArr[i] = tokenStore[i];
      }
      sort(tokenArr,tokenArr+max_token);

      int total_tokens = max_token;
      //To eliminate repetitive elements in sorted array
      for(int i=0;i<max_token;++i)
      {
        for(j=i+1;j<max_token;)
        {
          if(tokenArr[i]==tokenArr[j])
          {
            for(int k=j;k<max_token-1;++k)
            {
              tokenArr[k]=tokenArr[k+1];
            }
            --max_token;
          } 
          else
          {
            ++j;
          }
        }
      }

      LoadContractFile("contract.txt",tokenArr);
      int SleepDuration = atoi(argv[1]);
      int TokenIndex=0;
      while(1)
      {
        TokenIndex = TokenIndex%total_tokens;
        Send3LReq(iSockId, iUserID, strTMID, sBranchId, llNNF,ContractInfoMap,tokenStore,TokenIndex);        
        std::cout << "Order Entry End" << std::endl;
//            sleep(SleepDuration);
        usleep(SleepDuration*1000000);

        for(int32_t i=0; i<2; i++)
        {
          ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
          if(iNoOfBytesRem > 0)
          {
            ProcessExchResp(chDataBuff, iNoOfBytesRem);
          }
          usleep(100);
        }
      }
    }

  for(int32_t i=0; i<100; i++)
  {
    ReceiveFromExchange(chDataBuff, iNoOfBytesRem, iSockId, sConnStatus);
    if(iNoOfBytesRem > 0)
    {
      ProcessExchResp(chDataBuff, iNoOfBytesRem);
    }
    usleep(100);
  }

  sleep(1);
    
  return 0;
}

//        SendNSECMAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF, 14356, 1175351400, 1, 65090);
//        SendNSEFOAddReq(iSockId, iUserID, strTMID, sBranchId, llNNF, 48353, 1175351400, 1, 7330);
//        SendNSEFOAddReqSL_NonTrim(iSockId, iUserID, strTMID, sBranchId, llNNF, 35022, 1182090600, 2, 2033501,2073500);
//        SendNSECMAddReqSL_NonTrim(iSockId, iUserID, strTMID, sBranchId, llNNF, 3812,2, 38000,38000);

