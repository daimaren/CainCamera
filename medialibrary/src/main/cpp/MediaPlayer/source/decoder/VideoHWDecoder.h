//
// Created by 1 on 2019/11/25.
//

#ifndef CAINCAMERA_VIDEOHWDECODER_H
#define CAINCAMERA_VIDEOHWDECODER_H


#include <sync/MediaClock.h>
#include "MediaDecoder.h"

class VideoHWDecoder : public MediaDecoder{
public:
    VideoHWDecoder();
    virtual ~VideoHWDecoder();

    void start() override;

    void stop() override;

    void flush() override;

    void run() override;

    int getFrameSize();
private:

private:
    FrameQueue *frameQueue;         // 帧队列

    bool mExit;                     // 退出标志
    Thread *decodeThread;           // 解码线程
    MediaClock *masterClock;        // 主时钟
};


#endif //CAINCAMERA_VIDEOHWDECODER_H
