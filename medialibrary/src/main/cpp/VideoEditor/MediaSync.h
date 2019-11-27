//
// Created by 1 on 2019/11/27.
//

#ifndef CAINCAMERA_MEDIASYNC_H
#define CAINCAMERA_MEDIASYNC_H

#include "utils.h"

class MediaSync {
public:
    MediaSync();
    bool init(JavaVM *g_jvm, jobject obj, bool isHWDecode);

private:
    void createDecoderInstance();
private:
    bool mIsHWDecode;
    JavaVM *mJvm;
    jobject mObj;
};


#endif //CAINCAMERA_MEDIASYNC_H
