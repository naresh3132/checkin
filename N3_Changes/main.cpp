 /* 
 * File:   main.cpp
 * Author: NareshRK
 *
 * Created on August 29, 2018, 4:35 PM
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <stdint.h>
#include <vector>
#include <thread>
#include "spsc_atomic1.h"
#include "ConfigReader.h"
#include "FileAttributeAssignment.h" 
#include "TCPHandler.h"
#include "FastFeedHandling.h"

using namespace std;

char chMainLogBuff[400];
extern std::vector<OrderBook> vOrderBook;
extern CONNINFO *gpConnFO;
extern CONNINFO *gpConnCM;


bool HandleFO(ConfigFile& oConfigStore, connectionMap& connMap, int& iSockFdFO)
{
  TCPCONNECTION* pTcpConn;
  
  bool bDataRecv = false;
  int16_t *ipLength = new int16_t;
  int16_t iLength;
  connectionMap ConnMap;
  iSockFdFO = pTcpConn->CreateTCPClient(oConfigStore.sIpAddressExchFO.c_str() , oConfigStore.iPortExchFO,oConfigStore.iRecvBuff); //create connection for FO Simulator
  if(iSockFdFO < 0)
  {
    return false;
  }
  CONNINFO *pConn = new CONNINFO;
  pConn->status = LOGGED_OFF;
  pConn->dealerID = oConfigStore.iUserID_FO;
  memcpy(pConn->IP,oConfigStore.sIpAddressExchFO.c_str(),sizeof(pConn->IP));
  gpConnFO = pConn;
  connMap.insert(make_pair(iSockFdFO,pConn));

  int iRetVal = SendLogonReqFO(iSockFdFO, oConfigStore, pConn);
  
  while(1)
  {
   
    if(!pTcpConn->RecvData(pConn->msgBuffer, pConn->msgBufSize, iSockFdFO, pConn))
    {
      if(pConn->msgBufSize <= 0)
      {
        continue;
      }
    }
    
    ipLength = (int16_t*)pConn->msgBuffer;
    iLength = __bswap_16(*ipLength);
    if(pConn->msgBufSize < iLength)
    {
//      sprintf(chMainLogBuff, "HandleFO |iBytesToBeProcessed %d|Required Length %d|", pConn->msgBufSize, iLength);
//      Logger::getLogger().log(DEBUG, chMainLogBuff);
      continue;
    }
    iRetVal = ProcRcvdMsgFO(pConn->msgBuffer, pConn->msgBufSize, iSockFdFO, pConn, ConnMap);
    if(iRetVal < 0)
    {
      sprintf(chMainLogBuff, "USERID %d|SIGN ON FAILED|SYSTEM SHUTDOWN",oConfigStore.iUserID_FO);
      Logger::getLogger().log(DEBUG, chMainLogBuff);
      pTcpConn->closeFD(iSockFdFO);
      return false;
    }
    else
    {
      break;
    }
    
    
  }
  
  return true;
}

bool HandleCM(ConfigFile& oConfigStore, connectionMap& connMap, int& iSockFdCM)
{
  TCPCONNECTION* pTcpConn;
  connectionMap ConnMap;
  iSockFdCM = pTcpConn->CreateTCPClient(oConfigStore.sIpAddressExchCM.c_str() , oConfigStore.iPortExchCM,oConfigStore.iRecvBuff); //create connection for CM Simulator
  if(iSockFdCM < 0)
  {
    return false;
  }
  CONNINFO *pConn = new CONNINFO;
  pConn->status = LOGGED_OFF;
  pConn->dealerID = oConfigStore.iUserID_CM;
  memcpy(pConn->IP,oConfigStore.sIpAddressExchCM.c_str(),sizeof(pConn->IP));
  gpConnCM = pConn;
  connMap.insert(make_pair(iSockFdCM,pConn));

  int iRetVal = SendLogonReqFO(iSockFdCM, oConfigStore, pConn);

  do
  {
    pTcpConn->RecvData(pConn->msgBuffer, pConn->msgBufSize, iSockFdCM, pConn);

    iRetVal = ProcRcvdMsgFO(pConn->msgBuffer, pConn->msgBufSize, iSockFdCM, pConn, ConnMap);
    if(iRetVal < 0)
    {
      std::cout<<"USERID::" <<oConfigStore.iUserID_CM<<"|SIGN ON FAILED|SYSTEM SHUTDOWN"<<std::endl;
      pTcpConn->closeFD(iSockFdCM);
      return false;
    }
  }while(pConn->msgBufSize != 0);
  
  return true;
}


int main(int argc, char** argv)
{
  bool lbretVal;
  char cDilimiter = '=';
  ConfigReader loConfigReader(CONFIG_FILE_NAME,cDilimiter);  //Read Config file
  loConfigReader.dump();
  vOrderBook.reserve(1000);   //Order Book
  ConfigFile loConfigStore;   
  loConfigStore.StoreConfigData(loConfigReader);  //Store config data
  
  
  ERRORCODEINFO *pErrorInfo;      //Error Code file read
  ERRORCODEINFO loErrInfo[200];
  int iErrIndex;
  if((lbretVal = pErrorInfo->LoadErrorFile(loErrInfo, loConfigStore.sErrorFile.c_str(), iErrIndex)) == false)
  {
    sleep(2);
    return 0;
  }
  else
  {
    std::cout<<"Error File Info:"<<std::endl;
    for( int i = 0; i < iErrIndex ; i++)
    {
      pErrorInfo->dump(loErrInfo[i]);
    }
  }
  

  TOKENSUBSCRIPTION loTokenSub(loConfigStore.iMaxToken);    //Token file read
  if((lbretVal = loTokenSub.LoadSubscriptionFile(loConfigStore.sTokenFile.c_str())) == false)
  {
    sleep(2);
    return 0;
  }
  else
  {
    std::cout<<"Tokens Subscribed";
    for (int i = 0; i< loTokenSub.iNoOfToken; i++)
    {
        std::cout<<"|"<<loTokenSub.iaTokenArr[i];
    }
    std::cout<<std::endl;
  }
  
  
  
  CONTRACTINFO loContInfo;    //Contract file read
  ContractInfoMap ContInfoMap;
  if((lbretVal = loContInfo.LoadContractFile(loConfigStore.sContractFilePath.c_str(),loTokenSub.iaTokenArr,loTokenSub.iNoOfToken,ContInfoMap)) == false)
  {
    sleep(2);
    return 0;
  }
  else
  {
     std::cout<<"Contract Info"<<std::endl;
     loContInfo.dump(ContInfoMap);
  }
  
  
  DEALERINFO loDealerInfo;      //Dealer file read
  dealerToIdMap DealToIdMap;
  dealerInfoMap DealInfoMap;
  if((lbretVal = loDealerInfo.LoadDealerFile(loConfigStore.sDealerFile.c_str(),DealToIdMap,DealInfoMap)) == false)
  {
    std::cout<<"Error:"<<std::endl;
    sleep(2);
    return 0;
  }
  else
  {
    std::cout<<"Dealer Info:"<<std::endl;
    loDealerInfo.dump(DealToIdMap, DealInfoMap);
  }
 
  TCPCONNECTION loTcpConn;
  int lnSockFdUI,lnSockFdFO,lnSockFdCM;

  
  connectionMap connMap;
  
  Logger::getLogger().setLevel(ERROR);
    
  Logger::getLogger().setDump("n3_logger", loConfigStore.iCore);
  snprintf(chMainLogBuff,250,"N3 Version: 1.0.0.0");
  Logger::getLogger().log(DEBUG, chMainLogBuff);
  
  if(loConfigStore.FOSeg)   //Connection to FO simulator
  {
    if(!HandleFO(loConfigStore,connMap,lnSockFdFO))
    {
      exit(1);
    }
  }
  
  if(loConfigStore.CMSeg) //Connection to CM simulator
  {
    if(!HandleCM(loConfigStore,connMap,lnSockFdCM))
    {
      exit(1);
    }  
  }
  ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF = new ProducerConsumerQueue<DATA_RECEIVED>(1000);   //Queue for Token depth Main to FF thread
  ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain = new ProducerConsumerQueue<DATA_RECEIVED>(1000);   //Queue for Token depth FF thread to Main
  
  
  /*BroadCast Handling*/

  FastFeedHandler *pFF;
  std::thread thread_FF ;
  thread_FF = std::thread(&FastFeedHandler::HandleFFConnection, pFF, std::ref(loConfigStore), std::ref(loTokenSub), std::ref(Inqptr_MainToFF), std::ref(Inqptr_FFToMain) ); 
  
  /*Broadcast Handling end*/
  
  
  lnSockFdUI = loTcpConn.CreateTCPConnection(loConfigStore.sIpAddressUI.c_str() , loConfigStore.iPortUI); //create connection for UI
  if(lnSockFdUI < 0)
  {
    return false;
  }
  
  loTcpConn.TCPProcessHandling(lnSockFdUI, lnSockFdFO, lnSockFdCM, DealToIdMap, DealInfoMap, ContInfoMap, loTokenSub, loConfigStore, connMap, loErrInfo, std::ref(Inqptr_MainToFF), std::ref(Inqptr_FFToMain));
  
  thread_FF.join();
  
  return 0;
}

