//
// Created by Glen on 2020/1/8.
//

#ifndef MEDIA_Recorder_H
#define MEDIA_Recorder_H

#include <SafetyQueue.h>
#include <Thread.h>
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "RecordParams.h"
#include "handler.h"
#include "utils.h"
#include "live_audio_packet_queue.h"
#include "live_video_packet_queue.h"
#include "live_common_packet_pool.h"
#include "live_audio_packet_pool.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/eval.h>
#include <libavutil/display.h>
#include <libavutil/pixfmt.h>
#include "stdio.h"
#include "unistd.h"
#ifdef __cplusplus
};
#endif

#define DUMP_YUV_BUFFER    0
#define DUMP_SW_ENCODER_H264_BUFFER    0
#define DUMP_HW_ENCODER_H264_BUFFER    1

#define H264_NALU_TYPE_NON_IDR_PICTURE                                  1
#define H264_NALU_TYPE_IDR_PICTURE                                      5
#define H264_NALU_TYPE_SEQUENCE_PARAMETER_SET                           7
#define H264_NALU_TYPE_PICTURE_PARAMETER_SET							8
#define H264_NALU_TYPE_SEI                                          	6

#define AUDIO_QUEUE_ABORT_ERR_CODE               -100200
#define VIDEO_QUEUE_ABORT_ERR_CODE               -100201

#define is_start_code(code)	(((code) & 0x0ffffff) == 0x01)

enum RenderThreadMessage {
    MSG_RENDER_FRAME = 0,
    MSG_EGL_THREAD_CREATE,
    MSG_EGL_CREATE_PREVIEW_SURFACE,
    MSG_SWITCH_CAMERA_FACING,
    MSG_SWITCH_FILTER,
    MSG_PREPARE_RECORDING,
    MSG_START_RECORDING,
    MSG_STOP_RECORDING,
    MSG_EGL_DESTROY_PREVIEW_SURFACE,
    MSG_EGL_THREAD_EXIT,
    MSG_FRAME_AVAILIBLE
};

typedef unsigned char byte;
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPRESENTATIONTIMEANDROIDPROC)(EGLDisplay display, EGLSurface surface, khronos_stime_nanoseconds_t time);

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
static GLfloat CAMERA_TEXTURE_ROTATED_90[8] = {
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 0.0f
};

static GLfloat CAMERA_TEXTURE_ROTATED_180[8] = {
        1.0f, 0.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
};

static GLfloat CAMERA_TEXTURE_ROTATED_270[8] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
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

class MiniRecorderHandler;
class HWEncoderHandler;

class MiniRecorder {
public:
    MiniRecorder();

    virtual ~MiniRecorder();

    void prepareEGLContext(ANativeWindow *window, JNIEnv *env, JavaVM *g_jvm, jobject obj, int screenWidth, int screenHeight, int cameraFacingId);
    void notifyFrameAvailable();
    // 设置录制监听器
    void setOnRecordListener(OnRecordListener *listener);
    // 准备录制器
    int prepare();
    // 释放资源
    void release();
    // 开始录制
    void startRecord();
    void startRecording();
    // 停止录制
    void stopRecord();
    void stopRecording();
    // drainEncodedData
    void drainEncodedData();
    // 是否正在录制
    bool isRecording();

    RecordParams *getRecordParams();

    virtual bool initialize();
    virtual void renderFrame();
    virtual void destroyEGLContext();
    int prepare_l();
    int pushAudioBufferToQueue(short* samples, int size);
private:
    int createVideoEncoder();
    int createAudioEncoder();
    int destoryAudioEncoder();
    //HW Encoder
    void createHWEncoder();
    void createSurfaceRender();
    void destroyHWEncoder();
    //audio encoder
    void cpyToAudioSamples(short* sourceBuffer, int cpyLength);
    void flushAudioBufferToQueue();
    int alloc_audio_stream(const char * codec_name);
    int alloc_avframe();
    int encodeAudio(LiveAudioPacket **audioPacket);
    int getAudioFrame(int16_t * samples, int frame_size, int nb_channels, double* presentationTimeMills);
    int cpyToSamples(int16_t * samples, int samplesInShortCursor, int cpyPacketBufferSize, double* presentationTimeMills);
    void discardAudioPacket();
    int getAudioPacket();
    int processAudio();
    int mixtureMusicBuffer(long frameNum, short* accompanySamples, int accompanySampleSize, short* audioSamples, int audioSampleSize);
    int getAudioPacket(LiveAudioPacket ** audioPacket);
    //MediaMux
    int initFFmepg();
    int deinitFFmpeg();
    int getH264Packet(LiveVideoPacket ** packet);
    int writeFrame();
    int write_audio_frame(AVFormatContext *oc, AVStream *st);
    int write_video_frame(AVFormatContext *oc, AVStream *st);
    AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, char *codec_name);
    void parseH264SequenceHeader(uint8_t* in_pBuffer, uint32_t in_ui32Size,
                                 uint8_t** inout_pBufferSPS, int& inout_ui32SizeSPS,
                                 uint8_t** inout_pBufferPPS, int& inout_ui32SizePPS);
    uint32_t findStartCode(uint8_t* in_pBuffer, uint32_t in_ui32BufferSize,
                           uint32_t in_ui32Code, uint32_t& out_ui32ProcessedBytes);
    // EGL functions
    bool initEGL();
    EGLSurface createWindowSurface(ANativeWindow* pWindow);
    // OpenGL functions
    void fillTextureCoords();
    float flip(float i);
    bool initTextureFBO();
    bool initRenderer();
    bool initCopier();
    GLuint loadProgram(char* pVertexSource, char* pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char* pSource);
    bool   checkGlError(const char* op);
    int initDecodeTexture();
    void renderToView(GLuint texID, int screenWidth, int screenHeight);
    void renderToAutoFitTexture(GLuint inputTexId, int width, int height, GLuint outputTexId);
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

    static void *startPreviewThread(void *myself);
    void processPreviewMessage();
    static void *startHardWareEncodeThread(void *myself);
    void videoEncodeLoop();
    static void *startWriteThread(void *myself);
    void writerLoop();
    static void *startAudioEncodeThread(void *myself);
    void audioEncodeLoop();
private:
    FILE* mflvFile;
    FILE* mDumpYuvFile;
    FILE* mDumpH264File;
    Mutex mMutex;
    Condition mCondition;

    OnRecordListener *mRecordListener;

    int64_t duration;
    //for status
    bool mAbortRequest; // 停止请求
    bool mStartRequest; // 开始录制请求
    bool mExit;         // 完成退出标志
    bool mIsVideoEncoding = false;
    //HW Encode变量
    LiveCommonPacketPool *packetPool;
    jbyteArray mEncoderOutputBuf = NULL;
    EGLSurface mEncoderSurface;
    ANativeWindow *mEncoderWindow;
    bool mUseHardWareEncoding = true;
    bool mIsSPSUnWriteFlag = false;
    //Audio变量
    LivePacketPool* 	  pcmPacketPool;
    LiveCommonPacketPool* accompanyPacketPool;
    LiveAudioPacketPool *aacPacketPool;
    int audioSamplesCursor = 0;
    int audioBufferSize;
    short* audioSamples;
    int audioBufferTimeMills;

    int packetBufferSize;
    short* packetBuffer;
    int packetBufferCursor;
    double  packetBufferPresentationTimeMills;
    //Audio Encode变量
    bool mIsAudioEncoding = false;
    AVCodecContext *avCodecContext;
    AVFrame         *encode_frame;
    int64_t         audio_next_pts;
    uint8_t         **audio_samples_data;
    int       		audio_nb_samples; //todo 还没初始化
    int 			audio_samples_size;
    //Media Writer变量
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *video_st;
    AVStream *audio_st;
    AVBitStreamFilterContext *bsfc;
    bool mIsWriting = false;
    int lastPresentationTimeMs;
    double lastAudioPacketPresentationTimeMills;
    bool isWriteHeaderSuccess = false;
    long sendLatestFrameTimemills;
    uint8_t *headerData;
    int headerSize;
    //预览视窗
    ANativeWindow *mNativeWindow;
    //反射调用变量
    JavaVM *mJvm;
    jobject mObj;

    int mScreenWidth;
    int mScreenHeight;

    int mDegress;
    int mFacingId;
    int mTextureWidth;
    int mTextureHeight;
    int mBitRateKbs;
    int mFrameRate;

    RecordParams *mRecordParams;    // 录制参数

    //OpenGL变量
    GLfloat* mTextureCoords;
    int mTextureCoordsSize;

    GLuint mDecodeTexId = 0;
    GLuint mFBO;
    GLuint mRotateTexId = 0;
    GLuint inputTexId;
    GLuint outputTexId;
    //EGL变量
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

    //Encoder Surface Render
    GLuint mSurfaceRenderProgId;
    char* mSurfaceRenderVertexShader;
    char* mSurfaceRenderFragmentShader;
    GLuint mSurfaceRenderVertexCoords;
    GLuint mSurfaceRenderTextureCoords;
    GLuint mSurfaceRenderUniformTexture;
    bool mIsSurfaceRenderInit;
    //OpenGL download
    GLuint mDownloadFBO;
    //OpenGL Filter

    //preview
    MiniRecorderHandler *mPreviewHandler;
    MsgQueue            *mPreviewMsgQueue;
    //hareware encode
    HWEncoderHandler    *mEncodeHandler;
    MsgQueue            *mEncodeMsgQueue;
    //thread id
    pthread_t           mPreviewThreadId;
    pthread_t           mAudioEncoderThreadId;
    pthread_t           mVideoEncoderThreadId;
    pthread_t           mWriterThreadId;
};
/**
 * MiniRecorderHandler
 */
class MiniRecorderHandler : public Handler{
private:
    MiniRecorder *mMiniRecorder;
public:
    MiniRecorderHandler(MiniRecorder *recorder, MsgQueue *queue)
        : Handler(queue) {
        mMiniRecorder = recorder;
}

void handleMessage(Msg *msg) {
    int what = msg->getWhat();
    //LOGD("handle msg is %d", what);
    switch(what) {
        case MSG_EGL_THREAD_CREATE:
            mMiniRecorder->initialize();
            break;
        case MSG_RENDER_FRAME:
            mMiniRecorder->renderFrame();
            break;
        case MSG_PREPARE_RECORDING:
            mMiniRecorder->prepare_l();
            break;
        case MSG_START_RECORDING:
            mMiniRecorder->startRecording();
            break;
        case MSG_STOP_RECORDING:
            mMiniRecorder->stopRecording();
            break;
        default:
            break;
    }
}
};
/**
 * HWEncoderHandler
 */
class HWEncoderHandler : public Handler {
private:
    MiniRecorder *mMiniRecorder;
public:
    HWEncoderHandler(MiniRecorder *recorder, MsgQueue *queue) :
            Handler(queue) {
        this->mMiniRecorder = recorder;
    }

    void handleMessage(Msg *msg) {
        int what = msg->getWhat();
        //LOGD("Encode handle msg is %d", what);
        switch (what) {
            case MSG_FRAME_AVAILIBLE:
                mMiniRecorder->drainEncodedData();
                break;
        }
    }
};
#endif
