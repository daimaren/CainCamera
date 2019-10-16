//
// Created by 1 on 2019/10/9.
//

#include "MediaRecorder.h"

MediaRecorder::MediaRecorder() {

}

MediaRecorder::~MediaRecorder() {

}

/**
 * 设置录制监听器
 * @param listener
 */
void MediaRecorder::setOnRecordListener(OnRecordListener *listener) {
    mRecordListener = listener;
}

/**
 * 释放资源
 */
void MediaRecorder::release() {
    stopRecord();
    // 等待退出
    mMutex.lock();
    while (!mExit) {
        mCondition.wait(mMutex);
    }
    mMutex.unlock();

    if (mRecordListener != nullptr) {
        delete mRecordListener;
        mRecordListener = nullptr;
    }

    if (mRecordThread != nullptr) {
        delete mRecordThread;
        mRecordThread = nullptr;
    }
}

int MediaRecorder::prepare() {

}

/**
 * 开始录制
 */
void MediaRecorder::startRecord() {
    mMutex.lock();
    mAbortRequest = false;
    mStartRequest = true;
    mCondition.signal();
    mMutex.unlock();

    if (mRecordThread == nullptr) {
        mRecordThread = new Thread(this);
        mRecordThread->start();
        mRecordThread->detach();
    }
}

/**
 * 停止录制
 */
void MediaRecorder::stopRecord() {
    mMutex.lock();
    mAbortRequest = true;
    mCondition.signal();
    mMutex.unlock();
    if (mRecordThread != nullptr) {
        mRecordThread->join();
        delete mRecordThread;
        mRecordThread = nullptr;
    }
}

/**
 * 判断是否正在录制
 * @return
 */
bool MediaRecorder::isRecording() {
    bool recording = false;
    mMutex.lock();
    recording = !mAbortRequest && mStartRequest && !mExit;
    mMutex.unlock();
    return recording;
}

void MediaRecorder::run() {

}

void MediaRecorder::prepareEGLContext(ANativeWindow *window, int screenWidth, int screenHeight,
                                      int cameraFacingId) {

    eglCore = new EGLCore();
    eglCore->init();
    previewSurface = eglCore->createWindowSurface(window);
    if (previewSurface != NULL) {
        eglCore->makeCurrent(previewSurface);
    }
    renderer = new RecordingPreviewRenderer();
    configCamera();
    //renderer->init(degress, facingId == CAMERA_FACING_FRONT, textureWidth, textureHeight, cameraWidth, cameraHeight);
    //this->startCameraPreview();
}

void MediaRecorder::configCamera() {
    // 录制回调监听器
    if (mRecordListener != nullptr) {
        mRecordListener->onRecordStart();
    }
}






