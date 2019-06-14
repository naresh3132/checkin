
#include "All_Structures.h"
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "spsc_atomic1.h"
#include<cassert>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include <string.h>
#include<unordered_map>

int TaskSetCPU( int nCoreID_);

int TaskSet(unsigned int nCoreID_, int nPid_, std::string &strStatus);

bool binarySearch(int arr[], int size, int token, int* loc);

//extern std::atomic<bool> terminate; 



