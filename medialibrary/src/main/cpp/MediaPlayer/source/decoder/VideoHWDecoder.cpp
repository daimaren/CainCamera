//
// Created by 1 on 2019/11/25.
//

#include "VideoHWDecoder.h"

void VideoHWDecoder::start() {
    MediaDecoder::start();
    if (frameQueue) {
        frameQueue->start();
    }
    if (!decodeThread) {
        decodeThread = new Thread(this);
        decodeThread->start();
        mExit = false;
    }
}

void VideoHWDecoder::stop() {
    MediaDecoder::stop();
    if (frameQueue) {
        frameQueue->abort();
    }
    mMutex.lock();
    while (!mExit) {
        mCondition.wait(mMutex);
    }
    mMutex.unlock();
    if (decodeThread) {
        decodeThread->join();
        delete decodeThread;
        decodeThread = NULL;
    }
}

void VideoHWDecoder::flush() {
    mMutex.lock();
    MediaDecoder::flush();
    if (frameQueue) {
        frameQueue->flush();
    }
    mCondition.signal();
    mMutex.unlock();
}

int VideoHWDecoder::getFrameSize() {
    Mutex::Autolock lock(mMutex);
    return frameQueue ? frameQueue->getFrameSize() : 0;
}

void VideoHWDecoder::run() {

}



