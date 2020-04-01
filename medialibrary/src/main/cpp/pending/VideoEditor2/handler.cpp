//
// Created by 1 on 2019/11/1.
//

#include "handler.h"

#define LOG_TAG "Handler"

/******************* Handler class *******************/
Handler::Handler(MsgQueue* queue) {
    this->mQueue = queue;
}

Handler::~Handler() {
}

int Handler::postMessage(Msg* msg){
    msg->handler = this;
	//LOGD("enqueue msg what is %d", msg->getWhat());
    return mQueue->enqueueMessage(msg);
}
