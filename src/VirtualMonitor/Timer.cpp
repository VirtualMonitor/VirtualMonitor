#include "Timer.h"

Timer::Timer(Notifiable *n):
    notifier(n),
    waiting(true),
    queue(0),
    threadRunning(false)
{
    enable();
}

Timer::~Timer()
{
    disable();
}

int Timer::enable()
{
    if (threadRunning)
    {
        fprintf(stderr, "Thread already running");
        return -1;
    }

    pthread_mutex_init(&resetMutex,NULL);
    pthread_cond_init(&resetCond,NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int rc = pthread_create(&thread_id, &attr, Timer::thread_start, (void *)this);
    if (rc != 0)
        fprintf(stderr, "pthread_create");       

    threadRunning=true;
    return rc;

}

timeval Timer::addMillis(timeval inTime, int millis) {
  int secs = millis / 1000;
  millis = millis % 1000;
  inTime.tv_sec += secs;
  inTime.tv_usec += millis * 1000;
  if (inTime.tv_usec >= 1000000) {
    inTime.tv_sec++;
    inTime.tv_usec -= 1000000;
  }
  return inTime;
}

int Timer::disable(){

    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&resetMutex);
    pthread_cond_destroy(&resetCond);

    int s = pthread_cancel(thread_id);
    if (s != 0)
        fprintf(stderr, "pthread_cancel");

    void *res;

    s = pthread_join(thread_id, &res);
    if (s != 0)
        fprintf(stderr, "pthread_join");
    if (res == PTHREAD_CANCELED)
        printf("main(): thread was succesfully canceled\n");
    else
        printf("main(): thread wasn't canceled (shouldn't happen!)\n");

    threadRunning=false;
    return s;
}

void Timer::start(long msec)
{   
    waitmsec=msec;
    timeval timeNow;
    gettimeofday(&timeNow, 0);
    timeval waitTimeVal;
    waitTimeVal.tv_sec = msec / 1000;
    waitTimeVal.tv_usec = (msec % 1000) * 1000;

    //struct tm *tm = localtime(&resetTime.tv_sec);
    //char s[64];
    //strftime(s, sizeof(s), "%c", tm);
    //fprintf(stderr, "Reset time: %s\n", s);


    //tm = localtime(&dueTime.tv_sec);
    //strftime(s, sizeof(s), "%c", tm);

    //fprintf(stderr, "Due time: %s\n", s);

    pthread_mutex_lock(&resetMutex);
    timeradd(&timeNow,&waitTimeVal,&dueTime);
    if (waiting && queue==0)
    {
        fprintf(stderr, "Timer not runnig timers queued: %d", ++queue);
        pthread_cond_signal(&resetCond);          
    }
    pthread_mutex_unlock(&resetMutex);
}

void *Timer::thread_start(void *state)
{
    fprintf(stderr, "Thread starting");
    Timer *self = (Timer *)state;

    pthread_mutex_lock(&self->resetMutex);
    while (true)
    {
        self->waiting=true;
        pthread_cond_wait(&self->resetCond, &self->resetMutex);
        self->queue--;
        //locked coming out
        self->waiting=false;
        timeval timeNow;
        gettimeofday(&timeNow, 0);

        while (timercmp(&timeNow, &self->dueTime,<))
        {
            timeval waitfor;
            timersub(&self->dueTime,&timeNow,&waitfor);
            pthread_mutex_unlock(&self->resetMutex);
            fprintf(stderr, "Sleeping for number of seconds: %d\n", waitfor.tv_sec);
            sleep(waitfor.tv_sec);
            fprintf(stderr, "Sleep nmber of microseconds: %d\n", waitfor.tv_usec);
            usleep(waitfor.tv_usec);
            pthread_mutex_lock(&self->resetMutex);
            gettimeofday(&timeNow, 0);
        }
        pthread_mutex_unlock(&self->resetMutex);
        self->notifier->notifyEvent(self);
        pthread_mutex_lock(&self->resetMutex);
    }

    \
}

//void *Timer::thread_start(void * state)
//{
//    Timer *self = (Timer *)state;
//    while (true)
//    {
//        usleep(self->waitmsec);
//        self->notifier->notifyEvent(self);
//    }
//}


