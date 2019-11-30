//
// Created by 1 on 2019/11/1.
//

#ifndef CAINCAMERA_Msg_H
#define CAINCAMERA_Msg_H

#include <pthread.h>
#define MESSAGE_QUEUE_LOOP_QUIT_FLAG 19900909

class Handler;

class Msg {
private:
    int what;
    int arg1;
    int arg2;
    void* obj;
public:
    Msg();
    Msg(int what);
    Msg(int what, int arg1, int arg2);
    Msg(int what, void* obj);
    Msg(int what, int arg1, int arg2, void* obj);
    virtual ~Msg();

    int execute();
    int getWhat(){
        return what;
    };
    int getArg1(){
        return arg1;
    };
    int getArg2(){
        return arg2;
    };
    void* getObj(){
        return obj;
    };
    Handler* handler;
};

typedef struct MsgNode {
    Msg *msg;
    struct MsgNode *next;
    MsgNode(){
        msg = NULL;
        next = NULL;
    }
} MsgNode;

class MsgQueue {
private:
    MsgNode* mFirst;
    MsgNode* mLast;
    int mNbPackets;
    bool mAbortRequest;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;
    const char* queueName;


public:
    MsgQueue();
    MsgQueue(const char* queueNameParam);
    ~MsgQueue();

    void init();
    void flush();
    int enqueueMessage(Msg* msg);
    int dequeueMessage(Msg **msg, bool block);
    int size();
    void abort();

};

#endif //CAINCAMERA_Msg_H
