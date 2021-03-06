//
// Created by wjy on 2019/11/27.
// 目标是实现一个简单高效到播放器
//
#if defined(__ANDROID__)

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <Mutex.h>
#include <assert.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "utils.h"
#include "VideoEditor.h"

VideoEditor* videoEditor = NULL;
static ANativeWindow* window = 0;
/**
 * prepare
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_cgfay_media_editor_VideoEditor_prepare(JNIEnv * env, jobject obj, jstring srcFilePath, jint width, jint height, jobject surface, jboolean isHWDecode) {
    ALOGI("jni prepare");
    JavaVM *jvm = NULL;
    env->GetJavaVM(&jvm);
    jobject g_obj = env->NewGlobalRef(obj);
    char* filePath = (char*)env->GetStringUTFChars(srcFilePath, NULL);
    if (NULL == videoEditor) {
        videoEditor = new VideoEditor();
    }
    window = ANativeWindow_fromSurface(env, surface);
    if (NULL != videoEditor) {
        jboolean initCode = videoEditor->init(filePath, jvm, obj, isHWDecode);
        videoEditor->onSurfaceCreated(window, width, height);
    }
    env->ReleaseStringUTFChars(srcFilePath, filePath);
}

JNIEXPORT void JNICALL Java_com_cgfay_media_editor_VideoEditor_play(JNIEnv * env, jobject obj) {
    if(NULL != videoEditor) {
        videoEditor->play();
    }
}

#endif  /* defined(__ANDROID__) */