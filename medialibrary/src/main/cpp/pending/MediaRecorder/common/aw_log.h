//
// Created by Administrator on 2018/3/8.
//

#ifndef NATIVE_LIBRARY_AW_LOG_H
#define NATIVE_LIBRARY_AW_LOG_H

#include <android/log.h>
#define TAG "CainMedia"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)

#endif //NATIVE_LIBRARY_AW_LOG_H
