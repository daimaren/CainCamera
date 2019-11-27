#ifndef NATIVE_LIBRARY_LOG_H
#define NATIVE_LIBRARY_LOG_H

#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>

typedef signed short SInt16;
typedef unsigned char byte;

#define TAG "CainMedia"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)

#endif //NATIVE_LIBRARY_LOG_H
