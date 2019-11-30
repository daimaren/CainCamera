#ifndef VIDEO_EDITOR_H
#define VIDEO_EDITOR_H

#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <string>
#include <EGL/egl.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
};

#include "utils.h"
#include "movie_frame.h"
#include "circle_texture_queue.h"
#include "audio_output.h"
#include "VideoOutput.h"
//解码文件时的缓冲区的最小和最大值
#define LOCAL_MIN_BUFFERED_DURATION   			0.5
#define LOCAL_MAX_BUFFERED_DURATION   			0.8
#define LOCAL_AV_SYNC_MAX_TIME_DIFF         	    0.05
#define OPENGL_VERTEX_COORDNATE_CNT			    8
static GLfloat DECODER_COPIER_GL_VERTEX_COORDS[8] = {
        -1.0f, -1.0f,	// 0 top left
        1.0f, -1.0f,	// 1 bottom left
        -1.0f, 1.0f,  // 2 bottom right
        1.0f, 1.0f,	// 3 top right
};

static GLfloat DECODER_COPIER_GL_TEXTURE_COORDS_NO_ROTATION[8] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
};

//模块化设计
class VideoEditor {
public:
    VideoEditor();
    virtual ~VideoEditor();

    bool prepare(char *srcFilePath, JavaVM *jvm, jobject obj, bool isHWDecode);
    void play();
    void seek(float position);
    void pause();
    virtual void destroy();
    float getDuration();

    static int videoCallbackGetTex(FrameTexture** frameTex, void* ctx, bool forceGetFrame);
    static int audioCallbackFillData(byte* buf, size_t bufSize, void* ctx);
    int consumeAudioFrames(byte* outData, size_t bufSize);

    void onSurfaceCreated(ANativeWindow* window, int widht, int height);
    void onSurfaceDestroyed();

    void signalOutputFrameAvailable();

private:
    bool prepare_l();
    bool openFile();
    bool initEGL();
    EGLSurface createWindowSurface(ANativeWindow *pWindow);
    EGLSurface createOffscreenSurface(int width, int height);

    void initVideoOutput(ANativeWindow* window);
    bool initAudioOutput();

    void initDecoderThread();
    bool canDecode();
    void decode();
    void decodeFrames();
    void processDecodingFrame(bool& good);
    std::list<MovieFrame*>* decodeFrames(int* decodeVideoErrorState);
    bool addFrames(float thresholdDuration, std::list<MovieFrame*>* frames);
    bool decodeVideoFrame(AVPacket packet, int* decodeVideoErrorState);
    void uploadTexture();
    bool decodeAudioFrames(AVPacket* packet, std::list<MovieFrame*> * result, float& decodedDuration, int* decodeVideoErrorState);
    void destroyDecoderThread();

    static void* startDecoderThread(void* ptr);
    static void* prepareThreadCB(void *self);
    static void* upThreadCB(void *myself);
private:
    JavaVM *mJvm;
    jobject mObj;
    char *uri;

    ANativeWindow* mWindow;
    int mScreenWidth;
    int mScreenHeight;
    bool mIsPlaying;
    bool mIsHWDecode;
    bool isDestroyed;
    bool isOnDecoding;;

    bool mIsUserCancelled;
    pthread_t mThreadId;
    /** decoder **/
    pthread_t videoDecoderThread;
    bool isDecodingFrames;
    bool pauseDecodeThreadFlag;
    bool is_eof;
    pthread_mutex_t videoDecoderLock;
    pthread_cond_t videoDecoderCondition;

    VideoOutput* videoOutput;
    AudioOutput* audioOutput;
    //video Queue & audio Queue manager
    pthread_mutex_t audioFrameQueueMutex;
    std::queue<AudioFrame*> *audioFrameQueue;
    CircleFrameTextureQueue* circleFrameTextureQueue;
    AudioFrame* currentAudioFrame;

    /** FFMPEG **/
    AVFormatContext *pFormatCtx;
    /** 视频流变量 **/
    AVCodecContext *videoCodecCtx;
    AVCodec *videoCodec;
    AVFrame *videoFrame;
    int videoStreamIndex;
    int width;
    int height;
    /** 音频流变量 **/
    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;
    int audioStreamIndex;
    AVFrame *audioFrame;
    /** 重采样变量 **/
    SwrContext *swrContext;
    void *swrBuffer;
    int swrBufferSize;

    /** HW decoder **/
    bool isMediaCodecInit;
    GLuint decodeTexId;
    jbyteArray inputBuffer;
    /** SW decoder **/
    /** EGL **/
    EGLDisplay mEGLDisplay;
    EGLConfig mEGLConfig;
    EGLContext mEGLContext;
    EGLSurface copyTexSurface;
    /** OpenGL **/
    GLfloat* vertexCoords;
    GLfloat* textureCoords;
    GLuint mFBO;
    GLuint outputTexId;
    //for uploader
    pthread_t upThreadId;
};

#endif