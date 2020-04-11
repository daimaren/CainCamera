//
// Created by Glen on 2020/1/08.
//
#include "MiniRecorder.h"
#include "live_audio_packet_queue.h"
#include "live_video_packet_queue.h"

/**
 * 主要到事情说三遍，为了吃透核心代码，先不要做封装，所有核心代码写在这一个cpp里
 * 完成音视频的采集、编码、封包成 mp4 输出 miniRecorder，核心是调用ffmpeg的api来编码和复用
 */
MiniRecorder::MiniRecorder() : mRecordListener(nullptr), mAbortRequest(true),
                                     mStartRequest(false), mExit(true),mCopyIsInitialized(false),
                                    mIsGLInitialized(false), mIsSurfaceRenderInit(false){
    mRecordParams = new RecordParams();
    mGLVertexShader = OUTPUT_VIEW_VERTEX_SHADER;
    mGLFragmentShader = OUTPUT_VIEW_FRAG_SHADER;

    mCopyVertexShader = NO_FILTER_VERTEX_SHADER;
    mCopyFragmentShader = GPU_FRAME_FRAGMENT_SHADER;
    mTextureCoords = NULL;
    mTextureCoordsSize = 8;

    mSurfaceRenderVertexShader = OUTPUT_VIEW_VERTEX_SHADER;
    mSurfaceRenderFragmentShader = OUTPUT_VIEW_FRAG_SHADER;

    mTextureCoords = new GLfloat[mTextureCoordsSize];
    GLfloat* tempTextureCoords;
    tempTextureCoords = CAMERA_TEXTURE_NO_ROTATION;
    memcpy(mTextureCoords, tempTextureCoords, mTextureCoordsSize * sizeof(GLfloat));

    mMsgQueue = new MsgQueue("MiniRecorder MsgQueue");
    mHandler = new MiniRecorderHandler(this, mMsgQueue);
}

MiniRecorder::~MiniRecorder() {
    release();
    if (mRecordParams != nullptr) {
        delete mRecordParams;
        mRecordParams = nullptr;
    }

    mCopyIsInitialized = false;
    if (mCopyProgramId) {
        glDeleteProgram(mCopyProgramId);
        mCopyProgramId = 0;
    }
    ALOGI("after delete Copier");
    mIsGLInitialized = false;
    if (mGLProgramId) {
        glDeleteProgram(mGLProgramId);
        mGLProgramId = 0;
    }
    ALOGI("after delete render");
    if (mRotateTexId) {
        ALOGI("delete mRotateTexId");
        glDeleteTextures(1, &mRotateTexId);
    }
    if (mFBO) {
        ALOGI("delete mFBO");
        glDeleteFramebuffers(1, &mFBO);
    }
    if (mDownloadFBO) {
        ALOGI("delete mDownloadFBO");
        glDeleteFramebuffers(1, &mDownloadFBO);
    }
    if (mDecodeTexId) {
        ALOGI("delete mDecodeTexId");
        glDeleteTextures(1, &mDecodeTexId);
    }
    if (inputTexId) {
        ALOGI("delete inputTexId ..");
        glDeleteTextures(1, &inputTexId);
    }
    if(outputTexId){
        ALOGI("delete outputTexId ..");
        glDeleteTextures(1, &outputTexId);
    }
    if(mTextureCoords){
        delete[] mTextureCoords;
        mTextureCoords = NULL;
    }
}

bool MiniRecorder::initialize() {
    initEGL();
    mPreviewSurface = createWindowSurface(mNativeWindow);
    if (mPreviewSurface != NULL) {
        eglMakeCurrent(mEGLDisplay, mPreviewSurface, mPreviewSurface, mEGLContext);
    }
    //get camera info from app layer
    mDegress = 270; //90 ok
    mTextureWidth = 720; //720
    mTextureHeight = 1280; //1280
    mBitRateKbs = 900;
    mFrameRate = 20;
    ALOGI("screen : {%d, %d}", mScreenWidth, mScreenHeight);
    ALOGI("Texture : {%d, %d}", mTextureWidth, mTextureHeight);
    fillTextureCoords();
    if (!initCopier()) {
        ALOGE("initCopier failed");
        return false;
    }
    initRenderer();
    int ret = initDecodeTexture();
    if (ret < 0) {
        ALOGI("init texture failed");
        if (mDecodeTexId) {
            glDeleteTextures(1, &mDecodeTexId);
        }
    }
    initTextureFBO();
    startCameraPreview();
    return true;
}

void MiniRecorder::fillTextureCoords() {
    GLfloat* tempTextureCoords;
    switch (mDegress) {
        case 90:
            tempTextureCoords = CAMERA_TEXTURE_ROTATED_90;
            break;
        case 180:
            tempTextureCoords = CAMERA_TEXTURE_ROTATED_180;
            break;
        case 270:
            tempTextureCoords = CAMERA_TEXTURE_ROTATED_270;
            break;
        case 0:
        default:
            tempTextureCoords = CAMERA_TEXTURE_NO_ROTATION;
            break;
    }
    memcpy(mTextureCoords, tempTextureCoords, mTextureCoordsSize * sizeof(GLfloat));

    if(1){
        mTextureCoords[1] = flip(mTextureCoords[1]);
        mTextureCoords[3] = flip(mTextureCoords[3]);
        mTextureCoords[5] = flip(mTextureCoords[5]);
        mTextureCoords[7] = flip(mTextureCoords[7]);
    }
}

float MiniRecorder::flip(float i) {
    if (i == 0.0f) {
        return 1.0f;
    }
    return 0.0f;
}

/**
 * prepareEGLContext
 */
void MiniRecorder::prepareEGLContext(ANativeWindow *window, JNIEnv *env, JavaVM *g_jvm, jobject obj,
                                      int screenWidth, int screenHeight, int cameraFacingId) {
    ALOGI("prepareEGLContext");
    mJvm = g_jvm;
    mObj = obj;
    mNativeWindow = window;
    mScreenWidth = screenWidth;
    mScreenHeight = screenHeight;
    mFacingId = cameraFacingId;
    mHandler->postMessage(new Msg(MSG_EGL_THREAD_CREATE));
    pthread_create(&mThreadId, 0, threadStartCallback, this);
}

void *MiniRecorder::threadStartCallback(void *myself) {
    ALOGI("threadStartCallback");
    MiniRecorder *recorder = (MiniRecorder *) myself;
    recorder->processMessage();
    pthread_exit(0);
    return 0;
}

void MiniRecorder::processMessage() {
    bool renderingEnabled = true;
    while (renderingEnabled) {
        Msg *msg = NULL;
        if (mMsgQueue->dequeueMessage(&msg, true) > 0) {
            if (MESSAGE_QUEUE_LOOP_QUIT_FLAG == msg->execute()) {
                renderingEnabled = false;
            }
            delete msg;
        }
    }
}

void *MiniRecorder::encoderThreadCallback(void *myself) {
    MiniRecorder *recorder = (MiniRecorder *) myself;
    recorder->encodeLoop();
    pthread_exit(0);
    return 0;
}

void MiniRecorder::encodeLoop() {
    while (mIsEncoding) {
        Msg *msg = NULL;
        if (mMsgEncodeQueue->dequeueMessage(&msg, true) > 0) {
            if (MESSAGE_QUEUE_LOOP_QUIT_FLAG == msg->execute()) {
                mIsEncoding = false;
            }
            delete msg;
        }
    }
    ALOGI("HWEncoderAdapter encode Thread ending...");
}

void *MiniRecorder::writerThreadCallback(void *myself) {
    ALOGI("writerThreadCallback");
    MiniRecorder *recorder = (MiniRecorder *) myself;
    recorder->writerLoop();
    pthread_exit(0);
    return 0;
}

void MiniRecorder::writerLoop() {
    while (mIsWriting) {
        //write

    }
    ALOGI("writer Thread ending...");
}

void MiniRecorder::destroyEGLContext() {
    ALOGI("destroyEGLContext");
    if (mHandler) {
        mHandler->postMessage(new Msg(MSG_EGL_THREAD_EXIT));
        mHandler->postMessage(new Msg(MESSAGE_QUEUE_LOOP_QUIT_FLAG));
    }
    pthread_join(mThreadId, 0);
    if (mMsgQueue) {
        mMsgQueue->abort();
        delete mMsgQueue;
        mMsgQueue = NULL;
    }
    delete mHandler;
    mHandler = NULL;
    ALOGI("MiniRecorder thread stopped");
}

void MiniRecorder::renderFrame() {
    //相机视频帧更新到纹理
    updateTexImage();
    //处理纹理
    processFrame();
    if (mPreviewSurface != EGL_NO_SURFACE) {
        eglMakeCurrent(mEGLDisplay, mPreviewSurface, mPreviewSurface, mEGLContext);
        renderToViewWithAutofit(mRotateTexId, mScreenWidth, mScreenHeight, mTextureWidth, mTextureHeight);
        eglSwapBuffers(mEGLDisplay, mPreviewSurface);
    }
    if (mIsEncoding) {
        if (mIsHWEncode) {
            //hw encode
            eglMakeCurrent(mEGLDisplay, mEncoderSurface, mEncoderSurface, mEGLContext);
            renderToView(mRotateTexId, mTextureWidth, mTextureHeight);
            //postMessage
            if (mEncodeHandler) {
                mEncodeHandler->postMessage(new Msg(MSG_FRAME_AVAILIBLE));
            }
            eglSwapBuffers(mEGLDisplay, mEncoderSurface);
        } else {
            //get RGBA data, then convert to I420
            uint8_t *rgbaData = (uint8_t *) malloc((size_t) mTextureWidth * mTextureHeight * 4);
            if (rgbaData == nullptr) {
                ALOGE("Could not allocate memory");
                return;
            }
            downloadImageFromTexture(mRotateTexId, rgbaData, mTextureWidth, mTextureHeight);
            //todo
            free(rgbaData);
        }
    }
}

/**
 * 准备录制
 */
void MiniRecorder::startRecord() {
    if (mHandler)
        mHandler->postMessage(new Msg(MSG_START_RECORDING));
}

/**
 * 准备录制
 */
int MiniRecorder::prepare() {
    if (mHandler)
        mHandler->postMessage(new Msg(MSG_PREPARE_RECORDING));
}

/**
 * 开始录制
 */
void MiniRecorder::startRecording() {
    createMediaWriter();
    startProducer();
}

/**
 * 通知有新的一帧
 */
void MiniRecorder::notifyFrameAvailable() {
    if (mHandler) {
        mHandler->postMessage(new Msg(MSG_RENDER_FRAME));
    }
}

/**
 * 停止录制
 */
void MiniRecorder::stopRecord() {
    ALOGI("stopRecord_l");
    mIsEncoding = false;

    mEncodeHandler->postMessage(new Msg(MESSAGE_QUEUE_LOOP_QUIT_FLAG));
    pthread_join(mEncoderThreadId, 0);
    if (mMsgEncodeQueue) {
        mMsgEncodeQueue->abort();
        delete mMsgEncodeQueue;
        mMsgEncodeQueue = NULL;
    }
    delete mEncodeHandler;
    mEncodeHandler = NULL;

    if (mIsHWEncode) {
        destroyHWEncoder();
#if DUMP_HW_ENCODER_H264_BUFFER
        if (mDumpH264File) {
            fclose(mDumpH264File);
        }
#endif
        if (EGL_NO_SURFACE != mEncoderSurface) {
            eglDestroySurface(mEGLDisplay, mEncoderSurface);
            mEncoderSurface = EGL_NO_SURFACE;
        }
        if (NULL != mEncoderWindow) {
            ANativeWindow_release(mEncoderWindow);
            mEncoderWindow = NULL;
        }
        mIsSurfaceRenderInit = false;
        glDeleteProgram(mSurfaceRenderProgId);
    } else {
        //SW Encode
    }
}
/**
 * 准备阶段，初始化所有的资源，比如source、codec、writer
 */
int MiniRecorder::prepare_l() {

    createMediaWriter();
}

/**
 * 判断是否正在录制
 * @return
 */
bool MiniRecorder::isRecording() {
    bool recording = false;
    return recording;
}

/**
 * 设置录制监听器
 * @param listener
 */
void MiniRecorder::setOnRecordListener(OnRecordListener *listener) {
    mRecordListener = listener;
}

/**
 * 释放资源
 */
void MiniRecorder::release() {
    // 等待退出
    mMutex.lock();
    while (!mExit) {
        mCondition.wait(mMutex);
    }
    mMutex.unlock();

    if (mRecordListener != nullptr) {
        delete mRecordListener;
        mRecordListener = nullptr;
    }
}

RecordParams* MiniRecorder::getRecordParams() {
    return mRecordParams;
}

/**
 * createProducer
 */
int MiniRecorder::startProducer() {
    RecordParams *params = mRecordParams;
    mRecordParams->pixelFormat = 0;
    mRecordParams->width = mTextureWidth;
    mRecordParams->height = mTextureHeight;
    if (params->rotateDegree % 90 != 0) {
        ALOGE("invalid rotate degree: %d", params->rotateDegree);
        return -1;
    }

    int ret;

    ALOGI("Record to file: %s, width: %d, height: %d", params->dstFile, params->width,
         params->height);

    if (mIsHWEncode) {
        mMsgEncodeQueue = new MsgQueue("HWEncoder message queue");
        mEncodeHandler = new HWEncoderHandler(this, mMsgEncodeQueue);
        createHWEncoder();
        createSurfaceRender();
        pthread_create(&mEncoderThreadId, 0, encoderThreadCallback, this);
        mIsSPSUnWriteFlag = true;
        mIsEncoding = true;
#if DUMP_HW_ENCODER_H264_BUFFER
        mDumpH264File = fopen("/storage/emulated/0/dump.h264", "wb");
        if (!mDumpH264File) {
            ALOGI("DumpH264File open error");
        }
#endif
    } else {
        //SoftEncoder
    }
    ALOGI("prepare done");
    return 0;
}

//===============HW Encoder Start=============================/
void MiniRecorder::createHWEncoder() {
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        ALOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        ALOGI("getJNIEnv failed");
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID createMediaCodecSurfaceEncoderFunc = env->GetMethodID(jcls, "createMediaCodecSurfaceEncoderFromNative",
                                                            "(IIII)V");
        if (NULL != createMediaCodecSurfaceEncoderFunc) {
            env->CallVoidMethod(mObj, createMediaCodecSurfaceEncoderFunc, mTextureWidth, mTextureHeight, mBitRateKbs, mFrameRate);
        }
        jmethodID getEncodeSurfaceFromNativeFunc = env->GetMethodID(jcls,
                                                                    "getEncodeSurfaceFromNative",
                                                                    "()Landroid/view/Surface;");
        if (NULL != getEncodeSurfaceFromNativeFunc) {
            jobject surface = env->CallObjectMethod(mObj, getEncodeSurfaceFromNativeFunc);
            mEncoderWindow = ANativeWindow_fromSurface(env, surface);
            mEncoderSurface = createWindowSurface(mEncoderWindow);
        }
    }
    jbyteArray  tempOutputBuf = env->NewByteArray(mTextureWidth * mTextureHeight * 3 / 2);
    mEncoderOutputBuf = static_cast<jbyteArray >(env->NewGlobalRef(tempOutputBuf));
    env->DeleteLocalRef(tempOutputBuf);
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        ALOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
    }
}

void MiniRecorder::createSurfaceRender() {
    mSurfaceRenderProgId = loadProgram(mSurfaceRenderVertexShader, mSurfaceRenderFragmentShader);
    if (!mSurfaceRenderProgId) {
        ALOGI("mSurfaceRenderProgId nullptr");
        return;
    }
    mSurfaceRenderVertexCoords = glGetAttribLocation(mSurfaceRenderProgId, "position");
    checkGlError("glGetAttribLocation vPosition");
    mSurfaceRenderTextureCoords = glGetAttribLocation(mSurfaceRenderProgId, "texcoord");
    checkGlError("glGetAttribLocation vTexCords");
    mSurfaceRenderUniformTexture = glGetUniformLocation(mSurfaceRenderProgId, "yuvTexSampler");
    checkGlError("glGetAttribLocation yuvTexSampler");
    mIsSurfaceRenderInit = true;
}

void MiniRecorder::drainEncodedData() {
    if (!mIsEncoding) {
        ALOGI("have stop record, drainEncodedData return");
        return;
    }
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        ALOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        ALOGI("getJNIEnv failed");
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID drainEncoderFunc = env->GetMethodID(jcls, "pullH264StreamFromDrainEncoderFromNative",
                                                         "([B)J");
        if (NULL != drainEncoderFunc) {
            long bufferSize = (long) env->CallLongMethod(mObj, drainEncoderFunc, mEncoderOutputBuf);
            byte* outputData = (uint8_t*)env->GetByteArrayElements(mEncoderOutputBuf, 0);
            int size = (int) bufferSize;
#ifdef DUMP_HW_ENCODER_H264_BUFFER
            //dump H.264 data to file
            int count = fwrite(outputData, size, 1, mDumpH264File);
            //ALOGI("write h264 size %d len %d", count, size); //从log来看，只有sps，没有pps
#endif
            //todo push to queue
            env->ReleaseByteArrayElements(mEncoderOutputBuf, (jbyte *) outputData, 0);
        }
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        ALOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
    }
}

void MiniRecorder::destroyHWEncoder() {
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        ALOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        ALOGI("getJNIEnv failed");
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID closeMediaCodecFunc = env->GetMethodID(jcls, "closeMediaCodecCalledFromNative",
                                                                        "()V");
        if (NULL != closeMediaCodecFunc) {
            env->CallVoidMethod(mObj, closeMediaCodecFunc);
        }
    }
    if (mEncoderOutputBuf) {
        env->DeleteGlobalRef(mEncoderOutputBuf);
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        ALOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
    }
}
//===============HW Encoder End=============================/
//===============Media Writer Start=============================/

int MiniRecorder::createMediaWriter() {
    AVCodec *video_codec = NULL;
    AVCodec *audio_codec = NULL;
    //1.register all codecs and formats
    av_register_all();
    //2.alloc output context
    avformat_alloc_output_context2(&oc, NULL, "flv", mRecordParams->dstFile);
    if (!oc) {
        ALOGI("avformat_alloc_output_context2 failed");
        return -1;
    }
    fmt = oc->oformat;
    //3.add video stream and audio stream to AVFormatContext
    fmt->video_codec = AV_CODEC_ID_H264;
    video_st = add_stream(oc, &video_codec, fmt->video_codec, NULL);
    if (video_st && video_codec) {
        ALOGI("add video stream success");
    }
    audio_st = add_stream(oc, &audio_codec, AV_CODEC_ID_AAC, NULL);
    if (audio_st && audio_codec) {
        ALOGI("add audio stream success");
    }
    if (!(fmt->flags & AVFMT_NOFILE)) {
        int ret = avio_open2(&oc->pb, mRecordParams->dstFile, AVIO_FLAG_WRITE, &oc->interrupt_callback, NULL);
        if (ret < 0) {
            ALOGI("Could not open '%s': %s\n", mRecordParams->dstFile, av_err2str(ret));
            return -1;
        }
    }
    pthread_create(&mWriterThreadId, 0, threadStartCallback, this);
}

int MiniRecorder::writeFrame() {
    int ret = 0;
    /* Compute current audio and video time. */
    double video_time = lastPresentationTimeMs / 1000.0f;
    double audio_time = lastAudioPacketPresentationTimeMills / 1000.0f;

    ALOGI("video_time is %lf, audio_time is %f", video_time, audio_time);
    /* write interleaved audio and video frames */
    if (!video_st || (video_st && audio_st && audio_time < video_time)) {
        ret = write_audio_frame(oc, audio_st);
    } else if (video_st) {
        ret = write_video_frame(oc, video_st);
    }
    sendLatestFrameTimemills = getCurrentTimeMills();
    duration = MIN(audio_time, video_time);
    return ret;
}

int MiniRecorder::closeMediaWriter() {

}

int MiniRecorder::write_audio_frame(AVFormatContext *oc, AVStream *st) {
    int ret = AUDIO_QUEUE_ABORT_ERR_CODE;
    LiveAudioPacket *audioPacket = NULL;
    //ret = fillAACPacket(&audioPacket)
    if (ret > 0) {
        AVPacket pkt = {0}; // data and size must be 0;
        av_init_packet(&pkt);
        lastAudioPacketPresentationTimeMills = audioPacket->position;
        pkt.data = audioPacket->data;
        pkt.size = audioPacket->size;
        pkt.dts = pkt.pts = lastAudioPacketPresentationTimeMills / 1000.0f / av_q2d(st->time_base);
        pkt.duration = 1024;
        pkt.stream_index = st->index;
        AVPacket newpacket;
        av_init_packet(&newpacket);
        ret = av_bitstream_filter_filter(bsfc, st->codec, NULL, &newpacket.data, &newpacket.size,
                                         pkt.data, pkt.size, pkt.flags & AV_PKT_FLAG_KEY);
        if (ret >= 0) {
            newpacket.pts = pkt.pts;
            newpacket.dts = pkt.dts;
            newpacket.duration = pkt.duration;
            newpacket.stream_index = pkt.stream_index;
            ret = av_interleaved_write_frame(oc, &newpacket);
            if (ret != 0) {
                ALOGI("Error while writing audio frame: %s\n", av_err2str(ret));
            }
        } else {
            ALOGI("Error av_bitstream_filter_filter: %s\n", av_err2str(ret));
        }
        av_free_packet(&newpacket);
        av_free_packet(&pkt);
        delete audioPacket;
    } else {
        ret = AUDIO_QUEUE_ABORT_ERR_CODE;
    }
    return ret;
}

int MiniRecorder::write_video_frame(AVFormatContext *oc, AVStream *st) {
    int ret = 0;
    AVCodecContext *c = st->codec;

    LiveVideoPacket *h264Packet = NULL;
    //fillH264Packet(&h264Packet);
    if (h264Packet == NULL) {
        ALOGE("fillH264PacketCallback get null packet");
        return VIDEO_QUEUE_ABORT_ERR_CODE;
    }
    int bufferSize = (h264Packet)->size;
    uint8_t* outputData = (uint8_t *) ((h264Packet)->buffer);
    lastPresentationTimeMs = h264Packet->timeMills;

    AVPacket pkt = { 0 };
    av_init_packet(&pkt);
    pkt.stream_index = st->index;

    int64_t cal_pts = lastPresentationTimeMs / 1000.0f / av_q2d(video_st->time_base);
    int64_t pts = h264Packet->pts == PTS_PARAM_UN_SETTIED_FLAG ? cal_pts : h264Packet->pts;
    int64_t dts = h264Packet->dts == DTS_PARAM_UN_SETTIED_FLAG ? pts : h264Packet->dts == DTS_PARAM_NOT_A_NUM_FLAG ? AV_NOPTS_VALUE : h264Packet->dts;
//    LOGI("h264Packet is {%llu, %llu}", h264Packet->pts, h264Packet->dts);
    int nalu_type = (outputData[4] & 0x1F);
//    LOGI("Final is {%llu, %llu} nalu_type is %d", pts, dts, nalu_type);
    if (nalu_type == H264_NALU_TYPE_SEQUENCE_PARAMETER_SET) {

        headerSize = bufferSize;
        headerData = new uint8_t[headerSize];
        memcpy(headerData, outputData, bufferSize);

        uint8_t* spsFrame = 0;
        uint8_t* ppsFrame = 0;

        int spsFrameLen = 0;
        int ppsFrameLen = 0;

        parseH264SequenceHeader(headerData, headerSize, &spsFrame, spsFrameLen,
                                &ppsFrame, ppsFrameLen);

        // Extradata contains PPS & SPS for AVCC format
        int extradata_len = 8 + spsFrameLen - 4 + 1 + 2 + ppsFrameLen - 4;
        c->extradata = (uint8_t*) av_mallocz(extradata_len);
        c->extradata_size = extradata_len;
        c->extradata[0] = 0x01;
        c->extradata[1] = spsFrame[4 + 1];
        c->extradata[2] = spsFrame[4 + 2];
        c->extradata[3] = spsFrame[4 + 3];
        c->extradata[4] = 0xFC | 3;
        c->extradata[5] = 0xE0 | 1;
        int tmp = spsFrameLen - 4;
        c->extradata[6] = (tmp >> 8) & 0x00ff;
        c->extradata[7] = tmp & 0x00ff;
        int i = 0;
        for (i = 0; i < tmp; i++)
            c->extradata[8 + i] = spsFrame[4 + i];
        c->extradata[8 + tmp] = 0x01;
        int tmp2 = ppsFrameLen - 4;
        c->extradata[8 + tmp + 1] = (tmp2 >> 8) & 0x00ff;
        c->extradata[8 + tmp + 2] = tmp2 & 0x00ff;
        for (i = 0; i < tmp2; i++)
            c->extradata[8 + tmp + 3 + i] = ppsFrame[4 + i];

        int ret = avformat_write_header(oc, NULL);
        if (ret < 0) {
            ALOGI("Error occurred when opening output file: %s\n", av_err2str(ret));
        } else{
            isWriteHeaderSuccess = true;
        }
    } else {
        if (nalu_type == H264_NALU_TYPE_IDR_PICTURE || nalu_type == H264_NALU_TYPE_SEI) {
            pkt.size = bufferSize;
            pkt.data = outputData;

            if(pkt.data[0] == 0x00 && pkt.data[1] == 0x00 &&
               pkt.data[2] == 0x00 && pkt.data[3] == 0x01){
                bufferSize -= 4;
                pkt.data[0] = ((bufferSize) >> 24) & 0x00ff;
                pkt.data[1] = ((bufferSize) >> 16) & 0x00ff;
                pkt.data[2] = ((bufferSize) >> 8) & 0x00ff;
                pkt.data[3] = ((bufferSize)) & 0x00ff;
            }

            pkt.pts = pts;
            pkt.dts = dts;
            pkt.flags = AV_PKT_FLAG_KEY;
            c->frame_number++;
        }
        else {
            pkt.size = bufferSize;
            pkt.data = outputData;

            if(pkt.data[0] == 0x00 && pkt.data[1] == 0x00 &&
               pkt.data[2] == 0x00 && pkt.data[3] == 0x01){
                bufferSize -= 4;
                pkt.data[0] = ((bufferSize ) >> 24) & 0x00ff;
                pkt.data[1] = ((bufferSize ) >> 16) & 0x00ff;
                pkt.data[2] = ((bufferSize ) >> 8) & 0x00ff;
                pkt.data[3] = ((bufferSize )) & 0x00ff;
            }

            pkt.pts = pts;
            pkt.dts = dts;
            pkt.flags = 0;
            c->frame_number++;
        }

        if (pkt.size) {
//        		LOGI("pkt : {%llu, %llu}", pkt.pts, pkt.dts);
            ret = av_interleaved_write_frame(oc, &pkt);
            if (ret != 0) {
                ALOGI("Error while writing Video frame: %s\n", av_err2str(ret));
            }
        } else {
            ret = 0;
        }
    }
    delete h264Packet;
    return ret;
}

void MiniRecorder::parseH264SequenceHeader(uint8_t* in_pBuffer, uint32_t in_ui32Size,
                                                     uint8_t** inout_pBufferSPS, int& inout_ui32SizeSPS,
                                                     uint8_t** inout_pBufferPPS, int& inout_ui32SizePPS) {
    uint32_t ui32StartCode = 0x0ff;

    uint8_t* pBuffer = in_pBuffer;
    uint32_t ui32BufferSize = in_ui32Size;

    uint32_t sps = 0;
    uint32_t pps = 0;

    uint32_t idr = in_ui32Size;

    do {
        uint32_t ui32ProcessedBytes = 0;
        ui32StartCode = findStartCode(pBuffer, ui32BufferSize, ui32StartCode,
                                      ui32ProcessedBytes);
        pBuffer += ui32ProcessedBytes;
        ui32BufferSize -= ui32ProcessedBytes;

        if (ui32BufferSize < 1)
            break;

        uint8_t val = (*pBuffer & 0x1f);

        if (val == 5)
            idr = pps + ui32ProcessedBytes - 4;

        if (val == 7)
            sps = ui32ProcessedBytes;

        if (val == 8)
            pps = sps + ui32ProcessedBytes;

    } while (ui32BufferSize > 0);

    *inout_pBufferSPS = in_pBuffer + sps - 4;
    inout_ui32SizeSPS = pps - sps;

    *inout_pBufferPPS = in_pBuffer + pps - 4;
    inout_ui32SizePPS = idr - pps + 4;
}

uint32_t MiniRecorder::findStartCode(uint8_t* in_pBuffer, uint32_t in_ui32BufferSize,
                                               uint32_t in_ui32Code, uint32_t& out_ui32ProcessedBytes) {
    uint32_t ui32Code = in_ui32Code;

    const uint8_t * ptr = in_pBuffer;
    while (ptr < in_pBuffer + in_ui32BufferSize) {
        ui32Code = *ptr++ + (ui32Code << 8);
        if (is_start_code(ui32Code))
            break;
    }

    out_ui32ProcessedBytes = (uint32_t)(ptr - in_pBuffer);

    return ui32Code;
}

AVStream *MiniRecorder::add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, char *codec_name) {
    AVCodecContext *c;
    AVStream *st;
    if (AV_CODEC_ID_NONE == codec_id) {
        *codec = avcodec_find_encoder_by_name(codec_name);
    } else {
        *codec = avcodec_find_encoder(codec_id);
    }
    if (!(*codec)) {
        ALOGI("Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        return NULL;
    }
    ALOGI("\n find encoder name is '%s' ", (*codec)->name);

    st = avformat_new_stream(oc, *codec);
    if (!st) {
        ALOGI("Could not allocate stream\n");
        return NULL;
    }
    st->id = oc->nb_streams - 1;
    c = st->codec;

    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            ALOGI("audioChannels is %d audioSampleRate is %d", mRecordParams->channels, mRecordParams->sampleRate);
            c->sample_fmt = AV_SAMPLE_FMT_S16;
            c->codec_type = AVMEDIA_TYPE_AUDIO;
            c->sample_rate = mRecordParams->sampleRate;
            c->channel_layout = mRecordParams->channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
            c->flags |= CODEC_FLAG_GLOBAL_HEADER;
            break;
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = AV_CODEC_ID_H264;
            // 设置最大比特率
            if (mRecordParams->maxBitRate > 0) {
                c->rc_max_rate = mRecordParams->maxBitRate;
                c->rc_buffer_size = (int) mRecordParams->maxBitRate;
            }
            c->width = mRecordParams->width;
            c->height = mRecordParams->height;

            st->avg_frame_rate.num = 30000;
            st->avg_frame_rate.den = (int) (30000 / mRecordParams->frameRate);

            c->time_base.den = 30000;
            c->time_base.num = (int) (30000 / mRecordParams->frameRate);
            c->gop_size = mRecordParams->frameRate;
            c->qmin = 10;
            c->qmax = 30;
            c->pix_fmt = AV_PIX_FMT_YUV420P;
            av_opt_set(c->priv_data, "preset", "ultrafast", 0);
            av_opt_set(c->priv_data, "tune", "zerolatency", 0);

            /* Some formats want stream headers to be separate. */
            if (oc->oformat->flags & AVFMT_GLOBALHEADER)
                c->flags |= CODEC_FLAG_GLOBAL_HEADER;

            ALOGI("sample_aspect_ratio = %d   %d", c->sample_aspect_ratio.den,
                     c->sample_aspect_ratio.num);
            break;
        default:
            break;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    return st;
}

//===============Media Writer End=============================/

//===============EGL Method Start=============================/
bool MiniRecorder::initEGL() {
    mEGLDisplay = EGL_NO_DISPLAY;
    mEGLContext = EGL_NO_CONTEXT;

    EGLint numConfigs;
    EGLint width;
    EGLint height;
    const EGLint attribs[] = { EGL_BUFFER_SIZE, 32, EGL_ALPHA_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    if((mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        ALOGI("eglGetDisplay error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(mEGLDisplay, 0 , 0)) {
        ALOGI("eglInitialize error %d", eglGetError());
        return false;
    }
    if (!eglChooseConfig(mEGLDisplay, attribs, &mEGLConfig, 1, &numConfigs)) {
        ALOGI("eglChooseConfig error %d", eglGetError());
        return false;
    }
    EGLint  eglContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    if (!(mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, EGL_NO_CONTEXT, eglContextAttributes ))) {
        ALOGI("eglCreateContext error %d", eglGetError());
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGI("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    return true;
}

EGLSurface MiniRecorder::createWindowSurface(ANativeWindow *pWindow) {
    EGLSurface surface = NULL;
    EGLint format;
    if (!pWindow) {
        ALOGI("pWindow null");
        return NULL;
    }
    if (!eglGetConfigAttrib(mEGLDisplay, mEGLConfig, EGL_NATIVE_VISUAL_ID, &format)) {
        ALOGI("eglGetConfigAttrib error");
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGI("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return NULL;
    }
    ANativeWindow_setBuffersGeometry(pWindow, 0, 0, format);
    if (!(surface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, pWindow, 0))) {
        ALOGI("eglCreateWindowSurface error %d", eglGetError());
        return NULL;
    }
    return surface;
}
//===============EGL Method End============================/

//===============Native Call Java Method Start=============================/
void MiniRecorder::updateTexImage() {
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        ALOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        ALOGI("getJNIEnv failed");
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID updateTexImageCallback = env->GetMethodID(jcls, "updateTexImageFromNative",
                                                            "()V");
        if (NULL != updateTexImageCallback) {
            env->CallVoidMethod(mObj, updateTexImageCallback);
        }
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        ALOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
    }
}

void MiniRecorder::startCameraPreview() {
    ALOGI("startCameraPreview");
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        ALOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        ALOGE("%s getJNIEnv failed", __FUNCTION__);
        return;
    }
    if (mObj == NULL) {
        ALOGE("%s mObj failed", __FUNCTION__);
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID  startPreviewCallback = env->GetMethodID(jcls, "startPreviewFromNative", "(I)V");
        if (startPreviewCallback != NULL) {
            env->CallVoidMethod(mObj, startPreviewCallback, mDecodeTexId);
        }
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        ALOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
        return;
    }
}
//===============Native Call Java Method End=============================/

//===============OpenGL Functions Start=============================/

bool MiniRecorder::initTextureFBO() {
    //inputTexId
    glGenTextures(1, &inputTexId);
    checkGlError("glGenTextures inputTexId");
    glBindTexture(GL_TEXTURE_2D, inputTexId);
    checkGlError("glBindTexture inputTexId");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint internalFormat = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei) mTextureWidth, (GLsizei) mTextureHeight, 0, internalFormat, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    //outputTexId
    glGenTextures(1, &outputTexId);
    checkGlError("glGenTextures outputTexId");
    glBindTexture(GL_TEXTURE_2D, outputTexId);
    checkGlError("glBindTexture outputTexId");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei) mTextureWidth, (GLsizei) mTextureHeight, 0, internalFormat, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    // init FBO
    glGenFramebuffers(1, &mFBO);
    checkGlError("glGenFramebuffers mFBO");
    glGenFramebuffers(1, &mDownloadFBO);
    checkGlError("glGenFramebuffers mDownloadFBO");
    // init texture
    glGenTextures(1,&mRotateTexId);
    checkGlError("glGenTextures mRotateTexId");
    glBindTexture(GL_TEXTURE_2D, mRotateTexId);
    checkGlError("glBindTexture mRotateTexId");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (mDegress == 90 || mDegress == 270)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTextureHeight, mTextureWidth, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTextureWidth, mTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool MiniRecorder::initRenderer() {
    mGLProgramId = loadProgram(mGLVertexShader, mGLFragmentShader);
    if (!mGLProgramId) {
        ALOGI("%s loadProgram failed", __FUNCTION__);
        return false;
    }

    if (!mGLProgramId) {
        ALOGE("Could not create program.");
        return false;
    }
    mGLVertexCoords = glGetAttribLocation(mGLProgramId, "position");
    checkGlError("glGetAttribLocation vPosition");
    mGLTextureCoords = glGetAttribLocation(mGLProgramId, "texcoord");
    checkGlError("glGetAttribLocation vTexCords");
    mGLUniformTexture = glGetUniformLocation(mGLProgramId, "yuvTexSampler");
    checkGlError("glGetAttribLocation yuvTexSampler");
    mIsGLInitialized = true;
    return true;
}

bool MiniRecorder::initCopier() {
    mCopyProgramId = loadProgram(mCopyVertexShader, mCopyFragmentShader);
    if (!mCopyProgramId) {
        ALOGE("Could not create program.");
        return false;
    } else {
        ALOGI("loadProgram success");
    }

    mCopyVertexCoords = glGetAttribLocation(mCopyProgramId, "vPosition");
    checkGlError("glGetAttribLocation vPosition");
    mCopyTextureCoords = glGetAttribLocation(mCopyProgramId, "vTexCords");
    checkGlError("glGetAttribLocation vTexCords");
    mCopyUniformTexture = glGetUniformLocation(mCopyProgramId, "yuvTexSampler");
    checkGlError("glGetAttribLocation yuvTexSampler");

    mCopyUniformTexMatrix = glGetUniformLocation(mCopyProgramId, "texMatrix");
    checkGlError("glGetUniformLocation mUniformTexMatrix");

    mCopyUniformTransforms = glGetUniformLocation(mCopyProgramId, "trans");
    checkGlError("glGetUniformLocation mUniformTransforms");

    mCopyIsInitialized = true;
    return true;
}

GLuint MiniRecorder::loadProgram(char *pVertexSource, char *pFragmentSource) {
    GLuint programId;
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        ALOGI("vertexShader null");
        return 0;
    }
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        ALOGI("vertexShader null");
        return 0;
    }
    programId = glCreateProgram();
    if (programId) {
        glAttachShader(programId, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(programId, fragmentShader);
        checkGlError("glAttachShader");
        glLinkProgram(programId);

        GLint linkStatus = GL_FALSE;
        glGetProgramiv(programId, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(programId, bufLength, NULL, buf);
                    ALOGI("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(programId);
            programId = 0;
        }
    }
    return programId;
}

GLuint MiniRecorder::loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*)malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    ALOGI("%s %S glCompileShader error %d\n %s\n", __FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            } else {
                ALOGI("Guessing at GL_INFO_LOG_LENGTH size\n");
                char* buf = (char*) malloc(0x1000);
                if (buf) {
                    glGetShaderInfoLog(shader, 0x1000, NULL, buf);
                    ALOGI("%s %S Could not compile shader %d:\n%s\n",__FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            }
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

bool MiniRecorder::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        ALOGE("after %s() glError (0x%x)\n", op, error);
        return true;
    }
    return false;
}

int MiniRecorder::initDecodeTexture() {
    mDecodeTexId = 0;
    glGenTextures(1, &mDecodeTexId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mDecodeTexId);
    if (checkGlError("glBindTexture")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    return 1;
}

void MiniRecorder::renderToView(GLuint texID, int screenWidth, int screenHeight) {
    glViewport(0, 0, screenWidth, screenHeight);
    if (!mIsSurfaceRenderInit) {
        ALOGI("mIsSurfaceRenderInit false");
        return;
    }
    glUseProgram(mSurfaceRenderProgId);

    static const GLfloat _vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glVertexAttribPointer(mGLVertexCoords, 2, GL_FLOAT, 0, 0, _vertices);
    glEnableVertexAttribArray(mGLVertexCoords);
    static const GLfloat texCoords[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };
    glVertexAttribPointer(mGLTextureCoords, 2, GL_FLOAT, 0, 0, texCoords);
    glEnableVertexAttribArray(mGLTextureCoords);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texID);
    glUniform1i(mSurfaceRenderUniformTexture, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mGLVertexCoords);
    glDisableVertexAttribArray(mGLTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MiniRecorder::renderToViewWithAutofit(GLuint texId, int screenWidth, int screenHeight, int texWidth, int texHeight) {
    glViewport(0, 0, screenWidth, screenHeight);

    if (!mIsGLInitialized) {
        ALOGE("render not init");
        return;
    }

    int fitHeight = (int) ((float) texHeight * screenWidth / texWidth + 0.5f);
    float cropRatio = (float) (fitHeight - screenHeight) / (2 * fitHeight);

    if (cropRatio < 0) {
        cropRatio = 0.0f;
    }

    glUseProgram(mGLProgramId);
    static const GLfloat _vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,
                                        1.0f, 1.0f};
    glVertexAttribPointer(mGLVertexCoords, 2, GL_FLOAT, 0, 0, _vertices);
    glEnableVertexAttribArray(mGLVertexCoords);
    static const GLfloat texCoords[] = {0.0f, cropRatio, 1.0f, cropRatio, 0.0f, 1.0f - cropRatio,
                                        1.0f, 1.0f - cropRatio};
    glVertexAttribPointer(mGLTextureCoords, 2, GL_FLOAT, 0, 0, texCoords);
    glEnableVertexAttribArray(mGLTextureCoords);
    //bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    glUniform1i(mGLUniformTexture, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mGLVertexCoords);
    glDisableVertexAttribArray(mGLTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MiniRecorder::processFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    checkGlError("glBindFramebuffer mFBO");

    if (mDegress == 90 || mDegress == 270)
        glViewport(0, 0, mTextureHeight, mTextureWidth);
    else
        glViewport(0, 0, mTextureWidth, mTextureHeight);

    GLfloat* vertexCoords = CAMERA_TRIANGLE_VERTICES;
    //旋转纹理
    renderWithCoords(mRotateTexId, vertexCoords, mTextureCoords);
    int rotateTexWidth = mTextureWidth;
    int rotateTexHeight = mTextureHeight;
    if (mDegress == 90 || mDegress == 270){
        rotateTexWidth = mTextureHeight;
        rotateTexHeight = mTextureWidth;
    }
    //renderToAutoFitTexture(mRotateTexId, rotateTexWidth, rotateTexHeight, inputTexId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MiniRecorder::renderToAutoFitTexture(GLuint inputTexId, int width, int height, GLuint outputTexId) {
    float textureAspectRatio = (float)height / (float)width;
    float viewAspectRatio = (float)mTextureHeight / (float)mTextureWidth;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    if(textureAspectRatio > viewAspectRatio){
        //Update Y Offset
        int expectedHeight = (int)((float)height*mTextureWidth/(float)width+0.5f);
        yOffset = (float)(expectedHeight-mTextureHeight)/(2*expectedHeight);
//		ALOGI("yOffset is %.3f", yOffset);
    } else if(textureAspectRatio < viewAspectRatio){
        //Update X Offset
        int expectedWidth = (int)((float)(height * mTextureWidth) / (float)mTextureHeight + 0.5);
        xOffset = (float)(width - expectedWidth)/(2*width);
//		ALOGI("xOffset is %.3f", xOffset);
    }

    glViewport(0, 0, mTextureWidth, mTextureHeight);

    if (!mIsGLInitialized) {
        ALOGE("ViewRenderEffect::renderEffect effect not initialized!");
        return;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexId, 0);
    checkGlError("PassThroughRender::renderEffect glFramebufferTexture2D");
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGI("failed to make complete framebuffer object %x", status);
    }

    glUseProgram(mGLProgramId);
    const GLfloat _vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glVertexAttribPointer(mGLVertexCoords, 2, GL_FLOAT, 0, 0, _vertices);
    glEnableVertexAttribArray(mGLVertexCoords);
    GLfloat texCoords[] = { xOffset, yOffset, (GLfloat)1.0 - xOffset, yOffset, xOffset, (GLfloat)1.0 - yOffset,
                            (GLfloat)1.0 - xOffset, (GLfloat)1.0 - yOffset };
    glVertexAttribPointer(mGLTextureCoords, 2, GL_FLOAT, 0, 0, texCoords);
    glEnableVertexAttribArray(mGLTextureCoords);

    /* Binding the input texture */
    glActiveTexture (GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexId);
    glUniform1i(mGLUniformTexture, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mGLVertexCoords);
    glDisableVertexAttribArray(mGLTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

void MiniRecorder::renderWithCoords(GLuint texId, GLfloat *vertexCoords, GLfloat *textureCoords) {
    glBindTexture(GL_TEXTURE_2D, texId);
    checkGlError("glBindTexture");

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texId, 0);
    checkGlError("glFramebufferTexture2D");

    glUseProgram(mCopyProgramId);
    if (!mCopyIsInitialized) {
        return;
    }

    glVertexAttribPointer(mCopyVertexCoords, 2, GL_FLOAT, GL_FALSE, 0, vertexCoords);
    glEnableVertexAttribArray (mCopyVertexCoords);
    glVertexAttribPointer(mCopyTextureCoords, 2, GL_FLOAT, GL_FALSE, 0, textureCoords);
    glEnableVertexAttribArray (mCopyTextureCoords);
    /* Binding the input texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mDecodeTexId);
    glUniform1i(mCopyUniformTexture, 0);

    float texTransMatrix[4 * 4];
    matrixSetIdentityM(texTransMatrix);
    glUniformMatrix4fv(mCopyUniformTexMatrix, 1, GL_FALSE, (GLfloat *) texTransMatrix);

    float rotateMatrix[4 * 4];
    matrixSetIdentityM(rotateMatrix);
    glUniformMatrix4fv(mCopyUniformTransforms, 1, GL_FALSE, (GLfloat *) rotateMatrix);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mCopyVertexCoords);
    glDisableVertexAttribArray(mCopyTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

void MiniRecorder::matrixSetIdentityM(float *m)
{
    memset((void*)m, 0, 16*sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void MiniRecorder::downloadImageFromTexture(GLuint texId, void *imageBuf, unsigned int imageWidth,
                                             unsigned int imageHeight) {
    glBindTexture(GL_TEXTURE_2D, texId);
    checkGlError("glBindTexture texId");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, mDownloadFBO);
    checkGlError("glBindFramebuffer mDownloadFBO");
    // attach tex to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
    checkGlError("glFramebufferTexture2D");
    // download image
    glReadPixels(0, 0, (GLsizei)imageWidth, (GLsizei)imageHeight, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)imageBuf);
    checkGlError("glReadPixels failed");

    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
//===============OpenGL Functions End=============================/














