/* 
 * File:   perf.h
 * Author: 
 *
 * Created on 24 May, 2015, 3:59 PM
 */

#include <time.h>
#include <stdint.h>
#include "log.h"
#include "perf.h"
#include <mutex>          // std::mutex, std::try_lock

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

#include "setaffinity.h"


#include <shmContainerCreator.h>

#define _USE_PTHREAD_

class PerfTimer
{
  bool            initialize;
  bool            exitFlag;  
  unsigned char   core;
#ifndef _USE_PTHREAD_
  std::thread     thTimer; 
#else
  pthread_t       thTimer;
#endif
  timespec        tsCurrTime;
  volatile int64_t currTimeStamp;
  unsigned int    refCount;
  pid_t           timerThreadId;
  
  int TimerThread()
  {
    timerThreadId = syscall(SYS_gettid) ; //gettid();
    
    std::string errstring("PerfTimer: Pinned to Core:" + std::to_string(core));  ;
    TaskSet((unsigned int)core, 0, errstring);
    Logger::getLogger().log(INFO , errstring);

    while(!exitFlag)
    {
      clock_gettime( CLOCK_TYPE , &tsCurrTime);      
      currTimeStamp = (((tsCurrTime.tv_sec * 1000000000L) + (tsCurrTime.tv_nsec)));
    }
    
    return 0;
  }
  
  static void * ThreadFunc(void *threadParam)
  {
    PerfTimer *thisObj = (PerfTimer*)threadParam;
    
    thisObj->TimerThread();
  }
  
public:
  PerfTimer(int _core): initialize(false), exitFlag(false), core(_core), currTimeStamp(0), refCount(0)
  {
    tsCurrTime.tv_sec = tsCurrTime.tv_nsec = 0;
    
    //thTimer = std::thread(&PerfTimer::TimerThread,this);
    //initialize = true;
  }
  
  int startTimer()
  {
    //if(!initialize || refCount <= 0 )
    if(!isRunning())
    {
#ifndef _USE_PTHREAD_
      //thTimer = std::thread(&PerfTimer::TimerThread,this);
#else
      int iret1 = pthread_create( &thTimer, NULL, PerfTimer::ThreadFunc, (void*) this);
#endif 
      initialize = true;
    }
    ++refCount;
  }
  
  bool isRunning()
  {
    if(timerThreadId)
    {
      return !kill(timerThreadId,0);
    }
//    if(!threadRunning.try_lock())
//    {
//      return true;
//    }
//    threadRunning.unlock();
    return false;
  }

  
  
  ~PerfTimer()
  {
    if(--refCount <= 0)
    {
      exitFlag = true;
      //thTimer.join();
      initialize = false;
    }
  }
  
  inline long GetTime()
  {
    return currTimeStamp;
  }
};

static PerfTimer *perfTimer=nullptr;


bool initPerfTimer(const std::string KeyFilePath, short timerCore)
{
  bool result = false;
  
  std::cout <<"touch: " << KeyFilePath << std::endl; 
  
  std::string strCreateKeyFile ="touch " + KeyFilePath;  
  system(strCreateKeyFile.c_str());
  
  static shmContainerCreator<PerfTimer> shmPerfTimer(KeyFilePath.c_str(),timerCore);
  perfTimer = shmPerfTimer.getPointer();
  
  if(perfTimer)
  {
    perfTimer->startTimer();
  }
  
  return !!perfTimer;
}

int64_t getPerfTime()
{
  return perfTimer->GetTime();
}

void printCoreRefTime(const char *logMsg/*=nullptr*/)
{
  TS_TYPE tsRef; 
  GET_PERF_TIME(tsRef);
  
  std::string tempStr(logMsg);
  tempStr = tempStr + std::string("CPU Core Referance Time:") + std::to_string(TIMESTAMP1(tsRef));
  std::cout<<tempStr <<std::endl;
  Logger::getLogger().log(INFO , std::move(std::string(tempStr)));
}

//bool isLatencyRequired(int msgCode)
//{
//  switch(msgCode)
//  {
//  case MSG_KEEP_ALIVE_ACK:
//    return false;
//  }
//  return true;
//}

bool logOMSLatencyEvent(short component, int &fwdPkt, int &retPkt , OMS_PERF_TIME &perfTime)
{
  char logBuffer[768]= {0};
  const long start    = TIMESTAMP1(perfTime.tsStart);
  const long fwdStart = TIMESTAMP1(perfTime.tsFwdStart);
  const long fwdEnd   = TIMESTAMP1(perfTime.tsFwdGoAhead); //TIMESTAMP(perfTime.tsFwdEnd);
  const long fwdGoAhead   = TIMESTAMP1(perfTime.tsFwdGoAhead);
  const long retStart = TIMESTAMP1(perfTime.tsRetStart);
  const long retEnd   = TIMESTAMP1(perfTime.tsRetEnd);
  const long end      = TIMESTAMP1(perfTime.tsEnd);
  const long fwdLatency  = fwdEnd-fwdStart;
  
//  if(component != COMP_T4)
//  {
//    snprintf(logBuffer, sizeof(logBuffer), "PERF_T4|FwdPkt|%4d|RetPkt|%4d|FwdLatcy|%9d|RetLatcy|%4d|Latency|%9d"
//            " |arrivalDiff|%ld|startDiff|%ld",      
//            fwdPkt, retPkt, fwdEnd-fwdStart, retEnd-retStart , end-start,
//            0,0
//            );
//  }
//  else
  {
    snprintf(logBuffer, sizeof(logBuffer), "|PERF_T%d|FwdPkt|%4d|RetPkt|%4d|FwdLatcy|%6d|FwdWait|%6d|RetLatcy|%6d|Latency|%6d"
            " |arrivalDiff|%8ld|startDiff|%6ld",
            component, fwdPkt, retPkt, 
            fwdLatency, fwdGoAhead - fwdEnd ,retEnd-retStart, end-start,
            perfTime.diffArrival,perfTime.diffStart
            );
  }
  
  Logger::getLogger().log(PERF , std::move(std::string(logBuffer)));
  memset(&perfTime, 0 , sizeof(perfTime));
  return true; 
}

bool logTCPServerLatencyEvent(short component, int &fwdPkt, OMS_PERF_TIME &perfTime)
{
  char logBuffer[768]= {0};
  const long start    = TIMESTAMP1(perfTime.tsStart);
  const long fwdStart = TIMESTAMP1(perfTime.tsFwdStart);
  const long fwdEnd   = TIMESTAMP1(perfTime.tsFwdGoAhead); //TIMESTAMP(perfTime.tsFwdEnd);
  const long fwdGoAhead   = TIMESTAMP1(perfTime.tsFwdGoAhead);
  const long retStart = TIMESTAMP1(perfTime.tsRetStart);
  const long retEnd   = TIMESTAMP1(perfTime.tsRetEnd);
  const long end      = TIMESTAMP1(perfTime.tsEnd);
  const long fwdLatency  = fwdEnd-fwdStart;
  
  snprintf(logBuffer, sizeof(logBuffer), "|PERF_T%d|FwdPkt|%4d|FwdLatcy|%6d|FwdWait|%6d|RetLatcy|%6d|Latency|%6d"
                                         " |ProcessingLatency|%8ld|TotalLatency|%6ld",
                                          component, fwdPkt,
                                          fwdLatency, fwdGoAhead - fwdEnd ,retEnd-retStart, end-start,
                                          perfTime.diffArrival,perfTime.diffStart
          );

  
  Logger::getLogger().log(PERF , std::move(std::string(logBuffer)));
  memset(&perfTime, 0 , sizeof(perfTime));
  return true; 
}