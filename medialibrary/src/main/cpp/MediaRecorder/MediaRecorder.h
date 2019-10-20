//
// Created by CainHuang on 2019/8/17.
//

#ifndef MEDIA_Recorder_H
#define MEDIA_Recorder_H

#include <AVMediaHeader.h>
#include <SafetyQueue.h>
#include <AVMediaData.h>
#include <Thread.h>
#include <AVFormatter.h>
#include <YuvConvertor.h>
#include <jni.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "RecordParams.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "aw_encode_flv.h"
#include "aw_all.h"
#include "stdio.h"

#ifdef __cplusplus
};
#endif

static char* OUTPUT_VIEW_VERTEX_SHADER =
        "attribute vec4 position;    \n"
        "attribute vec2 texcoord;   \n"
        "varying vec2 v_texcoord;     \n"
        "void main(void)               \n"
        "{                            \n"
        "   gl_Position = position;  \n"
        "   v_texcoord = texcoord;  \n"
        "}                            \n";

static char* OUTPUT_VIEW_FRAG_SHADER =
        "varying highp vec2 v_texcoord;\n"
        "uniform sampler2D yuvTexSampler;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(yuvTexSampler, v_texcoord);\n"
        "}\n";

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

class MediaRecorder : public Runnable {
public:
    MediaRecorder();

    virtual ~MediaRecorder();

    void prepareEGLContext(ANativeWindow *window, JavaVM *g_jvm, jobject obj, int screenWidth, int screenHeight, int cameraFacingId);
    void notifyFrameAvailable();
    // 设置录制监听器
    void setOnRecordListener(OnRecordListener *listener);

    // 准备录制器
    int prepare();

    // 释放资源
    void release();

    // 录制媒体数据
    int recordFrame(AVMediaData *data);

    // 开始录制
    void startRecord();

    // 停止录制
    void stopRecord();

    // 是否正在录制
    bool isRecording();

    void run() override;

    RecordParams *getRecordParams();

private:
    void saveFlvAudioTag(aw_flv_audio_tag *audio_tag);
    void saveFlvVideoTag(aw_flv_video_tag *video_tag);
    void saveSpsPpsAndAudioSpecificConfigTag();
    void save_audio_data(aw_flv_audio_tag *audio_tag);
    void save_video_data(aw_flv_video_tag *video_tag);
    void save_script_data(aw_flv_script_tag *script_tag);
    void save_flv_tag_to_file(aw_flv_common_tag *commonTag);
    // EGL functions
    bool initEGL();
    bool createWindowSurface(ANativeWindow* pWindow);
    bool initRenderer();
    // OpenGL
    GLuint loadProgram(char* pVertexSource, char* pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char* pSource);
    bool   checkGlError(const char* op);
    int initTexture();
    void renderToViewWithAutofit(GLuint texId, int screenWidth, int screenHeight, int texWidth, int texHeight);
    // callback function
    void startCameraPreview();
    void updateTexImage();

private:
    FILE* mFile;
    Mutex mMutex;
    Condition mCondition;
    Thread *mRecordThread;
    OnRecordListener *mRecordListener;
    SafetyQueue<AVMediaData *> *mFrameQueue;
    aw_data *s_output_buf = NULL;
    int64_t duration;

    bool mAbortRequest; // 停止请求
    bool mStartRequest; // 开始录制请求
    bool mExit;         // 完成退出标志
    bool isSpsPpsAndAudioSpecificConfigSent = false;
    bool isEncoding = false;

    ANativeWindow *mNativeWindow;
    JavaVM *mJvm;
    jobject mObj;

    int mScreenWidth;
    int mScreenHeight;
    int mTextureWidth;
    int mTextureHeight;
    int mFacingId;

    RecordParams *mRecordParams;    // 录制参数
    YuvConvertor *mYuvConvertor;    // Yuv转换器

    //EGL
    EGLDisplay mEGLDisplay;
    EGLConfig mEGLConfig;
    EGLContext mEGLContext;
    EGLSurface mPreviewSurface;
    //OpenGL
    char* mVertexShader;
    char* mFragmentShader;
    GLuint mGLProgramId;
    GLuint mGLVertexCoords;
    GLuint mGLTextureCoords;
    GLint mGLUniformTexture;
    bool mIsGLInitialized;
    GLuint mDecodeTexId;
};


#endif //FLVRecorder_H
