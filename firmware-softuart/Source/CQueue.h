/****************************************************\
CQueue.h
Function: circular queue implemented in C
Date: 2009-03-05
Version: V1.0
Author: Guo Gaihua
\****************************************************/


#ifndef __CQUEUE__
#define __CQUEUE__

#define MAX_QUEUE_SIZE 17

typedef int q_int;

#define __QUEUE_LOCK__

#ifdef __QUEUE_LOCK__
	#include <RTL.h>
#endif

//typedef void* QUEUE_ELEMENT
typedef void* QUEUE_ELEMENT;

typedef struct Queue
{
	q_int head;
	q_int tail;
	QUEUE_ELEMENT pBuf[MAX_QUEUE_SIZE];
	q_int maxsize;
#ifdef __QUEUE_LOCK__
	OS_MUT mutex;
#endif
}Queue;

#ifdef __QUEUE_LOCK__
    #define QUEUE_CREATELOCK(x) os_mut_init(x->mutex)
    #define QUEUE_RELEASELOCK(x)  
    #define QUEUE_LOCK(x) os_mut_wait(x->mutex,0xFFFF)
    #define QUEUE_ULOCK(x) os_mut_release(x->mutex)
#else
    #define QUEUE_CREATELOCK(x) 
    #define QUEUE_RELEASELOCK(x)  
    #define QUEUE_LOCK(x) 
    #define QUEUE_ULOCK(x)
#endif



void InitialQueue(Queue *queue);
void ReleaseQueue(Queue *queue);
q_int QueuePush(Queue *queue, QUEUE_ELEMENT buf);
QUEUE_ELEMENT QueuePop(Queue *queue);
q_int QueueSize(Queue *queue);

#endif

