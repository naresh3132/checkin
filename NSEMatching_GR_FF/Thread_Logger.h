/* 
 * File:   Thread_Logger.h
 * Author: sneha
 *
 * Created on July 21, 2016, 6:52 PM
 */

#ifndef THREAD_LOGGER_H
#define	THREAD_LOGGER_H

#include "All_Structures.h"
#include"spsc_atomic1.h"
void StartLog(ProducerConsumerQueue<DATA_RECEIVED>* qptr, int _segMode);

#endif	/* THREAD_LOGGER_H */

