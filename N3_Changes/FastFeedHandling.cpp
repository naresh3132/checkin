#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include "FastFeedHandling.h"
#include "DealerStructure.h"

char logBufBrdcst[400];
bool gbPrintFFData = false;

FastFeedHandler::FastFeedHandler(int32_t size)
{
  m_iRecvDataSize   = size;
  m_pcRecvData       = new char[size];
  m_iByteRemaining  = 0;
}

inline void SwapDouble(char* pData)
{
  char TempData = 0;
  for(int32_t nStart = 0, nEnd = 7; nStart < nEnd; nStart++, nEnd--)
  {
    TempData = pData[nStart];
    pData[nStart] = pData[nEnd];
    pData[nEnd] = TempData;
  }
}

void HandleDepthRequest(DATA_RECEIVED *pRecvData, TokenIndexMap& mpTokenIndex, FFOrderBook oFFStore[], ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain)
{
  DATA_RECEIVED oSendData;
  DEALER::TOKEN_DEPTH_RESPONSE oTokDepRes;
  
  DEALER::TOKEN_DEPTH_REQUEST *pTokDepReq = (DEALER::TOKEN_DEPTH_REQUEST*)pRecvData->msgBuffer;
  
  memcpy(&oTokDepRes.header,&pTokDepReq->header, sizeof(DEALER::MSG_HEADER));
  
  oTokDepRes.header.iMsgLen       = sizeof(oTokDepRes);
  oTokDepRes.header.iTransCode    = DEPTH_RESPONSE;
  
  auto it = mpTokenIndex.find(pTokDepReq->iToken);
  if(it != mpTokenIndex.end())
  {
    int Index = it->second;
    
    memcpy(&oTokDepRes.oDepthInfo, &oFFStore[Index], sizeof(oTokDepRes.oDepthInfo));
    oTokDepRes.oDepthInfo.Token = pTokDepReq->iToken;
    sprintf(logBufBrdcst,"MULTICAST|TOKEN DEPTH|Token %d|LTP %d|Bid Price(Qty) %d(%d)|%d(%d)|%d(%d)|%d(%d)|%d(%d)",
        oTokDepRes.oDepthInfo.Token, oTokDepRes.oDepthInfo.LTP,
        oTokDepRes.oDepthInfo.oDep[0].BidPrice, oTokDepRes.oDepthInfo.oDep[0].BidQty,
        oTokDepRes.oDepthInfo.oDep[1].BidPrice, oTokDepRes.oDepthInfo.oDep[1].BidQty,
        oTokDepRes.oDepthInfo.oDep[2].BidPrice, oTokDepRes.oDepthInfo.oDep[2].BidQty,
        oTokDepRes.oDepthInfo.oDep[3].BidPrice, oTokDepRes.oDepthInfo.oDep[3].BidQty,
        oTokDepRes.oDepthInfo.oDep[4].BidPrice, oTokDepRes.oDepthInfo.oDep[4].BidQty);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
    
    sprintf(logBufBrdcst,"MULTICAST|TOKEN DEPTH|Token %d|LTP %d|Sell Price(Qty) %d(%d)|%d(%d)|%d(%d)|%d(%d)|%d(%d)",
        oTokDepRes.oDepthInfo.Token, oTokDepRes.oDepthInfo.LTP,
        oTokDepRes.oDepthInfo.oDep[0].AskPrice, oTokDepRes.oDepthInfo.oDep[0].AskQty,
        oTokDepRes.oDepthInfo.oDep[1].AskPrice, oTokDepRes.oDepthInfo.oDep[1].AskQty,
        oTokDepRes.oDepthInfo.oDep[2].AskPrice, oTokDepRes.oDepthInfo.oDep[2].AskQty,
        oTokDepRes.oDepthInfo.oDep[3].AskPrice, oTokDepRes.oDepthInfo.oDep[3].AskQty,
        oTokDepRes.oDepthInfo.oDep[4].AskPrice, oTokDepRes.oDepthInfo.oDep[4].AskQty);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
    
        
    oSendData.MyFd = pRecvData->MyFd;
    oSendData.ptrConnInfo = pRecvData->ptrConnInfo; 
    memcpy(oSendData.msgBuffer, (void*)&oTokDepRes, sizeof(oSendData.msgBuffer));
    Inqptr_FFToMain->enqueue(oSendData);
  }
  else
  {
    sprintf(logBufBrdcst,"MULTICAST|TOKEN DEPTH|Token %d not found",pTokDepReq->iToken);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
  }
}

int FastFeedHandler::CreateFFReciever( const char* pcIpaddress, int32_t nPortNumber, const char* pcIfaceIp)
{
  
  int Flag_On = 1;
  int iSockFd;
  struct sockaddr_in sMulticast_addr;
  struct ip_mreq sMc_req;
  socklen_t Addrlen;
  
  if ((iSockFd=socket(AF_INET,SOCK_DGRAM,0)) < 0)      
   {
      perror("socket");
      return 0;
   }
  
  if ((setsockopt(iSockFd, SOL_SOCKET, SO_REUSEADDR, &Flag_On,sizeof(Flag_On))) < 0)
  {
    perror("MULTICAST|Reuse Address failed");
    return 0;
  }
  
  memset(&sMulticast_addr,0,sizeof(sMulticast_addr));
  sMulticast_addr.sin_family        = AF_INET;
  sMulticast_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
  sMulticast_addr.sin_port          = htons(nPortNumber);
  Addrlen                           = sizeof(sMulticast_addr);


  if ((bind(iSockFd, (struct sockaddr *) &sMulticast_addr, Addrlen)) < 0)
  {
    perror("MULTICAST|bind failed");
    return 0;
  }

  memset(&sMc_req,0,sizeof(sMc_req));
  
  sMc_req.imr_multiaddr.s_addr      = inet_addr(pcIpaddress);
  sMc_req.imr_interface.s_addr      = inet_addr(pcIfaceIp);


  if ((setsockopt(iSockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &sMc_req, sizeof(sMc_req))) < 0)
  {
    perror("MULTICAST|Membership failed");
    return 0;
  }
  sprintf(logBufBrdcst,"MULTICAST|Receiver|Socket Created %d|IP %s|Port %d", iSockFd, pcIpaddress, nPortNumber );
  Logger::getLogger().log(DEBUG, logBufBrdcst);
  
  return iSockFd;
}

void UpdateFFOrderBook(int Index, FFOrderBook oFFStore[], ExchangeStructsFO::NNF_MBP_DATA& PriceData )
{

  oFFStore[Index].LTP   = __bswap_32(PriceData.lLTP);
  oFFStore[Index].Token = PriceData.lToken;
//  std::cout<<"LTP::"<<oFFStore[Index].LTP<<"|TOKEN "<<oFFStore[Index].Token<<std::endl;
  for(int k=0; k<5; k++)
  {
    oFFStore[Index].oDep[k].BidPrice = __bswap_32(PriceData.MBPRecord[k].lPrice);
    oFFStore[Index].oDep[k].BidQty   = __bswap_32(PriceData.MBPRecord[k].lQty);
    oFFStore[Index].oDep[k].AskPrice = __bswap_32(PriceData.MBPRecord[k+5].lPrice);
    oFFStore[Index].oDep[k].AskQty   = __bswap_32(PriceData.MBPRecord[k+5].lQty);
//    std::cout<<"Bid Price::"<<oFFStore[Index].oDep[k].BidPrice<<"|Bid Qty::"<<oFFStore[Index].oDep[k].BidQty<<std::endl;
//    std::cout<<"Ask Price::"<<oFFStore[Index].oDep[k].AskPrice<<"|Ask Qty::"<<oFFStore[Index].oDep[k].AskQty<<std::endl;
  }

}

int FastFeedHandler::ProcessDepth(ExchangeStructsFO::NNF_MBP_PACKET *pMBPData, TokenIndexMap& mpTokenIndex, FFOrderBook oFFStore[], int iServFFSockFd, struct sockaddr_in* groupSock)
{
  
  FFOrderBookTouchLine oFFTouchLine;  
  int nRet = 0;
  pMBPData->bcastHeader.lLogTime = (long32_t)__bswap_32((long32_t)pMBPData->bcastHeader.lLogTime);
  pMBPData->wNoOfRecords         = (short)__bswap_16((unsigned short)pMBPData->wNoOfRecords);
  
  nRet = pMBPData->wNoOfRecords;
  
  for(int n = 0; n < nRet; ++n)
  {
    
    if(pMBPData->PriceData[n].lToken == 0)
    {
      printf("Token is zero in depth packet.\n");
      continue;
    }
    pMBPData->PriceData[n].lToken = (long32_t)__bswap_32((long32_t)pMBPData->PriceData[n].lToken);
    
    auto it = mpTokenIndex.find(pMBPData->PriceData[n].lToken);
    if(it != mpTokenIndex.end())
    {
  
      UpdateFFOrderBook(it->second, oFFStore, pMBPData->PriceData[n]);   //Update Fast feed Order book
      int Index = it->second;
      
      //store touchline and send it to clients
      
      oFFTouchLine.LTP    = oFFStore[Index].LTP   ;
      oFFTouchLine.Token  = oFFStore[Index].Token ;
      
      oFFTouchLine.oBestBidAsk.AskPrice = oFFStore[Index].oDep[0].AskPrice  ;
      oFFTouchLine.oBestBidAsk.AskQty   = oFFStore[Index].oDep[0].AskQty    ;
      oFFTouchLine.oBestBidAsk.BidPrice = oFFStore[Index].oDep[0].BidPrice  ;
      oFFTouchLine.oBestBidAsk.BidQty   = oFFStore[Index].oDep[0].BidQty    ;

      if(sendto(iServFFSockFd,&oFFTouchLine, sizeof(oFFTouchLine), 0, (struct sockaddr*)groupSock, sizeof(struct sockaddr_in)) < 0)
      {
          perror("Sending datagram message error");
          std::cout << "Sending datagram message error" << std::endl;
      }
      else
      {
        if ( gbPrintFFData == true )  //Config parameter to print ff data or not
        {
          sprintf(logBufBrdcst,"MULTICAST|SERVER|Token %d|LTP %d|Ask Price %d|Ask Qty %d|Bid Price %d|Bid Qty %d", oFFTouchLine.Token,oFFTouchLine.LTP, oFFTouchLine.oBestBidAsk.AskPrice,
          oFFTouchLine.oBestBidAsk.AskQty, oFFTouchLine.oBestBidAsk.BidPrice, oFFTouchLine.oBestBidAsk.BidQty);
        
          Logger::getLogger().log(DEBUG, logBufBrdcst);
        }
      }

    }
    else
    {
//      sprintf(logBufBrdcst,"MULTICAST|SERVER|Token not found %d", pMBPData->PriceData[n].lToken);
//      Logger::getLogger().log(DEBUG, logBufBrdcst);
//      std::cout<<"Token not found::"<<pMBPData->PriceData[n].lToken<<std::endl;
    }
    
    
  }

  return nRet;
}


void FastFeedHandler::ProcessFFData( TokenIndexMap& mpTokenIndex, FFOrderBook oFFStore[] , int iServFFSockFd, struct sockaddr_in* groupSock)
{
  
  if(m_iByteRemaining > 0)
  {
    
    unsigned char strInBuff[2048], strOutBuff[2048];
    int nSize = 0;
    lzo_uint nInSize  = 0, nOutSize = 0 ;
    int nCompLen = 0;
    size_t szExchgPreface = 8;

    int nErrInfo;
    m_iByteRemaining = 0;
//    m_iByteRemaining -= sizeof(ExchangeStructsFO::BCAST_HEADER);
    ExchangeStructsFO::BCAST_HEADER *pBcastHeader = NULL;
    ExchangeStructsFO::BCAST_COMPRESSION *pCompPkt = NULL;
    ExchangeStructsFO::NNF_BCAST_HEADER *pPktHeader = NULL;

    pBcastHeader = (ExchangeStructsFO::BCAST_HEADER*)m_pcRecvData;
    pBcastHeader->wNoOfPackets = __bswap_16((unsigned short)pBcastHeader->wNoOfPackets);
    short wData = pBcastHeader->wNoOfPackets;
    
    nSize = 0;
    nCompLen = 0;

    for(int nPacketCount = 0; nPacketCount < wData; ++nPacketCount)
    {
      pPktHeader = NULL;
      pCompPkt = (ExchangeStructsFO::BCAST_COMPRESSION*)(pBcastHeader->cPackets + nSize);
      pCompPkt->wCompressionLen = __bswap_16((unsigned short)pCompPkt->wCompressionLen);
    
      
      if(pCompPkt->wCompressionLen == 0)
      {
          pPktHeader = (ExchangeStructsFO::NNF_BCAST_HEADER*)(pCompPkt->cBroadcastData + szExchgPreface);
          pPktHeader->wTransCode = __bswap_16((unsigned short)pPktHeader->wTransCode);
//          std::cout << "TxnCode " << pPktHeader->wTransCode << std::endl;

          pPktHeader->wMsgLen = __bswap_16((unsigned short)pPktHeader->wMsgLen);

          if(pPktHeader->wMsgLen > 512)
          {
            sprintf(logBufBrdcst,"Not an Exchange packet" );
            Logger::getLogger().log(DEBUG, logBufBrdcst);
            break;
          }
          // DPR Fix carried from IV {
          nSize += pPktHeader->wMsgLen  + szExchgPreface + sizeof(short);
          // } DPR Fix carried from IV 
          nCompLen = pPktHeader->wMsgLen + sizeof(short);
      }
      else
      {
        nCompLen = pCompPkt->wCompressionLen;

        memset(strInBuff,0,2048);
        memset(strOutBuff,0,2048);
        nInSize = pCompPkt->wCompressionLen;
        nOutSize = 0;
        nErrInfo = 0;
        
        if(nCompLen < 0 || nCompLen > 512)
        {
          sprintf(logBufBrdcst,"Invalid Compression length. Len:%d", nCompLen);
          Logger::getLogger().log(DEBUG, logBufBrdcst); 
          break;
        }

        memcpy(strInBuff,pCompPkt->cBroadcastData,pCompPkt->wCompressionLen);

        nErrInfo = lzo1z_decompress(strInBuff, nInSize, strOutBuff, &nOutSize, NULL);

        if(nErrInfo != 0)
        {
          sprintf(logBufBrdcst,"FO:Error while lzo1z_decompress data : %d returned. %d\n",nErrInfo,nInSize);
          Logger::getLogger().log(DEBUG, logBufBrdcst); 
          break;
        }
        else
        {
          pPktHeader = (ExchangeStructsFO::NNF_BCAST_HEADER*)(strOutBuff + szExchgPreface);
          pPktHeader->wTransCode = __bswap_16((unsigned short)pPktHeader->wTransCode);
          
          pPktHeader->wMsgLen = __bswap_16((unsigned short)pPktHeader->wMsgLen);
          nSize += nCompLen + sizeof(short);
          
        }
        
      }

      if(pPktHeader->wTransCode==7208)
      {
        ExchangeStructsFO::NNF_MBP_PACKET *pMBPData = (ExchangeStructsFO::NNF_MBP_PACKET*)(pPktHeader);
        ProcessDepth(pMBPData, mpTokenIndex, oFFStore, iServFFSockFd, groupSock);
      }
    }
    
    
  }
}

void FastFeedHandler::ReceiveFFData()
{
  int iByteRecvd = 0;
  
  iByteRecvd = recvfrom(iSockFd, m_pcRecvData , m_iRecvDataSize, MSG_DONTWAIT, NULL, NULL);
  
//  std::cout<<"ReceiveFFData"<<iByteRecvd<<"|iSockFd:"<<iSockFd<<std::endl;
  if(iByteRecvd < 0  && ((errno == EAGAIN) ||(errno == EWOULDBLOCK)))
  {
    
  }
  else if(iByteRecvd > 0)
  {
    //    sremaining Bytes::td::cout<<"remaining Bytes::"<<iByteRecvd<<std::endl; 
    m_iByteRemaining = iByteRecvd;
  }
  else if(iByteRecvd == 0)
  {
    snprintf(logBufBrdcst, 200,"MULTICAST|Sock id %d|Disconnection Received",iSockFd );
    Logger::getLogger().log(DEBUG, logBufBrdcst); 
    exit(1);
  }
  else
  {
    return ;
  }
}

void AssignTokenIndex(TOKENSUBSCRIPTION& oTokenSub, TokenIndexMap& mpTokenIndex)
{
  for(int i = 0 ; i < oTokenSub.iNoOfToken ; i++)
  {
    mpTokenIndex.insert(std::make_pair(oTokenSub.iaTokenArr[i], i));
//    std::cout<<"Token::"<<oTokenSub.iaTokenArr[i]<<"|Index::"<<i<<std::endl;
  }
  
}

int FastFeedHandler::creatFFeedServer(const char* lszIpAddress, const char* lszFcast_IpAddress, int32_t lnFcast_PortNumber, int32_t iTTLOpt, int32_t iTTLVal, struct sockaddr_in& groupSock)
{
  int ret;      

  struct in_addr localInterface;
  ret = socket(AF_INET, SOCK_DGRAM, 0);
  if(ret < 0)
  {
      snprintf(logBufBrdcst, 200,"MULTICAST|Server|Opening datagram socket error");
      Logger::getLogger().log(DEBUG, logBufBrdcst);        
      exit(1);
  }
  else
  {
      snprintf(logBufBrdcst, 200,"MULTICAST|Server|Opening the datagram socket...OK");
      Logger::getLogger().log(DEBUG, logBufBrdcst);        
  }


  memset((char *) &groupSock, 0, sizeof(groupSock));
  groupSock.sin_family = AF_INET;
  groupSock.sin_addr.s_addr = inet_addr(lszFcast_IpAddress);
  groupSock.sin_port = htons(lnFcast_PortNumber);

  localInterface.s_addr = inet_addr(lszIpAddress);
  if(setsockopt(ret, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface)) < 0)
  {
     snprintf(logBufBrdcst, 200,"MULTICAST|Server|Setting local interface error");
     Logger::getLogger().log(DEBUG, logBufBrdcst);
  }
  else
  {  
      snprintf(logBufBrdcst, 200,"MULTICAST|Server|Setting the local interface %s...OK", lszIpAddress);
      Logger::getLogger().log(DEBUG, logBufBrdcst);
  }

  snprintf(logBufBrdcst, 200,"MULTICAST|Server|TTL Option %d|TTL Value %d", iTTLOpt, iTTLVal);
  Logger::getLogger().log(DEBUG, logBufBrdcst); 
  if (iTTLOpt == 1)
  {
    u_char ttl = iTTLVal;
    if (setsockopt(ret, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
      snprintf(logBufBrdcst, 200,"MULTICAST|Server|Setting TTL error");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
    else
    {
      snprintf(logBufBrdcst, 200,"MULTICAST|Server|Setting TTL...OK");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
  }
  else if (iTTLOpt == 2)
  {
    int32_t TTL = iTTLVal;
    if(setsockopt(ret, IPPROTO_IP, IP_TTL, (char*)&TTL, sizeof(TTL)) < 0)
    {
        snprintf(logBufBrdcst, 200,"MULTICAST|Setting TTL error");
        Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
    else
    {
      snprintf(logBufBrdcst, 200,"MULTICAST|Setting TTL...OK");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
    }
  }
  
  return ret;
}

void FastFeedHandler::HandleFFConnection(ConfigFile& oConfigdata, TOKENSUBSCRIPTION& oTokenSub, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_MainToFF, ProducerConsumerQueue<DATA_RECEIVED> *Inqptr_FFToMain)
{
   
  TaskSett(oConfigdata.iCore, 0);  
  
  snprintf(logBufBrdcst, 200,"FastFeed Thread: Pinned to Core :%d", oConfigdata.iCore);
  Logger::getLogger().log(DEBUG, logBufBrdcst);
  
  
  gbPrintFFData = oConfigdata.bPrintFFData; //Config parameter to print fast feed data
  
  FastFeedHandler *FF_CM,*FF_FO;    
  FastFeedHandler FF_Serv;
  TokenIndexMap mpTokenIndex;
  struct sockaddr_in groupSock;
  DATA_RECEIVED oDataRecv;
  AssignTokenIndex(oTokenSub, mpTokenIndex);
  
  FFOrderBook oFFStore[oTokenSub.iNoOfToken];   //Top 5 best buy and sell order book
  
  if(oConfigdata.CMSeg)   //Fast feed data receive for CM Seg
  {
    FF_CM = new FastFeedHandler(oConfigdata.iRecvBuff);
    FF_CM->iSockFd = CreateFFReciever(oConfigdata.sExchFFIP.c_str(), oConfigdata.iExchFFPort, oConfigdata.sIfaceIP.c_str());
  
    if(FF_CM->iSockFd <= 0)
    {
      snprintf(logBufBrdcst, 200,"Error in created Socked for multi-cast Receiver");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
      exit(1);
    }
  }
  if(oConfigdata.FOSeg) //Fast feed data receive for FO Seg
  {
    FF_FO = new FastFeedHandler(oConfigdata.iRecvBuff);
    FF_FO->iSockFd = CreateFFReciever(oConfigdata.sExchFFIP.c_str(), oConfigdata.iExchFFPort, oConfigdata.sIfaceIP.c_str());

    if(FF_FO->iSockFd <= 0)
    {
      snprintf(logBufBrdcst, 200,"Error in created Socked for multi-cast Receiver");
      Logger::getLogger().log(DEBUG, logBufBrdcst);
      exit(1);
    }
  }
  //Create Multi-cast server to send Fast feed data to connected clients
  FF_Serv.iSockFd  = FF_Serv.creatFFeedServer(oConfigdata.sIpAddressUI.c_str(), oConfigdata.sFFServIP.c_str(), oConfigdata.iFFServPort, oConfigdata.iTTLOpt, oConfigdata.iTTLVal, groupSock);
  
  
  if(FF_Serv.iSockFd <= 0)
  {
    snprintf(logBufBrdcst, 200,"Error in created Socked for multi-cast Server|IP %s|Port %d",oConfigdata.sFFServIP.c_str(),oConfigdata.iFFServPort);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
    exit(1);
  }
  else
  {
    snprintf(logBufBrdcst, 200,"MULTICAST|Server|Socked created for multi-cast Server|IP %s|Port %d",oConfigdata.sFFServIP.c_str(),oConfigdata.iFFServPort);
    Logger::getLogger().log(DEBUG, logBufBrdcst);
  }
  
  while(1)
  {
    if(oConfigdata.CMSeg) //Handle CM Multi-cast data
    {
      FF_CM->ReceiveFFData();
    }
    if(oConfigdata.FOSeg) //Handle FO Multi-cast data
    {
      FF_FO->ReceiveFFData();
      
      FF_FO->ProcessFFData( mpTokenIndex, oFFStore, FF_Serv.iSockFd, &groupSock );
    }
    
    if(Inqptr_MainToFF->dequeue(oDataRecv))  //Handle Token Depth request
    {
      HandleDepthRequest(&oDataRecv, mpTokenIndex, oFFStore, Inqptr_FFToMain);
    }
  }
}
