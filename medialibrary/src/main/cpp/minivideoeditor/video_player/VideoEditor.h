#ifndef VIDEO_EDITOR_H
#define VIDEO_EDITOR_H

#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <string>
#include <EGL/egl.h>
#include <list>
#include <android/AVMessageQueue.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
};

#include "circle_texture_queue.h"
#include "audio_output.h"
#include "video_output.h"
#include "common/opengl_media/movie_frame.h"

//解码文件时的缓冲区的最小和最大值
#define LOCAL_MIN_BUFFERED_DURATION   			0.5
#define LOCAL_MAX_BUFFERED_DURATION   			0.8
#define LOCAL_AV_SYNC_MAX_TIME_DIFF         	0.05
#define OPENGL_VERTEX_COORDNATE_CNT			    8
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

enum ThreadMessage {
    MSG_PREPARE,
};
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
static char* YUV_FRAME_FRAGMENT_SHADER =
        "varying highp vec2 yuvTexCoords;\n"
        "uniform sampler2D s_texture_y;\n"
        "uniform sampler2D s_texture_u;\n"
        "uniform sampler2D s_texture_v;\n"
        "void main(void)\n"
        "{\n"
        "highp float y = texture2D(s_texture_y, yuvTexCoords).r;\n"
        "highp float u = texture2D(s_texture_u, yuvTexCoords).r - 0.5;\n"
        "highp float v = texture2D(s_texture_v, yuvTexCoords).r - 0.5;\n"
        "\n"
        "highp float r = y + 1.402 * v;\n"
        "highp float g = y - 0.344 * u - 0.714 * v;\n"
        "highp float b = y + 1.772 * u;\n"
        "gl_FragColor = vec4(r,g,b,1.0);\n"
        "}\n";

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

class VideoEditorHandler;
//模块化设计
class VideoEditor {
public:
    VideoEditor();
    virtual ~VideoEditor();

    bool prepare(char *srcFilePath, bool isHWDecode);
    bool prepare_l();
    void start();
    void seekTo(float position);
    void pause();
    void resume();
    bool isPlaying();
    float getDuration();
    float getCurrentPosition();
    virtual void destroy();

    static int videoCallbackGetTex(FrameTexture** frameTex, void* ctx, bool forceGetFrame);
    static int audioCallbackFillData(byte* buf, size_t bufSize, void* ctx);

    void onSurfaceCreated(ANativeWindow* window, int widht, int height);
    void onSurfaceDestroyed();

    void signalOutputFrameAvailable();
    AVMessageQueue* getMessageQueue();
private:
    bool initFFmpeg();
    bool initEGL();
    bool releaseEGL();
    EGLSurface createWindowSurface(ANativeWindow *pWindow);
    EGLSurface createOffscreenSurface(int width, int height);

    void initVideoOutput(ANativeWindow* window);
    bool initAudioOutput();

    void avStreamFPSTimeBase(AVStream *st, float defaultTimeBase, float *pFPS, float *pTimeBase);

    void initDecoderThread();
    bool canDecode();
    void decode();
    void decodeFrames();
    void processDecodingFrame(bool& good, float duration);
    std::list<MovieFrame*>* decodeFrames(float minDuration, int* decodeVideoErrorState);
    bool addFrames(float thresholdDuration, std::list<MovieFrame*>* frames);
    bool decodeVideoFrame(AVPacket packet, int* decodeVideoErrorState);
    bool decodeAudioFrames(AVPacket* packet, std::list<MovieFrame*> * result, float& decodedDuration, float minDuration, int* decodeVideoErrorState);
    AudioFrame * handleAudioFrame();
    int processAudioData(short *sample, int size, float position, byte** buffer);
    int consumeAudioFrames(byte* outData, size_t bufferSize);
    int fillAudioData(byte* outData, int bufferSize);
    void clearAudioFrameQueue();
    void closeFile();
    void closeAudioStream();
    void closeVideoStream();

    void signalDecodeThread();
    void destroyDecoderThread();

    void initConverter();
    void uploadTexture();
    void drawFrame();
    float updateTexImage();
    void updateYUVTexImage(VideoFrame* yuvFrame);
    VideoFrame* handleVideoFrame();
    void copyFrameData(uint8_t * dst, uint8_t * src, int width, int height, int linesize);
    void pushToVideoQueue(GLuint inputTexId, int width, int height, float position);
    FrameTexture* getCorrectRenderTexture(bool forceGetFrame);
    void initCircleQueue(int videoWidth, int videoHeight);
    /** OpenGL **/
    GLuint loadProgram(char* pVertexSource, char* pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char* pSource);
    bool   checkGlError(const char* op);
    void matrixSetIdentityM(float *m);
    void initTexture();
    void initCopier();
    void initPassRender();
    void renderWithCoords(GLuint texId, GLfloat* vertexCoords, GLfloat* textureCoords);
    void renderToTexture(GLuint inputTexId, GLuint outputTexId);

    void processMessage();
    static void* startDecoderThread(void* ptr);
    static void* threadStartCallback(void *self);
    static void* upThreadCB(void *myself);
private:
    char *mUri;
    ANativeWindow* mWindow;
    int mScreenWidth;
    int mScreenHeight;
    bool mIsHWDecode;
    /** Play Status **/
    bool mIsPlaying;
    bool isDestroyed;
    bool isOnDecoding;
    bool isCompleted;

    bool mIsUserCancelled;
    /** decoder **/
    pthread_t videoDecoderThread;
    bool isDecodingFrames;
    bool pauseDecodeThreadFlag;
    bool is_eof;
    bool isInitializeDecodeThread;
    pthread_mutex_t videoDecoderLock;
    pthread_cond_t videoDecoderCondition;

    VideoOutput* videoOutput;
    AudioOutput* audioOutput;
    //video Queue & audio Queue manager
    pthread_mutex_t audioFrameQueueMutex;
    std::queue<AudioFrame*> *audioFrameQueue;
    float syncMaxTimeDiff;
    float minBufferedDuration;
    float maxBufferedDuration;
    CircleFrameTextureQueue* circleFrameTextureQueue;
    AudioFrame* currentAudioFrame;
    //封装格式上下文
    AVFormatContext *mFormatCtx;
    /** 视频流变量 **/
    AVCodecContext *mVideoCodecCtx;
    AVCodec *videoCodec;
    AVFrame *videoFrame;
    float fps;
    float videoTimeBase;
    int mVideoStreamIndex;
    int mWidth;
    int mHeight;
    bool seek_req;
    /** 音频流变量 **/
    AVCodecContext *mAudioCodecCtx;
    AVCodec *mAudioCodec;
    int audioStreamIndex;
    float audioTimeBase;
    AVFrame *audioFrame;
    /** 重采样变量 **/
    SwrContext *swrContext;
    void *swrBuffer;
    int swrBufferSize;

    float position;
    double moviePosition;
    float bufferedDuration;
    int currentAudioFramePos;
    /** HW decoder **/
    bool isMediaCodecInit;
    GLuint decodeTexId;
    jbyteArray inputBuffer;
    /** SW decoder **/

    //TextureFrame* textureFrame;
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
    /** Copier **/
    char* mCopierVertexShader;
    char* mCopierFragmentShader;

    bool mIsCopierInitialized;
    GLuint mCopierProgId;
    GLuint mCopierVertexCoords;
    GLuint mCopierTextureCoords;
    GLint mCopierUniformTexMatrix;
    GLint mCopierUniformTransforms;
    GLuint textures[3];
    GLint _uniformSamplers[3];
    /** PassRender **/
    char* mPassRenderVertexShader;
    char* mPassRenderFragmentShader;

    bool mIsPassRenderInitialized;
    GLuint mPassRenderProgId;
    GLuint mPassRenderVertexCoords;
    GLuint mPassRenderTextureCoords;
    GLint mPassRenderUniformTexture;

    //播放器消息队列
    VideoEditorHandler *mHandler;
    MessageQueue *mMsgQueue;
    pthread_t mThreadId;
    //回调消息队列
    AVMessageQueue *mCbMsgQueue;
};

class VideoEditorHandler : public Handler{
private:
    VideoEditor *mVideoEditor;

public:
    VideoEditorHandler(VideoEditor *videoEditor, MessageQueue *queue)
            : Handler(queue) {
        mVideoEditor = videoEditor;
    }

    void handleMessage(Message *msg) {
        int what = msg->getWhat();
        //LOGD("handle msg is %d", what);
        switch(what) {
            case MSG_PREPARE:
                mVideoEditor->prepare_l();;
                break;
        }
    }
};

#endif