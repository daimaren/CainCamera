/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
#include "../libvocal_processor/record_processor.h"
/* Header for class com_timeapp_shawn_recorder_pro_recorder_NativeRecordProcessor */

#ifndef _Included_com_cgfay_media_NativeRecordProcessor
#define _Included_com_cgfay_media_NativeRecordProcessor
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_timeapp_shawn_recorder_pro_recorder_NativeRecordProcessor
 * Method:    init
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_cgfay_media_NativeRecordProcessor_init(JNIEnv *, jobject, jint, jint, jobject);

JNIEXPORT void JNICALL Java_com_cgfay_media_NativeRecordProcessor_setAudioEffect(JNIEnv *, jobject, jint, jobject);

/*
 * Class:     com_timeapp_shawn_recorder_pro_recorder_NativeRecordProcessor
 * Method:    flushAudioBufferToQueue
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_cgfay_media_NativeRecordProcessor_flushAudioBufferToQueue
(JNIEnv *, jobject, jint);
JNIEXPORT void JNICALL Java_com_cgfay_media_NativeRecordProcessor_destroy
(JNIEnv *, jobject, jint);

/*
 * Class:     com_timeapp_shawn_recorder_pro_recorder_NativeRecordProcessor
 * Method:    pushAudioBufferToQueue
 * Signature: (I[SI)I
 */
JNIEXPORT jint JNICALL Java_com_cgfay_media_NativeRecordProcessor_pushAudioBufferToQueue(JNIEnv *, jobject, jint, jshortArray, jint);

#ifdef __cplusplus
}
#endif
#endif