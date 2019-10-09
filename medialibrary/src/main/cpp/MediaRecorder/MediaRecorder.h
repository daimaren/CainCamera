//
// Created by 1 on 2019/10/9.
//

#ifndef CAINCAMERA_MEDIARECORDER_H
#define CAINCAMERA_MEDIARECORDER_H

#include <android/native_window.h>
#include "../include/Thread.h"
#include "egl_core.h"
#include "recording_preview_renderer.h"

/**
 * 录制监听器
 */
class OnRecordListener {
public:
    // 录制器准备完成回调
    virtual void onRecordStart() = 0;
    // 正在录制
    virtual void onRecording(float duration) = 0;
    // 录制完成回调
    virtual void onRecordFinish(bool success, float) = 0;
    // 录制出错回调
    virtual void onRecordError(const char *msg) = 0;
};

class MediaRecorder : public Runnable{
public:
    MediaRecorder();

    virtual ~MediaRecorder();

    // 设置录制监听器
    void setOnRecordListener(OnRecordListener *listener);

    void prepareEGLContext(ANativeWindow *window, int screenWidth, int screenHeight, int cameraFacingId);
    void configCamera();

    // 准备录制器
    int prepare();

    // 释放资源
    void release();

    // 开始录制
    void startRecord();

    // 停止录制
    void stopRecord();

    // 是否正在录制
    bool isRecording();

    void run() override;

private:

private:
    Mutex mMutex;
    Condition mCondition;
    Thread *mRecordThread;
    OnRecordListener *mRecordListener;
    //SafetyQueue<AVMediaData *> *mFrameQueue;
    bool mAbortRequest; // 停止请求
    bool mStartRequest; // 开始录制请求
    bool mExit;         // 完成退出标志

    EGLCore *eglCore;
    EGLSurface previewSurface;
    RecordingPreviewRenderer *renderer;
};

#endif //CAINCAMERA_MEDIARECORDER_H
