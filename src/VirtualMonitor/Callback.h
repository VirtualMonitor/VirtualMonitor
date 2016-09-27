#ifndef CALLBACK_H
#define CALLBACK_H


class Notifiable
{
public:    
    virtual void notifyEvent(const void *caller) = 0;
};

#endif // CALLBACK_H

