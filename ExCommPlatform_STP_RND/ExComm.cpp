#include <bits/error_constants.h>

#include "ExComm.h"
#include "nsefo_exch_structs.h"
#include "nsecm_exch_structs.h"
#include "md5.h"
#include "SimMD5/MsgValidator.h"
#include <unordered_map>
#include <functional>

//const int16_t LOG_BUFF_SIZE               = 768;
//const int32_t RECV_BUFF_SIZE              = 10485760; //10MB
//const int32_t MAX_TCP_RECV_BUFF_SIZE      = 134217728; //128MB
//const int32_t SEND_BUFF_SIZE              = 2048;


char chLogBuff[512] = {0};
char chDataBuff[SEND_BUFF_SIZE] = {0};
uint32_t iDataLen = 0;
int16_t iRetVal = 0;
int32_t iExchSendSeqNo = 0;
char logBuf[500];

struct InfoStore
{
  int64_t ExchOrdId;
  int32_t LMT;
};

std::unordered_map<int32_t,InfoStore> mIntOrdIdExchOrdId;


int16_t ConnectToNSEFOExch(string strConnectionIP, uint32_t iConnectionPort, string strTMID, uint32_t iUserID, uint16_t sBranchId, uint32_t iExchVer, string strPassword, int32_t& iSockId, int16_t& iNoOfStreams)
{
    struct sockaddr_in server;
    //Create socket
    iSockId = socket(AF_INET , SOCK_STREAM , 0);
    if(iSockId == -1)
    {
        //sprintf(chLogBuff, "[SYS] Could not create socket|Error %d|%s|", errno, strerror(errno));
        //cout << chLogBuff << endl;
        usleep(RECONNECTION_DELAY);
        return -1;
    }
	
    sprintf(chLogBuff,  "[SYS] Created Socket"); 
    cout << chLogBuff << endl;
	
    int lnRecvBufSize = MAX_TCP_RECV_BUFF_SIZE;
    if(0 != setsockopt(iSockId, SOL_SOCKET, SO_RCVBUF, &lnRecvBufSize, sizeof(lnRecvBufSize)))	
    {
        sprintf(chLogBuff, "[SYS] setsockopt failed for setting RecvBufSize"); 
        cout << chLogBuff << endl;
        usleep(RECONNECTION_DELAY);
        return -1;
    }	 
    server.sin_family = AF_INET;
  
 
    sprintf(chLogBuff, "[SYS] Attempting Connection on | %s | %d |ConnectToExch", strConnectionIP.c_str(), iConnectionPort); 
    cout << chLogBuff << endl;

    server.sin_addr.s_addr = inet_addr(strConnectionIP.c_str());
    server.sin_port = htons(iConnectionPort);
	
    //Connect to remote server
    if (connect(iSockId , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //sprintf(chLogBuff, "[SYS] connect() failed:[%d][%s]", errno, strerror(errno)); 
        //cout << chLogBuff << endl;
        usleep(RECONNECTION_DELAY);
        return -1;
    }
   
    int nReuseAddr = 1;
    if(setsockopt(iSockId, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuseAddr, sizeof(nReuseAddr)) < 0)
    {
      sprintf(chLogBuff, "[SYS] Error while setting REUSEADDR.");
      cout << chLogBuff << endl;
      close(iSockId);
      usleep(RECONNECTION_DELAY);
      return -1;
    }
	
    fcntl(iSockId, F_SETFL, O_NONBLOCK);    //We are using Non Blocking Sockets
    char flag = 0;
    setsockopt(iSockId, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof flag);
	
    sprintf(chLogBuff, "[SYS] Connected to Exch |%s:%d|SocketId %d|ConnectToExch", strConnectionIP.c_str(), iConnectionPort, iSockId);
    cout << chLogBuff << endl;
    
//    SendGRReq(strTMID, iUserID, iSockId);
    
    SendLogonReq(strTMID, iUserID, sBranchId, iExchVer, strPassword, iSockId);
    return iRetVal;
}

int16_t GetNSEFOBODDownload(uint32_t iUserID, uint16_t iStreamId, uint64_t llExchJiffy, char* pchBuffer, int32_t iSockId)
{
    NSEFO_MS_MESSAGE_DOWNLOAD_REQ oMsgDownloadReq;

    memset(&oMsgDownloadReq.tap_hdr.sLength, ' ', sLen_NSEFO_MsgDownload_Req);	

    oMsgDownloadReq.msg_hdr.TransactionCode = __bswap_16(NSEFO_DOWNLOAD_REQUEST);
    oMsgDownloadReq.msg_hdr.LogTime         = 0;
    oMsgDownloadReq.msg_hdr.AlphaChar[0]	= ASCII_OFFSET_FOR_NUMBERS + iStreamId;
    //oMsgDownloadReq.msg_hdr.AlphaChar[0]    = iStreamId;

    //oMsgDownloadReq.msg_hdr.AlphaChar[1]	= 0;
    oMsgDownloadReq.msg_hdr.TraderId        = __bswap_32(iUserID);
    oMsgDownloadReq.msg_hdr.ErrorCode       = 0;
    //	oMsgDownloadReq.msg_hdr.Timestamp   //Initialized to space
    //	oMsgDownloadReq.msg_hdr.TimeStamp1	//Initialized to 0
    //	oMsgDownloadReq.msg_hdr.TimeStamp2	//Initialized to space
    oMsgDownloadReq.msg_hdr.MessageLength   = __bswap_16(sLen_NSEFO_MsgDownload_Req - sLen_NSEFO_Tap_Hdr);

    double dExchSeqNo = 0;
    SwapDouble((char*)&dExchSeqNo);
    //memcpy(&oMsgDownloadReq.SeqNo, &llExchJiffy, 8);
    oMsgDownloadReq.SeqNo = dExchSeqNo;

    oMsgDownloadReq.tap_hdr.iSeqNo         = __bswap_32(++iExchSendSeqNo);
    cout << "@@@@" << iExchSendSeqNo << endl;
    
    oMsgDownloadReq.tap_hdr.sLength         = __bswap_16(sLen_NSEFO_MsgDownload_Req);
    md5_ful27((char*)&oMsgDownloadReq.msg_hdr.TransactionCode, sLen_NSEFO_MsgDownload_Req-sLen_NSEFO_Tap_Hdr, oMsgDownloadReq.tap_hdr.CheckSum);


    //memcpy(pchBuffer, &oMsgDownloadReq.tap_hdr.sLength, sLen_NSEFO_MsgDownload_Req);
    iDataLen = sLen_NSEFO_MsgDownload_Req;


    //iRetVal = SendOrderToExchange(pchBuffer, iDataLen, iSockId);
    iRetVal = SendOrderToExchange((char*)&oMsgDownloadReq.tap_hdr.sLength, iDataLen, iSockId);
    
    sprintf(chLogBuff, "Sending MsgDnlReq for Stream %d", iStreamId);
    cout << chLogBuff << endl;

    return iRetVal;    
}

//bool ReceiveFromExchange(char* pchBuffer, int32_t& iBytesRemaining, int32_t iSockId, int16_t& sConnStatus)
//{
//    int32_t iBytesRcvd = 0;
//    bool bReturnNow = false;
//    int32_t iLoopcnt = 0;
//    
//    //iBytesRcvd = recv(iSockId , pchBuffer , RECV_BUFF_SIZE , 0);
//    
//    while(bReturnNow == false)
//    {
//      iBytesRcvd = recv(iSockId , pchBuffer , 500 , 0);
//      if( iBytesRcvd > 0 )
//      {
//        iBytesRemaining += iBytesRcvd;
//        bReturnNow = true;
//        return true;
//      }
//      else if((iBytesRcvd < 0) && ((errno != EAGAIN) ||(errno != EWOULDBLOCK)))
//      {
//        sConnStatus = DISCONNECTED;
//        bReturnNow = true;
//        return false;
//      }
//      
//      //cout << "Looping...." << ++iLoopcnt << endl;
//    }
//}

bool ReceiveFromExchange(char* pchBuffer, int32_t& iBytesRemaining, int32_t iSockId, int16_t& sConnStatus)
{
    int32_t iBytesRcvd = 0;
    iBytesRcvd = recv(iSockId , pchBuffer+iBytesRemaining , 500 , 0);

  
    if( (iBytesRcvd < 0) && ((errno == EAGAIN) ||(errno == EWOULDBLOCK)) )
    { 
        cout << iBytesRcvd << "|" << errno << std::endl;
        return(false);
    }
    else if(iBytesRcvd > 0) 
    { 
      iBytesRemaining += iBytesRcvd;
      sprintf(chLogBuff, "|###|BytesRecvd %d|BytesRem %d|", iBytesRcvd, iBytesRemaining);
      cout << chLogBuff << endl;

      return(true);
    }
    else
    {
      std::cout<<"Disconnection Received"<<std::endl;
      sConnStatus = DISCONNECTED;
      
      shutdown(iSockId,SHUT_RDWR);
      close(iSockId);
      exit(1);
      return(false);      
    }
}

int16_t SendOrderToExchange(char* pchBuffer, uint32_t iDataLength, int32_t iSockId)
{
    int32_t iBytesSend = 0;

    iBytesSend = write(iSockId, pchBuffer, iDataLength);
//    sprintf(chLogBuff, "[SYS] ### Send Bytes[%d] pchCurrBuffPosn[%0X] errno [%d]", iBytesSend, pchBuffer, errno);
//    cout << chLogBuff << endl;
    
    //if( write(iSockId, pchBuffer, iDataLength) > 0)
    if( iBytesSend > 0)
    {
        return 0;
    }
    else
    {
      return errno;
    }
}

int16_t DisconnectFromExchange(int32_t iSockId)
{
    shutdown(iSockId, SHUT_RDWR);
    close(iSockId);
    iSockId = 0;
    return 0;
}

/*New Login Process starts*/

int16_t SendGRReq(string strTMID,int32_t iUserId,int iSockId)
{
  NSEFO_MS_GR_REQUEST oGRrequest;
//  memset(&oGRrequest.tap_hdr.sLength, ' ', sizeof(MS_GR_REQUEST));
  oGRrequest.tap_hdr.sLength = __bswap_16(sizeof(MS_GR_REQUEST));
  oGRrequest.msg_hdr.TransactionCode	= __bswap_16(NSEFO_GR_REQUEST);
  oGRrequest.msg_hdr.LogTime          = 0;
  oGRrequest.msg_hdr.TraderId         = __bswap_32(iUserId);
  oGRrequest.msg_hdr.ErrorCode        = 0;
  oGRrequest.msg_hdr.MessageLength		= __bswap_16(sizeof(MS_GR_REQUEST) - sLen_NSEFO_Tap_Hdr);
  memcpy(oGRrequest.cBrokerID, strTMID.c_str(), strTMID.length());
  oGRrequest.wBoxId = __bswap_16(10);
  
  
  return SendOrderToExchange((char*)&oGRrequest.tap_hdr.sLength, sizeof(MS_GR_REQUEST), iSockId);
  
}

void ProcessGRResp(char* pchBuffer)
{
    NSEFO_MS_GR_RESPONSE oGRresp;
    
    

    char chBrokerName[26] = {0};
//    char IpAddress[16] = {0};
    
    oGRresp.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);

    if(oGRresp.msg_hdr.ErrorCode)
    {
      //NSEFO_MS_ERROR_RESPONSE *pError = (NSEFO_MS_ERROR_RESPONSE *)(pchBuffer + sLen_NSEFO_Tap_Hdr);
      NSEFO_MS_ERROR_RESPONSE *pError = (NSEFO_MS_ERROR_RESPONSE *)(pchBuffer);
      sprintf(chLogBuff,"[RSP] Login Failed: %d | ErrorMsg:[%128.128s]", oGRresp.msg_hdr.ErrorCode, pError->ErrorMessage);
      std::cout << chLogBuff << std::endl;
    }
    else
    {
      memcpy(chBrokerName, oGRresp.cBrokerID, 25);
      
      sprintf(chLogBuff, "[RSP] |Txn %d|Err %d|BrokerName %s|Strm %d|cIPAddress %s|iPort %d|cSessionKey %s",
        oGRresp.msg_hdr.TransactionCode, oGRresp.msg_hdr.ErrorCode, chBrokerName, oGRresp.msg_hdr.TimeStamp2[7],oGRresp.cIPAddress,__bswap_32(oGRresp.iPort),oGRresp.cSessionKey);
      std::cout << chLogBuff << std::endl;

      sprintf(chLogBuff, "[RSP] Logon Successful, Sending SysInfo Request...");
      std::cout << chLogBuff << std::endl;
    }
  
}

int16_t SendBoxSignOnReq(string strTMID,int32_t iUserId)
{
  NSEFO_MS_BOX_SIGN_ON_REQUEST_IN oBoxSignOnRequest;
  memset(&oBoxSignOnRequest.tap_hdr.sLength, ' ', sizeof(MS_BOX_SIGN_ON_REQUEST_IN));
  oBoxSignOnRequest.msg_hdr.TransactionCode	= __bswap_16(NSEFO_BOX_SIGN_ON_REQUEST_IN);
  oBoxSignOnRequest.msg_hdr.LogTime          = 0;
  oBoxSignOnRequest.msg_hdr.TraderId         = __bswap_32(iUserId);
  oBoxSignOnRequest.msg_hdr.ErrorCode        = 0;
  oBoxSignOnRequest.msg_hdr.MessageLength		= __bswap_16(sizeof(NSEFO_BOX_SIGN_ON_REQUEST_IN) - sLen_NSEFO_Tap_Hdr);
  memcpy(oBoxSignOnRequest.cBrokerId, strTMID.c_str(), strTMID.length());
  oBoxSignOnRequest.wBoxId = __bswap_32(10);
  
  return 0;
  
}
 
/*New Login Process Ends*/


int16_t SendLogonReq(string strTMID, int32_t iUserId, int16_t sBranchId, int32_t iExchVer, string strPassword, int32_t iSockId)
{
    NSEFO_MS_SIGNON_REQ oExLognReq;
    memset(&oExLognReq.tap_hdr.sLength, ' ', sizeof(NSEFO_MS_SIGNON_REQ));

    oExLognReq.msg_hdr.TransactionCode	= __bswap_16(NSEFO_SIGN_ON_REQUEST_IN);
    oExLognReq.msg_hdr.LogTime          = 0;
    //oExLognReq.msg_hdr.AlphaChar      //Initialized to space
    oExLognReq.msg_hdr.TraderId         = __bswap_32(iUserId);
    oExLognReq.msg_hdr.ErrorCode        = 0;
    //	oExLognReq.msg_hdr.Timestamp			//Initialized to space
    //	oExLognReq.msg_hdr.TimeStamp1			//Initialized to 0
    //	oExLognReq.msg_hdr.TimeStamp2			//Initialized to space
    oExLognReq.msg_hdr.MessageLength		= __bswap_16(sizeof(NSEFO_MS_SIGNON_REQ) - sLen_NSEFO_Tap_Hdr);

    oExLognReq.UserId                   = __bswap_32(iUserId);
    memcpy(oExLognReq.Passwd, strPassword.c_str(), strPassword.length());

    memcpy(oExLognReq.NewPassword, "        ", 8);

    //oExLognReq.TraderName             //Initialized to spaces
    oExLognReq.LastPasswdChangeDate     = 0;
    memcpy(oExLognReq.BrokerId, strTMID.c_str(), strTMID.length());

    //oExLognReq.Reserved1              //Initialized to space
    oExLognReq.BranchId                 = __bswap_16(sBranchId);
    oExLognReq.VersionNumber            = __bswap_32(iExchVer);
    //oExLognReq.Reserved2
    oExLognReq.UserType                 = 0;
    oExLognReq.SequenceNumber           = 0;
    //oExLognReq.WsClassName            //Initialized to spaces
    //oExLognReq.BrokerStatus           //Initialized to spaces
    oExLognReq.ShowIndex                = 'T';  //T: for Trimmed NNF Protocol
    //oExLognReq.st_broker_eligibility_per_mkt. = 0;						//NNED TO COME BACK TO THIS
    oExLognReq.UserType               = 0;		//1:Trading Member Only
    //oExLognReq.ClearingStatus         //Initialized to spaces
    //oExLognReq.BrokerName             //Initialized to spaces

    oExLognReq.tap_hdr.sLength          = __bswap_16(sLen_NSEFO_Logon_Req);

    oExLognReq.tap_hdr.iSeqNo         = __bswap_32(++iExchSendSeqNo);
    cout << "@@@@" << iExchSendSeqNo << endl;

    //oExLognReq.tap_hdr.sResrvSeqNo      = 0; 
    //oExLognReq.tap_hdr.sMsgCnt          = __bswap_16(1); //HARDCODED, as specified in NNF doc
    md5_ful27((char*)&oExLognReq.msg_hdr.TransactionCode, sLen_NSEFO_Logon_Req-sLen_NSEFO_Tap_Hdr, oExLognReq.tap_hdr.CheckSum);

    sprintf(chLogBuff, "[REQ] STEP 1: Sending SendLogonReq to Exchange");
    cout << chLogBuff << endl;

    return SendOrderToExchange((char*)&oExLognReq.tap_hdr.sLength, sLen_NSEFO_Logon_Req, iSockId);
}


void ProcessLogonResp(char* pchBuffer)
{  
    NSEFO_MS_SIGNON_RESP oExLogonResp;
    char chBrokerName[26] = {0};

    oExLogonResp.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);

    if(oExLogonResp.msg_hdr.ErrorCode)
    {
      //NSEFO_MS_ERROR_RESPONSE *pError = (NSEFO_MS_ERROR_RESPONSE *)(pchBuffer + sLen_NSEFO_Tap_Hdr);
      NSEFO_MS_ERROR_RESPONSE *pError = (NSEFO_MS_ERROR_RESPONSE *)(pchBuffer);
      sprintf(chLogBuff,"[RSP] Login Failed: %d | ErrorMsg:[%128.128s]", oExLogonResp.msg_hdr.ErrorCode, pError->ErrorMessage);
      std::cout << chLogBuff << std::endl;
    }
    else
    {
      memcpy(chBrokerName, oExLogonResp.BrokerName, 25);
      time_t rawtime = 0;
      time(&rawtime);
      int32_t iDaysSinceLastPwdChanged = (rawtime - (__bswap_32(oExLogonResp.LastPasswdChangeDate) + GAP_1970_1980)) / SECONDS_IN_A_DAY;

      sprintf(chLogBuff, "[RSP] |Txn %d|Err %d|BrokerName %s|Strm %d|LastPwdChngDate %d|DaysSinceLastPwdChanged %d|", oExLogonResp.msg_hdr.TransactionCode, oExLogonResp.msg_hdr.ErrorCode, chBrokerName, oExLogonResp.msg_hdr.TimeStamp2[7], __bswap_32(oExLogonResp.LastPasswdChangeDate)+GAP_1970_1980, iDaysSinceLastPwdChanged);
      std::cout << chLogBuff << std::endl;

      sprintf(chLogBuff, "[RSP] Logon Successful, Sending SysInfo Request...");
      std::cout << chLogBuff << std::endl;
    }
}

int16_t SendSysInfoReq(int32_t iUserId, int32_t iSockId)
{
	NSEFO_MS_SYSTEM_INFO_REQ oSysInfoReq;
	
	memset(&oSysInfoReq.tap_hdr.sLength, ' ', sLen_NSEFO_SysInfo_Req);	
	
	oSysInfoReq.msg_hdr.TransactionCode	= __bswap_16(NSEFO_SYSTEM_INFORMATION_IN);
	oSysInfoReq.msg_hdr.LogTime         = 0;
	//oSysInfoReq.msg_hdr.AlphaChar     //Initialized to space
	oSysInfoReq.msg_hdr.TraderId        = __bswap_32(iUserId);
	oSysInfoReq.msg_hdr.ErrorCode       = 0;
	//	oSysInfoReq.msg_hdr.Timestamp		//Initialized to space
	//	oSysInfoReq.msg_hdr.TimeStamp1	//Initialized to 0
	memset(oSysInfoReq.msg_hdr.TimeStamp2, 0, 8);
	oSysInfoReq.msg_hdr.MessageLength   = __bswap_16(sLen_NSEFO_SysInfo_Req - sLen_NSEFO_Tap_Hdr);
	
	oSysInfoReq.LastUpdatePortfolioTime = 0;
	
	
	oSysInfoReq.tap_hdr.sLength         = __bswap_16(sLen_NSEFO_SysInfo_Req);
  oSysInfoReq.tap_hdr.iSeqNo        = __bswap_32(++iExchSendSeqNo);
  cout << "@@@@" << iExchSendSeqNo << endl;
	//oSysInfoReq.tap_hdr.sResrvSeqNo     = 0; 
	//oSysInfoReq.tap_hdr.sMsgCnt         = __bswap_16(1); //HARDCODED, as specified in NNF doc
	//md5_hash_Full((char*)&oSysInfoReq.tap_hdr.sMsgCnt, sLen_SysInfo_Req-sLen_Tap_Hdr+2, oSysInfoReq.tap_hdr.CheckSum);
  md5_ful27((char*)&oSysInfoReq.msg_hdr.TransactionCode, sLen_NSEFO_SysInfo_Req-sLen_NSEFO_Tap_Hdr, oSysInfoReq.tap_hdr.CheckSum);
	
	//std::cout << "Full Size " << sLen_SysInfo_Req << " Twiddled Size " << oSysInfoReq.tap_hdr.sLength << std::endl;
	
  sprintf(chLogBuff, "[REQ] STEP 2: Sending SendsysInfoReq to Exchange");
  cout << chLogBuff << endl;
  
  return SendOrderToExchange((char*)&oSysInfoReq.tap_hdr.sLength, sLen_NSEFO_SysInfo_Req, iSockId);
  
	
}

void ProcessSysInfoResp(char* pchBuffer, int16_t& iNoOfStreams)
{
  NSEFO_MS_SYSTEM_INFO_DATA oSysInfoResp;
  
  oSysInfoResp.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);
  iNoOfStreams = oSysInfoResp.msg_hdr.AlphaChar[0];

  sprintf(chLogBuff, "[RSP] |Txn %d|Err %d|Strm:%d|DQPercent %d|", oSysInfoResp.msg_hdr.TransactionCode, oSysInfoResp.msg_hdr.ErrorCode, oSysInfoResp.msg_hdr.AlphaChar[0], oSysInfoResp.DiscQtyPerAllwd);
  cout << chLogBuff << endl;

//  sprintf(chLogBuff, "[REQ] Downloaded System Info, Sending UpdateLDB Request...");
//  cout << chLogBuff << endl;
}

int16_t SendMsgDnldReq(uint32_t iUserID, uint16_t iStreamId, uint64_t llExchJiffy, char* pchBuffer, int32_t iSockId)
{
    NSEFO_MS_MESSAGE_DOWNLOAD_REQ oMsgDownloadReq;

    memset(&oMsgDownloadReq.tap_hdr.sLength, ' ', sLen_NSEFO_MsgDownload_Req);	

    oMsgDownloadReq.msg_hdr.TransactionCode = __bswap_16(NSEFO_DOWNLOAD_REQUEST);
    oMsgDownloadReq.msg_hdr.LogTime         = 0;
    oMsgDownloadReq.msg_hdr.AlphaChar[0]	= ASCII_OFFSET_FOR_NUMBERS + iStreamId;
    //oMsgDownloadReq.msg_hdr.AlphaChar[0]    = iStreamId;

    //oMsgDownloadReq.msg_hdr.AlphaChar[1]	= 0;
    oMsgDownloadReq.msg_hdr.TraderId        = __bswap_32(iUserID);
    oMsgDownloadReq.msg_hdr.ErrorCode       = 0;
    //	oMsgDownloadReq.msg_hdr.Timestamp   //Initialized to space
    //	oMsgDownloadReq.msg_hdr.TimeStamp1	//Initialized to 0
    //	oMsgDownloadReq.msg_hdr.TimeStamp2	//Initialized to space
    oMsgDownloadReq.msg_hdr.MessageLength   = __bswap_16(sLen_NSEFO_MsgDownload_Req - sLen_NSEFO_Tap_Hdr);

//    double dExchSeqNo = 0;
//    SwapDouble((char*)&dExchSeqNo);
//    oMsgDownloadReq.SeqNo = dExchSeqNo;

    memcpy(&oMsgDownloadReq.SeqNo, &llExchJiffy, 8);

    oMsgDownloadReq.tap_hdr.iSeqNo         = __bswap_32(++iExchSendSeqNo);
    cout << "@@@@" << iExchSendSeqNo << endl;
    oMsgDownloadReq.tap_hdr.sLength         = __bswap_16(sLen_NSEFO_MsgDownload_Req);
    md5_ful27((char*)&oMsgDownloadReq.msg_hdr.TransactionCode, sLen_NSEFO_MsgDownload_Req-sLen_NSEFO_Tap_Hdr, oMsgDownloadReq.tap_hdr.CheckSum);

  sprintf(chLogBuff, "[REQ] STEP 3: Sending MsgDnldReq to Exchange|Strm %d|ESeq %ld", iStreamId, llExchJiffy);
  cout << chLogBuff << endl;
    
    
    return SendOrderToExchange((char*)&oMsgDownloadReq.tap_hdr.sLength, iDataLen, iSockId);
}


void ProcessExchResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_CUSTOM_HEADER*  pCustHdr = NULL;
	NSEFO_CUSTOM_HEADER   oCustHdr;
  short SEGMENT=2;
  pCustHdr = (NSEFO_CUSTOM_HEADER*)pchBuffer;
  oCustHdr.sLength = __bswap_16(pCustHdr->sLength);
  
  //oCustHdr.iSeqNo	  = __bswap_32(pCustHdr->iSeqNo);
  //oCustHdr.sTransCode = __bswap_16(pCustHdr->sTransCode);
  
  
	if(iBytesToBeProcessed < oCustHdr.sLength)
	{
    sprintf(chLogBuff, "[SYS] |iBytesToBeProcessed %d|oCustHdr.sLength %d|", iBytesToBeProcessed, oCustHdr.sLength);
    cout << chLogBuff << endl;
    return;
	}


  oCustHdr.iSeqNo	  = __bswap_32(pCustHdr->iSeqNo);
  oCustHdr.sTransCode = __bswap_16(pCustHdr->sTransCode);
  sprintf(chLogBuff, "|TxnCode %d|Len %d|SeqNo %d|ProcessExchResp|iBytesToBeProcessed %d|", oCustHdr.sTransCode, oCustHdr.sLength, oCustHdr.iSeqNo, iBytesToBeProcessed);
  cout << chLogBuff << endl;
  
  switch(oCustHdr.sTransCode)
  {
    case(NSEFO_SIGN_ON_REQUEST_OUT):	//2301
    {
      ProcessLogonResp(pchBuffer);
    }
    break;
    
    case(NSEFO_GR_RESPONSE):
    {
      ProcessGRResp(pchBuffer);
    }
    break;
//    case(NSEFO_SYSTEM_INFORMATION_OUT):	//1601
//    {
//      ProcessSysInfoResp(pchBuffer, iDummy);
//    }
//    break;
    
    case(NSEFO_TWOL_ORDER_CONFIRMATION):      //2125
    case(NSEFO_TWOL_ORDER_ERROR):             //2155		
    {
      Process2LAddResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_TWOL_ORDER_CXL_CONFIRMATION):	//2131				
    {
      Process2LCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break;

    case(NSEFO_THRL_ORDER_CONFIRMATION):      //2126
    case(NSEFO_THRL_ORDER_ERROR):             //2156		
    {
      Process3LAddResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_THRL_ORDER_CXL_CONFIRMATION):	//2132				
    {
      Process3LCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    case(NSEFO_MOD_CNF_TR):
    case(NSEFO_ADD_CNF_TR):	//20073				
    {
      if(SEGMENT==1)
      {
        PorcessNSECMAddResp(pchBuffer, iBytesToBeProcessed);
      }
      else
      {
        PorcessNSEFOAddResp(pchBuffer, iBytesToBeProcessed);
      }
    }
    break;
    
    case(NSEFO_CAN_CNF_TR):	//20075				
    {
      PorcessNSEFOCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break; 
    
    case(NSEFO_TRD_CNF_TR):	//20222				
    {
      if(SEGMENT==1)
      {
       
        ProcessTradeCMResp_Trimmed(pchBuffer, iBytesToBeProcessed);
      }
      else
      {
        ProcessTradeResp_Trimmed(pchBuffer, iBytesToBeProcessed);
      }
    }
    break; 
    
    case(NSEFO_SL_TRIGGER):
    {
      if(SEGMENT == 1)
      {
      ProcessExchSLTrigResp_NonTrim(pchBuffer, iBytesToBeProcessed);
      }
      else
      {
//        ProcessExchAddResp_NonTrim(pchBuffer, iBytesToBeProcessed);
      }
    }
    break;
    case(NSEFO_ADD_REJ):
    case(NSEFO_CAN_CNF):
    case(NSEFO_MOD_CNF):
    case(NSEFO_ADD_CNF):
    {
      if(SEGMENT == 1)
      {
      ProcessExchAddModCanCMResp_NonTrim(pchBuffer, iBytesToBeProcessed);
      }
      else
      {
        ProcessExchAddResp_NonTrim(pchBuffer, iBytesToBeProcessed);
      }
    }
    break;
    
//    case(NSEFO_ADD_REJ):	//2231
//    case(NSEFO_MOD_REJ):	//2042			
    case(NSEFO_CAN_REJ):	//2072				
    {
      ProcessNSEFOErrorResp(pchBuffer, iBytesToBeProcessed);
    }
    break; 
    
    
    
    case(NSEFO_DOWNLOAD_HEADER):	//7011
    {
      NSEFO_MS_MESSAGE_DOWNLOAD_HEADER oMsgDnldHdr;

      oMsgDnldHdr.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);

      sprintf(chLogBuff,"[RSP] |Txn %d|Err %d|Strm %d", oMsgDnldHdr.msg_hdr.TransactionCode, oMsgDnldHdr.msg_hdr.ErrorCode, oMsgDnldHdr.msg_hdr.AlphaChar[0]);
      cout << chLogBuff << endl;
    }
    break;

    case(NSEFO_DOWNLOAD_DATA):	//7021
    {
      NSEFO_MS_MESSAGE_DOWNLOAD_DATA oMsgDnldData;

      oMsgDnldData.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);

      sprintf(chLogBuff,"[RSP] |Txn %d|Err %d|InnerTxn %d|Strm %d", oMsgDnldData.msg_hdr.TransactionCode, oMsgDnldData.msg_hdr.ErrorCode, oMsgDnldData.inner_hdr.TransactionCode, oMsgDnldData.msg_hdr.AlphaChar[0]);
      cout << chLogBuff << endl;
      
      ProcessMsgDnld(pchBuffer, iBytesToBeProcessed);

//				if(bFullRecovery == false)
//        {
//          bPushToRespQue = ProcessMsgDnld();
//        }
//        else
//        {
//          bPushToRespQue = ProcessMsgDnld_FullRecovery();
//        }
    }
    break;

    case(NSEFO_DOWNLOAD_TRAILER):	//7031
    {
      NSEFO_MS_MESSAGE_DOWNLOAD_TRAILER oMsgDnldTrailer;

      oMsgDnldTrailer.Initialize(pchBuffer + sLen_NSEFO_Tap_Hdr);

      sprintf(chLogBuff,"[RSP] |Txn%d|Err %d|Strm %d|", oMsgDnldTrailer.msg_hdr.TransactionCode, oMsgDnldTrailer.msg_hdr.ErrorCode, oMsgDnldTrailer.msg_hdr.AlphaChar[0]);
      cout << chLogBuff << endl;

      sprintf(chLogBuff,"[RSP] Message Downloaded completed for |Strm %d|", oMsgDnldTrailer.msg_hdr.AlphaChar[0]);
      cout << chLogBuff << endl;
    }
    break;
      
    default:
    {
      sprintf(chLogBuff, "|UnHandled TxnCode %d|ProcessExchResp|", oCustHdr.sTransCode);
      cout << chLogBuff << endl;
    }
  }
  
//  memcpy(pchBuffer, pchBuffer+oCustHdr.sLength, iBytesToBeProcessed - oCustHdr.sLength);
  memmove(pchBuffer, pchBuffer+oCustHdr.sLength, iBytesToBeProcessed - oCustHdr.sLength);
  iBytesToBeProcessed -= oCustHdr.sLength;
  
  sprintf(chLogBuff, "|RemBytes %d|ProcessExchResp", iBytesToBeProcessed);
  cout << chLogBuff << endl;
  
  
}

void Send2LReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF)
{
    NSEFO_MS_SPD_OE_REQUEST      oExchSpdAddModCanReq(iUserId, strTMID, sBranchId, 1);
    oExchSpdAddModCanReq.msg_hdr.TransactionCode                = __bswap_16(NSEFO_TWOL_BOARD_LOT_IN);
    oExchSpdAddModCanReq.filler                                 = 1;
    memset(oExchSpdAddModCanReq.AccountNUmber1, ' ', 10);
    memcpy(oExchSpdAddModCanReq.AccountNUmber1, "AAB", 3);

    //Leg 1
    oExchSpdAddModCanReq.Token1                                 = __bswap_32(44865);
    oExchSpdAddModCanReq.SecurityInformation1.ExpiryDate        = __bswap_32(1175351400);
    memcpy(oExchSpdAddModCanReq.SecurityInformation1.InstrumentName, "FUTIDX", 6);
    memcpy(oExchSpdAddModCanReq.SecurityInformation1.OptionType, "XX", 2);
    oExchSpdAddModCanReq.SecurityInformation1.StrikePrice	= __bswap_32(-1);
    memcpy(oExchSpdAddModCanReq.SecurityInformation1.Symbol, "BANKNIFTY", 9);
    oExchSpdAddModCanReq.BuySell1                               = __bswap_16(1);
    oExchSpdAddModCanReq.TotalVolRemaining1                     = __bswap_32(400);
    oExchSpdAddModCanReq.Volume1                                = __bswap_32(400);
    oExchSpdAddModCanReq.Price1                                 = __bswap_32(2000000);
    oExchSpdAddModCanReq.OrderFlags1.IOC                        = 1;	


    //Leg 2
    oExchSpdAddModCanReq.leg2.Token2                            = __bswap_32(36872);
    oExchSpdAddModCanReq.leg2.SecurityInformation2.ExpiryDate   = __bswap_32(1177770600);
    memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.InstrumentName,  "FUTIDX", 6);
    memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.OptionType,  "XX", 2);
    oExchSpdAddModCanReq.leg2.SecurityInformation2.StrikePrice	= __bswap_32(-1);
    memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.Symbol, "BANKNIFTY", 9);
    oExchSpdAddModCanReq.leg2.BuySell2                          = __bswap_16(1);
    oExchSpdAddModCanReq.leg2.TotalVolRemaining2                = __bswap_32(400);
    oExchSpdAddModCanReq.leg2.Volume2                           = __bswap_32(400); 
    oExchSpdAddModCanReq.leg2.Price2                            = __bswap_32(2000000); 
    oExchSpdAddModCanReq.leg2.OrderFlags2.IOC                   = 1;	
    

    memset(oExchSpdAddModCanReq.Settlor1, ' ', 12);
    memcpy(oExchSpdAddModCanReq.Settlor1, strTMID.c_str(), strTMID.length());
    
    oExchSpdAddModCanReq.NnfField                             = llNNF;
    //oExchSpdAddModCanReq.NnfField                             = 400098001001000;	//HARDCODED
    //oExchSpdAddModCanReq.NnfField                               = NNF_Fields[oReq._header._lUser]; //400098001001000;	//HARDCODED	
    SwapDouble((char*)&oExchSpdAddModCanReq.NnfField);

    oExchSpdAddModCanReq.tap_hdr.iSeqNo         = __bswap_32(++iExchSendSeqNo);
    cout << "@@@@" << iExchSendSeqNo << endl;
    
    md5_ful27((char*)&oExchSpdAddModCanReq.msg_hdr.TransactionCode, sLen_NSEFO_SpdML_Req-sLen_NSEFO_Tap_Hdr, oExchSpdAddModCanReq.tap_hdr.CheckSum);
    
    SendOrderToExchange((char*)&oExchSpdAddModCanReq.tap_hdr.sLength, sLen_NSEFO_SpdML_Req, iSockId);
} 


void Process2LAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_SPD_OE_REQUEST* pExSpdMLData			= (NSEFO_MS_SPD_OE_REQUEST*)pchBuffer;
  double dEOrd = pExSpdMLData->OrderNumber1;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|Error %d|EOrd %0.0f|Token1 %d|Token2 %d|Process2LAddResp|",  __bswap_16(pExSpdMLData->msg_hdr.ErrorCode), dEOrd, __bswap_32(pExSpdMLData->Token1), __bswap_32(pExSpdMLData->leg2.Token2));
  cout << chLogBuff << endl;
}

void Process2LCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_SPD_OE_REQUEST* pExSpdMLData			= (NSEFO_MS_SPD_OE_REQUEST*)pchBuffer;
  double dEOrd = pExSpdMLData->OrderNumber1;
  SwapDouble((char*)&dEOrd);

  sprintf(chLogBuff, "|Error %d|EOrd %0.0f|Token1 %d|Token2 %d|Process2LCanResp|",  __bswap_16(pExSpdMLData->msg_hdr.ErrorCode), dEOrd, __bswap_32(pExSpdMLData->Token1), __bswap_32(pExSpdMLData->leg2.Token2));
  cout << chLogBuff << endl;
}

int flag =0;
map<int,CONTRACTINFO>::iterator ContractItr;
void Send3LReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF,std::map<int, CONTRACTINFO> &ContractInfoMap,int *tokenStore,int &tokenIdx)
{
    NSEFO_MS_SPD_OE_REQUEST      oExchSpdAddModCanReq(iUserId, strTMID, sBranchId, 1);
    oExchSpdAddModCanReq.msg_hdr.TransactionCode                = __bswap_16(NSEFO_THRL_BOARD_LOT_IN);
    oExchSpdAddModCanReq.filler                                 = 1;
    memset(oExchSpdAddModCanReq.AccountNUmber1, ' ', 10);
    memcpy(oExchSpdAddModCanReq.AccountNUmber1, "AAB", 3);
    
//    for(int i=tokenIdx;i<3;i++)
//    {
//      std::cout<<"Tokens::"<<tokenStore[i]<<std::endl;
//    }
    //Leg 1
    ContractItr = ContractInfoMap.find(tokenStore[tokenIdx]);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchSpdAddModCanReq.Token1                                 = __bswap_32(tokenStore[tokenIdx]);
      oExchSpdAddModCanReq.SecurityInformation1.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchSpdAddModCanReq.SecurityInformation1.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchSpdAddModCanReq.SecurityInformation1.OptionType, ContractItr->second.OptionType, 2);
      oExchSpdAddModCanReq.SecurityInformation1.StrikePrice	= __bswap_32(-1);
      memcpy(oExchSpdAddModCanReq.SecurityInformation1.Symbol, ContractItr->second.Symbol, 9);
      oExchSpdAddModCanReq.BuySell1                               = __bswap_16(1);
      oExchSpdAddModCanReq.TotalVolRemaining1                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchSpdAddModCanReq.Volume1                                = oExchSpdAddModCanReq.TotalVolRemaining1;
      if(flag==0)
      {
        oExchSpdAddModCanReq.Price1                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=1;
      }
      else
      {
        oExchSpdAddModCanReq.Price1                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=0;
      }
      oExchSpdAddModCanReq.OrderFlags1.IOC = 1;
    }
//    std::cout<<"Price1::"<<tokenStore[tokenIdx]<<"::"<<((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2)<<std::endl;
    tokenIdx++;


    //Leg 2
    ContractItr = ContractInfoMap.find(tokenStore[tokenIdx]);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchSpdAddModCanReq.leg2.Token2                                 = __bswap_32(tokenStore[tokenIdx]);
      oExchSpdAddModCanReq.leg2.SecurityInformation2.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.OptionType, ContractItr->second.OptionType, 2);
      oExchSpdAddModCanReq.leg2.SecurityInformation2.StrikePrice	= __bswap_32(-1);
      memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.Symbol, ContractItr->second.Symbol, 9);
      oExchSpdAddModCanReq.leg2.BuySell2                               = __bswap_16(1);
      oExchSpdAddModCanReq.leg2.TotalVolRemaining2                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchSpdAddModCanReq.leg2.Volume2                                = oExchSpdAddModCanReq.leg2.TotalVolRemaining2;
      if(flag==0)
      {
        oExchSpdAddModCanReq.leg2.Price2                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=1;
      }
      else
      {
        oExchSpdAddModCanReq.leg2.Price2                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=0;
      }
      oExchSpdAddModCanReq.leg2.OrderFlags2.IOC = 1;
    }
//    std::cout<<"Price2::"<<tokenStore[tokenIdx]<<"::"<<((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2)<<std::endl;
    tokenIdx++;
    

    //Leg 3
    ContractItr = ContractInfoMap.find(tokenStore[tokenIdx]);
//    std::cout<<ContractItr->second.ExpiryDate<<std::endl;
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchSpdAddModCanReq.leg3.Token3                                 = __bswap_32(tokenStore[tokenIdx]);
      oExchSpdAddModCanReq.leg3.SecurityInformation3.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchSpdAddModCanReq.leg2.SecurityInformation2.OptionType, ContractItr->second.OptionType, 2);
      oExchSpdAddModCanReq.leg3.SecurityInformation3.StrikePrice	= __bswap_32(-1);
      memcpy(oExchSpdAddModCanReq.leg3.SecurityInformation3.Symbol, ContractItr->second.Symbol, 9);
      oExchSpdAddModCanReq.leg3.BuySell3                               = __bswap_16(1);
      oExchSpdAddModCanReq.leg3.TotalVolRemaining3                      = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchSpdAddModCanReq.leg3.Volume3                                = oExchSpdAddModCanReq.leg3.TotalVolRemaining3;
      if(flag==0)
      {
        oExchSpdAddModCanReq.leg3.Price3                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=1;
      }
      else
      {
        oExchSpdAddModCanReq.leg3.Price3                                 = __bswap_32((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2);
        flag=0;
      }
      oExchSpdAddModCanReq.leg3.OrderFlags3.IOC = 1;
//      std::cout<<"Token::"<< __bswap_32(oExchSpdAddModCanReq.leg3.Token3)<<"Expiry::"<< __bswap_32(oExchSpdAddModCanReq.leg3.SecurityInformation3.ExpiryDate)<<
//        "Instument Name::"<< oExchSpdAddModCanReq.leg2.SecurityInformation2.InstrumentName<<"OptionType::"<<oExchSpdAddModCanReq.leg2.SecurityInformation2.OptionType
//        <<"Symbol::"<<oExchSpdAddModCanReq.leg3.SecurityInformation3.Symbol <<"BuySell1::"<< __bswap_16(oExchSpdAddModCanReq.leg3.BuySell3)  <<
//        "Volume::"<< __bswap_32(oExchSpdAddModCanReq.leg3.TotalVolRemaining3)<<"Price::"<<__bswap_32(oExchSpdAddModCanReq.leg3.Price3) <<std::endl;
    }
//      std::cout<<"Price3::"<<tokenStore[tokenIdx]<<"::"<<((ContractItr->second.HighDPR + ContractItr->second.LowDPR)/2)<<std::endl;
    tokenIdx++;
   

    memset(oExchSpdAddModCanReq.Settlor1, ' ', 12);
    memcpy(oExchSpdAddModCanReq.Settlor1, strTMID.c_str(), strTMID.length());
    
    oExchSpdAddModCanReq.NnfField                             = llNNF;
    //oExchSpdAddModCanReq.NnfField                             = 400098001001000;	//HARDCODED
    //oExchSpdAddModCanReq.NnfField                               = NNF_Fields[oReq._header._lUser]; //400098001001000;	//HARDCODED	
    SwapDouble((char*)&oExchSpdAddModCanReq.NnfField);

    oExchSpdAddModCanReq.tap_hdr.iSeqNo         = __bswap_32(++iExchSendSeqNo);
    cout << "@@@@" << iExchSendSeqNo << endl;
    
//    md5_ful27((char*)&oExchSpdAddModCanReq.msg_hdr.TransactionCode, sLen_NSEFO_SpdML_Req-sLen_NSEFO_Tap_Hdr, oExchSpdAddModCanReq.tap_hdr.CheckSum);
    ComputeChecksum((char*)&oExchSpdAddModCanReq);
    SendOrderToExchange((char*)&oExchSpdAddModCanReq.tap_hdr.sLength, sLen_NSEFO_SpdML_Req, iSockId);
} 


void Process3LAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_SPD_OE_REQUEST* pExSpdMLData			= (NSEFO_MS_SPD_OE_REQUEST*)pchBuffer;

  sprintf(chLogBuff, "|Error %d|Process3LAddResp|",  __bswap_16(pExSpdMLData->msg_hdr.ErrorCode));
  cout << chLogBuff << endl;
}

void Process3LCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_SPD_OE_REQUEST* pExSpdMLData			= (NSEFO_MS_SPD_OE_REQUEST*)pchBuffer;

  sprintf(chLogBuff, "|Error %d|Token1 %d|Token2 %d|Token3 %d|Process3LCanResp|",  __bswap_16(pExSpdMLData->msg_hdr.ErrorCode), __bswap_32(pExSpdMLData->Token1), __bswap_32(pExSpdMLData->leg2.Token2), __bswap_32(pExSpdMLData->leg3.Token3));
  cout << chLogBuff << endl;
}
  
void ProcessTradeResp_Trimmed(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_TRADE_CONFIRMATION_TR* pExTrdData = (NSEFO_TRADE_CONFIRMATION_TR*)(pchBuffer + sLen_NSEFO_Tap_Hdr);
  double dEOrd = pExTrdData->ResponseOrderNumber;
  SwapDouble((char*)&dEOrd);
  
  sprintf(chLogBuff, "|TRD_RSP|EOrd %0.0f|ETrd %d|Price %d|Qty %d|Token %d|ProcessTradeResp_Trimmed|", dEOrd, __bswap_32(pExTrdData->FillNumber), __bswap_32(pExTrdData->FillPrice), __bswap_32(pExTrdData->FillQuantity), __bswap_32(pExTrdData->Token));
  cout << chLogBuff << endl;
}

void ProcessTradeCMResp_Trimmed(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  TRADE_CONFIRMATION_TR* pExTrdData = (TRADE_CONFIRMATION_TR*)(pchBuffer + sLen_NSEFO_Tap_Hdr);
  double dEOrd = pExTrdData->ResponseOrderNumber;
  SwapDouble((char*)&dEOrd);
//std::cout<<__bswap_16(pExTrdData->TransactionCode)<<"|"<< pExTrdData->FillNumber<<"|"<< __bswap_32(pExTrdData->FillPrice)<<"|"<< dEOrd<<"|"<<  __bswap_32(pExTrdData->FillQuantity)<<std::endl;
  sprintf(chLogBuff, "|TRD_RSP|EOrd %0.0f|ETrd %d|Price %d|Qty %dProcessTradeResp_Trimmed|", dEOrd, __bswap_32(pExTrdData->FillNumber), __bswap_32(pExTrdData->FillPrice), __bswap_32(pExTrdData->FillQuantity));
  cout << chLogBuff << endl;
}


unordered_map <int64_t,int32_t> StoreLastModified;
unordered_map <int64_t,int32_t>::iterator iterLM;
inline void ProcessExchAddResp_NonTrim(char *pchCurrBuffPosn,int32_t& iBytesToBeProcessed)
	{
  
		NSEFO_MS_OE_RESPONSE * pExData = (NSEFO_MS_OE_RESPONSE*)(pchCurrBuffPosn + sLen_NSEFO_Tap_Hdr );
		double dEOrd = pExData->OrderNumber;
    SwapDouble((char*)&dEOrd);
    std::cout<<__bswap_16(pExData->msg_hdr.TransactionCode)<<"|"<< __bswap_16(pExData->msg_hdr.ErrorCode)<<"|"<<pExData->InsOrdId.TransactionId<<"|"<< dEOrd<<"|"<< __bswap_32(pExData->TokenNo)<<std::endl;
    sprintf(chLogBuff, "|TxnCode %d|Error %d|IOrd %d|EOrd %0.0f|Token %d|ProcessNSEFOErrorResp|",  __bswap_16(pExData->msg_hdr.TransactionCode), __bswap_16(pExData->msg_hdr.ErrorCode), pExData->InsOrdId.TransactionId, dEOrd, __bswap_32(pExData->TokenNo));
    cout << chLogBuff << endl;
		
    StoreLastModified.insert(make_pair(dEOrd,pExData->LastModified));
    
	}
  
inline void ProcessExchSLTrigResp_NonTrim(char *pchCurrBuffPosn,int32_t& iBytesToBeProcessed)
	{
  
		MS_TRADE_CONFIRM * pExData = (MS_TRADE_CONFIRM*)(pchCurrBuffPosn + sLen_NSEFO_Tap_Hdr );
		double dEOrd = pExData->ResponseOrderNumber;
    SwapDouble((char*)&dEOrd);
//    std::cout<<__bswap_16(pExData->msg_hdr.TransactionCode)<<"|"<< __bswap_16(pExData->msg_hdr.ErrorCode)<<"|"<<pExData->TransactionId<<"|"<< dEOrd<<"|"<< __bswap_32(pExData->Price)<<std::endl;
    sprintf(chLogBuff, "|TxnCode %d|Error %d|EOrd %0.0f|Price %d|LastModified %d|ProcessNSEFOErrorResp|",  __bswap_16(pExData->msg_hdr.TransactionCode), __bswap_16(pExData->msg_hdr.ErrorCode), dEOrd, __bswap_32(pExData->Price),__bswap_32(pExData->ActivityTime));
    cout << chLogBuff << endl;
		
    StoreLastModified.insert(make_pair(dEOrd,__bswap_32(pExData->ActivityTime)));
    iterLM = StoreLastModified.find(dEOrd);
    
	}

inline void ProcessExchAddModCanCMResp_NonTrim(char *pchCurrBuffPosn,int32_t& iBytesToBeProcessed)
	{
  
		_MS_OE_RESPONSE * pExData = (_MS_OE_RESPONSE*)(pchCurrBuffPosn + sLen_NSEFO_Tap_Hdr );
		double dEOrd = pExData->OrderNumber;
    SwapDouble((char*)&dEOrd);
//    std::cout<<__bswap_16(pExData->msg_hdr.TransactionCode)<<"|"<< __bswap_16(pExData->msg_hdr.ErrorCode)<<"|"<<pExData->TransactionId<<"|"<< dEOrd<<"|"<< __bswap_32(pExData->Price)<<std::endl;
    sprintf(chLogBuff, "|TxnCode %d|Error %d|IOrd %d|EOrd %0.0f|Price %d|LastModified %d|ProcessNSEFOErrorResp|",  __bswap_16(pExData->msg_hdr.TransactionCode), __bswap_16(pExData->msg_hdr.ErrorCode), pExData->TransactionId, dEOrd, __bswap_32(pExData->Price),__bswap_32(pExData->LastModified));
    cout << chLogBuff << endl;
		
    StoreLastModified.insert(make_pair(dEOrd,__bswap_32(pExData->LastModified)));
    iterLM = StoreLastModified.find(dEOrd);
    
	}
 

void ProcessTradeResp_NonTrimmed(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_TRADE_CONFIRM* pExTrdData = (NSEFO_MS_TRADE_CONFIRM*)(pchBuffer + sLen_NSEFO_Full_Hdr);
  double dEOrd = pExTrdData->ResponseOrderNumber;
  SwapDouble((char*)&dEOrd);
  
  sprintf(chLogBuff, "|TRD_RSP|EOrd %0.0f|ETrd %d|Price %d|Qty %d|Token %d|ProcessTradeResp_NonTrimmed|", dEOrd, __bswap_32(pExTrdData->FillNumber), __bswap_32(pExTrdData->FillPrice), __bswap_32(pExTrdData->FillQuantity), __bswap_32(pExTrdData->Token));
  cout << chLogBuff << endl;
}
void PortFolioFile_Box_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo_Box PFinfo[],int PfIndex)
{
  int32_t lnStrike1,lnStrike2,lnDiffInStrike,lnCall1Price,lnCall2Price,lnPut1Price,lnPut2Price;
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());
  
  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
  if(ContractItr != ContractInfoMap.end())
  {
    lnStrike1 = ContractItr->second.StrikePrice;
  }
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
  if(ContractItr != ContractInfoMap.end())
  {
    lnStrike2 = ContractItr->second.StrikePrice;
  }
  lnDiffInStrike = abs(lnStrike1 - lnStrike2);
  
   
  if(PFinfo[PfIndex].ConRev == 9)
  {
   
    lnCall2Price = (PFinfo[PfIndex].Spread + lnDiffInStrike)/4;
    lnCall1Price = 3*lnCall2Price ;
    lnPut1Price = lnCall1Price;
    lnPut2Price = lnCall2Price;
    
    /*===========================4444================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token4);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token4);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(2);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnPut2Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token4<<endl;
    }
    
     /*===========================1111================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token1);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(1);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      
      oExchAddReq.Price = __bswap_32(lnCall1Price);
      if(ContractItr->second.StrikePrice <= PFinfo[PfIndex].Spread)
      {
        std::cout<<"Spread should not be greater or equal to strike price"<<std::endl;
      }
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
    }
    
  /*===========================2222================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(1);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnPut1Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    }
    
     /*===========================3333================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(2);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnCall2Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
     
    /*==============================================================*/
    
  }
  else if(PFinfo[PfIndex].ConRev == 10)
  {
    
    
    lnCall2Price = (abs(PFinfo[PfIndex].Spread - lnDiffInStrike))/4;
    lnCall1Price = 3*lnCall2Price ;
    lnPut1Price = lnCall1Price;
    lnPut2Price = lnCall2Price;
    
    /*===========================4444================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token4);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token4);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(2);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnPut2Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d|Buy/Sell:%d|Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume),__bswap_16(oExchAddReq.BuySellIndicator), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      

      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token4<<endl;
    }
    
    /*===========================1111================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token1);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(1);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      
      oExchAddReq.Price = __bswap_32(lnCall1Price);
      if(ContractItr->second.StrikePrice <= PFinfo[PfIndex].Spread)
      {
        std::cout<<"Spread should not be greater or equal to strike price"<<std::endl;
      }
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d|Buy/Sell:%d|Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume),__bswap_16(oExchAddReq.BuySellIndicator), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      

      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
    }
    
  /*===========================2222================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(1);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnPut1Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d|Buy/Sell:%d|Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume),__bswap_16(oExchAddReq.BuySellIndicator), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;

      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    }
    
     /*===========================3333================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      oExchAddReq.BuySellIndicator                               = __bswap_16(2);
      oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
      
      oExchAddReq.Price = __bswap_32(lnCall2Price);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d|Buy/Sell:%d|Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume),__bswap_16(oExchAddReq.BuySellIndicator), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
      

    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
     
    /*==============================================================*/
    
  }
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}

void PortFolioFile_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
 
  if(PFinfo[PfIndex].ConRev == 1)
  {
    
    /*===========================3333================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    /*===========================2222================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
      exit(1);
    }
    
    /*===========================1111================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(-1);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token1);
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(PFinfo[PfIndex].Strike+PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      
      if(PFinfo[PfIndex].Strike <= PFinfo[PfIndex].Spread)
      {
        std::cout<<"Spread should not be greater or equal to strike price"<<std::endl;
        exit(1);
      }
      oExchAddReq.DisclosedVolume         = oExchAddReq.Volume;
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
      
      ComputeChecksum((char*)&oExchAddReq);
      
      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
      exit(1);
    }
    
  
    
     
    
    /*===========================3333================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    /*Token 2 P*/
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
      exit(1);
    }
    usleep(50000);
    /*===========================3333================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    /*Token 2 P*/
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      
      
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
      exit(1);
    }
    
    /*==============================================================*/
    
    
  }
  else if(PFinfo[PfIndex].ConRev == 2)
  {
    /*==============1==============*/
    
    /*==============3==============*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      ComputeChecksum((char*)&oExchAddReq);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    
    /*================================2====================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      ComputeChecksum((char*)&oExchAddReq);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    }
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token1);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(PFinfo[PfIndex].Strike - PFinfo[PfIndex].Spread);
      
      if(PFinfo[PfIndex].Strike <= PFinfo[PfIndex].Spread)
      {
        std::cout<<"Spread should not be greater or equal to strike price"<<std::endl;
        exit(1);
      }
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
      
      
//      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
//
//      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
//      
//      ComputeChecksum((char*)&oExchAddReq);
//      
//      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
//      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
//          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
//      std::cout<<logBuf<<std::endl;
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
    }
    
    
    /*==============3==============*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;

      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    /*================================2====================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
   
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    }
    
    usleep(50000);
    
    /*==============3==============*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
      oExchAddReq.BuySellIndicator                        = __bswap_16(1);
      oExchAddReq.Price                                   = __bswap_32(2*PFinfo[PfIndex].Spread);
      
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;

      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token3<<endl;
    }
    
    /*================================2====================================*/
    
    ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
    if(ContractItr != ContractInfoMap.end())
    {
      memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
      memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
      memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
      
      oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
      oExchAddReq.contract_desc_tr.StrikePrice            = __bswap_32(ContractItr->second.StrikePrice);
      oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
      oExchAddReq.DisclosedVolume                         = oExchAddReq.Volume;
      
      oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
      oExchAddReq.BuySellIndicator                        = __bswap_16(2);
      oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
      
      if(__bswap_32(oExchAddReq.Price) < 0)
      {
        std::cout<<"Price is negative::"<<__bswap_32(oExchAddReq.Price)<<".Check Spread"<<std::endl;
        exit(1);
      }
      oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

      oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
      
      ComputeChecksum((char*)&oExchAddReq);
      
      SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
      snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
          iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
      std::cout<<logBuf<<std::endl;
   
      
    }
    else
    {
      std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    }
    
  }
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}

void PortFolioFile_Box_SendNSEFOAddReq_Recovery(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo_Box PFinfo[],int PfIndex)
{
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
 
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token1);
  if(ContractItr != ContractInfoMap.end())
  {
    /*============================Sell Order======================*/


    oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token1);
    oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(ContractItr->second.ExpiryDate);
    memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(ContractItr->second.StrikePrice);
    memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
    oExchAddReq.BuySellIndicator                               = __bswap_16(2);
    oExchAddReq.Volume                     = __bswap_32(ContractItr->second.MinimumLotQuantity);

    oExchAddReq.Price = __bswap_32(ContractItr->second.StrikePrice);
    if(ContractItr->second.StrikePrice <= PFinfo[PfIndex].Spread)
    {
      std::cout<<"Spread should not be greater or equal to strike price"<<std::endl;
    }
    oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;


    /*============================BUY Order======================*/
    oExchAddReq.BuySellIndicator                               = __bswap_16(1);
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;


  }
  else
  {
    std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
  }
    
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}
void SingleLeg_SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  static int iCounter = 5;
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
 
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
  if(ContractItr != ContractInfoMap.end())
  {
    /*============================Buy Order======================*/
    static int iPrice = 2700000;
    memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
    
    oExchAddReq.contract_desc_tr.StrikePrice            =  __bswap_32(ContractItr->second.StrikePrice);
    oExchAddReq.Volume                                  = __bswap_32(100);
    oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
    
    oExchAddReq.BuySellIndicator                        = __bswap_16(1);
    oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
    if(iCounter)
    {
      oExchAddReq.Price                                   = __bswap_32(iPrice + iCounter*5);
      iCounter--;
    }
    else
    {
      oExchAddReq.Price                                   = __bswap_32(iPrice + iCounter*5);
      iCounter = 5;
    }
    if(PFinfo[PfIndex].Strike <= PFinfo[PfIndex].Spread)
    {
      std::cout<<"Spread("<<PFinfo[PfIndex].Spread<<") should not be greater or equal to strike price("<<PFinfo[PfIndex].Strike<<")"<<std::endl;
      exit(1);
    }
    oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    
    ComputeChecksum((char*)&oExchAddReq);
    
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;

  }
  else
  {
    std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    exit(1);
  }
    
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}



void SingleLeg_SendNSECDAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
 
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
  if(ContractItr != ContractInfoMap.end())
  {
    /*============================Buy Order======================*/

    memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
    
    oExchAddReq.contract_desc_tr.StrikePrice            =  __bswap_32(ContractItr->second.StrikePrice);
    oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity * ContractItr->second.Multiplier);
    oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
    
    oExchAddReq.BuySellIndicator                        = __bswap_16(1);
    oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
    oExchAddReq.Price                                   = __bswap_32(ContractItr->second.LowDPR);
    if(PFinfo[PfIndex].Strike <= PFinfo[PfIndex].Spread)
    {
      std::cout<<"Spread("<<PFinfo[PfIndex].Spread<<") should not be greater or equal to strike price("<<PFinfo[PfIndex].Strike<<")"<<std::endl;
      exit(1);
    }
    oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     
    
    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    
    ComputeChecksum((char*)&oExchAddReq);
    
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;

  }
  else
  {
    std::cout<<"Token not found::"<<PFinfo[PfIndex].Token2<<endl;
    exit(1);
  }
    
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}


void SingleLeg_SendNSECDModReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
  if(ContractItr != ContractInfoMap.end())
  {
    NSEFO_MS_OM_REQUEST_TR oExchModReq(iUserId, strTMID, sBranchId, 1, 1);

    memset(oExchModReq.AccountNumber, ' ', 10);
    memcpy(oExchModReq.AccountNumber, "AAK",3);
    oExchModReq.OrderFlags.IOC   = 0;
    oExchModReq.OrderFlags.Day   = 1;
    oExchModReq.OrderFlags.SL    = 0;
    oExchModReq.NnfField         = llNNF;
    SwapDouble((char*)&oExchModReq.NnfField);
    auto iter = mIntOrdIdExchOrdId.find(iExchSendSeqNo - 1);
    InfoStore loInfoS = iter->second;
    std::cout<<"first::"<<iter->first<<"|second::"<<loInfoS.ExchOrdId<<"|"<<__bswap_32(loInfoS.LMT)<<std::endl;
    if(iter != mIntOrdIdExchOrdId.end())
    {
      oExchModReq.OrderNumber       = loInfoS.ExchOrdId;
    }
    else
    {
      std::cout<<"Order not found"<<endl;
    }

    SwapDouble((char*)&oExchModReq.OrderNumber);
    memcpy(oExchModReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchModReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    memcpy(oExchModReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);


    oExchModReq.contract_desc_tr.StrikePrice            =  __bswap_32(ContractItr->second.StrikePrice);
    oExchModReq.Volume                                  = __bswap_32(2*ContractItr->second.MinimumLotQuantity * ContractItr->second.Multiplier);
    oExchModReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);

    oExchModReq.BuySellIndicator                        = __bswap_16(1);
    oExchModReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
    oExchModReq.Price                                   = __bswap_32(ContractItr->second.LowDPR);


    oExchModReq.TransactionCode                             = __bswap_16(NSECM_MOD_REQ_TR);


    oExchModReq.DisclosedVolume                             = oExchModReq.Volume;
    oExchModReq.InsOrdId.TransactionId                      = iter->first;     
    oExchModReq.tap_hdr.iSeqNo                              = __bswap_32(iExchSendSeqNo+1);
    oExchModReq.LastModified                                = loInfoS.LMT;

    SendOrderToExchange((char*)&oExchModReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchModReq.InsOrdId.TransactionId, __bswap_32(oExchModReq.Price), __bswap_32(oExchModReq.Volume), __bswap_32(oExchModReq.TokenNo));
    std::cout<<logBuf<<std::endl;
  }
  

}

void SingleLeg_SendNSECDCanReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token2);
  if(ContractItr != ContractInfoMap.end())
  {
    NSEFO_MS_OM_REQUEST_TR oExchModReq(iUserId, strTMID, sBranchId, 1, 1);

    memset(oExchModReq.AccountNumber, ' ', 10);
    memcpy(oExchModReq.AccountNumber, "AAK",3);
    oExchModReq.OrderFlags.IOC   = 0;
    oExchModReq.OrderFlags.Day   = 1;
    oExchModReq.OrderFlags.SL    = 0;
    oExchModReq.NnfField         = llNNF;
    SwapDouble((char*)&oExchModReq.NnfField);
    auto iter = mIntOrdIdExchOrdId.find(iExchSendSeqNo - 1);
    InfoStore loInfoS = iter->second;
    std::cout<<"first::"<<iter->first<<"|second::"<<loInfoS.ExchOrdId<<"|"<<__bswap_32(loInfoS.LMT)<<std::endl;
    if(iter != mIntOrdIdExchOrdId.end())
    {
      oExchModReq.OrderNumber       = loInfoS.ExchOrdId;
    }
    else
    {
      std::cout<<"Order not found"<<endl;
    }

    SwapDouble((char*)&oExchModReq.OrderNumber);
    memcpy(oExchModReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchModReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    memcpy(oExchModReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);


    oExchModReq.contract_desc_tr.StrikePrice            =  __bswap_32(ContractItr->second.StrikePrice);
    oExchModReq.Volume                                  = __bswap_32(2*ContractItr->second.MinimumLotQuantity * ContractItr->second.Multiplier);
    oExchModReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);

    oExchModReq.BuySellIndicator                        = __bswap_16(1);
    oExchModReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token2);
    oExchModReq.Price                                   = __bswap_32(ContractItr->second.LowDPR);


    oExchModReq.TransactionCode                             = __bswap_16(NSECM_MOD_REQ_TR);


    oExchModReq.DisclosedVolume                             = oExchModReq.Volume;
    oExchModReq.InsOrdId.TransactionId                      = iter->first;     
    oExchModReq.tap_hdr.iSeqNo                              = __bswap_32(iExchSendSeqNo+1);
    oExchModReq.LastModified                                = loInfoS.LMT;

    SendOrderToExchange((char*)&oExchModReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchModReq.InsOrdId.TransactionId, __bswap_32(oExchModReq.Price), __bswap_32(oExchModReq.Volume), __bswap_32(oExchModReq.TokenNo));
    std::cout<<logBuf<<std::endl;
  }
  

}


void PortFolioFile_SendNSEFOAddReq_Recovery(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, std::map<int, CONTRACTINFO> &ContractInfoMap,storePortFolioInfo PFinfo[],int PfIndex)
{
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);
  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);
 
  ContractItr = ContractInfoMap.find(PFinfo[PfIndex].Token3);
  if(ContractItr != ContractInfoMap.end())
  {
    /*============================Sell Order======================*/

    memcpy(oExchAddReq.contract_desc_tr.InstrumentName, ContractItr->second.InstumentName, 6);
    memcpy(oExchAddReq.contract_desc_tr.OptionType, ContractItr->second.OptionType, 2);
    memcpy(oExchAddReq.contract_desc_tr.Symbol, ContractItr->second.Symbol, 9);
    
    oExchAddReq.contract_desc_tr.StrikePrice            =  __bswap_32(ContractItr->second.StrikePrice);
    oExchAddReq.Volume                                  = __bswap_32(ContractItr->second.MinimumLotQuantity);
    oExchAddReq.contract_desc_tr.ExpiryDate             = __bswap_32(ContractItr->second.ExpiryDate);
    
    oExchAddReq.BuySellIndicator                        = __bswap_16(2);
    oExchAddReq.TokenNo                                 = __bswap_32(PFinfo[PfIndex].Token3);
    oExchAddReq.Price                                   = __bswap_32(4*PFinfo[PfIndex].Spread);
    if(PFinfo[PfIndex].Strike <= PFinfo[PfIndex].Spread)
    {
      std::cout<<"Spread("<<PFinfo[PfIndex].Spread<<") should not be greater or equal to strike price("<<PFinfo[PfIndex].Strike<<")"<<std::endl;
      exit(1);
    }
    oExchAddReq.DisclosedVolume  = oExchAddReq.Volume;
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    
    ComputeChecksum((char*)&oExchAddReq);
    
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;


    /*============================BUY Order======================*/
    oExchAddReq.BuySellIndicator        = __bswap_16(1);
    oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

    oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
    
    
    ComputeChecksum((char*)&oExchAddReq);
    
    SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
    snprintf(logBuf, 500, "Thread_ME|FD %d|ADD ORDER REQ|Order# %d|Price::%d|Volume::%d| Token %d", 
        iSockId, oExchAddReq.InsOrdId.TransactionId, __bswap_32(oExchAddReq.Price), __bswap_32(oExchAddReq.Volume), __bswap_32(oExchAddReq.TokenNo));
    std::cout<<logBuf<<std::endl;


  }
  else
  {
    std::cout<<"Token not found::"<<PFinfo[PfIndex].Token1<<endl;
    exit(1);
  }
    
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

}

void SendNSEFOAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice)
{
  auto start_time_Q = chrono::high_resolution_clock::now();
  NSEFO_MS_OE_REQUEST_TR   oExchAddReq(iUserId, strTMID, sBranchId, 1, 1);

  oExchAddReq.TokenNo = __bswap_32(iToken);
  oExchAddReq.contract_desc_tr.ExpiryDate        = __bswap_32(iExpiry);
  //memcpy(oExchAddReq.contract_desc_tr.InstrumentName, "FUTIDX", 6);
  memcpy(oExchAddReq.contract_desc_tr.InstrumentName, "OPTIDX", 6);
  memcpy(oExchAddReq.contract_desc_tr.OptionType, "XX", 2);
  oExchAddReq.contract_desc_tr.StrikePrice	= __bswap_32(-1);
  //memcpy(oExchAddReq.contract_desc_tr.Symbol, "BANKNIFTY", 9);
  memcpy(oExchAddReq.contract_desc_tr.Symbol, "NIFTY", 9);

  memset(oExchAddReq.AccountNumber, ' ', 10);
  memcpy(oExchAddReq.AccountNumber, "AAK",3);

  oExchAddReq.BuySellIndicator = __bswap_16(iBuySell);
  oExchAddReq.DisclosedVolume  = __bswap_32(0);
  oExchAddReq.Volume           = __bswap_32(75);
  //oExchAddReq.Volume           = __bswap_32(2500);
  //oExchAddReq.Price            = __bswap_32(2000000);
  oExchAddReq.Price            = __bswap_32(iPrice);
  //oExchAddReq.Price            = __bswap_32(27000);
  oExchAddReq.OrderFlags.IOC   = 0;
  oExchAddReq.OrderFlags.Day   = 1;
  oExchAddReq.OrderFlags.SL    = 0;
    
  memset(oExchAddReq.Settlor, ' ', 12);
  memcpy(oExchAddReq.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddReq.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddReq.NnfField);

  oExchAddReq.InsOrdId.TransactionId  = iExchSendSeqNo;     

  oExchAddReq.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  //cout << "@@@@" << iExchSendSeqNo << endl;

  md5_ful27((char*)&oExchAddReq.TransactionCode, sLen_NSEFO_Add_Req-sLen_NSEFO_Tap_Hdr, oExchAddReq.tap_hdr.CheckSum);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  SendOrderToExchange((char*)&oExchAddReq.tap_hdr.sLength, sLen_NSEFO_Add_Req, iSockId);
}


void SendNSECMAddReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  MS_OE_REQUEST_TR oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1);  
  
  oExchAddModCanReq_NonTrim.TransactionCode = __bswap_16(20000);
  

  memset(oExchAddModCanReq_NonTrim.sec_info.Series, ' ' , 2);
// oExchAddModCanReq_NonTrim.sec_info.Series[0]='E';
// oExchAddModCanReq_NonTrim.sec_info.Series[1]='Q';
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Series, "EQ",2);
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Symbol, "MINDTREE", 10);
  

        ////////////////////
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.TransactionId = iExchSendSeqNo;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
  
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(3000);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);

  oExchAddModCanReq_NonTrim.OrderFlags.IOC   = 0;
  oExchAddModCanReq_NonTrim.OrderFlags.Day   = 1;
  oExchAddModCanReq_NonTrim.OrderFlags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolume = __bswap_32(0);

  oExchAddModCanReq_NonTrim.OrderFlags.Modified = 0;
  oExchAddModCanReq_NonTrim.OrderFlags.Traded = 0;
  
//////////////////////

  std::cout<<"in"<<std::endl;
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sizeof(MS_OE_REQUEST_TR), iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}

/*NK*/

void SendNSECMAddReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken,int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  _MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1);  
  
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSECM_ADD_REQ);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';

  memset(oExchAddModCanReq_NonTrim.sec_info.Series, ' ' , 2);
// oExchAddModCanReq_NonTrim.sec_info.Series[0]='E';
// oExchAddModCanReq_NonTrim.sec_info.Series[1]='Q';
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Series, "EQ",2);
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Symbol, "ZEEL", 10);
  
  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[0] = oExchAddModCanReq_NonTrim.sec_info.Symbol[0];
  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[1] = oExchAddModCanReq_NonTrim.sec_info.Symbol[1];
     
        ////////////////////
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.TransactionId = iExchSendSeqNo;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
  
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
  
//////////////////////

 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSECM_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}

void SendNSECMModReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken,int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t OrderNumber,int32_t InternalOrderID)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  _MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1);

  
  iterLM = StoreLastModified.find(OrderNumber);
  
  if ( iterLM == StoreLastModified.end() )
  {
    cout << "Not found";
  }
  oExchAddModCanReq_NonTrim.LastModified = __bswap_32(iterLM->second);
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSECM_MOD_REQ);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';

  memset(oExchAddModCanReq_NonTrim.sec_info.Series, ' ' , 2);

  memcpy(oExchAddModCanReq_NonTrim.sec_info.Series, "EQ",2);
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Symbol, "ZEEL", 10);

  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[0] = oExchAddModCanReq_NonTrim.sec_info.Symbol[0];
  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[1] = oExchAddModCanReq_NonTrim.sec_info.Symbol[1];
     
        ////////////////////
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.TransactionId = InternalOrderID ;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
  
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
  
  oExchAddModCanReq_NonTrim.OrderNumber = OrderNumber;
  SwapDouble((char*) &oExchAddModCanReq_NonTrim.OrderNumber);
//////////////////////

 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSECM_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}

void SendNSECMCanReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken,int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t OrderNumber,int32_t InternalOrderID)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  _MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1);

  
  iterLM = StoreLastModified.find(OrderNumber);
  
  if ( iterLM == StoreLastModified.end() )
  {
    cout << "Not found";
  }
  oExchAddModCanReq_NonTrim.LastModified = __bswap_32(iterLM->second);
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSECM_CAN_REQ);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';

  memset(oExchAddModCanReq_NonTrim.sec_info.Series, ' ' , 2);

  memcpy(oExchAddModCanReq_NonTrim.sec_info.Series, "EQ",2);
  memcpy(oExchAddModCanReq_NonTrim.sec_info.Symbol, "ZEEL", 10);

  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[0] = oExchAddModCanReq_NonTrim.sec_info.Symbol[0];
  oExchAddModCanReq_NonTrim.msg_hdr.AlphaChar[1] = oExchAddModCanReq_NonTrim.sec_info.Symbol[1];
     
        ////////////////////
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.TransactionId = InternalOrderID ;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
  
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
  
  oExchAddModCanReq_NonTrim.OrderNumber = OrderNumber;
  SwapDouble((char*) &oExchAddModCanReq_NonTrim.OrderNumber);
//////////////////////

 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSECM_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}



void SendNSEFOAddReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  NSEFO_MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1, 1);  
  
  oExchAddModCanReq_NonTrim.TokenNo = __bswap_32(iToken);
  oExchAddModCanReq_NonTrim.contract_desc.ExpiryDate        = __bswap_32(iExpiry);
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSEFO_ADD_REQ);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.InstrumentName, "FUTIDX", 6);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.OptionType, "XX", 2);
  oExchAddModCanReq_NonTrim.contract_desc.StrikePrice	= __bswap_32(-1);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.Symbol, "DJIA", 4);
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.filler = iExchSendSeqNo;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
  
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSEFO_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}
void SendNSEFOModReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t OrderNumber,int32_t InternalOrderID)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  NSEFO_MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1, 1);  
  iterLM = StoreLastModified.find(OrderNumber);
  if ( iterLM == StoreLastModified.end() )
  {
    cout << "Not found";
  }
  oExchAddModCanReq_NonTrim.LastModified = iterLM->second;
  oExchAddModCanReq_NonTrim.TokenNo = __bswap_32(iToken);
  oExchAddModCanReq_NonTrim.contract_desc.ExpiryDate        = __bswap_32(iExpiry);
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSEFO_MOD_REQ);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.InstrumentName, "FUTIDX", 6);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.OptionType, "XX", 2);
  oExchAddModCanReq_NonTrim.contract_desc.StrikePrice	= __bswap_32(-1);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.Symbol, "DJIA", 4);
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.filler = InternalOrderID;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
   oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.OrderNumber = OrderNumber;
  SwapDouble((char*) &oExchAddModCanReq_NonTrim.OrderNumber);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSEFO_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}

void SendNSEFOCanReqSL_NonTrim(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, int32_t iToken, int32_t iExpiry, int16_t iBuySell, int32_t iPrice,int32_t iTriggerPrice,int64_t OrderNumber,int32_t InternalOrderID)
{
  auto start_time_Q = chrono::high_resolution_clock::now(); 
  NSEFO_MS_OE_REQUEST oExchAddModCanReq_NonTrim(iUserId, strTMID, sBranchId, 1, 1);  
  iterLM = StoreLastModified.find(OrderNumber);
  if ( iterLM == StoreLastModified.end() )
  {
    cout << "Not found";
  }
  oExchAddModCanReq_NonTrim.LastModified = iterLM->second;
  oExchAddModCanReq_NonTrim.TokenNo = __bswap_32(iToken);
  oExchAddModCanReq_NonTrim.contract_desc.ExpiryDate        = __bswap_32(iExpiry);
  oExchAddModCanReq_NonTrim.msg_hdr.TransactionCode = __bswap_16(NSEFO_CAN_REQ);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.InstrumentName, "FUTIDX", 6);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.OptionType, "XX", 2);
  oExchAddModCanReq_NonTrim.contract_desc.StrikePrice	= __bswap_32(-1);
  oExchAddModCanReq_NonTrim.ModifiedCancelledBy = 'T';
  oExchAddModCanReq_NonTrim.BookType = __bswap_16(3);
  memcpy(oExchAddModCanReq_NonTrim.contract_desc.Symbol, "DJIA", 4);
  
  memset(oExchAddModCanReq_NonTrim.AccountNumber, ' ', 10);
  memcpy(oExchAddModCanReq_NonTrim.AccountNumber, "AAK",3);
  oExchAddModCanReq_NonTrim.filler = InternalOrderID;
  oExchAddModCanReq_NonTrim.tap_hdr.iSeqNo          = __bswap_32(++iExchSendSeqNo);
  oExchAddModCanReq_NonTrim.BuySellIndicator = __bswap_16(iBuySell);
   oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.DisclosedVolume  = __bswap_32(0);
  oExchAddModCanReq_NonTrim.Volume           = __bswap_32(30);
  
  oExchAddModCanReq_NonTrim.OrderNumber = OrderNumber;
  SwapDouble((char*) &oExchAddModCanReq_NonTrim.OrderNumber);
  
  oExchAddModCanReq_NonTrim.Price            = __bswap_32(iPrice);
  oExchAddModCanReq_NonTrim.TriggerPrice = __bswap_32(iTriggerPrice);
  oExchAddModCanReq_NonTrim.st_order_flags.IOC   = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Day   = 1;
  oExchAddModCanReq_NonTrim.st_order_flags.SL    = 1;
    
  memset(oExchAddModCanReq_NonTrim.Settlor, ' ', 12);
  memcpy(oExchAddModCanReq_NonTrim.Settlor, strTMID.c_str(), strTMID.length());

  oExchAddModCanReq_NonTrim.NnfField         = llNNF;
  SwapDouble((char*)&oExchAddModCanReq_NonTrim.NnfField);

  
  oExchAddModCanReq_NonTrim.DisclosedVolumeRemaining = __bswap_32(0);
  oExchAddModCanReq_NonTrim.TotalVolumeRemaining = __bswap_32(30);
  oExchAddModCanReq_NonTrim.VolumeFilledToday = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Modified = 0;
  oExchAddModCanReq_NonTrim.st_order_flags.Traded = 0;
 
  SendOrderToExchange((char*)&oExchAddModCanReq_NonTrim.tap_hdr.sLength, sLen_NSEFO_AddModCan_Req_NonTrim, iSockId);
  auto end_time_Q = chrono::high_resolution_clock::now();
  cout << "####################ChronoTime Que " << chrono::duration_cast<chrono::nanoseconds>(end_time_Q - start_time_Q).count() << endl;

  
}
/*NK*/
void PorcessNSECMAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  MS_OE_RESPONSE_TR* pExData = (MS_OE_RESPONSE_TR*)(pchBuffer + sLen_NSEFO_Tap_Hdr);		
  
  double dEOrd = pExData->OrderNumber;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|Error %d|IOrd %d|EOrd %0.0f|PorcessNSEFOAddResp|",  __bswap_16(pExData->ErrorCode), pExData->TransactionId, dEOrd);
  cout << chLogBuff << endl;
}


void PorcessNSEFOAddResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  InfoStore loInfoSt;
  NSEFO_MS_OE_RESPONSE_TR* pExData = (NSEFO_MS_OE_RESPONSE_TR*)(pchBuffer + sLen_NSEFO_Tap_Hdr);		
  
  double dEOrd = pExData->OrderNumber;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|Error %d|IOrd %d|EOrd %0.0f|Token %d|LMT %d|PorcessNSEFOAddResp|",  __bswap_16(pExData->ErrorCode), pExData->InsOrdId.TransactionId, dEOrd, __bswap_32(pExData->TokenNo), __bswap_32(pExData->LastModified));
  cout << chLogBuff << endl;
  loInfoSt.ExchOrdId = dEOrd;
  loInfoSt.LMT       = pExData->LastModified;
  mIntOrdIdExchOrdId.insert((std::make_pair((int32_t)pExData->InsOrdId.TransactionId, loInfoSt)));
  
  
  auto iter = mIntOrdIdExchOrdId.find((int32_t)pExData->InsOrdId.TransactionId);
  
  if(iter != mIntOrdIdExchOrdId.end())
  {
//    oExchModReq.OrderNumber       = loInfoS.ExchOrdId;
    mIntOrdIdExchOrdId[(int32_t)pExData->InsOrdId.TransactionId] = loInfoSt;
  }
  else
  {
    mIntOrdIdExchOrdId.insert((std::make_pair((int32_t)pExData->InsOrdId.TransactionId, loInfoSt)));
//    std::cout<<"Order not found"<<endl;
  }
}

void SendNSEFOModReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, double dEOrd, int32_t iLMT, int32_t iTTQ)
{
}

void PorcessNSEFOModResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
}

void SendNSEFOCanReq(int32_t iSockId, int32_t iUserId, string strTMID, int16_t sBranchId, int64_t llNNF, double dEOrd, int32_t iLMT, int32_t iTTQ)
{
}

void PorcessNSEFOCanResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_OE_RESPONSE_TR* pExData = (NSEFO_MS_OE_RESPONSE_TR*)(pchBuffer + sLen_NSEFO_Tap_Hdr);		
  
  double dEOrd = pExData->OrderNumber;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|Error %d|IOrd %d|EOrd %0.0f|Token %d|PorcessNSEFOCanResp|",  __bswap_16(pExData->ErrorCode), pExData->InsOrdId.TransactionId, dEOrd, __bswap_32(pExData->TokenNo));
  cout << chLogBuff << endl;
}

void PorcessNSEFOCanResp_NonTrimmed(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_OE_RESPONSE* pExData = (NSEFO_MS_OE_RESPONSE*)(pchBuffer + sLen_NSEFO_Full_Hdr);		
  
  double dEOrd = pExData->OrderNumber;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|Error %d|IOrd %d|EOrd %0.0f|Token %d|PorcessNSEFOCanResp_NonTrimmed|",  __bswap_16(pExData->msg_hdr.ErrorCode), pExData->InsOrdId.TransactionId, dEOrd, __bswap_32(pExData->TokenNo));
  cout << chLogBuff << endl;
}

void ProcessNSEFOErrorResp(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
  NSEFO_MS_OE_RESPONSE* pExData = (NSEFO_MS_OE_RESPONSE*)(pchBuffer + sLen_NSEFO_Tap_Hdr);		
  
  double dEOrd = pExData->OrderNumber;
  SwapDouble((char*)&dEOrd);
  sprintf(chLogBuff, "|TxnCode %d|Error %d|IOrd %d|EOrd %0.0f|Token %d|ProcessNSEFOErrorResp|",  __bswap_16(pExData->msg_hdr.TransactionCode), __bswap_16(pExData->msg_hdr.ErrorCode), pExData->InsOrdId.TransactionId, dEOrd, __bswap_32(pExData->TokenNo));
  cout << chLogBuff << endl;
}

void ProcessMsgDnld(char* pchBuffer, int32_t& iBytesToBeProcessed)
{
	NSEFO_MS_MESSAGE_DOWNLOAD_DATA* pMsgDnldData = (NSEFO_MS_MESSAGE_DOWNLOAD_DATA*)(pchBuffer + sLen_NSEFO_Tap_Hdr);
	switch(__bswap_16(pMsgDnldData->inner_hdr.TransactionCode))
	{
    case(NSEFO_TWOL_ORDER_CONFIRMATION):		//2125
    {
      Process2LAddResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_THRL_ORDER_CONFIRMATION):		//2126				
    {
      Process3LCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_TWOL_ORDER_CXL_CONFIRMATION):	//2131				
    {
      Process2LCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_THRL_ORDER_CXL_CONFIRMATION):	//2132				
    {
      Process3LCanResp(pchBuffer, iBytesToBeProcessed);
    }
    break;

    case(NSEFO_TRD_CNF):	//2222
    {
      ProcessTradeResp_NonTrimmed(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    case(NSEFO_CAN_CNF):	//2075
    {
      PorcessNSEFOCanResp_NonTrimmed(pchBuffer, iBytesToBeProcessed);
    }
    break;
    
    default:
    {
      sprintf(chLogBuff, "|Unhandled TxnCode %d|ProcessMsgDnld|", (__bswap_16(pMsgDnldData->inner_hdr.TransactionCode)));
      cout << chLogBuff << endl;
    }
  }
  
}