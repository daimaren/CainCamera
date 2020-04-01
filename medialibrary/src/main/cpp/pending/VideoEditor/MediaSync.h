//
// Created by 1 on 2019/11/27.
//

#ifndef CAINCAMERA_MEDIASYNC_H
#define CAINCAMERA_MEDIASYNC_H

#include "utils.h"

//解码文件时的缓冲区的最小和最大值
#define LOCAL_MIN_BUFFERED_DURATION   			0.5
#define LOCAL_MAX_BUFFERED_DURATION   			0.8
#define LOCAL_AV_SYNC_MAX_TIME_DIFF         	0.05

class UploadCBImpl {

};

class MediaSync {
public:
    MediaSync();
    bool init(JavaVM *g_jvm, jobject obj, bool isHWDecode);
    bool validAudio();
private:
    void createDecoderInstance();
private:
    bool mIsHWDecode;
    bool mIsDestroyed;
    bool mIsDecoding;

    JavaVM *mJvm;
    jobject mObj;
    VideoDecoder*   mDecoder;
};


#endif //CAINCAMERA_MEDIASYNC_H
