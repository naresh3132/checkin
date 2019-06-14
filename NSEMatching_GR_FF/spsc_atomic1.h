#ifndef _PRODUCER_CONSUMER_QUEUE1_H_
#define _PRODUCER_CONSUMER_QUEUE1_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>
#include <list>
#include <boost/lockfree/spsc_queue.hpp>

template<class T,  int Size = 10000>
class ProducerConsumerQueue
{
  public:
    explicit ProducerConsumerQueue(int nQueueLength = 10000)
    {
    }

    ~ProducerConsumerQueue()
    {
    }

    public:
    template<class ...Args>
    bool enqueue(Args&&... recordArgs)
    {
        return m_ptrQ.push(T(std::forward<Args>(recordArgs)...));
    }

    bool dequeue(T& record)
    {        
        return m_ptrQ.pop(record);
    }

  private:
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Size> > m_ptrQ;
};


#endif
