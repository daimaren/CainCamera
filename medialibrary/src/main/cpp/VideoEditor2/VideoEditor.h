#ifndef VIDEO_EDITOR_H
#define VIDEO_EDITOR_H

#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <GLES2/gl2.h>
extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};

#include "utils.h"
#include "movie_frame.h"

//解码文件时的缓冲区的最小和最大值
#define LOCAL_MIN_BUFFERED_DURATION   			0.5
#define LOCAL_MAX_BUFFERED_DURATION   			0.8
#define LOCAL_AV_SYNC_MAX_TIME_DIFF         	0.05

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

    static int audioCallbackFillData(byte* buf, size_t bufSize, void* ctx);
    int consumeAudioFrames(byte* outData, size_t bufSize);
    static int videoCallbackGetTex();

    bool initMediaSync();

    void onSurfaceCreated(ANativeWindow* window, int widht, int height);
    void onSurfaceDestroyed();

    void signalOutputFrameAvailable();

private:
    bool prepare_l();
    static void* threadCB(void *self);
    static void* upThreadCB(void *myself);
    void initVideoOutput(ANativeWindow* window);
    bool initAudioOutput();
private:
    JavaVM *mJvm;
    jobject mObj;
    char *uri;

    ANativeWindow* mWindow;
    int mScreenWidth;
    int mScreenHeight;
    bool mIsPlaying;
    bool mIsHWDecode;
    bool mIsDestroyed;
    bool mIsDecoding;

    bool mIsUserCancelled;
    pthread_t mThreadId;
    //video Queue & audio Queue manager
    pthread_mutex_t audioFrameQueueMutex;
    std::queue<AudioFrame*> *audioFrameQueue;
    /** OpenGL **/
    /** FFMPEG **/
    AVFormatContext *pFormatCtx;

    /** 视频流解码变量 **/
    AVCodecContext *videoCodecCtx;
    AVCodec *videoCodec;
    AVFrame *videoFrame;
    std::list<int>* videoStreams;
    int videoStreamIndex;
    int width;
    int height;
    /** 音频流解码变量 **/
    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;
    AVFrame *audioFrame;
    //for decoder
    //for HW decoder
    bool isMediaCodecInit;
    GLuint decodeTexId;
    jbyteArray inputBuffer;
    //for SW decoder
    //for uploader
    GLfloat* vertexCoords;
    GLfloat* textureCoords;
    pthread_t upThreadId;
};

#endif