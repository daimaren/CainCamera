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
#include "handler.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "aw_encode_flv.h"
#include "aw_all.h"
#include "stdio.h"

#ifdef __cplusplus
};
#endif

enum RenderThreadMessage {
    MSG_RENDER_FRAME = 0,
    MSG_EGL_THREAD_CREATE,
    MSG_EGL_CREATE_PREVIEW_SURFACE,
    MSG_SWITCH_CAMERA_FACING,
    MSG_SWITCH_FILTER,
    MSG_START_RECORDING,
    MSG_STOP_RECORDING,
    MSG_EGL_DESTROY_PREVIEW_SURFACE,
    MSG_EGL_THREAD_EXIT
};

typedef unsigned char byte;

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

static char* NO_FILTER_VERTEX_SHADER =
        "attribute vec4 vPosition;\n"
        "attribute vec4 vTexCords;\n"
        "varying vec2 yuvTexCoords;\n"
        "uniform highp mat4 texMatrix;\n"
        "uniform highp mat4 trans; \n"
        "void main() {\n"
        "  yuvTexCoords = (texMatrix*vTexCords).xy;\n"
        "  gl_Position = trans * vPosition;\n"
        "}\n";

static char* GPU_FRAME_FRAGMENT_SHADER =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "uniform samplerExternalOES yuvTexSampler;\n"
        "varying vec2 yuvTexCoords;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(yuvTexSampler, yuvTexCoords);\n"
        "}\n";

static GLfloat CAMERA_TRIANGLE_VERTICES[8] = {
        -1.0f, -1.0f,	// 0 top left
        1.0f, -1.0f,	// 1 bottom left
        -1.0f, 1.0f,  // 2 bottom right
        1.0f, 1.0f,	// 3 top right
};
static GLfloat CAMERA_TEXTURE_NO_ROTATION[8] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
};

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

class MediaRecorderHandler;

class MediaRecorder : public Runnable {
public:
    MediaRecorder();

    virtual ~MediaRecorder();

    void prepareEGLContext(ANativeWindow *window, JNIEnv *env, JavaVM *g_jvm, jobject obj, int screenWidth, int screenHeight, int cameraFacingId);
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

    virtual bool initialize();
    virtual void renderFrame();
    virtual void destroyEGLContext();
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
    // OpenGL
    bool initRenderer();
    bool initCopier();
    GLuint loadProgram(char* pVertexSource, char* pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char* pSource);
    bool   checkGlError(const char* op);
    int initDecodeTexture();
    void renderToViewWithAutofit(GLuint texId, int screenWidth, int screenHeight, int texWidth, int texHeight);
    void initFilter();
    void processFrame();
    void renderWithCoords(GLuint texId, GLfloat* vertexCoords, GLfloat* textureCoords);
    void matrixSetIdentityM(float *m);
    void copyYUY2Image(GLuint ipTex, byte* yuy2ImageBuffer, int width, int height);
    void downloadImageFromTexture(GLuint texId, void *imageBuf, unsigned int imageWidth, unsigned int imageHeight);
    // callback function
    void startCameraPreview();
    void updateTexImage();

    static void *threadStartCallback(void *myself);
    void processMessage();

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
    bool mIsEncoding = false;

    ANativeWindow *mNativeWindow;
    JavaVM *mJvm;
    jobject mObj;

    GLfloat* mTextureCoords;
    int mTextureCoordsSize;

    int mScreenWidth;
    int mScreenHeight;
    int mTextureWidth;
    int mTextureHeight;
    int mFacingId;

    RecordParams *mRecordParams;    // 录制参数
    YuvConvertor *mYuvConvertor;    // Yuv转换器

    GLuint mDecodeTexId = 0;
    GLuint mFBO;
    GLuint mRotateTexId = 0;
    //EGL
    EGLDisplay mEGLDisplay;
    EGLConfig mEGLConfig;
    EGLContext mEGLContext;
    EGLSurface mPreviewSurface;
    //OpenGL Render
    char* mGLVertexShader;
    char* mGLFragmentShader;
    GLuint mGLProgramId;
    GLuint mGLVertexCoords;
    GLuint mGLTextureCoords;
    GLint mGLUniformTexture;
    bool mIsGLInitialized;

    //OpenGL Copier
    char* mCopyVertexShader;
    char* mCopyFragmentShader;
    GLuint mCopyProgramId;
    GLuint mCopyVertexCoords;
    GLuint mCopyTextureCoords;
    GLint mCopyUniformTexMatrix;
    GLint mCopyUniformTransforms;
    GLint mCopyUniformTexture;
    bool mCopyIsInitialized;

    //OpenGL download
    GLuint mDownloadFBO;
    //OpenGL Filter

    //msg
    MediaRecorderHandler *mHandler;
    MsgQueue *mMsgQueue;
    pthread_t mThreadId;
};

class MediaRecorderHandler : public Handler{
private:
    MediaRecorder *mMediaRecorder;

public:
    MediaRecorderHandler(MediaRecorder *recorder, MsgQueue *queue)
        : Handler(queue) {
        mMediaRecorder = recorder;
}

void handleMessage(Msg *msg) {
    int what = msg->getWhat();
    //LOGD("handle msg is %d", what);
    switch(what) {
        case MSG_EGL_THREAD_CREATE:
            mMediaRecorder->initialize();
            break;
        case MSG_RENDER_FRAME:
            mMediaRecorder->renderFrame();
            break;
    }
}
};

#endif //FLVRecorder_H
