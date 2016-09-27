#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include "Callback.h"


class Timer
{

public:


    Timer(Notifiable *n);
    ~Timer();       
    void start(long msec);
    Notifiable *notifier;  
    bool waiting;
    int queue;
    int waitmsec;

private:
    static void *thread_start(void *state);
    inline timeval addMillis(timeval inTime, int millis) ;
    int disable();
    int enable();
    bool threadRunning;
    pthread_t thread_id;
    pthread_attr_t attr; 
    timeval dueTime;
    pthread_mutex_t resetMutex;
    pthread_cond_t resetCond;



};





#endif // TIMER
