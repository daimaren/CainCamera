//
// Created by CainHuang on 2019/8/17.
//
#if defined(__ANDROID__)

#include <jni.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <Mutex.h>
#include <assert.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "MiniRecorder.h"
#include "AVMediaHeader.h"

static JavaVM *javaVM = nullptr;

static JNIEnv *getJNIEnv() {
    JNIEnv *env;
    assert(javaVM != nullptr);
    if (javaVM->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return nullptr;
    }
    return env;
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    return JNI_VERSION_1_4;
}

//-------------------------------------- JNI回调监听器 ----------------------------------------------
class JNIOnRecordListener : public OnRecordListener {
public:
    JNIOnRecordListener(JavaVM *vm, JNIEnv *env, jobject listener);

    virtual ~JNIOnRecordListener();

    void onRecordStart() override;

    void onRecording(float duration) override;

    void onRecordFinish(bool success, float duration) override;

    void onRecordError(const char *msg) override;

private:
    JNIOnRecordListener();

    JavaVM *javaVM;
    jobject mJniListener;               // java接口创建的全局对象
    jmethodID jmid_onRecordStart;       // 录制开始回调
    jmethodID jmid_onRecording;         // 正在录制回调
    jmethodID jmid_onRecordFinish;      // 录制完成回调
    jmethodID jmid_onRecordError;       // 录制出错回调
};

JNIOnRecordListener::JNIOnRecordListener(JavaVM *vm, JNIEnv *env, jobject listener) {
    this->javaVM = vm;
    if (listener != nullptr) {
        mJniListener = env->NewGlobalRef(listener);
    } else {
        mJniListener = nullptr;
    }
    jclass javaClass = env->GetObjectClass(listener);
    if (javaClass != nullptr) {
        jmid_onRecordStart = env->GetMethodID(javaClass, "onRecordStart", "()V");
        jmid_onRecording = env->GetMethodID(javaClass, "onRecording", "(F)V");
        jmid_onRecordFinish = env->GetMethodID(javaClass, "onRecordFinish", "(ZF)V");
        jmid_onRecordError = env->GetMethodID(javaClass, "onRecordError", "(Ljava/lang/String;)V");
    } else {
        jmid_onRecordStart = nullptr;
        jmid_onRecording = nullptr;
        jmid_onRecordFinish = nullptr;
        jmid_onRecordError = nullptr;
    }
}

JNIOnRecordListener::~JNIOnRecordListener() {
    if (mJniListener != nullptr) {
        JNIEnv *env = getJNIEnv();
        env->DeleteGlobalRef(mJniListener);
        mJniListener = nullptr;
    }
}

void JNIOnRecordListener::onRecordStart() {
    LOGD("onRecordStart");
    if (jmid_onRecordStart != nullptr) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, nullptr) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(mJniListener, jmid_onRecordStart);
        javaVM->DetachCurrentThread();
    }
}

void JNIOnRecordListener::onRecording(float duration) {
    //LOGD("onRecording");
    if (jmid_onRecording != nullptr) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, nullptr) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(mJniListener, jmid_onRecording, duration);
        javaVM->DetachCurrentThread();
    }
}

void JNIOnRecordListener::onRecordFinish(bool success, float duration) {
    LOGD("onRecordFinish: %d", success);
    if (jmid_onRecordFinish != nullptr) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, nullptr) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(mJniListener, jmid_onRecordFinish, success, duration);
        javaVM->DetachCurrentThread();
    }
}

void JNIOnRecordListener::onRecordError(const char *msg) {
    LOGD("onRecordError: %s", msg);
    if (jmid_onRecordError != nullptr) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, nullptr) != JNI_OK) {
            return;
        }
        jstring jmsg = nullptr;
        if (msg != nullptr) {
            jmsg = jniEnv->NewStringUTF(msg);
        } else {
            jmsg = jniEnv->NewStringUTF("");
        }
        jniEnv->CallVoidMethod(mJniListener, jmid_onRecordError, jmsg);
        javaVM->DetachCurrentThread();
    }
}

//--------------------------------------------------------------------------------------------------

/**
 * 初始化一个录制器对象
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_nativeInit(JNIEnv *env, jobject thiz) {
    MiniRecorder *recorder = new MiniRecorder();
    return (jlong) recorder;
}

/**
 * prepareEGLContext
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_prepareEGLContext(JNIEnv *env, jobject thiz, jlong handle, jobject surface, jint screenWidth, jint screenHeight,
                                                                  jint cameraFacingId) {
    MiniRecorder *recorder = (MiniRecorder *)handle;
    JavaVM *jvm = NULL;
    env->GetJavaVM(&jvm);
    jobject obj = env->NewGlobalRef(thiz);
    if (surface != 0 && recorder) {
        ANativeWindow *pWindow = ANativeWindow_fromSurface(env, surface);
        if (pWindow) {
            recorder->prepareEGLContext(pWindow, env, jvm, obj, screenWidth, screenHeight, cameraFacingId);
        }
    }
}

/**
 * prepareEGLContext
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_destroyEGLContext(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *)handle;
    if (recorder) {
        recorder->destroyEGLContext();
    }
}

/**
 * notifyFrameAvailable
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_notifyFrameAvailable(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *)handle;
    if (recorder) {
        recorder->notifyFrameAvailable();
    }
}
/**
 * 释放录制器对象
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_nativeRelease(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *)handle;
    if (recorder != nullptr) {
        recorder->release();
        delete recorder;
    }
}

/**
 * 设置录制监听器
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setRecordListener(JNIEnv *env, jobject thiz, jlong handle, jobject listener) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        JNIOnRecordListener *recordListener = new JNIOnRecordListener(javaVM, env, listener);
        recorder->setOnRecordListener(recordListener);
    }
}

/**
 * 设置录制输出路径
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setOutput(JNIEnv *env, jobject thiz, jlong handle, jstring dstPath_) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        const char *dstPath = env->GetStringUTFChars(dstPath_, nullptr);
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setOutput(dstPath);
        env->ReleaseStringUTFChars(dstPath_, dstPath);
    }
}

/**
 * 设置音频编码器名称
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setAudioEncoder(JNIEnv *env, jobject thiz, jlong handle, jstring encoder_) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        const char *encoder = env->GetStringUTFChars(encoder_, nullptr);
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setAudioEncoder(encoder);
        env->ReleaseStringUTFChars(encoder_, encoder);
    }
}

/**
 * 设置视频编码器名称
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setVideoEncoder(JNIEnv *env, jobject thiz, jlong handle, jstring encoder_) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        const char *encoder = env->GetStringUTFChars(encoder_, nullptr);
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setVideoEncoder(encoder);
        env->ReleaseStringUTFChars(encoder_, encoder);
    }
}

/**
 * 设置音频AVFilter
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setAudioFilter(JNIEnv *env, jobject thiz, jlong handle, jstring filter_) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        const char *filter = env->GetStringUTFChars(filter_, nullptr);
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setAudioFilter(filter);
        env->ReleaseStringUTFChars(filter_, filter);
    }
}

/**
 * 设置视频AVFilter
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setVideoFilter(JNIEnv *env, jobject thiz, jlong handle, jstring filter_) {

    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        const char *filter = env->GetStringUTFChars(filter_, nullptr);
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setVideoFilter(filter);
        env->ReleaseStringUTFChars(filter_, filter);
    }

}

/**
 * 设置录制视频旋转角度
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setVideoRotate(JNIEnv *env, jobject thiz, jlong handle, jint rotate) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setRotate(rotate);
    }
}

/**
 * 设置视频录制参数
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setVideoParams(JNIEnv *env, jobject thiz, jlong handle,
                                                               jint width, jint height, jint frameRate, jint pixelFormat, jlong maxBitRate, jint quality) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setVideoParams(width, height, frameRate, pixelFormat, maxBitRate, quality);
    }
}

/**
 * 设置音频录制参数
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_setAudioParams(JNIEnv *env, jobject thiz, jlong handle,
                                                               jint sampleRate, jint sampleFormat, jint channels) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        RecordParams *recordParams = recorder->getRecordParams();
        recordParams->setAudioParams(sampleRate, sampleFormat, channels);
    }
}

/**
 * 录制一帧视频帧
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_recordVideoFrame(JNIEnv *env, jobject thiz, jlong handle,
                                                                 jbyteArray data_, jint length, jint width, jint height, jint pixelFormat) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr && recorder->isRecording()) {
    }
    return -1;
}

/**
 * 录制一帧音频帧
 */
extern "C" JNIEXPORT jint JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_recordAudioFrame(JNIEnv *env, jobject thiz, jlong handle,
                                                                 jbyteArray data_, jint length) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr && recorder->isRecording()) {
        uint8_t *pcmData = (uint8_t *) malloc((size_t) length);
        if (pcmData == nullptr) {
            LOGE("Could not allocate memory");
            return -1;
        }
        jbyte *data = env->GetByteArrayElements(data_, nullptr);
        memcpy(pcmData, data, (size_t) length);
        env->ReleaseByteArrayElements(data_, data, 0);

        free(pcmData);
    }
    return -1;
}

/**
 * 准备录制
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_prepare(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        recorder->prepare();
    }
}

/**
 * 开始录制
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_startRecord(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        recorder->startRecord();
    }
}

/**
 * 停止录制
 */
extern "C" JNIEXPORT void JNICALL
Java_com_cgfay_media_recorder_MiniVideoRecorder_stopRecord(JNIEnv *env, jobject thiz, jlong handle) {
    MiniRecorder *recorder = (MiniRecorder *) handle;
    if (recorder != nullptr) {
        recorder->stopRecord();
    }
}

#endif  /* defined(__ANDROID__) */