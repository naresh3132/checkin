#ifndef LOG_H_
#define LOG_H_

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>
#include <sched.h>
#include <iostream>

#define MSG_SIZE 512
#define QUEUE_SIZE 1000000

const int TRACE=0, DEBUG=1, INFO=2, ERROR=3, FATAL=4;
const std::string levels[] = {"TRACE","DEBUG","INFO","ERROR","FATAL"};


class LogMsg
{
  public:
  char _message[MSG_SIZE];
  uint32_t _level;
  LogMsg(int level, std::string message)
  {
    memset(_message,'\0',MSG_SIZE);
    _level=level;
    strcpy(_message,message.c_str());
    _message[MSG_SIZE-1]='\0';
  };
  LogMsg()
  {
    memset(_message,'\0',MSG_SIZE);
    _level=-1;
  }

  void set(int level, std::string message)
  {
    memset(_message,'\0',MSG_SIZE);
    _level=level;
    strcpy(_message,message.c_str());
    _message[MSG_SIZE-1]='\0';
  };

  void reset()
  {
    memset(_message,'\0',MSG_SIZE);
    _level=-1;
  };
};


class Logger
{
  Logger();
  Logger(const Logger&);
  Logger& operator=(const Logger&);
  
  void backgroundThread();
  
  std::thread _bgthread;

  int _logLevel;

  static std::unique_ptr<Logger> _instance;
  static std::once_flag _onceFlag;
  
  const uint32_t _size;
  std::atomic<bool>* const _flag;
  std::atomic<unsigned long long> _readIndex;
  std::atomic<unsigned long long> _writeIndex;
  
  LogMsg* const _records;
    
  bool _exit;

  public:
  ~Logger();
  static Logger& getLogger();
  void setDump();
  void setLevel(const int& level);
  
  void log(const int& level, const std::string message);
  void exit(void);
};

#endif
