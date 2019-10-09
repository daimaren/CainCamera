//
// Created by 1 on 2019/10/9.
//

#ifndef CAINCAMERA_COMMON_H
#define CAINCAMERA_COMMON_H

#if defined(__ANDROID__)

#include <android/log.h>

#define JNI_TAG "MediaRecorder"

#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR,   JNI_TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO,    JNI_TAG, format, ##__VA_ARGS__)
#define LOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG,   JNI_TAG, format, ##__VA_ARGS__)
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN,    JNI_TAG, format, ##__VA_ARGS__)
#define LOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE, JNI_TAG, format, ##__VA_ARGS__)

#else

#define LOGE(format, ...) {}
#define LOGI(format, ...) {}
#define LOGD(format, ...) {}
#define LOGW(format, ...) {}
#define LOGV(format, ...) {}

#endif

typedef unsigned char byte;

static inline long getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif //CAINCAMERA_COMMON_H
