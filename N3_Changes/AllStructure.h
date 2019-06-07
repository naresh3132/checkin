/* 
 * File:   AllStructure.h
 * Author: NareshRK
 *
 * Created on September 4, 2018, 11:23 AM
 */
#include<iostream>
#include<atomic>
#include<string.h>
//#include<bits/unordered_map.h>
#include "FileAttributeAssignment.h"
#include "log.h"

#ifndef ALLSTRUCTURE_H
#define	ALLSTRUCTURE_H



class CONNINFO
{
  
  public:  
    std::atomic<int> status;
    std::atomic<int> recordCnt;
    char msgBuffer[3000];
    int32_t msgBufSize;
    char IP[16];
    int32_t dealerID;
  
  CONNINFO()
  {
    this->status = CONNECTED;
    this->msgBufSize = 0;
    this->recordCnt = 0;
    memset (this->msgBuffer, 0, sizeof(this->msgBuffer));
    memset (this->IP, 0, sizeof(this->IP));
    this->dealerID = 0;
  }
};

typedef std::unordered_map<int,CONNINFO*> connectionMap;
typedef connectionMap::iterator connectionItr;

class DATA_RECEIVED
{  
public:
  int MyFd;
  char   msgBuffer[2048];
  CONNINFO* ptrConnInfo;
  int64_t recvTimeStamp;   
    
  DATA_RECEIVED()
  {
    this->MyFd      = 0;
    memset(this->msgBuffer, 0, 2048);  
    this->ptrConnInfo = NULL;
    this->recvTimeStamp = 0;
  }
    
  DATA_RECEIVED(const DATA_RECEIVED& stDataReceived)
  {
    this->MyFd      = stDataReceived.MyFd;
    memcpy(this->msgBuffer, stDataReceived.msgBuffer, 2048);  
    this->ptrConnInfo = stDataReceived.ptrConnInfo;
    this->recvTimeStamp = stDataReceived.recvTimeStamp;
  }
    
};

//typedef std::unordered_map<int,CONNINFO*> ExchFdToUserIDMap;
//typedef ExchFdToUserIDMap::iterator ExchFdToUserIDItr;

#endif	/* ALLSTRUCTURE_H */

