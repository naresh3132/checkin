#include <iostream>
#include <stdlib.h>
#include <assert.h>
#include <error.h>
#include "FileAttributeAssignment.h"
#include "ConfigReader.h" 
#include "CommonFunction.h"
#include "nsecm_constants.h"

using namespace std;


void ConfigFile::StoreConfigData(ConfigReader& ConfigStore)
{
  sContractFilePath             = ConfigStore.getProperty("CONTRACT_FILE_PATH");
  sSecurityFilePath             = ConfigStore.getProperty("SECURITY_FILE_PATH");
  sTokenFile                    = ConfigStore.getProperty("TOKEN_FILE");
  sDealerFile                   = ConfigStore.getProperty("DEALER_FILE");
  sErrorFile                    = ConfigStore.getProperty("ERROR_CODE_FILE_PATH");
  iSendBuff                     = atoi(ConfigStore.getProperty("SENDBUFFER_SIZE").c_str());
  iRecvBuff                     = atoi(ConfigStore.getProperty("RECVBUFFER_SIZE").c_str());
  sIpAddressExchFO              = ConfigStore.getProperty("IP_ADDRESS_EXCH_FO");
  iPortExchFO                   = atoi(ConfigStore.getProperty("PORT_EXCH_FO").c_str());
  sIpAddressExchCM              = ConfigStore.getProperty("IP_ADDRESS_EXCH_CM");
  iPortExchCM                   = atoi(ConfigStore.getProperty("PORT_EXCH_CM").c_str());
  sExchFFIP                     = ConfigStore.getProperty("FCAST_IP_RECV");
  iExchFFPort                   = atoi(ConfigStore.getProperty("FCAST_PORT_RECV").c_str());
  sIfaceIP                      = ConfigStore.getProperty("INTERFACE_IP");
  sFFServIP                     = ConfigStore.getProperty("FCAST_IP_SERVER");
  iFFServPort                   = atoi(ConfigStore.getProperty("FCAST_PORT_SERVER").c_str());
  
  iTTLOpt                       = atoi(ConfigStore.getProperty("TTL_OPTION").c_str());
  iTTLVal                       = atoi(ConfigStore.getProperty("TTL_VALUE").c_str());
  
  sIpAddressUI                  = ConfigStore.getProperty("IP_ADDRESS_UI");
  iPortUI                       = atoi(ConfigStore.getProperty("PORT_UI").c_str());
  iCore                         = atoi(ConfigStore.getProperty("CORE").c_str());
  iMaxToken                     = atoi(ConfigStore.getProperty("MAX_TOKEN").c_str());
  
  sTMID_FO                      = ConfigStore.getProperty("TMID_FO");
  iUserID_FO                    = atoi(ConfigStore.getProperty("USERID_FO").c_str());
  sBranchId_FO                  = atoi(ConfigStore.getProperty("BRANCHID_FO").c_str());
  iExchVer_FO                   = atoi(ConfigStore.getProperty("EXCH_VER_FO").c_str());
  iNNF_FO                       = atoll(ConfigStore.getProperty("NNF_ID_FO").c_str());
  sPassword_FO                  = ConfigStore.getProperty("PASSWORD_FO");
  char cFOSeg                   = ConfigStore.getProperty("FO_SEG").c_str()[0];
  if(cFOSeg == 'y' || cFOSeg == 'Y')
  {
    FOSeg = true;
  }
  char cCMSeg                   = ConfigStore.getProperty("CM_SEG").c_str()[0];
  if(cCMSeg == 'y' || cCMSeg == 'Y')
  {
    CMSeg = true;
  }
  
  char cFF                   = ConfigStore.getProperty("ENABLE_FASTFEED").c_str()[0];
  if(cFF == 'y' || cFF == 'Y')
  {
    FastFeedFlag = true;
  }
  
  char cPrintFFData          = ConfigStore.getProperty("PRINT_FASTFEED_DATA").c_str()[0];
  if(cPrintFFData == 'y' || cPrintFFData == 'Y')
  {
    bPrintFFData = true;
  }
}

bool TOKENSUBSCRIPTION::LoadSubscriptionFile (const char* pStrFileName)
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
          if (iNoOfToken == 0)
          {
              iaTokenArr[iNoOfToken] = token;
          }
          else if (token > iaTokenArr[iNoOfToken-1])
          {
              iaTokenArr[iNoOfToken] = token;
          }
          else
          {
              found = binarySearch(iaTokenArr, iNoOfToken, token, NULL);
              if (found == false)
              {
                 int i = iNoOfToken -1;
                 while (token<iaTokenArr[i] && i>= 0)
                 {
                    iaTokenArr[i+1] = iaTokenArr[i];
                    i--;
                 }
                 iaTokenArr[i+1] = token;
              }
              else
              {
                  iNoOfToken--;   
              }
          }

          iNoOfToken++;
          if (iNoOfToken > iMaxToken)
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

bool ERRORCODEINFO::LoadErrorFile(ERRORCODEINFO oErrInfo[], const char* pStrFileName, int& iErrIndex)
{
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pStrFileName << std::endl;
    return false;
  }  

  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  iErrIndex = 0;
  int fieldCnt = 0;
  while(!feof(fp))
  {
    char* pLine = NULL;
    size_t sizeLine = 1024;
    fieldCnt = 1;  
    char* fieldValue = NULL;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      
      while (fieldCnt <= 2 )
      {
        fieldValue = strsep(&pLine, "|");
        
        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case (int)ERROR_CASE::ERRORCODE:
              oErrInfo[iErrIndex].iErrorCode = atoi(fieldValue);
              break;
            case (int)ERROR_CASE::ERRORSTRING:
            {
              std::string sErrStr = fieldValue;
              sErrStr.erase(std::remove(sErrStr.end() - 1, sErrStr.end(), '\n'), sErrStr.end());
              
              memcpy(oErrInfo[iErrIndex].cErrString,sErrStr.c_str(),sErrStr.size() );
              
              iErrIndex++;
              break;
            }
            default:
              break;
         }
        }
        else
        {
          std::cout<<"Error while reading field number:"<<fieldCnt<<std::endl;
          return false;
        }
        fieldCnt++;
      }
      
    }
    else
    {
      
    } 
  } 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  
  return true;
}

void ERRORCODEINFO::dump(ERRORCODEINFO& oErrInfo)
{
  std::cout<<"ErrorCode::" << oErrInfo.iErrorCode<< "|Error String " <<oErrInfo.cErrString<<std::endl;
}

bool CONTRACTINFO::LoadContractFile (const char* pStrFileName,int* TokenArr,int32_t iNoOfToken,ContractInfoMap& ContInfoMap)
{
  
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {
    std::cout << " Error while opening " << pStrFileName << std::endl;
    return false;
  }  

  char* pTemp = NULL;
  size_t sizeTemp = 1024;  
  if(getline(&pTemp,&sizeTemp,fp) != -1) // to remove the first line containing the file version 
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
      CONTRACTINFO *objContractData = new CONTRACTINFO();
      while (fieldCnt <= 54 )
      {
//        std::cout<<" Token Index "<<TokenArr[tokenIdx]<<std::endl;
        fieldValue = strsep(&pLine, "|");
        
        if(TokenArr[tokenIdx] != atoi (fieldValue) && fieldCnt==1)
        {
          delete objContractData;
          break;
        }
        if (fieldValue != NULL)
        {
          switch(fieldCnt)
          {
            case TOKEN:
              objContractData->iSeg  = SEG_NSEFO;
              objContractData->Token = atoi(fieldValue);
              break;
            case INSTRUMENTNAME:
              memcpy(objContractData->InstumentName,fieldValue,6);
              break;
            case SYMBOL:
              memcpy(objContractData->Symbol,fieldValue,11);
              break;
            case OPTIONTYPE:
              memcpy(objContractData->OptionType,fieldValue,3);
              break;
            case EXPIRY:
              objContractData->ExpiryDate = atoi(fieldValue);
              break;
            case STRIKE:
              objContractData->StrikePrice = atoi(fieldValue);  
              break;
            case MINLOTQTY:
              objContractData->MinimumLotQuantity = atoi(fieldValue);
              break;
            case SERIES:
              memcpy(objContractData->cSeries,fieldValue,3);
              break;
            case TICKSIZE:
              objContractData->iTickSize = atoi(fieldValue);
              break;
            case LOWDPR:
              objContractData->LowDPR = atoi(fieldValue);
              break;
            case HIGHDPR:
              objContractData->HighDPR = atoi(fieldValue);
              break;
            case TRADINGSYMBOL:
              memcpy(objContractData->TradingSymbols,fieldValue,26);
              ContInfoMap.insert(make_pair(TokenArr[tokenIdx],objContractData));
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
      if(tokenIdx != iNoOfToken)
      {
        std::cout<<"Token Not Found :"<<TokenArr[tokenIdx]<<std::endl;
        exit(1);
      }
      std::cout << errno << " returned while reading line." << std::endl; 
    } 
  } //while(!feof(fp)) 
  
  if (fp != NULL)
  {
      fclose(fp);
  }
  
  return true;
} 

void CONTRACTINFO::dump (ContractInfoMap& ContractInfoMap)
{
  iterContInfo it;
  for(it=ContractInfoMap.begin() ; it!=ContractInfoMap.end(); ++it)
  {
    std::cout<<"Token:"<<it->first<<"|Symbol:"<<it->second->Symbol<<"|LOT:"<<it->second->MinimumLotQuantity<<"|OptionType:"<<it->second->OptionType<<"|LowDPR:"<<it->second->LowDPR<<"|HighDPR:"<<it->second->HighDPR<<
      "|Trading Symbol:"<<it->second->TradingSymbols<<"|Tick Size:"<<it->second->iTickSize<<"|Series:"<<it->second->cSeries<<std::endl;
    
  }
}


bool DEALERINFO::LoadDealerFile(const char* pStrFileName,dealerToIdMap& DealToIdMap,dealerInfoMap& DealInfoMap )
{
  assert(pStrFileName);
  iterdealerToIdMap it;
  
  bool bRet = true;
  
  FILE *fp = fopen(pStrFileName, "r+");
  if(fp == NULL)
  {	
    std::cout << " Error while opening " << pStrFileName << std::endl;
    bRet = false;
    return bRet;
  }  
  
  size_t sizeLine = 30;
  int32_t dealerID = 0;
  int index = 1001;
  while(!feof(fp))
  {
    char* pLine = NULL;
    if(getline(&pLine, &sizeLine, fp) != -1)
    {
      std::string sDealer = pLine;
      sDealer.erase(std::remove(sDealer.end() - 1, sDealer.end(), '\n'), sDealer.end());
      
      it = DealToIdMap.find(sDealer);
      if(it ==  DealToIdMap.end())
      {
        DEALERINFO *oDealerInfo = new DEALERINFO;
        DealToIdMap.insert(make_pair(sDealer,index));
        oDealerInfo->iStatus = LOGGED_OFF;
        oDealerInfo->iRecordCnt = 0;
        DealInfoMap.insert(make_pair(index,oDealerInfo));
        index++;
      }
      else
      {
        std::cout<<"Duplicate Dealer::"<<pLine<<endl;
        bRet = false;
      }
      
    } 
    else
    {
         std::cout<<"Reading Dealer file|Error "<<errno<<std::endl;
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

void DEALERINFO::dump (dealerToIdMap& DealToIdMap, dealerInfoMap& DealInfoMap)
{
  
  iterDealerInfo it;
  iterdealerToIdMap itId;
  for( itId=DealToIdMap.begin() ; itId!=DealToIdMap.end(); ++itId)
  {
    int iIndex = itId->second;
    it = DealInfoMap.find(iIndex);
    std::cout<<"Dealer:"<<itId->first<<"|DealerId:"<<iIndex<<"|RecordCnt:"<<it->second->iRecordCnt<<"|Status:"<<it->second->iStatus<<std::endl;
  }

}

