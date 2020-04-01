//
// Created by 1 on 2019/11/27.
//

#include "VideoDecoder.h"

VideoDecoder::VideoDecoder(JavaVM *jvm, jobject obj) {
    mJvm = jvm;
    mObh = obj;

    connectionRetry = 0;
}

