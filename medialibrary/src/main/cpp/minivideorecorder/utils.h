#ifndef MINI_RECORDER_UTILS_H
#define MINI_RECORDER_UTILS_H

#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <string>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>



typedef signed short SInt16;
typedef unsigned char byte;
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

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

static inline long getCurrentTimeMills()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//合并两个short，返回一个short
inline SInt16 TPMixSamples(SInt16 a, SInt16 b) {
    int tmp = a < 0 && b < 0 ? ((int) a + (int) b) - (((int) a * (int) b) / INT16_MIN) : (a > 0 && b > 0 ? ((int) a + (int) b) - (((int) a * (int) b) / INT16_MAX) : a + b);
    return tmp > INT16_MAX ? INT16_MAX : (tmp < INT16_MIN ? INT16_MIN : tmp);
}

//合并两个float，返回一个short
inline SInt16 TPMixSamplesFloat(float a, float b) {
    int tmp = a < 0 && b < 0 ? ((int) a + (int) b) - (((int) a * (int) b) / INT16_MIN) : (a > 0 && b > 0 ? ((int) a + (int) b) - (((int) a * (int) b) / INT16_MAX) : a + b);
    return tmp > INT16_MAX ? INT16_MAX : (tmp < INT16_MIN ? INT16_MIN : tmp);
}

//合成伴奏与录音,byte数组需要在客户端分配好内存---非清唱时候最终合成伴奏与录音调用
inline void mixtureAccompanyAudio(SInt16 *accompanyData, SInt16 *audioData, int size, byte *targetArray) {
    byte* tmpbytearray = new byte[2];
    for (int i = 0; i < size; i++) {
        SInt16 audio = audioData[i];
        SInt16 accompany = accompanyData[i];
        SInt16 temp = TPMixSamples(accompany, audio);
        converttobytearray(temp, tmpbytearray);
        targetArray[i * 2] = tmpbytearray[0];
        targetArray[i * 2 + 1] = tmpbytearray[1];
    }
    delete[] tmpbytearray;
}

//合成伴奏与录音,short数组需要在客户端分配好内存---非清唱时候边和边放调用
inline void mixtureAccompanyAudio(SInt16 *accompanyData, SInt16 *audioData, int size, short *targetArray) {
    for (int i = 0; i < size; i++) {
        SInt16 audio = audioData[i];
        SInt16 accompany = accompanyData[i];
        targetArray[i] = TPMixSamples(accompany, audio);
    }
}


inline char *strstr(char *s1, char *s2) {
    unsigned int i = 0;
    if (*s1 == 0) // 如果字符串s1为空
    {
        if (*s2) // 如果字符串s2不为空
            return (char*) NULL; // 则返回NULL
        return (char*) s1; // 如果s2也为空，则返回s1
    }

    while (*s1) // 串s1没有结束
    {
        i = 0;
        while (1) {
            if (s2[i] == 0) {
                return (char*) s1;
            }
            if (s2[i] != s1[i])
                break;
            i++;
        }
        s1++;
    }
    return (char*) NULL;
}

inline int strindex(char *s1, char *s2) {
    int nPos = -1;
    char *res = strstr(s1, s2);
    if (res)
        nPos = res - s1;
    return nPos;
}

#endif //MINI_RECORDER_UTILS_H
