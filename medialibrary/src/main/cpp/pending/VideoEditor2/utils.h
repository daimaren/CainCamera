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
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

#define TAG "CainMedia"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)

inline void converttobytearray(SInt16 source, byte* bytes2) {
    bytes2[0] = (byte) (source & 0xff);
    bytes2[1] = (byte) ((source >> 8) & 0xff);
}

inline void convertByteArrayFromShortArray(SInt16 *shortarray, int size, byte *bytearray) {
    byte* tmpbytearray = new byte[2];
    for (int i = 0; i < size; i++) {
        converttobytearray(shortarray[i], tmpbytearray);
        bytearray[i * 2] = tmpbytearray[0];
        bytearray[i * 2 + 1] = tmpbytearray[1];
    }
    delete[] tmpbytearray;
}

#endif //NATIVE_LIBRARY_LOG_H
