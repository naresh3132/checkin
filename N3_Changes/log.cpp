#include "log.h"
#include<string.h>
using namespace std;

std::unique_ptr<Logger> Logger::_instance;
std::once_flag Logger::_onceFlag;

Logger::Logger():_logLevel(INFO), _size(LOGGER_QUEUE_SIZE), _exit(false)
{
  
};



void Logger::setLevel(const int& level)
{
  if( level < 0 || level > PERF)
  {
    return;
  }
  _logLevel = level;
};


Logger::~Logger()
{
  _bgthread.join();
}

void Logger::log(const int& level, const std::string message)
{
  if(level > PERF)
  {
    return;
  }
  
  LogMsg* lpstLogMsg = new LogMsg();
  strncpy(lpstLogMsg->_message, message.c_str(), MSG_SIZE1);
  lpstLogMsg->_level  = level;
  
  std::unique_lock<std::mutex>lcLock(_mutex);  
  const bool lbStatus = m_cLogMsgList.empty();
  m_cLogMsgList.push_back(lpstLogMsg);
  if (lbStatus)
  {
    _cv.notify_one();
  }  
  
  return;
}

void Logger::exit(void)
{
  _exit = true;
}

bool Logger::GetLogMsg(LogMsg& stRecord)
{
  std::unique_lock<std::mutex>lcLockObject(_mutex);
  while(m_cLogMsgList.empty())
  {
    _cv.wait(lcLockObject);      
  }
  
  //Call assignment operator here
  LogMsg* lpstRecord = m_cLogMsgList.front();
  memcpy(&stRecord, lpstRecord, sizeof(LogMsg));  
  
  m_cLogMsgList.pop_front();
  delete lpstRecord;
  
  return true;
}

int TaskSett(unsigned int nCoreID_, int nPid_)
{
  int nRet = 0;
 // char strErrMsg[768] = {0};
  char logBufCF[200];
  unsigned int nThreads = std::thread::hardware_concurrency();
  cpu_set_t set;
  CPU_ZERO(&set);

  if(nCoreID_ >= nThreads)
  {
    snprintf(logBufCF,200, "Error : Supplied core id %d is invalid. Valid range is 0 - %d", nCoreID_, nThreads - 1);
    Logger::getLogger().log(ERROR,std::string(logBufCF));
    nRet = -1;
  }
  else
  {
    CPU_SET(nCoreID_,&set);

    if(sched_setaffinity(nPid_,sizeof(cpu_set_t), &set) < 0)
    {
      snprintf(logBufCF,200, "Error : %d returned while setting process affinity for process ID . Valid range is 0 - %d", errno,nPid_);
      Logger::getLogger().log(ERROR,std::string(logBufCF));
      nRet = -1;
    }
  }
  return nRet;
}

void Logger::backgroundThread()
{
  std::string lszeTaskeSet("LoggerThread: Pinned to Core :" + std::to_string(_nCoreId));   
  TaskSett(_nCoreId, 0);  
  std::cout << lszeTaskeSet << std::endl;
  
  time_t now = time(0);
  struct tm tstruct;
  char cfilename[80];
  tstruct = *localtime(&now);
  strftime(cfilename,sizeof(cfilename),"_%Y%m%d",&tstruct);
  std::string filename(cfilename);
  filename = _logFilePath + filename + std::string(".log");
  
  std::ofstream outputFile;
  outputFile.open(filename,std::fstream::app);
  
  if(!outputFile.is_open())
  {
      std::cout<<"ERROR:Failed to open log file :" << filename << std::endl ;
  }

  char lcDateString[32 + 1 ] = {0};  
  struct timeval tv;
  struct tm lcTm; 
  
  LogMsg record;
  while(!_exit)
  {
    if(!GetLogMsg(record))
    {
      continue;
    }

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &lcTm);

    int lnByteCount = snprintf(lcDateString, sizeof(lcDateString), "%02d:%02d:%02d.%06ld",
																lcTm.tm_hour, lcTm.tm_min, lcTm.tm_sec, tv.tv_usec);
    
    //outputFile << levels[record._level] << ": " << lcDateString <<": " << record._message <<std::endl<<std::flush;
    outputFile << "[" << levels[record._level] << "] [" << lcDateString <<"] " << record._message <<std::endl<<std::flush;
  }
};

void Logger::setDump(const std::string &filePathPrefix/*=""*/, int nCoreId)
{
  _logFilePath = filePathPrefix;
  _nCoreId = nCoreId;
  _bgthread = std::thread(&Logger::backgroundThread, _instance.get());
};

void Logger::setDump(int nCoreId)
{
  _nCoreId = nCoreId;  
  _bgthread = std::thread(&Logger::backgroundThread, _instance.get());
};

Logger& Logger::getLogger()
{
  std::call_once(_onceFlag,[]{_instance.reset(new Logger);});
  return *_instance.get();
};
