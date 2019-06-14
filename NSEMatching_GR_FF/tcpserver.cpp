#include "tcpserver.h"
#include "callbackmessages.h"

/**********************************************************************CReadyFdsForAllThread****************************************************************/

CReadyFdsForAllThread::CReadyFdsForAllThread(TCPServer* pcTCPServer): m_pcTCPServer(pcTCPServer)
{
	m_vReadyFdInfoForAllThread.reserve(NUMBER_OF_RECV_THREAD  + NUMBER_OF_SEND_THREAD);
}

void CReadyFdsForAllThread::ThreadReadyFdInit(const int nReserve)
{
	const bool lbIsDirty = false;
	tagReadyFdInfoForThread lvEachReadyFdInfoForThread(nReserve, lbIsDirty);
	m_vReadyFdInfoForAllThread.push_back(lvEachReadyFdInfoForThread);			
}

void CReadyFdsForAllThread::ThreadReadyFdStoreReserved(int nReserve)
{
	m_vReadyFdInfoForAllThread.reserve(nReserve);
}

bool CReadyFdsForAllThread::GetRecvThreadAndFdIdx(int nFd, int* nThreadIdx, int* nFdIdx)
{
	bool lbRetVal = true;
	FdIndexRegisterHash::const_accessor laccFdIndexInThread;

	if (m_ihFdThreadAndIndexInfo.find(laccFdIndexInThread, nFd))
	{
		*nThreadIdx = (laccFdIndexInThread->second).m_nRecvThreadIdx;
		*nFdIdx = (laccFdIndexInThread->second).m_nRecvFdIdx;
	}
	else
	{
		//Log message 
		lbRetVal = false;
	}
	laccFdIndexInThread.release();
	return lbRetVal;
}

bool CReadyFdsForAllThread::GetSendThreadAndFdIdx(int nFd, int* nThreadIdx, int* nFdIdx) const
{
	bool lbRetVal = true;
	FdIndexRegisterHash::const_accessor laccFdIndexInThread;
	if (m_ihFdThreadAndIndexInfo.find(laccFdIndexInThread, nFd))
	{
		*nThreadIdx = (laccFdIndexInThread->second).m_nSendThreadIdx;
		*nFdIdx = (laccFdIndexInThread->second).m_nSendFdIdx;
	}
	else
	{
		//Log error message
		lbRetVal = false;
	}
	laccFdIndexInThread.release();
	return lbRetVal;
}

//Function adds fd index in send or receive thread store depending on event raise
bool CReadyFdsForAllThread::AddFdIdxInThreadStore(const int nFd, const unsigned int nEpollEvent, ReadyFdInfoStore* pvReadyFdInfoStore)
{
  tagEpollEventDetails lstEpollEventDetails;
  
	bool lbRetVal = true;
	FdIndexRegisterHash::const_accessor laccFdIndexInThread;
	if (m_ihFdThreadAndIndexInfo.find(laccFdIndexInThread, nFd))
	{
		const tagFdIndexInThreads& lstFdIdxInfo = laccFdIndexInThread->second;
		if (nEpollEvent & EPOLLIN)  //EPOLLIN
		{
			pvReadyFdInfoStore[0].push_back(lstFdIdxInfo.m_nRecvFdIdx);
      lstEpollEventDetails.nEpollEvent = nEpollEvent;
      lstEpollEventDetails.nFd         = nFd;
      lstEpollEventDetails.nFdIdx      = lstFdIdxInfo.m_nRecvFdIdx;
      m_pcTCPServer->m_pcRecevEpollEventQ->enqueue(lstEpollEventDetails);      
		}

		if (nEpollEvent & EPOLLOUT)
		{
			pvReadyFdInfoStore[1].push_back(lstFdIdxInfo.m_nSendFdIdx);
      lstEpollEventDetails.nEpollEvent = nEpollEvent;
      lstEpollEventDetails.nFd         = nFd;
      lstEpollEventDetails.nFdIdx      = lstFdIdxInfo.m_nSendFdIdx;
      m_pcTCPServer->m_pcSendEpollEventQ->enqueue(lstEpollEventDetails);            
      
      snprintf(m_pcTCPServer->m_cControlString, sizeof(m_pcTCPServer->m_cControlString), "%s-%s(%d): Epollout received for FD(%d)", ERROR_LOCATION, nFd);
      m_pcTCPServer->LogMessage(INFO, m_pcTCPServer->m_cControlString);
		}
	}
	else
	{
		//g_cLogger.LogMessage("%s-%s(%d): Fd(%d) not found in hash", ERROR_LOCATION, nFd);
		lbRetVal = false;
	}
	laccFdIndexInThread.release();
	return lbRetVal;
}

/*
void ReleaseLockOfAllThreadReadyFd()			
{
	ReadyFdInfoForAllThread::iterator itor;

	for (itor = m_vReadyFdInfoForAllThread.begin(); itor != m_vReadyFdInfoForAllThread.end(); ++itor)
	{	
		(*itor).m_cReadyFdInfoScopeLock.release();
	}
}
*/

bool CReadyFdsForAllThread::AddRecvConnectionIdx(const int nFd, const int nThreadIdx, const int nFdIdx)
{
	FdIndexRegisterHash::accessor acc;
	bool lbRetVal = true;

	if (true == m_ihFdThreadAndIndexInfo.insert(acc, nFd))
	{
		(acc->second).m_nRecvThreadIdx = (short int)nThreadIdx;
		(acc->second).m_nRecvFdIdx = nFdIdx;
		//g_cLogger.LogMessage("Element is added in hash map= %d", nFd);
	}
	else
	{
		//TODO : Discuss with sir what to do if fd already present in hash map
		//Log error message for that fd
		lbRetVal = false;
	}
	acc.release();
	return lbRetVal;
}

bool CReadyFdsForAllThread::IsConnectionValid(const int& nFd, int& nRecvThreadIdx, int& nRecvThreadFdIdx)
{
	FdIndexRegisterHash::const_accessor acc;
	bool lbRetVal = true;

	if (false == m_ihFdThreadAndIndexInfo.find(acc, nFd))
	{
		//g_cLogger.LogMessage("Present key value = %d", acc->first); 
		lbRetVal = false;
	}
   nRecvThreadIdx = acc->second.m_nRecvThreadIdx;
   nRecvThreadFdIdx = acc->second.m_nRecvFdIdx;
	acc.release();
	return lbRetVal;
}

bool CReadyFdsForAllThread::AddSendConnectionIdx(const int nFd, const int nThreadIdx, const int nFdIdx)
{
	bool lbRetVal = true;

	FdIndexRegisterHash::accessor acc;	

	if (m_ihFdThreadAndIndexInfo.find(acc, nFd))
	{
		(acc->second).m_nSendThreadIdx = (short int)nThreadIdx;
		(acc->second).m_nSendFdIdx = nFdIdx;
	}
	else
	{		
		//g_cLogger.LogMessage("%s-%s(%d): Error connection Fd(%d) not found", ERROR_LOCATION, nFd);
		lbRetVal = false;
	}
	acc.release();
	return lbRetVal;
}


bool CReadyFdsForAllThread::RemoveConnectionIdxInfo(const int nSockFd)
{
	return m_ihFdThreadAndIndexInfo.erase(nSockFd);
}

/**********************************************************************CReadyFdsForAllThread****************************************************************/

FdVersionManager::FdVersionManager()
{

}

FdVersionManager::~FdVersionManager()
{

}

bool FdVersionManager::AddVersion(int nSockFd)
{
	bool lbRetVal = true;
	ConnectionVersionStore::accessor acc;
	if (true == m_ihFdVersionStore.insert(acc, nSockFd))
	{
		(acc->second) = 0;
	}
	else
	{
		lbRetVal = false;
	}
	acc.release();
	return lbRetVal;
}

bool FdVersionManager::GetVersion(int nSockFd, unsigned char* pcVersion)
{
  bool lbRetVal = true;
	ConnectionVersionStore::const_accessor acc;
	if (m_ihFdVersionStore.find(acc, nSockFd))
	{
		*pcVersion = (acc->second);
	}
	else
	{
		lbRetVal = false;
	}
	acc.release();
	return lbRetVal;
}

void FdVersionManager::IncrementVersion(int nSockFd)
{
	ConnectionVersionStore::accessor acc;
	if (m_ihFdVersionStore.find(acc, nSockFd))
	{
		++(acc->second);
	}
	acc.release();
}

TCPServer::TCPServer(Logger& cLogger, int  nMaxConnectionCount, bool cEnableKeepAlive): m_nNumberOfConnections(nMaxConnectionCount), m_cLogger(cLogger),g_cReadyFdsInfo(this) 
{
  m_nNumberOfRecvThreads    = 1;   
  m_nNumberOfSendThreads    = 1;   

  m_nRecvBufferSize         = 20480;
  m_nSendBufferSize         = 20480;
  
  m_nNumberOfConnections    = 100;
  
  //The first two parameters are expressed in seconds, and the last is the pure number. 
  //This means that the keepalive routines wait for two hours (7200 secs) before sending the first keepalive probe, and then resend it every 75 seconds. 
  //If no ACK response is received for nine consecutive times, the connection is marked as broken.
  
  if(cEnableKeepAlive == 'Y')
  {
    m_nKeepAliveCheckInterval = TCP_KEEP_ALIVE_ENABLE;  //Enable
    m_nTcpKeepIdle            = 30; //tcp_keepalive_time in sec 
    m_nTcp_KeepInterval       = 1;  //tcp_keepalive_intvl in sec
    m_nTcp_KeepCount          = 10; //Pure Number
  }
  else
  {
    m_nKeepAliveCheckInterval = TCP_KEEP_ALIVE_DISABLE;
  }
  m_nShutdown               = 0;
  
  m_stRecvInfoForThread.m_ivRecvMessages.reserve(m_nNumberOfConnections);
  m_stSendThreadInfo.m_ivSendMessages.reserve(m_nNumberOfConnections);    
  
  g_cReadyFdsInfo.ThreadReadyFdStoreReserved(NUMBER_OF_RECV_THREAD + NUMBER_OF_SEND_THREAD);
  
  //MultipleProducerConsumerQueue
  m_pcComonSendQ = new EpollOutEventQ();
  m_pcCommandQ = new CommandQ();
  
  m_pcRecevEpollEventQ = new TCPSERVER_QUEUE::ProducerConsumerQueue<tagEpollEventDetails, 100000>(100000);
  m_pcSendEpollEventQ = new TCPSERVER_QUEUE::ProducerConsumerQueue<tagEpollEventDetails, 100000>(100000);
  
  
  memset(m_cErrorString, 0, TCP_SERVER_ERROR_STR_LEN + 1);  
  memset(m_cControlString, 0, TCP_SERVER_ERROR_STR_LEN + 1);  
  memset(m_cAcceptString, 0, TCP_SERVER_ERROR_STR_LEN + 1);  
  memset(m_cRecevString, 0, TCP_SERVER_ERROR_STR_LEN + 1);  
  memset(m_cCommandString, 0, TCP_SERVER_ERROR_STR_LEN + 1);    
  
  m_nSendCoreId       = 0;  
  m_nRecvCoreId       = 0;
  m_nControllerCoreId = 0;
}

TCPServer::~TCPServer()
{
  delete m_pcComonSendQ;
  delete m_pcRecevEpollEventQ;
  delete m_pcSendEpollEventQ;
  delete m_pcCommandQ;
}

bool TCPServer::CreateServerListener(const char* pcListenerIpAddr, const unsigned short nListenerPort)
{
	int lnRetVal = 0;
	if (0 != m_stListenerInfo.m_nListenerFd)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Listener socket already created", ERROR_LOCATION);
    LogMessage(ERROR, m_cErrorString);
    return false;
	}

	m_stListenerInfo.m_nListenerFd = socket(AF_INET, SOCK_STREAM, 0);
	if (SYSTEM_CALL_ERROR == m_stListenerInfo.m_nListenerFd)
	{
		//g_cLogger.LogMessage("%s-%s(%d): Error while creating listener socket(%s)", ERROR_LOCATION, strerror(errno)); 
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): while creating listener socket(%s)", ERROR_LOCATION, strerror(errno));    
    LogMessage(ERROR, m_cErrorString);
		m_stListenerInfo.m_nListenerFd = 0;
		return false;
	}

	lnRetVal = SetListenerConfiguration(m_stListenerInfo.m_nListenerFd);
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): setting listener socket configuration for fd(%d)", ERROR_LOCATION, m_stListenerInfo.m_nListenerFd);    
    LogMessage(ERROR, m_cErrorString);
		close(m_stListenerInfo.m_nListenerFd);
		m_stListenerInfo.m_nListenerFd = 0;
	}
  
	sockaddr_in	lcListenerAddr;
	lcListenerAddr.sin_family = AF_INET;
	lcListenerAddr.sin_port = htons((short)nListenerPort);
	inet_aton(pcListenerIpAddr, &(lcListenerAddr.sin_addr));
	lnRetVal = bind(m_stListenerInfo.m_nListenerFd, (sockaddr*)&lcListenerAddr, sizeof(sockaddr_in));		  
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): binding address to listener socket(%d),error(%s)", ERROR_LOCATION, m_stListenerInfo.m_nListenerFd, strerror(errno)); 
    LogMessage(ERROR, m_cErrorString);
		close(m_stListenerInfo.m_nListenerFd);
		m_stListenerInfo.m_nListenerFd = 0;
		return false;
	}

  snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Port(%d) IPAdd(%s) Listener FD (%d)", ERROR_LOCATION, nListenerPort, pcListenerIpAddr, m_stListenerInfo.m_nListenerFd); 
  LogMessage(INFO, m_cErrorString);

	lnRetVal = listen(m_stListenerInfo.m_nListenerFd, SOMAXCONN);
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): listening for FD(%d) error(%s)", ERROR_LOCATION, m_stListenerInfo.m_nListenerFd, strerror(errno)); 
    LogMessage(ERROR, m_cErrorString);
    
		close(m_stListenerInfo.m_nListenerFd);
		m_stListenerInfo.m_nListenerFd = 0;
		return false;
	}
	return true;
}

bool TCPServer::CreateListenerEventHandler()
{
	int lnEpollCtlRet = 0;
	if (0 == m_stListenerInfo.m_nListenerFd)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): listener socket is not created", ERROR_LOCATION); 
    LogMessage(ERROR, m_cErrorString);
		return false;
	}

	m_stListenerInfo.m_nEpollAcceptFd = epoll_create(LISTENER_POLL_SIZE);
	if (SYSTEM_CALL_ERROR == m_stListenerInfo.m_nEpollAcceptFd)
	{
		//Log error message
		//g_cLogger.LogMessage("%s-%s(%d): Error in creating listener epoll fd,error(%s)", ERROR_LOCATION, strerror(errno));
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Creating Listener epoll fd failed reason (%s)", ERROR_LOCATION, strerror(errno)); 
    LogMessage(ERROR, m_cErrorString);    
    
    if (0 != m_stListenerInfo.m_nListenerFd)
    {
      close(m_stListenerInfo.m_nListenerFd);
      m_stListenerInfo.m_nListenerFd = 0;
    }    
		m_stListenerInfo.m_nEpollAcceptFd = 0;
		return false;
	}

	epoll_event lstListenerEvents;
	lstListenerEvents.events = EPOLLIN | EPOLLOUT;		// commented EPOLLET
	lstListenerEvents.data.fd = m_stListenerInfo.m_nListenerFd;
	lnEpollCtlRet = epoll_ctl(m_stListenerInfo.m_nEpollAcceptFd, EPOLL_CTL_ADD, m_stListenerInfo.m_nListenerFd, &lstListenerEvents);	
	if (SYSTEM_CALL_ERROR == lnEpollCtlRet)
	{
		//g_cLogger.LogMessage("%s-%s(%d): Error in adding to epoll controller for listener(%d),error(%s)", ERROR_LOCATION, m_stListenerInfo.m_nEpollAcceptFd, strerror(errno));
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Adding to epoll controller for Listener FD(%d) failed reason (%s)", ERROR_LOCATION, m_stListenerInfo.m_nEpollAcceptFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    
    if (0 != m_stListenerInfo.m_nListenerFd)
    {
      close(m_stListenerInfo.m_nListenerFd);
      m_stListenerInfo.m_nListenerFd = 0;
    }
    
    if (0 != m_stListenerInfo.m_nEpollAcceptFd)
    {
      close(m_stListenerInfo.m_nEpollAcceptFd);
      m_stListenerInfo.m_nEpollAcceptFd = 0;
    }    
		return false;
	}
	return true;
}

bool TCPServer::CreateEventHandler()
{
  bool lbRetVal = true;
  m_nEpollFd = epoll_create(m_nNumberOfConnections);
	if (SYSTEM_CALL_ERROR == m_nEpollFd)
	{
		//g_cLogger.LogMessage("%s-%s(%d): Error in creating controller epoll fd(%s)", ERROR_LOCATION, strerror(errno));
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Creating controller epoll fd(%s)", ERROR_LOCATION, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    
		m_nEpollFd = 0;
		lbRetVal = false;
	}
  
  return lbRetVal;
}

//bool TCPServer::UpdateConfigurationParameters(int nNumberOfRecvThreads, int nNumberOfSendThreads, int nRecvBufferSize, int nSendBufferSize, int nNumberOfConnections)
bool TCPServer::UpdateConfigurationParameters(int nRecvBufferSize, int nSendBufferSize)
{
//  m_nNumberOfRecvThreads    = nNumberOfRecvThreads;   
//  m_nNumberOfSendThreads    = nNumberOfSendThreads;   

  m_nRecvBufferSize         = nRecvBufferSize;
  m_nSendBufferSize         = nSendBufferSize;
  return true;
}

bool TCPServer::CreateTCPServerFramework(const char* pcListenerIpAddr, const unsigned short nListenerPort)
{
  bool lbStatus = false;
  lbStatus = CreateServerListener(pcListenerIpAddr, nListenerPort);  
  if(!lbStatus)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): CreateServerListener failed.", ERROR_LOCATION);
    LogMessage(ERROR, m_cErrorString);    
    return false;
  }
  
  lbStatus = CreateListenerEventHandler();
  if(!lbStatus)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): CreateListenerEventHandler failed.", ERROR_LOCATION);
    LogMessage(ERROR, m_cErrorString);    
    return false;
  }
  
  lbStatus = CreateEventHandler();  
  if(!lbStatus)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): CreateEventHandler failed.", ERROR_LOCATION);
    LogMessage(ERROR, m_cErrorString);    
    return false;
  }
  signal(SIGPIPE, SIG_IGN);  
  return true;
}

bool TCPServer::UpdateCoreId(int  nSendCoreId, int nRecvCoreId, int nControllerCoreId, int nCommandCoreId)
{
  m_nSendCoreId         = nSendCoreId;  
  m_nRecvCoreId         = nRecvCoreId;
  m_nControllerCoreId   = nControllerCoreId;
  m_nCommandCoreId      = nCommandCoreId; 
  return true;
}

int TCPServer::SetListenerConfiguration(int nSockFd)
{
	struct linger lstLingerOpt;
	lstLingerOpt.l_onoff = 1;
	lstLingerOpt.l_linger = 10;
	int lnRetVal = 0;
	int lnOptVal = 0;

	lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_LINGER, (const void *)(&lstLingerOpt), sizeof(lstLingerOpt));
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting Linger Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
		return SYSTEM_CALL_ERROR;
	}

	lnOptVal = 1;
	lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_REUSEADDR, (const void *)&lnOptVal, sizeof(lnOptVal));
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting Reuse add Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
		return SYSTEM_CALL_ERROR;
	}

	lnOptVal = m_nSendBufferSize;
  lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_SNDBUF, (const void *)&lnOptVal, sizeof(lnOptVal));
  if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting SO_SNDBUF Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    return SYSTEM_CALL_ERROR;
  }

	lnOptVal = m_nRecvBufferSize;
  lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_RCVBUF, (const void *)&lnOptVal, sizeof(lnOptVal));
  if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting SO_RCVBUF Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    return SYSTEM_CALL_ERROR;
  }

	if(m_nKeepAliveCheckInterval != TCP_KEEP_ALIVE_DISABLE)
	{
		lnOptVal = m_nKeepAliveCheckInterval; //optval = 1;
		lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&lnOptVal, sizeof(lnOptVal)) ;
		if (SYSTEM_CALL_ERROR == lnRetVal) 
		{
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting SO_KEEPALIVE Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
      LogMessage(ERROR, m_cErrorString);    
			return SYSTEM_CALL_ERROR;
		}

    //tcp_keepalive_time    7200
    //tcp_keepalive_intvl   75
    //tcp_keepalive_probes  9
    
    //The first two parameters are expressed in seconds, and the last is the pure number. 
    //This means that the keepalive routines wait for two hours (7200 secs) before sending the first keepalive probe, and then resend it every 75 seconds. 
    //If no ACK response is received for nine consecutive times, the connection is marked as broken.
    
    //tcp_keepalive_time
		lnOptVal = m_nTcpKeepIdle;
		lnRetVal = setsockopt(nSockFd, SOL_TCP, TCP_KEEPIDLE, (const void *)&lnOptVal, sizeof(lnOptVal));
		if (SYSTEM_CALL_ERROR == lnRetVal)
		{
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting TCP_KEEPIDLE Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
      LogMessage(ERROR, m_cErrorString);    
			return SYSTEM_CALL_ERROR;
		}

    //tcp_keepalive_probes
		lnOptVal = m_nTcp_KeepCount;
		lnRetVal = setsockopt(nSockFd, SOL_TCP, TCP_KEEPCNT, (const void *)&lnOptVal, sizeof(lnOptVal));
		if (SYSTEM_CALL_ERROR == lnRetVal)
		{
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting TCP_KEEPCNT Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
      LogMessage(ERROR, m_cErrorString);    
			return SYSTEM_CALL_ERROR;
		}

    //tcp_keepalive_intvl
		lnOptVal = m_nTcp_KeepInterval;
		lnRetVal = setsockopt(nSockFd, SOL_TCP, TCP_KEEPINTVL, (const void *)&lnOptVal, sizeof(lnOptVal));
		if (SYSTEM_CALL_ERROR == lnRetVal)
		{
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting TCP_KEEPINTVL Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
      LogMessage(ERROR, m_cErrorString);    
			return SYSTEM_CALL_ERROR;
		}
	}

	int lnNonBlocking = 1;
	lnRetVal = ioctl(nSockFd, FIONBIO, &lnNonBlocking);
	if (SYSTEM_CALL_ERROR == lnRetVal)	
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting FIONBIO Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
		return SYSTEM_CALL_ERROR;
	}

	lnOptVal = 1;
	lnRetVal = setsockopt(nSockFd, SOL_TCP, TCP_NODELAY, (const void *)(&lnOptVal), sizeof(lnOptVal));
  if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting TCP_NODELAY Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    return SYSTEM_CALL_ERROR;
  }
 
	return 1;
}

bool TCPServer::SetConnectionCallBack(FpConnectionNotifierCallBack pConnectionNotifierCallBack)
{
  ConnectionNotifierCallBack = pConnectionNotifierCallBack;    
  return true;
}

bool TCPServer::SetRecvMessageCallback(FpRecvMessageCallback pRecvMessageCallback)   
{
  RecvMessageCallback = pRecvMessageCallback;   
  return true;
}

bool TCPServer::StartTCPServer()
{
  if(RecvMessageCallback == NULL || ConnectionNotifierCallBack == NULL)
  {
    return false;
  }
  
  g_cReadyFdsInfo.ThreadReadyFdInit(m_nNumberOfConnections); 

  m_stRecvInfoForThread.m_inTotalConnectionCount = m_nNumberOfConnections;  
  m_stSendThreadInfo.m_inTotalConnectionCount = m_nNumberOfConnections;  
  
  tagSendConnectionInfo* lpstSendConnectionInfo = NULL;
  tagRecvInfoForConnection* lpstRecvInfoForConnection = NULL;
  for(int lnConnectionCount = 0; lnConnectionCount < m_nNumberOfConnections; ++lnConnectionCount)
  {
    lpstRecvInfoForConnection = new tagRecvInfoForConnection();
    m_stRecvInfoForThread.m_ivRecvMessages.push_back(lpstRecvInfoForConnection);
    
    lpstSendConnectionInfo = new tagSendConnectionInfo();
    m_stSendThreadInfo.m_ivSendMessages.push_back(lpstSendConnectionInfo);
  }
  
  
  m_cRecvThread = std::thread(&TCPServer::RecvThreadProc, this);
  m_cSendThread = std::thread(&TCPServer::SendThreadProc, this);    
  m_cControllerThread = std::thread(&TCPServer::ControlThreadProc, this);  
  m_cAcceptThread = std::thread(&TCPServer::AcceptThreadProc, this);  
  m_cCommandThread = std::thread(&TCPServer::CommandThreadProc, this);    
  return true;
}

bool TCPServer::DisconnectFD(const int64_t& nFD, char* pcMessage, int nLength)
{
  char lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1] = {0};  
  
  if(pcMessage == NULL && nLength != 0)
  {
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): pcMessage is NULL and Received Length %d  FD(%ld)", ERROR_LOCATION, nLength, nFD);
    LogMessage(INFO, lcErrorString) ;
    return false;
  }
  
  if(pcMessage != NULL && nLength == 0)
  {
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): pcMessage is NOT NULL and Received Length %d  FD(%ld)", ERROR_LOCATION, nLength, nFD);
    LogMessage(INFO, lcErrorString) ;
    return false;
  }  
  
  int lnThreadIdx = UNDEFINED_THREAD_IDX;
  int lnFdIdx     = UNDEFINED_FD_IDX;
  if(g_cReadyFdsInfo.GetSendThreadAndFdIdx(nFD, &lnThreadIdx, &lnFdIdx))
  {
    m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_inLoginStatus = CLIENT_LOGOFF_IN_PROGRESS;
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): DisconnectFD received for FD(%ld)", ERROR_LOCATION, nFD);
    LogMessage(INFO, lcErrorString) ;
  }
  else
  {
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): Invalid FD(%ld)", ERROR_LOCATION, nFD);
    LogMessage(ERROR, lcErrorString) ;
    return false;
  }
  
  unsigned char lcSockVersion = 0;
	g_stFdVersionManager.GetVersion(nFD, &lcSockVersion);
  AddLogoffCommand(nFD, lcSockVersion, LOGOFF_FROM_OTHER, EP_SERVER_ACTIVATED, pcMessage, nLength);
  
  return true;  
}

bool TCPServer::StopTCPServer()
{
  m_nShutdown = 1;  
  return true;
}

int TCPServer::TaskSet(unsigned int nCoreID_, int nPid_, std::string &strStatus)
{
  int nRet = 0;
  char lcErrMsg[512 + 1] = {0};
  unsigned int nThreads = std::thread::hardware_concurrency();
  cpu_set_t set;
  CPU_ZERO(&set);

  if(nCoreID_ >= nThreads)
  {
    sprintf(lcErrMsg, "Error : Supplied core id %d is invalid. Valid range is 0 - %d", nCoreID_, nThreads - 1);
    strStatus = lcErrMsg;
    nRet = -1;
  }
  else
  {
    CPU_SET(nCoreID_,&set);

    if(sched_setaffinity(nPid_,sizeof(cpu_set_t), &set) < 0)
    {
      sprintf(lcErrMsg, "Error : %d returned while setting process affinity for process ID . Valid range is 0 - %d", errno,nPid_);
      strStatus = lcErrMsg;
      nRet = -1;
    }
  }
  return nRet;
};

bool TCPServer::AddPartialPacketNEpollOut(tagSendDataUnit& stSendDataUnit, int nSendFdIdx)
{
  if(!m_stSendThreadInfo.m_ivSendMessages[nSendFdIdx]->pcPartialSendQ->enqueue(stSendDataUnit))
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Partial enqueue failed for FD(%d)in epoll out.", ERROR_LOCATION, stSendDataUnit.nFd);
    LogMessage(ERROR, m_cErrorString);                

    unsigned char lcSockVersion = 0; 
    const bool lbVersionRetVal = g_stFdVersionManager.GetVersion(stSendDataUnit.nFd, &lcSockVersion);
    if (false == lbVersionRetVal)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Error connection(%d) not found", ERROR_LOCATION, stSendDataUnit.nFd);
      LogMessage(ERROR, m_cErrorString);
    }
    // Set both Logoff in progress and logoff from send, so that SendThread and Logoff functions
    m_stSendThreadInfo.m_ivSendMessages[nSendFdIdx]->m_inLoginStatus = CLIENT_LOGOFF_IN_PROGRESS;
    const int lnOriginatedFrom = LOGOFF_FROM_SEND;
    AddLogoffCommand(stSendDataUnit.nFd, lcSockVersion, lnOriginatedFrom, EP_ABRUPT_DISCONNECTION);	                  
  }
  else 
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Adding FD(%d) in epoll out for PendingLength %d.", ERROR_LOCATION, stSendDataUnit.nFd, stSendDataUnit.nPendingLength);
    LogMessage(INFO, m_cErrorString);        

    if(!AddEpollout(stSendDataUnit.nFd))
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Epollout failed for FD(%d).", ERROR_LOCATION, stSendDataUnit.nFd);
      LogMessage(ERROR, m_cErrorString);
    }
    m_stSendThreadInfo.m_ivSendMessages[nSendFdIdx]->m_bIsWritable = false;        
  }
  return true;
}

void TCPServer::AcceptThreadProc()
{
  std::string errstring("Accept Thread: Pinned to Core:" + std::to_string(m_nCommandCoreId)); 
  TaskSet((unsigned int)m_nCommandCoreId, 0, errstring);
  m_cLogger.log(INFO , errstring);
  
	epoll_event lstListenerEvents[MAX_LISTENER_EVENTS];
	memset(lstListenerEvents, 0, sizeof(lstListenerEvents));
  
	sockaddr_in lstClientAddr;
	int lnAddrLen = sizeof(sockaddr_in);
	int lnEventCount = 0;
	int lnNewFd = 0;
	int lnEventIdx = 0;
	bool lbRetVal = false;
  
	if (0 == m_stListenerInfo.m_nListenerFd)
	{
		//g_cLogger.LogMessage("%s-%s(%d): Error listener socket is not created", ERROR_LOCATION);
		return;
	}

	if (0 == m_stListenerInfo.m_nEpollAcceptFd)
	{
		//g_cLogger.LogMessage("%s-%s(%d): Error listener epoll controller is not created", ERROR_LOCATION);
		return;
	}

	while(0 == m_nShutdown)
	{
    lnEventCount = epoll_wait(m_stListenerInfo.m_nEpollAcceptFd,
                   lstListenerEvents, 
                   MAX_LISTENER_EVENTS, 
                   LISTENER_EPOLL_WAIT_TIME);
    if (SYSTEM_CALL_ERROR != lnEventCount)
    {
      for (lnEventIdx = 0; lnEventIdx < lnEventCount; ++lnEventIdx)
      {
        if (m_stListenerInfo.m_nListenerFd == lstListenerEvents[lnEventIdx].data.fd)
        {
          lnNewFd = accept(m_stListenerInfo.m_nListenerFd, (sockaddr*)&lstClientAddr, (socklen_t*)&lnAddrLen);
          if (SYSTEM_CALL_ERROR == lnNewFd)
          {
            snprintf(m_cAcceptString, sizeof(m_cAcceptString), "%s-%s(%d): Error accepting new connection reason (%s).", ERROR_LOCATION, strerror(errno));
            LogMessage(ERROR, m_cAcceptString) ;
            continue;
          }

          SetClientConfiguration(lnNewFd);
          lbRetVal = Logon(lnNewFd, lstClientAddr);
          if (lbRetVal)
          {
            ++(m_stListenerInfo.m_inTotalConnections);
            int lnValue = m_stListenerInfo.m_inTotalConnections;
            snprintf(m_cAcceptString, sizeof(m_cAcceptString), "%s-%s(%d): Total Connection %d.", ERROR_LOCATION, lnValue);
            LogMessage(INFO, m_cAcceptString) ;
          }            
        }
      }
    }      
  }
  return;
}

void TCPServer::ControlThreadProc()
{
  std::string errstring("Control Thread: Pinned to Core:" + std::to_string(m_nControllerCoreId)); 
  TaskSet((unsigned int)m_nControllerCoreId, 0, errstring);
  m_cLogger.log(INFO , errstring);
  
	int lnEventCount = 0;
	int lnEventIdx = 0;
 
  bool lbRetVal = false;
        
	ErrHupFdStore::iterator itorErrHupFds;
	ErrHupFdStore::iterator itorErrHupFdsEnd;
	
  const short int lnTotalThreadCount = 2;
  ReadyFdInfoStore lvReadyFdInfoStore[lnTotalThreadCount];

  lvReadyFdInfoStore[0].reserve(m_nNumberOfConnections);
  lvReadyFdInfoStore[1].reserve(m_nNumberOfConnections);  

  //50% of max event
  const int lnNumberOfConnection = (m_nNumberOfConnections / 2);
	epoll_event* lpstEvents = new epoll_event[lnNumberOfConnection];	
   
  unsigned int lnEventValue = 0;

	//g_cLogger.LogMessage("Thread name = %s, and thread id = %d", __FUNCTION__, syscall(SYS_gettid));

	while(0 == m_nShutdown)
	{
    lnEventCount = epoll_wait(m_nEpollFd, lpstEvents, m_nNumberOfConnections, CONTROLLER_EPOLL_WAIT_TIME);
    if (0 < lnEventCount)
    {
      for (lnEventIdx = 0; lnEventIdx < lnEventCount; ++lnEventIdx)
      {					
        lnEventValue = lpstEvents[lnEventIdx].events;
        if ((lnEventValue & EPOLLERR) || (lnEventValue & EPOLLHUP) || (lnEventValue & EPOLLRDHUP))
        {
          m_vErrHupFds.push_back(lpstEvents[lnEventIdx].data.fd);
          snprintf(m_cControlString, sizeof(m_cControlString),"%s-%s(%d): hangup for FD(%d),eventvalue(%d)", ERROR_LOCATION, lpstEvents[lnEventIdx].data.fd, lnEventValue);
          LogMessage(ERROR, m_cControlString);              
          continue;
        }  
        if ((lnEventValue & EPOLLIN) || (lnEventValue &  EPOLLOUT))
        {
          g_cReadyFdsInfo.AddFdIdxInThreadStore(lpstEvents[lnEventIdx].data.fd, lnEventValue, lvReadyFdInfoStore);
        }
      }

      //g_cReadyFdsInfo.MarkReadyThreadStoreDirty(lvReadyFdInfoStore);
      if (!m_vErrHupFds.empty())
      {
        unsigned char lcSockVersion = 0;
        itorErrHupFdsEnd = m_vErrHupFds.end();

        for (itorErrHupFds = m_vErrHupFds.begin();  itorErrHupFds != itorErrHupFdsEnd; ++itorErrHupFds)
        {
          lbRetVal = g_stFdVersionManager.GetVersion(*itorErrHupFds, &lcSockVersion);
          if (false == lbRetVal)
          {
            snprintf(m_cControlString, sizeof(m_cControlString),"%s-%s(%d): socket fd(%d) not found", ERROR_LOCATION, *itorErrHupFds);
            LogMessage(ERROR, m_cControlString);              
            continue;
          }

          //Reason code has to change if keep alive time is active
          AddLogoffCommand(*itorErrHupFds, lcSockVersion, LOGOFF_FROM_OTHER, EP_ABRUPT_DISCONNECTION);	
        }
        m_vErrHupFds.clear();
      }
    }
    else if (SYSTEM_CALL_ERROR == lnEventCount)
    {
      snprintf(m_cControlString, sizeof(m_cControlString),"%s-%s(%d): Error in epoll wait in controller thread : %s", ERROR_LOCATION, strerror(errno));
      LogMessage(ERROR, m_cControlString);
    }
  }
	delete [] lpstEvents;
	return ;
}

void TCPServer::RecvThreadProc()
{
  std::string errstring("Recv thread: Pinned to Core:" + std::to_string(m_nRecvCoreId)); 
  TaskSet((unsigned int)m_nRecvCoreId, 0, errstring);
  m_cLogger.log(INFO , errstring);

  tagEpollEventDetails  lstEpollEventDetails;
  
  ReadyFdInfoStore lvReadyFds;  
  lvReadyFds.reserve(m_nNumberOfConnections);  
  
  int lnConnectionIdx = 0;
  int lnMessageBlockRet = 0;  
  tagRecvDataUnit  lstRecvDataUnit;
  const int lnHeaderFixSize = sizeof(int);
  while(m_nShutdown == 0)
  {
    if(!m_pcRecevEpollEventQ->dequeue(lstEpollEventDetails))
    {
      continue; 
    }
    
    lnConnectionIdx =  lstEpollEventDetails.nFdIdx;
    tagRecvInfoForConnection*& lpRecvInfoForConnection = m_stRecvInfoForThread.m_ivRecvMessages[lnConnectionIdx];
    if (ERASE_FD_VALUE != lpRecvInfoForConnection->m_nFd)
    {
      if(lpRecvInfoForConnection->m_inLoginStatus == CLIENT_LOGGED_IN)
      {      
        lnMessageBlockRet = recv(lpRecvInfoForConnection->m_nFd, lstRecvDataUnit.cBuffer, MAX_RECEIVE_MSG_LENGTH, MSG_DONTWAIT);
        if(lnMessageBlockRet > 0)
        {
          lstRecvDataUnit.nLength = lnMessageBlockRet;
          memcpy(lpRecvInfoForConnection->cBuffer + lpRecvInfoForConnection->nReceivedBytes, lstRecvDataUnit.cBuffer, lstRecvDataUnit.nLength);
          lpRecvInfoForConnection->nReceivedBytes += lstRecvDataUnit.nLength;            

          while(1)
          {
            const int lnPendingBufferCount = (lpRecvInfoForConnection->nReceivedBytes - lpRecvInfoForConnection->nProcessedLength);
            if(lnPendingBufferCount <= lnHeaderFixSize) //sizeof(int)
            {
              //std::cout << "  lnPendingBufferCount " << lnPendingBufferCount << " lnHeaderFixSize " << lnHeaderFixSize << std::endl;
              break;
            }

            int lnRecvLength = *(int*)(lpRecvInfoForConnection->cBuffer + lpRecvInfoForConnection->nProcessedLength);
            if(lnRecvLength > MAX_RECEIVE_MSG_LENGTH || lnRecvLength <= 0)
            {
              snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): Recv Length %d: FD(%d),mark for logoff with reason code(%d)", ERROR_LOCATION, lnRecvLength, lpRecvInfoForConnection->m_nFd, EP_LOW_LEVEL_ERROR);
              LogMessage(INFO, m_cRecevString);
              AddConnectionToLogoff(lnConnectionIdx, LOGOFF_FROM_RECV, EP_LOW_LEVEL_ERROR);
              break;
            }
            if(lnRecvLength > (lpRecvInfoForConnection->nReceivedBytes - lpRecvInfoForConnection->nProcessedLength - lnHeaderFixSize)) //
            {
              //std::cout << "partial packet received... " << lnRecvLength << " Pending Size " << lpRecvInfoForConnection->nReceivedBytes - lpRecvInfoForConnection->nProcessedLength - lnHeaderFixSize << std::endl;
              break;
            }
            else
            {
              //if(lnSendLength <= (lpRecvInfoForConnection->nReceivedBytes - lpRecvInfoForConnection->nProcessedLength - lnHeaderFixSize))
              //{
                RecvMessageCallback(lpRecvInfoForConnection->m_nFd, lpRecvInfoForConnection->cBuffer + lpRecvInfoForConnection->nProcessedLength + lnHeaderFixSize, lnRecvLength);
                lpRecvInfoForConnection->nProcessedLength += (lnRecvLength + lnHeaderFixSize);
                lpRecvInfoForConnection->nUnProcessedLength -= lpRecvInfoForConnection->nProcessedLength;
                //std::cout << " lnRecvLength  " << lnRecvLength << " Total Length " << lpRecvInfoForConnection->nReceivedBytes << " ProcessedLength " << lpRecvInfoForConnection->nProcessedLength << std::endl;                  
                if(lpRecvInfoForConnection->nProcessedLength == lpRecvInfoForConnection->nReceivedBytes)
                {
                  lpRecvInfoForConnection->nReceivedBytes = 0;
                  lpRecvInfoForConnection->nUnProcessedLength = 0;
                  lpRecvInfoForConnection->nProcessedLength = 0;  
                  break;
                }
              //}
            }
          }
        }
        else if (SYSTEM_CALL_ERROR == lnMessageBlockRet)
        {
          if (EWOULDBLOCK == errno)
          {
            snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): EWOULDBLOCK received for FD(%d) error string(%s)", ERROR_LOCATION, lpRecvInfoForConnection->m_nFd, strerror(errno));
            LogMessage(INFO, m_cRecevString);
          }
          else
          {
            snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): Recv for FD(%d) error(%s),mark for logoff with reason code(%d)", ERROR_LOCATION, lpRecvInfoForConnection->m_nFd, strerror(errno), EP_ABRUPT_DISCONNECTION);
            LogMessage(INFO, m_cRecevString);
            AddConnectionToLogoff(lnConnectionIdx, LOGOFF_FROM_RECV, EP_ABRUPT_DISCONNECTION);            
          }
        }
        else if (0 == lnMessageBlockRet)          
        {
          snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): Recv 0: FD(%d),mark for logoff with reason code(%d)", ERROR_LOCATION, lpRecvInfoForConnection->m_nFd, EP_CLIENT_ACTIVATED);
          LogMessage(INFO, m_cRecevString);
          AddConnectionToLogoff(lnConnectionIdx, LOGOFF_FROM_RECV, EP_CLIENT_ACTIVATED);
        }
      }
      else
      {
        snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): Client not logged FD(%d) ConnectionIdx (%d) and data is received", ERROR_LOCATION, lpRecvInfoForConnection->m_nFd, lnConnectionIdx);
        LogMessage(ERROR, m_cRecevString);
      }
    }
  }
  return;
}

void TCPServer::SendThreadProc()    
{
  std::string errstring("Send Thread: Pinned to Core:" + std::to_string(m_nSendCoreId)); 
  TaskSet((unsigned int)m_nSendCoreId, 0, errstring);
  m_cLogger.log(INFO , errstring);
  
  tagSendDataUnit lstSendDataUnit;  
  tagEpollEventDetails lstEpollEventDetails;
  tagEpollEventDetails lstSendEpollEventDetails;  
  int lnFdIdx = 0;
  while (true)
  {
    if(m_pcSendEpollEventQ->dequeue(lstEpollEventDetails))    
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): EPOLL out received for FD(%d) and SendFdIdx (%d).", ERROR_LOCATION, lstEpollEventDetails.nFd, lstEpollEventDetails.nFdIdx);
      LogMessage(INFO, m_cErrorString);    
      
      if(!RemoveEpollout(lstEpollEventDetails.nFd))
      {
        snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): RemoveEpollout failed for FD(%d).", ERROR_LOCATION, lstEpollEventDetails.nFd);
        LogMessage(ERROR, m_cErrorString);
      }
      
      m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->m_bIsWritable = true;
      while(!m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->pcPartialSendQ->isEmpty())
      {
        tagSendDataUnit* lpstSendDataUnit = m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->pcPartialSendQ->frontPtr();    
//        int lnPacketSize = *((int*)lpstSendDataUnit->cBuffer);
//        snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Partial PacketSize %d PendingLength %d SendLength %d  SequenceNo %d for FD(%d).", ERROR_LOCATION, lnPacketSize, lpstSendDataUnit->nPendingLength, lpstSendDataUnit->nSendLength, lpstSendDataUnit->nSequenceNo, lpstSendDataUnit->nFd);
//        LogMessage(ERROR, m_cErrorString);
        
        int lnByteCount = send(lpstSendDataUnit->nFd, lpstSendDataUnit->cBuffer + lpstSendDataUnit->nSendLength, lpstSendDataUnit->nPendingLength, MSG_NOSIGNAL);
        if(lnByteCount > 0)
        {
          if(lnByteCount == lpstSendDataUnit->nPendingLength)
          {
            m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->pcPartialSendQ->popFront();
//            snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Partial Packet send successfully for FD(%d) and PendingLength %d.", ERROR_LOCATION, lpstSendDataUnit->nFd, lpstSendDataUnit->nPendingLength);
//            LogMessage(INFO, m_cErrorString);
          }
          else
          {
            m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->m_bIsWritable = false;          
            lpstSendDataUnit->nPendingLength -= lnByteCount;
            lpstSendDataUnit->nSendLength += lnByteCount;              
              
            if(!AddEpollout(lpstSendDataUnit->nFd))
            {
              snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): epoll out failed for FD(%d).", ERROR_LOCATION, lpstSendDataUnit->nFd);
              LogMessage(ERROR, m_cErrorString);
            }
            snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Adding FD(%d) in epoll out in partial case for PendingLength %d SequenceNo %d", ERROR_LOCATION, lpstSendDataUnit->nFd, lpstSendDataUnit->nPendingLength, lpstSendDataUnit->nSequenceNo);
            LogMessage(INFO, m_cErrorString);        
            break;            
          }
        }
        else if (SYSTEM_CALL_ERROR == lnByteCount)
        {
          if (EWOULDBLOCK == errno)
          {
            m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->m_bIsWritable = false;          
            if(!AddEpollout(lpstSendDataUnit->nFd))
            {
              snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): epoll out failed for FD(%d).", ERROR_LOCATION, lpstSendDataUnit->nFd);
              LogMessage(ERROR, m_cErrorString);
            }
            snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Adding FD(%d) in epoll out in partial case for PendingLength %d", ERROR_LOCATION, lpstSendDataUnit->nFd, lpstSendDataUnit->nPendingLength);
            LogMessage(INFO, m_cErrorString);        
            break;
          }
          else
          {
            //snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Logoff from send for FD(%d) error %s and reason code(%d)", ERROR_LOCATION, lstSendDataUnit.nFd, strerror(errno), EP_ABRUPT_DISCONNECTION);
            //Send Disconnection from here later
            snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Incase of Partial send for FD(%d) error %s", ERROR_LOCATION, lstSendDataUnit.nFd, strerror(errno));
            LogMessage(ERROR, m_cErrorString);
            //Send Disconnection from here later            
          }
        }
        else if (0 == lnByteCount)
        {
          m_stSendThreadInfo.m_ivSendMessages[lstEpollEventDetails.nFdIdx]->pcPartialSendQ->popFront();          
          snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Partial Packet send failed ByteCount %d for PendingLength %d", ERROR_LOCATION, lnByteCount, lpstSendDataUnit->nPendingLength);
          LogMessage(INFO, m_cErrorString);
        }
      }
    }
    
    memset(&lstSendEpollEventDetails, 0, sizeof(tagEpollEventDetails));
    if(!m_pcComonSendQ->dequeue(lstSendEpollEventDetails))
    {
      continue;
    }
    
    tagEpollEventDetails* lpstSendEpollEventDetails = &lstSendEpollEventDetails;
    lnFdIdx = lpstSendEpollEventDetails->nFdIdx;
    if(m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_nFd != lpstSendEpollEventDetails->nFd)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): FD mismatch Internal FD(%d) and Send FD(%d)", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd, lpstSendEpollEventDetails->nFd);
      LogMessage(ERROR);      
      continue;      
    }
    
    if(CLIENT_LOGGED_IN != m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_inLoginStatus)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Client is not logged in Send for FD(%d)", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd);
      LogMessage(ERROR);      
      continue;
    }  
    
    if(m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->pcSendQ->isEmpty())
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Send Q is empty for FD(%d)", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd);
      LogMessage(ERROR);      
      continue;
    }
    
    if(m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_bIsWritable == false)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): FD(%d) not writable.", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd);
      LogMessage(ERROR, m_cErrorString);      
      
      //Add in Partial Send Q... START as since packet is dequeue from queue...we need to send this packet to client...
      if(m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->pcSendQ->dequeue(lstSendDataUnit))
      {
        if(!m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->pcPartialSendQ->enqueue(lstSendDataUnit))
        {
          snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): enqueue failed for FD(%d)forLength %d", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd, lstSendDataUnit.nPendingLength);
          LogMessage(ERROR, m_cErrorString);      
          
          unsigned char lcSockVersion = 0; 
          const bool lbVersionRetVal = g_stFdVersionManager.GetVersion(lstSendDataUnit.nFd, &lcSockVersion);
          if (false == lbVersionRetVal)
          {
            snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Error connection(%d) not found", ERROR_LOCATION, lstSendDataUnit.nFd);
            LogMessage(ERROR, m_cErrorString);
          }
          // Set both Logoff in progress and logoff from send, so that SendThread and Logoff functions
          m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_inLoginStatus = CLIENT_LOGOFF_IN_PROGRESS;
          const int lnOriginatedFrom = LOGOFF_FROM_SEND;
          AddLogoffCommand(lstSendDataUnit.nFd, lcSockVersion, lnOriginatedFrom, EP_ABRUPT_DISCONNECTION);	                  
        }
      }      
      //Add in Partial Send Q... END
      continue;
    }
    
    //tagSendDataUnit* lpstSendDataUnit = m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->pcSendQ->frontPtr();    
    if(!m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->pcSendQ->dequeue(lstSendDataUnit))
    {
      continue;
    }
    
    if(m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_nFd != lstSendDataUnit.nFd)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): FD mismatch Internal FD(%d) and Send FD(%d)", ERROR_LOCATION, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd, lstSendDataUnit.nFd);
      LogMessage(ERROR);      
      continue;      
    }    

//    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Send Data for FD(%d) PendingLength %d ", ERROR_LOCATION, lstSendDataUnit.nFd, lstSendDataUnit.nPendingLength);
//    LogMessage(TRACE, m_cErrorString);            

//    int lnPacketSize = *((int*)lstSendDataUnit.cBuffer);
//    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): PacketSize %d PendingLength %d SendLength %d SequenceNo %d for FD(%d).", ERROR_LOCATION, lnPacketSize, lstSendDataUnit.nPendingLength, lstSendDataUnit.nSendLength, lstSendDataUnit.nSequenceNo, lstSendDataUnit.nFd);
//    LogMessage(ERROR, m_cErrorString);
    

    
    int lnByteCount = send(lstSendDataUnit.nFd, lstSendDataUnit.cBuffer + lstSendDataUnit.nSendLength, lstSendDataUnit.nPendingLength, MSG_NOSIGNAL);
    if(lnByteCount > 0)
    {
      lstSendDataUnit.nPendingLength -= lnByteCount;
      lstSendDataUnit.nSendLength += lnByteCount;
      //If 
      if(lstSendDataUnit.nPendingLength != 0)
      {
        AddPartialPacketNEpollOut(lstSendDataUnit, lpstSendEpollEventDetails->nFdIdx);
      }

    }
    else if (SYSTEM_CALL_ERROR == lnByteCount)
    {
      if (EWOULDBLOCK == errno)
      {
        AddPartialPacketNEpollOut(lstSendDataUnit, lpstSendEpollEventDetails->nFdIdx);
      }
      else
      {
        snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Logoff from send for FD(%d) error %s and reason code(%d) SequenceNo %d", ERROR_LOCATION, lstSendDataUnit.nFd, strerror(errno), EP_ABRUPT_DISCONNECTION, lstSendDataUnit.nSequenceNo);
        LogMessage(ERROR, m_cErrorString);

        m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_bIsWritable = false;
        unsigned char lcSockVersion = 0; 
        const bool lbVersionRetVal = g_stFdVersionManager.GetVersion(lstSendDataUnit.nFd, &lcSockVersion);
        if (false == lbVersionRetVal)
        {
          snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Error connection(%d) not found", ERROR_LOCATION, lstSendDataUnit.nFd);
          LogMessage(ERROR, m_cErrorString);
        }
        // Set both Logoff in progress and logoff from send, so that SendThread and Logoff functions
        m_stSendThreadInfo.m_ivSendMessages[lpstSendEpollEventDetails->nFdIdx]->m_inLoginStatus = CLIENT_LOGOFF_IN_PROGRESS;
        const int lnOriginatedFrom = LOGOFF_FROM_SEND;
        AddLogoffCommand(lstSendDataUnit.nFd, lcSockVersion, lnOriginatedFrom, EP_ABRUPT_DISCONNECTION);	        
      }
    }
    else if (0 == lnByteCount)
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): send failed ByteCount %d for PendingLength %d", ERROR_LOCATION, lnByteCount, lstSendDataUnit.nPendingLength);
      LogMessage(FATAL, m_cErrorString);
    }    
  }
  
  return;
}

void TCPServer::CommandThreadProc()
{
  std::string errstring("Command Thread: Pinned to Core:" + std::to_string(m_nCommandCoreId)); 
  TaskSet((unsigned int)m_nCommandCoreId, 0, errstring);
  m_cLogger.log(INFO , errstring);
  
  tagCommandMsg lstCommandMsg;
  char          lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1] = {0};  
  
  while(m_nShutdown == 0)
  {
    if(!m_pcCommandQ->dequeue(lstCommandMsg))
    {
      continue;
    }
    
    int lnMsgCode = *(int*)lstCommandMsg.cBuffer;
    switch(lnMsgCode)
    {
      case 1200:
      {
        tagLogoffDetails* lpstLogoffDetails = (tagLogoffDetails*)lstCommandMsg.cBuffer;
        if(lstCommandMsg.nLength != sizeof(tagLogoffDetails))
        {
          snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): size mismatch in logoff case..", ERROR_LOCATION);
          LogMessage(ERROR, lcErrorString);          
          continue;
        }
        ProcessLogoffRequest(*lpstLogoffDetails);
      }
      break;
      
      default:
      {
        snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): Invalid MsgCode %d received in CommandThreadProc", ERROR_LOCATION, lnMsgCode);
        LogMessage(ERROR, lcErrorString);          
      }
      break;
    }
  }
    
  return;
}

bool TCPServer::SetClientConfiguration(int nSockFd)
{
	int lnRetVal = 0;
//	int lnNonBlocking = 1;
//	if ((ioctl(nSockFd, FIONBIO, &lnNonBlocking)) == -1)	
//	{
//    snprintf(m_cAcceptString, sizeof(m_cAcceptString), "%s-%s(%d): Setting FIONBIO failed for FD(%d)", ERROR_LOCATION, nSockFd);
//    LogMessage(ERROR, m_cAcceptString);          
//		return false;
//	}
  
  int flags = fcntl(nSockFd, F_GETFL, 0);
  fcntl(nSockFd, F_SETFL, flags | O_NONBLOCK);  
  
	//fcntl(nSockFd, F_SETFL, O_NONBLOCK);		//We are using Non Blocking Sockets
  //char flag = 0;
  //setsockopt(nSockFd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof flag);
  
  int lnOptVal = 1;
  lnRetVal = setsockopt(nSockFd, SOL_TCP, TCP_NODELAY, (const void *)(&lnOptVal), sizeof(lnOptVal));       
	if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cAcceptString, sizeof(m_cAcceptString), "%s-%s(%d): Setting TCP_NODELAY failed for FD(%d)", ERROR_LOCATION, nSockFd);
    LogMessage(ERROR, m_cAcceptString);          
    return false;
  }
  
	lnOptVal = m_nSendBufferSize;
  lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_SNDBUF, (const void *)&lnOptVal, sizeof(lnOptVal));
  if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting SO_SNDBUF Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    return SYSTEM_CALL_ERROR;
  }

	lnOptVal = m_nRecvBufferSize;
  lnRetVal = setsockopt(nSockFd, SOL_SOCKET, SO_RCVBUF, (const void *)&lnOptVal, sizeof(lnOptVal));
  if (SYSTEM_CALL_ERROR == lnRetVal)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Setting SO_RCVBUF Option failed FD(%d) reason(%s)", ERROR_LOCATION, nSockFd, strerror(errno));
    LogMessage(ERROR, m_cErrorString);    
    return SYSTEM_CALL_ERROR;
  }
  
  return true;
}

bool TCPServer::Logon(int nSockFd, const sockaddr_in& lstClientAddr)
{
	int lnThreadIdx = UNDEFINED_THREAD_IDX;
	int lnFdIdx = UNDEFINED_THREAD_IDX;
	int lnCtlRetVal = 0;
	bool lbRetVal = false;
	epoll_event events;
  
	MessageConnectEventNotification lstConnectEventNotification;
	StoreConnectionNetworkInfo(nSockFd, lstClientAddr, &lstConnectEventNotification);	
	
	g_stFdVersionManager.AddVersion(nSockFd);
	lbRetVal = AddNewConnection(nSockFd, &lnThreadIdx, &lnFdIdx);
	if (false == lbRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): adding recv thread for FD(%d)", ERROR_LOCATION, nSockFd);
    LogMessage(ERROR);
    
		RemoveConnection(nSockFd, lnThreadIdx, lnFdIdx);
		return false;
	}

	lbRetVal = g_cReadyFdsInfo.AddRecvConnectionIdx(nSockFd, lnThreadIdx, lnFdIdx);
	if (false == lbRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): adding send thread for FD(%d)", ERROR_LOCATION, nSockFd);
    LogMessage(ERROR);

		RemoveConnection(nSockFd, lnThreadIdx, lnFdIdx);
		return false;
	}
	SetConnectionLoginStatus(lnThreadIdx, lnFdIdx, CLIENT_LOGGED_IN);
                   
  ConnectionNotifierCallBack((char*)&lstConnectEventNotification, sizeof(MessageConnectEventNotification)); 
  
  lbRetVal = AddNewConnectionInSend(nSockFd, &lnThreadIdx, &lnFdIdx);  
	if (false == lbRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): adding send thread for FD(%d)", ERROR_LOCATION, nSockFd);
    LogMessage(ERROR);
    
		RemoveConnection(nSockFd, lnThreadIdx, lnFdIdx);
		return false;
	}  
  
  g_cReadyFdsInfo.AddSendConnectionIdx(nSockFd, lnThreadIdx, lnFdIdx);  
  SetSendConnectionLoginStatus(lnThreadIdx, lnFdIdx, CLIENT_LOGGED_IN);  
        
	// This fd should added to epoll controller after fd is marked as loggedin so
	// that it can be process in recv loop
	events.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
	events.data.fd = nSockFd;
	lnCtlRetVal = epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, nSockFd, &events );
	if (SYSTEM_CALL_ERROR == lnCtlRetVal)
	{
		//Log error message
		//g_cLogger.LogMessage("%s-%s(%d): Error in adding fd(%d) to epollcontroller in login, error(%s)", ERROR_LOCATION, g_stControllerInfo.m_nEpollFd, strerror(errno));
		RemoveConnection(nSockFd, lnThreadIdx, lnFdIdx);
		return false;
	}
   
	return true;
}

bool TCPServer::RemoveConnectionFromController(int nFd)
{
	int lnRetVal = 0;
	epoll_event event;
	
	lnRetVal = epoll_ctl(m_nEpollFd, EPOLL_CTL_DEL, nFd, &event);
	if (SYSTEM_CALL_ERROR == lnRetVal)
	{
    char   lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): Error while deleting connection fd(%d), error(%s)", ERROR_LOCATION, nFd, strerror(errno));
    LogMessage(ERROR, lcErrorString);
    return false;
	}
  
  return true;
}

bool TCPServer::AddEpollout(int nFd)
{
	epoll_event events;
	// Since events are replaced, adding epollout with all other events is required
	events.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
	events.data.fd = nFd;

	const int lnCtlRetVal = epoll_ctl(m_nEpollFd, EPOLL_CTL_MOD,  nFd, &events);
	if (SYSTEM_CALL_ERROR == lnCtlRetVal)
	{
    char   lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): adding epollout for connection(%d),error(%s)", ERROR_LOCATION, nFd, strerror(errno));
    LogMessage(ERROR, lcErrorString);
    return false;    
	}
	return true;
}

bool TCPServer::RemoveEpollout(int nFd)
{
	epoll_event events;

	// Adding all events except epollout amounts to removing epollout
	events.events = EPOLLET | EPOLLIN | EPOLLERR | EPOLLHUP;
	events.data.fd = nFd;

	const int lnCtlRetVal = epoll_ctl(m_nEpollFd, EPOLL_CTL_MOD, nFd,  &events);
	if (SYSTEM_CALL_ERROR == lnCtlRetVal)
	{
    char   lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
    snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): removing epollout for connection(%d),error(%s)", ERROR_LOCATION, nFd, strerror(errno));
    LogMessage(ERROR, lcErrorString);    
    return false;    
	}
	return true;
}


void TCPServer::StoreConnectionNetworkInfo(int nSockFd, const sockaddr_in& addr, MessageConnectEventNotification* pstConnectEventNotification)
{
	char lcIpAddr[IP_ADDRESS_LEN + 1] = {0};
	const char* lpcAddr = inet_ntop(AF_INET, &(addr.sin_addr), lcIpAddr, IP_ADDRESS_LEN);
	uint16_t lnClientPortNumber  = ntohs(addr.sin_port);
	pstConnectEventNotification->nKeyId = nSockFd;
	pstConnectEventNotification->stCommsMessageHeader.nMessageCode = 16000;
	strncpy(pstConnectEventNotification->cClientAddress, lcIpAddr, IP_ADDRESS_LEN);
	pstConnectEventNotification->nPortNumber = lnClientPortNumber;
  
  snprintf(m_cAcceptString, sizeof(m_cAcceptString), "%s-%s(%d): Connection FD(%d) received from IP Address(%s) Port(%d)", ERROR_LOCATION, nSockFd, lpcAddr, lnClientPortNumber);
  LogMessage(INFO, m_cAcceptString) ;
  return;
}


bool TCPServer::AddNewConnection(const int nFd, int* nThreadIdx, int* nFdIdx)
{
  *nThreadIdx  = 0;
  bool lbRetVal = true;
  int lnConnectionIdx = 0;
  tagRecvInfoForThread& lcRecvInfoForThread = m_stRecvInfoForThread;

  for (lnConnectionIdx = 0; lnConnectionIdx < lcRecvInfoForThread.m_inTotalConnectionCount; ++lnConnectionIdx)
  {
    tagRecvInfoForConnection* lpRecvInfo = lcRecvInfoForThread.m_ivRecvMessages[lnConnectionIdx];
    if (ERASE_FD_VALUE == lpRecvInfo->m_nFd)
    {
      lpRecvInfo->m_nFd = nFd;
      *nFdIdx = lnConnectionIdx;
      ++(lcRecvInfoForThread.m_inActiveConnections);
      break;
    }
  }
  
  char   lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
  snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): Recv FD(%d) ConnectionIdx(%d)", ERROR_LOCATION, nFd, lnConnectionIdx);
  LogMessage(INFO, lcErrorString);    
  
  if(lnConnectionIdx == lcRecvInfoForThread.m_inTotalConnectionCount)
  {
    lbRetVal = false;
  }
  
  return lbRetVal;
}

bool TCPServer::AddNewConnectionInSend(const int nFd, int* nThreadIdx, int* nFdIdx)
{
  *nThreadIdx  = 0;
  bool lbRetVal = true;
  int lnConnectionIdx = 0;
  tagSendThreadInfo& lcSendThreadInfo = m_stSendThreadInfo;
  for (lnConnectionIdx = 0; lnConnectionIdx < lcSendThreadInfo.m_inTotalConnectionCount; ++lnConnectionIdx)
  {
    tagSendConnectionInfo* lpSendConnectionInfo = lcSendThreadInfo.m_ivSendMessages[lnConnectionIdx];
    if (ERASE_FD_VALUE == lpSendConnectionInfo->m_nFd)
    {
      lpSendConnectionInfo->m_nFd = nFd;
      //gettimeofday(&(lpRecvInfo->m_stUnprocessedData.m_stLastRecvTimeStamp), NULL);
      *nFdIdx = lnConnectionIdx;
      ++(lcSendThreadInfo.m_inActiveConnections);
      break;
    }
  }
  
  char   lcErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
  snprintf(lcErrorString, sizeof(lcErrorString), "%s-%s(%d): Send FD(%d) ConnectionIdx(%d)" , ERROR_LOCATION, nFd, lnConnectionIdx);
  LogMessage(INFO, lcErrorString);    
  
  if(lnConnectionIdx == lcSendThreadInfo.m_inTotalConnectionCount)
  {
    lbRetVal = false;
  }
  
  return lbRetVal;
}

bool TCPServer::RemoveConnection(int nFd, int nThreadIdx, int nFdIdx)
{
  bool lbRetVal = true;
  
  tagRecvInfoForConnection* lpConnectionInfo = m_stRecvInfoForThread.m_ivRecvMessages[nFdIdx];
  if (nFd == lpConnectionInfo->m_nFd)
  {
    lpConnectionInfo->m_inLoginStatus = CLIENT_NOT_LOGGED_IN;
    lpConnectionInfo->m_nFd = ERASE_FD_VALUE;
    --(m_stRecvInfoForThread.m_inActiveConnections);        
  }
  return lbRetVal;  
}

void TCPServer::SetConnectionLoginStatus(int nThreadIdx, int nFdIdx, int nStatus)
{
  m_stRecvInfoForThread.m_ivRecvMessages[nFdIdx]->m_inLoginStatus = nStatus;//  SetConnectionLoginStatus(nStatus);
}

void TCPServer::SetSendConnectionLoginStatus(int nThreadIdx, int nFdIdx, int nStatus)
{
  m_stSendThreadInfo.m_ivSendMessages[nFdIdx]->m_inLoginStatus = nStatus;//  SetConnectionLoginStatus(nStatus);
}

bool TCPServer::SendMessageToClient(int nFd, char* pcMessage, int nLength)    
{
	int lnThreadIdx = UNDEFINED_THREAD_IDX; 
	int lnFdIdx = UNDEFINED_FD_IDX;	
  
  if(nLength > MAX_SEND_MSG_LENGTH || nLength <= 0)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Invalid send Length.Max supported Length %d and Received Length %d", ERROR_LOCATION, MAX_SEND_MSG_LENGTH, nLength);
    LogMessage(ERROR);
    return false;
  }

	bool lbIdxRetVal = g_cReadyFdsInfo.GetSendThreadAndFdIdx(nFd, &lnThreadIdx, &lnFdIdx);
	if (false == lbIdxRetVal)
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Client with FD(%d) not found", ERROR_LOCATION, nFd);
    LogMessage(ERROR);
		return false;
	}

	if ((UNDEFINED_THREAD_IDX == lnThreadIdx ) || (UNDEFINED_FD_IDX == lnFdIdx ))
	{
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Invalid ThreadID and FDIDx for FD(%d) not found", ERROR_LOCATION, nFd);
    LogMessage(ERROR);
		return false;
	}
  
  if(CLIENT_LOGGED_IN != m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_inLoginStatus)
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): Client is login status is invalid for FD(%d)", ERROR_LOCATION, nFd);
    LogMessage(ERROR);
  	return false;
  }
  
  if(nFd != m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd)  
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): System level critical error.Invalid FD(%d) and Internal FD(%d)", ERROR_LOCATION, nFd, m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_nFd);
    LogMessage(ERROR);
		return false;    
  }
  
  static int lnSequenceNo = 0;
  tagSendDataUnit  m_stSendDataUnit;
  m_stSendDataUnit.nFd = nFd;
  //m_stSendDataUnit.nLength = nLength + sizeof(int);
  m_stSendDataUnit.nPendingLength = nLength + sizeof(int);
  m_stSendDataUnit.nSendLength = 0;
  m_stSendDataUnit.nSequenceNo = ++lnSequenceNo;
  
  memcpy(m_stSendDataUnit.cBuffer, &nLength, sizeof(int));
  memcpy(m_stSendDataUnit.cBuffer + sizeof(int), pcMessage, nLength);
  
  if(!m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->pcSendQ->enqueue(m_stSendDataUnit))
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): send enqueue failed for FD(%d)", ERROR_LOCATION, nFd);
    LogMessage(ERROR);
    return false;
  }
  
  tagEpollEventDetails lstEpollEventDetails;
  
  lstEpollEventDetails.nFd = nFd;
  lstEpollEventDetails.nFdIdx = lnFdIdx;
    
  if(!m_pcComonSendQ->enqueue(lstEpollEventDetails))
  {
    snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): send enqueue failed in common Q for FD(%d)", ERROR_LOCATION, nFd);
    LogMessage(ERROR);
  }
  
  return true;
}

bool TCPServer::ProcessLogoffRequest(const tagLogoffDetails& stLogoffDetails)
{
  char lcInfoString[TCP_SERVER_ERROR_STR_LEN + 1] = {0};  
  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Logoff received for FD(%d) FdVersion %d OriginatedFrom %hd Reason %hd", ERROR_LOCATION, stLogoffDetails.nFd, 
                                                                                stLogoffDetails.cFdVersion, stLogoffDetails.nOriginatedFrom, stLogoffDetails.nReason);
  LogMessage(INFO, lcInfoString);
  
	int lnRecvThreadIdx = UNDEFINED_THREAD_IDX;
	int lnRecvFdIdx = UNDEFINED_FD_IDX;
	int lnSendThreadIdx = UNDEFINED_THREAD_IDX;
	int lnSendFdIdx = UNDEFINED_FD_IDX;

	bool lbRetVal = false;

	lbRetVal = g_cReadyFdsInfo.GetRecvThreadAndFdIdx(stLogoffDetails.nFd, &lnRecvThreadIdx, &lnRecvFdIdx);	
	if (false == lbRetVal)
	{
    snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): FD(%d) not found in logoff", ERROR_LOCATION, stLogoffDetails.nFd);
    LogMessage(INFO, lcInfoString);
		return false;
	}

	//In case send thread it has to return or not still has to decide

	lbRetVal = g_cReadyFdsInfo.GetSendThreadAndFdIdx(stLogoffDetails.nFd, &lnSendThreadIdx, &lnSendFdIdx);	

	unsigned char lcSockVersion;
	lbRetVal = g_stFdVersionManager.GetVersion(stLogoffDetails.nFd, &lcSockVersion);
	if (false == lbRetVal)
	{
    snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Get version of socket FD(%d) not found.", ERROR_LOCATION, stLogoffDetails.nFd);
    LogMessage(INFO, lcInfoString);
		return false;
	}
  
  if(m_stSendThreadInfo.m_ivSendMessages[lnSendFdIdx]->m_bIsWritable == false)
  {
    if(!RemoveEpollout(stLogoffDetails.nFd))
    {
      snprintf(m_cErrorString, sizeof(m_cErrorString), "%s-%s(%d): RemoveEpollout failed for FD(%d).", ERROR_LOCATION, stLogoffDetails.nFd);
      LogMessage(ERROR, m_cErrorString);
    }
  }
  
	int lnRecvLoginStatus = m_stRecvInfoForThread.m_ivRecvMessages[lnRecvFdIdx]->m_inLoginStatus;
	if ((CLIENT_NOT_LOGGED_IN != lnRecvLoginStatus) && (stLogoffDetails.cFdVersion == lcSockVersion))
	{
    m_stRecvInfoForThread.m_ivRecvMessages[lnRecvFdIdx]->Reset();
    --m_stRecvInfoForThread.m_inActiveConnections;
  }
  else
  {
    snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Version mismatch for socket FD(%d) Current SockVersion %d and Received SockVersion %d.", ERROR_LOCATION, stLogoffDetails.nFd, lcSockVersion, stLogoffDetails.cFdVersion);
    LogMessage(ERROR, lcInfoString);
		return false;  
  }
  
  int lnCount = 0;
  while(!m_stSendThreadInfo.m_ivSendMessages[lnSendFdIdx]->pcPartialSendQ->isEmpty() && lnCount++ < 10)
  {
    //memcpy(lstCommandMsg.cBuffer, &lstLogoffDetails, lstCommandMsg.nLength);
    snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Waiting for Partial send to clear for FD(%d) and Count %d.", ERROR_LOCATION, stLogoffDetails.nFd, lnCount);
    LogMessage(INFO, lcInfoString);
    usleep(250000);
    continue;
  }
  
  if(stLogoffDetails.nLength != 0)
  {
    send(stLogoffDetails.nFd, (const char*)&stLogoffDetails.cBuffer, stLogoffDetails.nLength, 0);
  }
  
//  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): case1  for FD(%d)", ERROR_LOCATION, stLogoffDetails.nFd);
//  LogMessage(INFO, lcInfoString);  
  
  //Do I need to take care partial send....
  m_stSendThreadInfo.m_ivSendMessages[lnSendFdIdx]->Reset();
  --m_stSendThreadInfo.m_inActiveConnections;
  
//  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Case 2  for FD(%d)", ERROR_LOCATION, stLogoffDetails.nFd);
//  LogMessage(INFO, lcInfoString);  
  
  
  --m_stListenerInfo.m_inTotalConnections;

  g_cReadyFdsInfo.RemoveConnectionIdxInfo(stLogoffDetails.nFd);
  g_stFdVersionManager.IncrementVersion(stLogoffDetails.nFd);
  
//  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Case 3  for FD(%d)", ERROR_LOCATION, stLogoffDetails.nFd);
//  LogMessage(INFO, lcInfoString);  
  
  
  RemoveConnectionFromController(stLogoffDetails.nFd);

//  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Case 4  for FD(%d)", ERROR_LOCATION, stLogoffDetails.nFd);
//  LogMessage(INFO, lcInfoString);  
  
  shutdown(stLogoffDetails.nFd, SHUT_RDWR);
  close(stLogoffDetails.nFd);  
  
//  snprintf(lcInfoString, sizeof(lcInfoString), "%s-%s(%d): Case 5  for FD(%d)", ERROR_LOCATION, stLogoffDetails.nFd);
//  LogMessage(INFO, lcInfoString);  
  
	MessageDisconnectEventNotification lstDisconnectEventNotification;
	lstDisconnectEventNotification.stCommsMsgHeader. nMessageCode =	16001;
	lstDisconnectEventNotification.stCommsMsgHeader.nTimeStamp = 0;
	lstDisconnectEventNotification.stCommsMsgHeader.nMessageLength = sizeof(MessageDisconnectEventNotification);
	lstDisconnectEventNotification.nKeyId =	stLogoffDetails.nFd;
	lstDisconnectEventNotification.nEventInitiatorReference	= stLogoffDetails.nReason;
	ConnectionNotifierCallBack((char*)&lstDisconnectEventNotification, sizeof(MessageDisconnectEventNotification));
  
  snprintf(m_cCommandString, sizeof(m_cCommandString), "%s-%s(%d): FD(%d) is logoff", ERROR_LOCATION, stLogoffDetails.nFd);
  LogMessage(INFO, m_cCommandString) ;
  
  return true;
}

void TCPServer::AddConnectionToLogoff(int nRecvFdIdx, int nOriginatedFrom, int nReason)
{
  int lnLoginStatus = m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_inLoginStatus;
  if (!(lnLoginStatus & CLIENT_LOGOFF_IN_PROGRESS))
  {
    lnLoginStatus = (lnLoginStatus | CLIENT_LOGOFF_IN_PROGRESS);
    m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_inLoginStatus = lnLoginStatus;
    unsigned char lcVersion = 0;
    if (g_stFdVersionManager.GetVersion(m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_nFd, &lcVersion) == false)
    {
      snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): FD(%d) not found in getting version", ERROR_LOCATION, m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_nFd);
      LogMessage(ERROR, m_cRecevString) ;
    }
    
    int lnThreadIdx = UNDEFINED_THREAD_IDX;
    int lnFdIdx     = UNDEFINED_FD_IDX;
    if(g_cReadyFdsInfo.GetSendThreadAndFdIdx(m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_nFd, &lnThreadIdx, &lnFdIdx))
    {
      m_stSendThreadInfo.m_ivSendMessages[lnFdIdx]->m_inLoginStatus = CLIENT_LOGOFF_IN_PROGRESS;
      snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): Send FD(%d) and Send Connection Idx %d", ERROR_LOCATION, m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_nFd, lnFdIdx);
      LogMessage(INFO, m_cRecevString) ;
    }
    
    AddLogoffCommand(m_stRecvInfoForThread.m_ivRecvMessages[nRecvFdIdx]->m_nFd, lcVersion, nOriginatedFrom, nReason);
  }
}

void TCPServer::AddLogoffCommand(int nSockFd, unsigned char cFdVersion, int nOriginatedFrom, int nReason, char* pcMessage, int nLength)
{
	tagLogoffDetails lstLogoffDetails;
  memset(&lstLogoffDetails, 0, sizeof(tagLogoffDetails));
  
	lstLogoffDetails.nFd = nSockFd;
	lstLogoffDetails.nMessageCode = 1200;
	lstLogoffDetails.cFdVersion = cFdVersion;
	lstLogoffDetails.nOriginatedFrom = nOriginatedFrom;
	lstLogoffDetails.nReason = nReason;

  //Append sizeof(int)
  memcpy(lstLogoffDetails.cBuffer, &nLength, sizeof(int));
  memcpy(lstLogoffDetails.cBuffer + sizeof(int), pcMessage, nLength);
  lstLogoffDetails.nLength = nLength + sizeof(int);  
  
  tagCommandMsg lstCommandMsg;
  memset(&lstCommandMsg, 0, sizeof(tagCommandMsg));
  
  lstCommandMsg.nLength = sizeof(tagLogoffDetails);  
  memcpy(lstCommandMsg.cBuffer, &lstLogoffDetails, lstCommandMsg.nLength);
  
  if(!m_pcCommandQ->enqueue(lstCommandMsg))
  {
    snprintf(m_cRecevString, sizeof(m_cRecevString), "%s-%s(%d): enqueue failed in command FD(%d)", ERROR_LOCATION, nSockFd);
    LogMessage(ERROR, m_cRecevString) ;  
    return;
  }
  return;
	
}
