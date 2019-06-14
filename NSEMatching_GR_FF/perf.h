/* 
 * File:   perf.h
 * Author: 
 *
 * Created on 24 May, 2015, 3:59 PM
 */

#ifndef PERF_H
#define	PERF_H

#include <thread>
#include <string>
#include <string.h>

#define __PERF__          1
#define __TIMER_THREAD__  1

//#define CLOCK_TYPE            CLOCK_REALTIME
#define CLOCK_TYPE              CLOCK_MONOTONIC
#define PKT_CLOCK_TYPE          CLOCK_MONOTONIC
//#define CLOCK_TYPE            CLOCK_MONOTONIC_RAW

#define TIMER_KEY               "/tmp/Timer.key"
//#define TIMER_CPU_CORE          7  

#ifdef __TIMER_THREAD__
  #define TIMESTAMP1(TS)          (TS)
  #define TS_TYPE                int64_t
  #define INIT_PERF_TIMER(TIMER_CPU_CORE)      initPerfTimer(TIMER_KEY,TIMER_CPU_CORE)
  #define GET_PERF_TIME(TS)      TS=getPerfTime();
  #define GET_PERF_TIME_NS(TS)   TS=getPerfTime();
#else
  #define TS_TYPE             timespec
  #define TIMESTAMP(TS)       ((((TS).tv_sec * 1000000000L) + ((TS).tv_nsec)))  
  #define INIT_PERF_TIMER()   0
  #define GET_PERF_TIME(TS)   clock_gettime( CLOCK_TYPE , &TS)
#endif


#ifdef __PERF__
  #define _PERF_INTERFACE_
  #define _TCP_INTERFACE_
  #define _PERF_T1_
  #define _PERF_T2_
  #define _PERF_T3_
  #define _PERF_T4_
  #define _PERF_GW_NSEFO_
  #define _PERF_GW_NSECM_
//  #define PRINT_CORE_REF_TIME(logStr)   printCoreRefTime(logStr)
#else

//#define PRINT_CORE_REF_TIME()   

//to calculate only T1 time uncomment following line
//#define _PERF_T1_ 1

//to calculate only T1 time uncomment following line
//#define _PERF_T2_ 1

//to calculate only T1 time uncomment following line
//#define _PERF_T3_ 1

#endif


#define PRINT_CORE_REF_TIME(logStr)   printCoreRefTime(logStr)

#define TCP_INTERFACE         0
#define COMP_INTERFACE        0
#define COMP_T1               1 
#define COMP_T2               2 
#define COMP_T3               3 
#define COMP_T4               4 
#define COMP_GW_T1            5
#define COMP_GW_T2            6

struct OMS_REF_TIME
{
    timespec  tsT0Ref;
    timespec  tsT1Ref;
    timespec  tsT2Ref;
    timespec  tsT3Ref;
    timespec  tsT4Ref;
};

struct OMS_PERF_TIME
{
  TS_TYPE  tsStart;
  TS_TYPE  tsFwdStart;
  TS_TYPE  tsFwdGoAhead;
  TS_TYPE  tsFwdEnd;
  TS_TYPE  tsRetStart;
  TS_TYPE  tsRetEnd;
  TS_TYPE  tsEnd;
  long      diffStart;
  long      diffArrival;
  OMS_PERF_TIME()
  {
    memset(this,0,sizeof(*this));
  }
};

bool initPerfTimer(const std::string KeyFilePath, short timerCore);
int64_t getPerfTime();

void printCoreRefTime(const char *logMsg=nullptr);

bool isLatencyRequired(int msgCode);

bool logOMSLatencyEvent(short component, int &fwdPkt, int &retPkt , OMS_PERF_TIME &perfTime);
bool logTCPServerLatencyEvent(short component, int &fwdPkt, OMS_PERF_TIME &perfTime);

#endif	/* PERF_H */

