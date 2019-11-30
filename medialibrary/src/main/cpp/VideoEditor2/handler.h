//
// Created by 1 on 2019/11/1.
//

#ifndef CAINCAMERA_HANDLER_H
#define CAINCAMERA_HANDLER_H

#include "MsgQueue.h"
extern "C" {
#include "aw_all.h"
}

class Handler {
private:
    MsgQueue* mQueue;

public:
    Handler(MsgQueue* mQueue);

    virtual ~Handler();

    virtual int postMessage(Msg *msg);
    virtual void handleMessage(Msg *msg){};
};

#endif //CAINCAMERA_HANDLER_H
