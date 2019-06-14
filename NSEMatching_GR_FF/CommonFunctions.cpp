
#include"CommonFunctions.h"
#include<time.h>

//std::atomic<bool> terminate(false);

char logBufCF[200];

int TaskSetCPU(int nCoreID_)
{
  int nRet = 0;
  //char strErrMsg[768] = {0};
  unsigned int nThreads = std::thread::hardware_concurrency();
  cpu_set_t set;
  
  snprintf (logBufCF, 200, "Pinning to core %d", nCoreID_);
  Logger::getLogger().log(DEBUG, logBufCF);	
  //std::cout<<"Pinning to core "<< nCoreID_<<std::endl;
  
  if (nCoreID_ == -1)
  {
     snprintf (logBufCF, 200,"CoreID configured: -1" );
     Logger::getLogger().log(DEBUG, logBufCF);
     //std::cout<<"CoreID configured: -1"<<std::endl;
      nRet = -1;
      return nRet;
  }
  
  CPU_ZERO(&set);

  if(nCoreID_ >= nThreads)
  {
    snprintf(logBufCF,200, "Error : Supplied core id %d is invalid. Valid range is 0 - %d", nCoreID_, nThreads - 1);
    Logger::getLogger().log(DEBUG, logBufCF);	
    //std::cout<<logBufCF<<std::endl;
    nRet = -1;
  }
  else
  {
    CPU_SET(nCoreID_,&set);

    if(sched_setaffinity(0,sizeof(cpu_set_t), &set) < 0)
    {
      snprintf(logBufCF,200, "Error : %d returned while setting process affinity for process ID . Valid range is 0 - %d", errno,nThreads - 1);
      Logger::getLogger().log(DEBUG, logBufCF);
      //std::cout<<logBufCF<<std::endl;      
      nRet = -1;
    }
  }
  return nRet;
}


int TaskSet(unsigned int nCoreID_, int nPid_, std::string &strStatus)
{
  int nRet = 0;
 // char strErrMsg[768] = {0};
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

bool binarySearch(int32_t* arr, int size, int token, int* loc)
{
    int low = 0, high = size-1, mid;
    bool found = false;
    
    while (low <= high)
    {
        mid = (low + high)/2;
        
        if (arr[mid] == token)
        {
            found = true;
            if (loc != NULL)
            {
              (*loc)= mid;
            }
            return found;
        }
        else if (arr[mid] > token)
        {
            high = mid -1;
        }
        else if (arr[mid] < token)
        {
            low = mid +1;
        }
    }
    return found;
}
