#include <iostream>
#include "CommonFunction.h"




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

