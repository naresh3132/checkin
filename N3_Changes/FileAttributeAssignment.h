/* 
 * File:   FileAttributeAssignment.h
 * Author: NareshRK
 *
 * Created on August 29, 2018, 5:37 PM
 */

#ifndef FILEATTRIBUTEASSIGNMENT_H
#define	FILEATTRIBUTEASSIGNMENT_H
#include<iostream>
#include <cstring>
#include <string>
#include <unordered_map>
#include <algorithm>
#define CONFIG_FILE_NAME		"config.ini"
#include "ConfigReader.h"
#include "define.h"
//#include "AllStructure.h"



class ConfigFile
{
public:
  bool FOSeg;
  bool CMSeg;
  bool FastFeedFlag;
  std::string sContractFilePath;
  std::string sSecurityFilePath;
  std::string sTokenFile;
  std::string sDealerFile;
  std::string sErrorFile;
  std::string sIpAddressExchCM;
  int32_t iPortExchCM;
  std::string sIpAddressExchFO;
  int32_t iPortExchFO;
  std::string sIpAddressUI;
  int32_t iPortUI;
  int16_t iCore;
  int32_t iMaxToken;
  int32_t iRecvBuff;
  int32_t iSendBuff;
  std::string sTMID_FO;
  int32_t iUserID_FO;
  int32_t iUserID_CM;
  int16_t sBranchId_FO;
  int32_t iExchVer_FO;
  int64_t iNNF_FO;
  std::string sPassword_FO;
  std::string sExchFFIP;
  int32_t iExchFFPort;
  
  std::string sIfaceIP;
  std::string sFFServIP;
  int32_t iFFServPort;
  int32_t iTTLOpt;
  int32_t iTTLVal;
  
  bool bPrintFFData;
  
  
  ConfigFile()
  {
    FOSeg        = false;
    CMSeg        = false;
    FastFeedFlag = false;
    bPrintFFData = false;
  }
  
  void StoreConfigData(ConfigReader &);
};



class TOKENSUBSCRIPTION
{
public:
  int iNoOfToken;
  int iMaxToken;
  int *iaTokenArr;
  
  TOKENSUBSCRIPTION(int MaxT):iMaxToken(MaxT)
  {
    iaTokenArr = new int[iMaxToken];
  }
  bool LoadSubscriptionFile(const char* sFileName);
};

enum class ERROR_CASE
{
  ERRORCODE   = 1,
  ERRORSTRING = 2
};

struct ERRORCODEINFO
{
  int16_t iErrorCode;
  char    cErrString[128];
  
  bool LoadErrorFile( ERRORCODEINFO oErrInfo[], const char* sFileName, int& iErrIndex);
  void dump(ERRORCODEINFO& );
};

enum  CONTRACT_CASE
{
   TOKEN = 1,
   ASSETTOKEN = 2,
   MINLOTQTY = 31,
   BOARDLOTQTY = 32,
   LOWDPR = 43,
   HIGHDPR = 44,
   INSTRUMENTNAME = 3,
   SYMBOL = 4,
   OPTIONTYPE = 9,
   SERIES = 5,
   EXPIRY = 7,
   TICKSIZE = 33,
   TRADINGSYMBOL = 54,
   STRIKE = 8
};

class CONTRACTINFO
{
public:
   int16_t iSeg;
   int32_t AssetToken;
   int32_t Token;
   char Symbol[10 + 1];
   char InstumentName[6 + 1];
   char OptionType[2 + 1];
   char TradingSymbols [25 + 1];
   char cSeries [ 2 + 1];
   uint16_t iTickSize;
   int32_t ExpiryDate;
   int32_t MinimumLotQuantity;
   int32_t HighDPR;
   int32_t LowDPR;
   int32_t StrikePrice;
   
   
  CONTRACTINFO()
  {
    AssetToken = 0;
    MinimumLotQuantity = 0;
    StrikePrice = 0;
  }
  
  bool LoadContractFile(const char* pStrFileName,int *TokenArr, int32_t iToken, std::unordered_map<int32_t,CONTRACTINFO*>& );
  void dump(std::unordered_map<int32_t,CONTRACTINFO*>& );
  
};
typedef std::unordered_map<int32_t,CONTRACTINFO*> ContractInfoMap;
typedef ContractInfoMap::iterator iterContInfo;


class DEALERINFO
{
public:
  char cIP[20];
  int32_t iStatus;
  int16_t shCOL;
  int32_t iDealerID;
  int32_t iRecordCnt;
  
  bool LoadDealerFile(const char* pStrFileName, std::unordered_map<std::string,int32_t>& , std::unordered_map<int32_t,DEALERINFO*>& );
  void dump( std::unordered_map<std::string,int32_t>& , std::unordered_map<int32_t,DEALERINFO*> &);
};

typedef std::unordered_map<std::string,int32_t> dealerToIdMap;
typedef dealerToIdMap::iterator iterdealerToIdMap;

typedef std::unordered_map<int32_t,DEALERINFO*> dealerInfoMap;
typedef dealerInfoMap::iterator iterDealerInfo;

#endif	/* FILEATTRIBUTEASSIGNMENT_H */

