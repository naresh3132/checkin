#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/time.h>
 #include <unistd.h>
#include <sys/fcntl.h>

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <tbb/tbb.h>
#include "tbb/scalable_allocator.h"
#include "tbb/concurrent_hash_map.h"

#include "spsc_atomic.h"
//#include "mpsc_atomic.h"
#include "log.h"

#include "callbackmessages.h"


typedef short (*FpRecvMessageCallback)(int64_t nKey, char* pcMessage, int nLength);
typedef short (* FpConnectionNotifierCallBack)(char* pcConnectionStatus, int nLength);

#define IP_ADDRESS_LEN              32
#define SYSTEM_CALL_ERROR           -1
#define TCP_KEEP_ALIVE_DISABLE      0
#define TCP_KEEP_ALIVE_ENABLE       1
#define LISTENER_POLL_SIZE          5
#define MAX_LISTENER_EVENTS         1
#define LISTENER_EPOLL_WAIT_TIME		1000
#define CONTROLLER_EPOLL_WAIT_TIME	0 //1000
#define UNDEFINED_THREAD_IDX				-1
#define UNDEFINED_FD_IDX            -1
#define ERASE_FD_VALUE              0

#define MAX_SEND_MSG_LENGTH         2048
#define MAX_RECEIVE_MSG_LENGTH      2048
#define NUMBER_OF_RECV_THREAD       1 
#define NUMBER_OF_SEND_THREAD       1
#define TCP_SERVER_ERROR_STR_LEN    512

#define COND_WAIT_TIME_INTERVAL			(50000000)	//50millisec
#define SEC_IN_NANO_SEC							(1000000000L)
#define SEC_IN_MICRO_SEC						(1000000L)

#define ERROR_LOCATION              __func__, __FILE__, __LINE__
#define LOGOFF_FROM_OTHER           0

typedef std::vector <int> ErrHupFdStore;

enum EConnectionStatusMarker
{
	CLIENT_LOGGED_IN								= 0,
	CLIENT_NOT_LOGGED_IN						= 1,
	CLIENT_LOGOFF_IN_PROGRESS				= 2,
	LOGOFF_FROM_RECV								= 4,
	LOGOFF_FROM_SEND								= 32
};

enum EpollError
{
	EP_OK									= 0,
	EP_DISCONNECTION_UNDEFINED				= -3000,
	EP_DISCONNECTION_TIMEOUT          = -2999,//because of keep alive  
	EP_CLIENT_ACTIVATED               = -2998,//client intiated 
	EP_ABRUPT_DISCONNECTION           = -2997,//client clicks on cross button or end task i.e no close socket issue by client app 
	EP_SERVER_ACTIVATED               = -2996,//server initiates close socket can be business integrity failure 
	EP_LOW_LEVEL_ERROR                = -2995,//protocol error i.e 5 bytes header, uncompression or fragmentation error 
	EP_ERROR_PROCESSING_MESSAGE       = -2994,// 
	EP_THROTTLER_ACTIVATED            = -2993	//bytes count throttling limits 
};


struct tagRecvDataUnit
{
  char          cBuffer[MAX_RECEIVE_MSG_LENGTH];
  int           nLength;  
  
//  tagRecvDataUnit()
//  {
//    Reset();
//  }
  
//  tagRecvDataUnit(const tagRecvDataUnit& stRecvDataUnit)
//  {
//    if(this == &stRecvDataUnit)
//        return;
//    
//    Copy(stRecvDataUnit);    
//  }
//  
//  tagRecvDataUnit& operator=(const tagRecvDataUnit& stRecvDataUnit)
//  {
//    if(this == &stRecvDataUnit)
//        return *this;
//    
//    Copy(stRecvDataUnit);    
//    return *this;
//  }  
  
  void Reset()
  {
    nLength   = 0;  
    memset(cBuffer, 0, MAX_RECEIVE_MSG_LENGTH);  
  }
  
  private:
    void Copy(const tagRecvDataUnit& stRecvDataUnit)
    {
      nLength = stRecvDataUnit.nLength;     
      memcpy(cBuffer, stRecvDataUnit.cBuffer, nLength);    
      return;      
    }
};

struct tagSendDataUnit
{
  int           nFd;  
  char          cBuffer[MAX_SEND_MSG_LENGTH + sizeof(int)];
  //int           nLength;  
  int           nSendLength;  
  int           nPendingLength;    
  int           nSequenceNo;
};

typedef struct tagListenerInfo
{
  int                m_nEpollAcceptFd;
  int                m_nListenerFd;
  int                m_nShutdown;		
  std::atomic <int>  m_inTotalConnections;    
		
  tagListenerInfo() : m_nEpollAcceptFd(0), m_nListenerFd(0), m_nShutdown(0)
  {
    m_inTotalConnections = 0;	
  }

  tagListenerInfo(const tagListenerInfo& other) :	m_nEpollAcceptFd(other.m_nEpollAcceptFd),
                                                  m_nListenerFd(other.m_nListenerFd),
                                                  m_nShutdown(other.m_nShutdown)
  {
    int lnTotalConnections = other.m_inTotalConnections;
    m_inTotalConnections = lnTotalConnections;
  }

  tagListenerInfo& operator=(const tagListenerInfo& other)
  {
    if (&other != this)
    {
      m_nEpollAcceptFd        = other.m_nEpollAcceptFd;
      m_nListenerFd           = other.m_nListenerFd;
      m_nShutdown             = other.m_nShutdown;
      int lnTotalConnections  = other.m_inTotalConnections;      
      m_inTotalConnections    = lnTotalConnections;
    }
    return *this;
  }
}tagListenerInfo;

struct tagEpollEventDetails
{
 int          nFd;
 unsigned int nEpollEvent;
 int          nFdIdx;
};


typedef struct tagFdIndexInThreads
{
  short         m_nRecvThreadIdx;
  short         m_nSendThreadIdx;
  int           m_nRecvFdIdx;
  int           m_nSendFdIdx;

  tagFdIndexInThreads() : m_nRecvThreadIdx(UNDEFINED_THREAD_IDX), m_nSendThreadIdx(UNDEFINED_THREAD_IDX),
              m_nRecvFdIdx(UNDEFINED_FD_IDX), m_nSendFdIdx(UNDEFINED_FD_IDX)
  {
  }	

  tagFdIndexInThreads(short nRecvThreadIdx, short nSendThreadIdx, int nIdxInRecvThread, int nSendFdIdx) : 
            m_nRecvThreadIdx(nRecvThreadIdx),
            m_nSendThreadIdx(nSendThreadIdx), 
            m_nRecvFdIdx(nIdxInRecvThread),
            m_nSendFdIdx(nSendFdIdx)
  {
  }	

  tagFdIndexInThreads(const tagFdIndexInThreads& other) :		
                  m_nRecvThreadIdx(other.m_nRecvThreadIdx),
                  m_nSendThreadIdx(other.m_nSendThreadIdx),
                  m_nRecvFdIdx(other.m_nRecvFdIdx),
                  m_nSendFdIdx(other.m_nSendFdIdx)

  {
  }

  tagFdIndexInThreads& operator=(const tagFdIndexInThreads& other)
  {
    if (&other != this)
    {
      m_nRecvThreadIdx = other.m_nRecvThreadIdx;
      m_nSendThreadIdx = other.m_nSendThreadIdx;
      m_nRecvFdIdx = other.m_nRecvFdIdx;
      m_nSendFdIdx = other.m_nSendFdIdx;
    }
    return *this;
  }
}tagFdIndexInThreads;

typedef std::vector<int> ReadyFdInfoStore;
typedef tbb::concurrent_hash_map<int, tagFdIndexInThreads, tbb::tbb_hash_compare <int>, 
									 tbb::scalable_allocator<std::pair<int, tagFdIndexInThreads> > > FdIndexRegisterHash;

typedef tbb::concurrent_hash_map<int, unsigned char, tbb::tbb_hash_compare <int>, 
								 tbb::scalable_allocator<std::pair<int, unsigned char> > > ConnectionVersionStore;

typedef struct tagReadyFdInfoForThread
{
  tagReadyFdInfoForThread() 
  {	
    m_ibIsDirty = false; 
  }

  tagReadyFdInfoForThread(int nReserve, bool bIsDirty)
  {	
    //Copy constructor does not have assignment operator
    m_ibIsDirty = bIsDirty;   
    m_vReadyInfoForThread.reserve(nReserve);
  }

  tagReadyFdInfoForThread(const tagReadyFdInfoForThread& other)  : m_vReadyInfoForThread(other.m_vReadyInfoForThread)	
  {
    const bool lbIsDirty = other.m_ibIsDirty;    
    m_ibIsDirty = lbIsDirty;
  }

  tagReadyFdInfoForThread& operator=(const tagReadyFdInfoForThread& other) 
  {
    if (&other != this)
    {
      const bool lbIsDirty = other.m_ibIsDirty;          
      m_ibIsDirty = lbIsDirty;//other.m_ibIsDirty;
      m_vReadyInfoForThread = other.m_vReadyInfoForThread;
    }
    return *this;
  } 

  std::atomic <bool> m_ibIsDirty;
  ReadyFdInfoStore m_vReadyInfoForThread;
}tagReadyFdInfoForThread;

struct tagLogoffDetails
{
  short int       nMessageCode;
  int             nFd;  
  unsigned char   cFdVersion;
  short           nOriginatedFrom;
  short           nReason;
  short           nLength;
  char            cBuffer[1024];
};

#define MAX_COMMAND_BUFFER_MSG      2048
struct tagCommandMsg
{
  int             nLength;
  char            cBuffer[MAX_COMMAND_BUFFER_MSG];
  
  tagCommandMsg()
  {
    nLength = 0;
    memset(cBuffer, 0, MAX_COMMAND_BUFFER_MSG);
  }
};


typedef std::vector <tagReadyFdInfoForThread> ReadyFdInfoForAllThread;

class TCPServer;
class CReadyFdsForAllThread
{
private:
  FdIndexRegisterHash       m_ihFdThreadAndIndexInfo;
  ReadyFdInfoForAllThread   m_vReadyFdInfoForAllThread;
  TCPServer*                m_pcTCPServer;

public:

  CReadyFdsForAllThread(TCPServer* pcTCPServer);
  
  void ThreadReadyFdStoreReserved(int nReserve);		
  void ThreadReadyFdInit(int nReserve); // First read thread must come according to thread number then after write thread
  
  bool GetRecvThreadAndFdIdx(int nFd, int* nThreadIdx, int* nFdIdx);
  bool GetSendThreadAndFdIdx(int nFd, int* nThreadIdx, int* nFdIdx) const ;

  bool AddFdIdxInThreadStore(const int nFd, const unsigned int nEpollEvent, ReadyFdInfoStore* pvReadyFdInfoStore);
  bool AddRecvConnectionIdx(int nFd, int nThreadIdx, int nFdIdx);
  bool AddSendConnectionIdx(int nFd, int nThreadIdx, int nFdIdx);
  bool RemoveConnectionIdxInfo(int nSockFd);
  bool IsConnectionValid(const int& nFd, int& nRecvThreadIdx, int& nRecvThreadFdIdx);
};

class FdVersionManager
{
  public:
    FdVersionManager();
    ~FdVersionManager();
    ConnectionVersionStore m_ihFdVersionStore;
    bool AddVersion(int nSockFd);
    bool GetVersion(int nSockFd, unsigned char* pcVersion);
    void IncrementVersion(int nSockFd);
};

#define MAX_INTERNAL_BUFFER_LEN 1048576

struct tagRecvInfoForConnection
{
  int                                 m_nFd;
  std::atomic<int>                    m_inLoginStatus;
  int                                 nProcessedLength;
  int                                 nUnProcessedLength;    
  int                                 nReceivedBytes;      
  char                                cBuffer[MAX_INTERNAL_BUFFER_LEN];
  
  //ProducerConsumerQueue<tagRecvDataUnit, 10000>* pcQueue;
  
  tagRecvInfoForConnection()
  {
    Reset();
    //pcQueue = new ProducerConsumerQueue<tagRecvDataUnit, 10000>(10000);
  }
  
  void Reset()
  {
    nProcessedLength  = 0;
    nUnProcessedLength = 0;        
    nReceivedBytes = 0;
    m_inLoginStatus = CLIENT_NOT_LOGGED_IN;  
    m_nFd = ERASE_FD_VALUE;
  }
};

struct tagSendInfoForConnection
{
  int                                 nFd;
  int                                 nLength;
  char                                cBuffer[MAX_SEND_MSG_LENGTH];
};

typedef tbb::concurrent_vector <tagRecvInfoForConnection*, tbb::scalable_allocator<tagRecvInfoForConnection*> > RecvInfoForConnectionStore;

typedef struct tagRecvInfoForThread
{
  short               m_nThreadIdx;
  int                 m_nShutdown;
  std::atomic <int>   m_inActiveConnections;
  std::atomic <int>   m_inRecvLogoffFdCount;
  std::atomic <int>   m_inTotalConnectionCount;
  
  RecvInfoForConnectionStore m_ivRecvMessages;  
}tagRecvInfoForThread;

typedef TCPSERVER_QUEUE::MultipleProducerConsumerQueue<tagSendDataUnit, 10000>SendQ;
typedef TCPSERVER_QUEUE::ProducerConsumerQueue<tagSendDataUnit, 10000>PartialSendQ;

typedef struct tagSendConnectionInfo
{
  int                         m_nFd;
  std::atomic <int>           m_inLoginStatus;
  bool                        m_bIsWritable;
  SendQ*                      pcSendQ; 
  PartialSendQ*               pcPartialSendQ;
  
  tagSendConnectionInfo()
  {
    pcSendQ = NULL;
    Reset();
    pcSendQ           = new SendQ(); 
    pcPartialSendQ    = new PartialSendQ(10000);
  }
  
  void Reset()
  {
    m_nFd             = ERASE_FD_VALUE;
    m_inLoginStatus   = CLIENT_NOT_LOGGED_IN;
    m_bIsWritable     = true;
    
    tagSendDataUnit lstSendDataUnit;
    while(pcSendQ && !pcSendQ->isEmpty())
    {
      pcSendQ->dequeue(lstSendDataUnit);
    }
  }
  
}tagSendConnectionInfo;

typedef tbb::concurrent_vector<tagSendConnectionInfo*, tbb::scalable_allocator<tagSendConnectionInfo*> > ConnectionSendInfoStore;

typedef struct tagSendThreadInfo
{
  short int           m_nThreadIdx;
  int                 m_nShutdown;
  
  std::atomic <int>   m_inActiveConnections;
  std::atomic <int>   m_inRecvLogoffFdCount;
  std::atomic <int>   m_inTotalConnectionCount;
	ConnectionSendInfoStore m_ivSendMessages;  
}tagSendThreadInfo;

typedef TCPSERVER_QUEUE::MultipleProducerConsumerQueue<tagEpollEventDetails, 100000>EpollOutEventQ;
typedef TCPSERVER_QUEUE::ProducerConsumerQueue<tagEpollEventDetails, 100000>EpollEventQ;
typedef TCPSERVER_QUEUE::MultipleProducerConsumerQueue<tagCommandMsg, 1000>CommandQ;

class TCPServer
{
  friend CReadyFdsForAllThread;
  
  public:
    TCPServer(Logger& cLogger, int nNumberOfConnections, bool cEnableKeepAlive);
    ~TCPServer();

  public:
  
    bool UpdateConfigurationParameters(int nRecvBufferSize, int nSendBufferSize);    
    bool CreateTCPServerFramework(const char* pcListenerIpAddr, const unsigned short nListenerPort);    
    bool UpdateCoreId(int  nSendCoreId, int nRecvCoreId, int nControllerCoreId, int nCommandCoreId);
    bool SetConnectionCallBack(FpConnectionNotifierCallBack pConnectionNotifierCallBack);
    bool SetRecvMessageCallback(FpRecvMessageCallback pRecvMessageCallback);    
    bool SendMessageToClient(int nFd, char* pcMessage, int nLength);    
    bool StartTCPServer();
    bool StopTCPServer();    
    bool DisconnectFD(const int64_t& nFD, char* pcMessage = NULL , int nLength = 0);    

    static int TaskSet(unsigned int nCoreID_, int nPid_, std::string &strStatus);        
    
  private:
    bool CreateServerListener(const char* pcListenerIpAddr, const unsigned short nListenerPort);
    bool CreateListenerEventHandler();
    bool CreateEventHandler();
    int SetListenerConfiguration(int nSockFd);    
    bool SetClientConfiguration(int nSockFd);
    
    //bool UpdateConfigurationParameters(int nNumberOfRecvThreads, int nNumberOfSendThreads, int nRecvBufferSize, int nSendBufferSize, int nNumberOfConnections);    
    //Send Connection
    bool AddNewConnectionInSend(const int nFd, int* nThreadIdx, int* nFdIdx);
    void SetSendConnectionLoginStatus(int nThreadIdx, int nFdIdx, int nStatus);    
    bool AddPartialPacketNEpollOut(tagSendDataUnit& stSendDataUnit, int nSendFdIdx);
    
    //Receive handinng...
    bool AddNewConnection(const int nFd, int* nThreadIdx, int* nFdIdx);    
    bool RemoveConnection(int nFd, int nThreadIdx, int nFdIdx);    
    void SetConnectionLoginStatus(int nThreadIdx, int nFdIdx, int nStatus);    
    void AddConnectionToLogoff(int nRecvFdIdx, int nOriginatedFrom, int nReason);    
    
    //Connection releated
    bool RemoveConnectionFromController(int nFd);
    bool AddEpollout(int nFd);
    bool RemoveEpollout(int nFd);    
    bool Logon(int nSockFd, const sockaddr_in& lstClientAddr);    
    void StoreConnectionNetworkInfo(int nSockFd, const sockaddr_in& addr, MessageConnectEventNotification* pstConnectEventNotification);    
    
    //CommandProcessor
    void AddLogoffCommand(int nSockFd, unsigned char cFdVersion, int nOriginatedFrom, int nReason, char* pcMessage = NULL , int nLength = 0);
    bool ProcessLogoffRequest(const tagLogoffDetails& stLogoffDetails);
    
    void AcceptThreadProc();
    void ControlThreadProc();    
    void RecvThreadProc();    
    void SendThreadProc(); 
    void CommandThreadProc();
    
    void LogMessage(const int& level)
    {
      m_cLogger.log(level, m_cErrorString);      
    }
  
  public:
    void LogMessage(const int& level, const std::string& szMsg)
    {
      m_cLogger.log(level, szMsg);      
    }
    
  private:
    FpRecvMessageCallback         RecvMessageCallback;    
    FpConnectionNotifierCallBack  ConnectionNotifierCallBack;
    tagListenerInfo               m_stListenerInfo;
    
    char                          m_cListenerIpAddr[IP_ADDRESS_LEN + 1];
    unsigned short                m_nListenerPort;   
    int                           m_nNumberOfRecvThreads;   
    int                           m_nNumberOfSendThreads;   

    int                           m_nRecvBufferSize;
    int                           m_nSendBufferSize;
    int                           m_nNumberOfConnections;
    
    int                           m_nKeepAliveCheckInterval;  
    int                           m_nTcpKeepIdle;
    int                           m_nTcp_KeepInterval;
    int                           m_nTcp_KeepCount;
    
    int                           m_nSendCoreId;  
    int                           m_nRecvCoreId;
    int                           m_nControllerCoreId;
    int                           m_nCommandCoreId;      
    
    int                           m_nEpollFd; //for data send & receive
    std::thread                   m_cAcceptThread;
    std::thread                   m_cControllerThread;    
    
    std::thread                   m_cRecvThread;        
    std::thread                   m_cSendThread;
    std::thread                   m_cCommandThread;
    
    std::atomic<int>              m_nShutdown;
    ErrHupFdStore                 m_vErrHupFds;
    
    EpollOutEventQ*               m_pcComonSendQ;     
    CommandQ*                     m_pcCommandQ;
    //tagSendDataUnit               m_stSendDataUnit;    
   
    Logger&                       m_cLogger;   
  public:
    CReadyFdsForAllThread         g_cReadyFdsInfo;
    FdVersionManager              g_stFdVersionManager;    
    tagRecvInfoForThread          m_stRecvInfoForThread;
    tagSendThreadInfo             m_stSendThreadInfo;
    EpollEventQ*                  m_pcRecevEpollEventQ;
    EpollEventQ*                  m_pcSendEpollEventQ;
    
    char                          m_cControlString[TCP_SERVER_ERROR_STR_LEN + 1];    
    char                          m_cErrorString[TCP_SERVER_ERROR_STR_LEN + 1];
    char                          m_cAcceptString[TCP_SERVER_ERROR_STR_LEN + 1];
    char                          m_cRecevString[TCP_SERVER_ERROR_STR_LEN + 1];
    char                          m_cCommandString[TCP_SERVER_ERROR_STR_LEN + 1];    
    
};

#endif