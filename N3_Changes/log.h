#ifndef __LOG_H__
#define __LOG_H__

#include <iostream>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <memory>
#include <functional>
#include <sched.h>

#include <time.h>
#include <sys/time.h>

#include <fstream>
#include <unistd.h>
#include <list>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>


#define MSG_SIZE1                      512
#define LOGGER_QUEUE_SIZE             1000
#define LOGGER_ERROR_MSG_LEN					255 //THis has to be syn with Logger
#define UNLIMITED_FILE_SIZE           -1
#define LOG_FILE_NAME                 255


#define SUCCESS       0
#define FAILURE       -1

const int TRACE=0, DEBUG=1, INFO=2, ERROR=3, FATAL=4, PERF=5;
const std::string levels[] = {"TRACE","DEBUG","INFO","ERROR","FATAL","PERF"};


class LogMsg
{
  public:
  char _message[MSG_SIZE1];
  uint32_t _level;
  LogMsg(int level, std::string message)
  {
    memset(_message,'\0',MSG_SIZE1);
    _level=level;
    strcpy(_message,message.c_str());
    _message[MSG_SIZE1-1]='\0';
  };
  LogMsg()
  {
    memset(_message,'\0',MSG_SIZE1);
    _level=-1;
  }

  void set(int level, std::string message)
  {
    memset(_message,'\0',MSG_SIZE1);
    _level=level;
    strcpy(_message,message.c_str());
    _message[MSG_SIZE1-1]='\0';
  };

  void reset()
  {
    memset(_message,'\0',MSG_SIZE1);
    _level=-1;
  };
};

typedef  std::list<LogMsg*>LogMsgList;

class Logger
{
  Logger();
  Logger(const Logger&);
  Logger& operator=(const Logger&);
  
  void backgroundThread();
  
  bool GetLogMsg(LogMsg& stRecord);
  
  std::thread                     _bgthread;
  int                             _logLevel;
  static std::unique_ptr<Logger>  _instance;
  static std::once_flag           _onceFlag;
  
  std::string                     _logFilePath;
  int32_t                         _nCoreId;
  
  const uint32_t                  _size;
    
  bool                            _exit;
  LogMsgList                      m_cLogMsgList;
	std::condition_variable         _cv;   
	std::mutex                      _mutex;    

  public:
  ~Logger();
  static Logger& getLogger();
  void setDump(const std::string &filePathPrefix/*=""*/, int nCoreId);
  void setDump(int nCoreId);
  void setLevel(const int& level);
  
  void log(const int& level, const std::string message);
  void exit(void);
};

int TaskSett(unsigned int nCoreID_, int nPid_);

#endif
