//
// Created by Glen on 2020/1/08.
//
#include "MiniRecorder.h"

/**
 * 主要到事情说三遍，为了吃透核心代码，先不要做封装，所有核心代码写在这一个cpp里
 * 第一阶段，先把相机画面显示出来
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
 * 启动录制
 */
void MiniRecorder::startRecord() {
    if (mHandler)
        mHandler->postMessage(new Msg(MSG_START_RECORDING));
}

void MiniRecorder::startRecording() {
    startConsumer();
    startProducer();
}

int MiniRecorder::startConsumer() {
    createMediaWriter((char*)mRecordParams->dstFile, mRecordParams->width, mRecordParams->height, mRecordParams->frameRate, mRecordParams->maxBitRate,
                        mRecordParams->sampleRate, mRecordParams->channels);
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

int MiniRecorder::prepare() {

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
    //video_st = add_stream();
}

int MiniRecorder::writeFrame() {

}

int MiniRecorder::closeMediaWriter() {

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
            ALOGI("audioBitRate is %d audioChannels is %d audioSampleRate is %d", audioBitRate,
                 audioChannels, audioSampleRate);
            c->sample_fmt = AV_SAMPLE_FMT_S16;
            c->bit_rate = audioBitRate;
            c->codec_type = AVMEDIA_TYPE_AUDIO;
            c->sample_rate = audioSampleRate;
            c->channel_layout = audioChannels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
            c->flags |= CODEC_FLAG_GLOBAL_HEADER;
            break;
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = AV_CODEC_ID_H264;
            c->bit_rate = videoBitRate;
            c->width = videoWidth;
            c->height = videoHeight;

            st->avg_frame_rate.num = 30000;
            st->avg_frame_rate.den = (int) (30000 / videoFrameRate);

            c->time_base.den = 30000;
            c->time_base.num = (int) (30000 / videoFrameRate);
            c->gop_size = videoFrameRate;
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














