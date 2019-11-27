//
// Created by 1 on 2019/11/27.
//

#include "MediaSync.h"

MediaSync::MediaSync() {

}

bool MediaSync::init(JavaVM *jvm, jobject obj, bool isHWDecode) {
    mJvm = jvm;
    mObj = obj;
    mIsHWDecode = isHWDecode;
    createDecoderInstance();
}

void MediaSync::createDecoderInstance() {
    if (mIsHWDecode) {

    }
}