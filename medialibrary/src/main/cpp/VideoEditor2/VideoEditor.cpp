#include <list>
#include "VideoEditor.h"
VideoEditor::VideoEditor()
    : isMediaCodecInit(false), inputBuffer(NULL), audioFrameQueue(NULL), circleFrameTextureQueue(NULL), currentAudioFrame(NULL){
    mScreenWidth = 0;
    mScreenHeight = 0;
}

VideoEditor::~VideoEditor() {

}

bool VideoEditor::prepare(char *srcFilePath, JavaVM *jvm, jobject obj, bool isHWDecode) {
    mIsPlaying = false;
    mIsHWDecode = isHWDecode;
    uri = srcFilePath;
    mJvm = jvm;
    mObj = obj;
    pthread_create(&mThreadId, 0, prepareThreadCB, this);
    mIsUserCancelled = false;
    return true;
}

void VideoEditor::onSurfaceCreated(ANativeWindow *window, int widht, int height) {
    ALOGI("onSurfaceCreated");
    if (NULL != window) {
        mWindow = window;
    }
    if (mIsUserCancelled) {
        return;
    }

    if (widht > 0 && height > 0) {
        mScreenWidth = widht;
        mScreenHeight = height;
    }
    if (!videoOutput) {
        initVideoOutput(window);
    } else {
        videoOutput->onSurfaceCreated(window);
    }
}

void VideoEditor::onSurfaceDestroyed() {

}

bool VideoEditor::prepare_l() {
    ALOGI("prepare_l");
    bool ret = false;

    if (mIsUserCancelled) {
        return true;
    }

    openFile();

    if (mIsHWDecode) {

    } else {
        //先实现软解
    }

    memcpy(vertexCoords, DECODER_COPIER_GL_VERTEX_COORDS, sizeof(GLfloat) * OPENGL_VERTEX_COORDNATE_CNT);
    memcpy(textureCoords, DECODER_COPIER_GL_TEXTURE_COORDS_NO_ROTATION, sizeof(GLfloat) * OPENGL_VERTEX_COORDNATE_CNT);
    initEGL();
    copyTexSurface = createOffscreenSurface(width, height);
    eglMakeCurrent(mEGLDisplay, copyTexSurface, copyTexSurface, mEGLContext);

    glGenFramebuffers(1, &mFBO);
    glGenTextures(1, &outputTexId);
    glBindTexture(GL_TEXTURE_2D, outputTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    pthread_create(&upThreadId, 0, upThreadCB, this);
    initAudioOutput();
    initDecoderThread();
    if (NULL != audioOutput) {
        audioOutput->start();
    }
    return true;
}

bool VideoEditor::openFile() {
    avcodec_register_all();
    av_register_all();
    pFormatCtx = avformat_alloc_context();
    if ((avformat_open_input(&pFormatCtx, uri, NULL, NULL) != 0)) {
        ALOGE("avformat_open_input failed");
        return false;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        ALOGE("Video decoder Stream info not found...");
        return false;
    }
    videoStreamIndex = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (AVMEDIA_TYPE_VIDEO == pFormatCtx->streams[i]->codec->codec_type) {
            videoStreamIndex = i;
        }
    }
    AVStream* videoStream = pFormatCtx->streams[videoStreamIndex];
    videoCodecCtx = videoStream->codec;
    videoCodec = avcodec_find_decoder(videoCodecCtx->codec_id);
    if (!videoCodec) {
        ALOGE("videoCodec null");
        return false;
    }
    if (avcodec_open2(videoCodecCtx, videoCodec, NULL) < 0) {
        ALOGE("avcodec_open2 failed");
        return false;
    }
    videoFrame = av_frame_alloc();
    if (!videoFrame) {
        ALOGE("videoFrame null");
        avcodec_close(videoCodecCtx);
        return false;
    }
    width = videoCodecCtx->width;
    height = videoCodecCtx->height;
    ALOGI("width=%d height=%d", width, height);

    audioStreamIndex = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (AVMEDIA_TYPE_AUDIO == pFormatCtx->streams[i]->codec->codec_type) {
            audioStreamIndex = i;
        }
    }
    AVStream *audioStream = pFormatCtx->streams[audioStreamIndex];
    audioCodecCtx = audioStream->codec;
    audioCodec = avcodec_find_decoder(audioCodecCtx->codec_id);
    if (!audioCodec) {
        ALOGE("audioCodec null");
        return false;
    }
    if (avcodec_open2(audioCodecCtx, audioCodec, NULL) < 0) {
        ALOGE("open audio codec failed...");
        avcodec_close(videoCodecCtx);
        return false;
    }
    if (audioCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16) {
        ALOGI("because of audio Codec Is Not Supported so we will init swresampler...");
        swrContext = swr_alloc_set_opts(NULL, av_get_default_channel_layout(audioCodecCtx->channels), AV_SAMPLE_FMT_S16,
                                        audioCodecCtx->sample_rate, av_get_default_channel_layout(audioCodecCtx->channels), audioCodecCtx->sample_fmt, audioCodecCtx->sample_rate, 0, NULL);
        if (!swrContext || swr_init(swrContext)) {
            if (swrContext)
                swr_free(&swrContext);
            avcodec_close(audioCodecCtx);
            ALOGE("init resampler failed...");
            return false;
        }
    }
    audioFrame = av_frame_alloc();
    if (audioFrame == NULL) {
        ALOGE("alloc audio frame failed...");
        if (swrContext)
            swr_free(&swrContext);
        avcodec_close(audioCodecCtx);
        return -1;
    }
}

void VideoEditor::initVideoOutput(ANativeWindow *window) {
    videoOutput = new VideoOutput();
    if (mEGLContext != EGL_NO_CONTEXT && window != NULL) {
        videoOutput->initOutput(window, mScreenWidth, mScreenHeight, videoCallbackGetTex, this, mEGLContext);
    } else {
        ALOGE("initVideoOutput failed");
    }
}

bool VideoEditor::initAudioOutput() {
    ALOGI("initAudioOutput");
    audioOutput = new AudioOutput();
    SLresult result = audioOutput->initSoundTrack(audioCodecCtx->channels, audioCodecCtx->sample_rate, audioCallbackFillData, this);
    if (SL_RESULT_SUCCESS != result) {
        ALOGI("audio manager failed on initialized...");
        delete audioOutput;
        audioOutput = NULL;
        return false;
    }
    return true;
}

void VideoEditor::play() {

}

void VideoEditor::pause() {

}

void VideoEditor::destroy() {

}

void VideoEditor::decode() {
    pthread_mutex_lock(&videoDecoderLock);
    pthread_cond_wait(&videoDecoderCondition,&videoDecoderLock);
    pthread_mutex_unlock(&videoDecoderLock);
    isDecodingFrames = true;
    decodeFrames();
    isDecodingFrames = false;
}

void VideoEditor::decodeFrames() {
    bool good = true;
    while (good) {
        good = false;
        if (canDecode()) {
            processDecodingFrame(good);
        } else {
            break;
        }
    }
}

void VideoEditor::processDecodingFrame(bool &good) {
    int decodeVideoErrorState;
    std::list<MovieFrame*>* frames = decodeFrames(&decodeVideoErrorState);
    if (NULL != frames) {
        if (!frames->empty()) {
            good = addFrames(0.8, frames);
        } else {
            ALOGI("frames is empty %d", (int )good);
        }
        delete frames;
    } else {
        ALOGI("why frames is NULL tell me why?");
    }
}

std::list<MovieFrame*>* VideoEditor::decodeFrames(int *decodeVideoErrorState) {
    bool finished = false;
    if (-1 == videoStreamIndex && -1 == audioStreamIndex) {
        ALOGE("decodeFrames error");
        return NULL;
    }
    std::list<MovieFrame*> *result = new std::list<MovieFrame*>();

    int ret = 0;
    char errString[128];
    AVPacket packet;
    while (!finished) {
        ret = av_read_frame(pFormatCtx, &packet);
        if (ret < 0) {
            ALOGE("av_read_frame return an error");
            if (ret != AVERROR_EOF) {
                av_strerror(ret, errString, 128);
                ALOGE("av_read_frame return an not AVERROR_EOF error : %s", errString);
            } else {
                ALOGI("input EOF");
                is_eof = true;
            }
            av_free_packet(&packet);
            break;
        }
        if (packet.stream_index == videoStreamIndex) {
            decodeVideoFrame(packet, decodeVideoErrorState);
        } else if (packet.stream_index == audioStreamIndex) {

        }
        av_free_packet(&packet);
    }
    if (is_eof) {

    }
    return
}

bool VideoEditor::decodeVideoFrame(AVPacket packet, int *decodeVideoErrorState) {
    int pktSize = packet.size;
    int gotFrame = 0;
    while (pktSize > 0) {
        int len = avcodec_decode_video2(videoCodecCtx, videoFrame, &gotFrame, &packet);
        if (len < 0) {
            ALOGI("decode video error, skip packet");
            *decodeVideoErrorState = 1;
            break;
        }
        if (gotFrame) {
            if (videoFrame->interlaced_frame) {
                //todo
            }
            //todo
        }
        if (0 == len) {
            break;
        }
        pktSize -= len;
    }
    return (bool) gotFrame;
}

bool VideoEditor::decodeAudioFrames(AVPacket *packet, std::list<MovieFrame *> *result,
                                    float &decodedDuration, int *decodeVideoErrorState) {

}

bool VideoEditor::addFrames(float thresholdDuration, std::list<MovieFrame *> *frames) {

}

bool VideoEditor::canDecode() {
    return !pauseDecodeThreadFlag && !isDestroyed && (videoStreamIndex != -1 || audioStreamIndex != -1);
}

void VideoEditor::initDecoderThread() {
    circleFrameTextureQueue->setIsFirstFrame(true);
    isDecodingFrames = false;
    pthread_mutex_init(&videoDecoderLock, NULL);
    pthread_cond_init(&videoDecoderCondition, NULL);
    pthread_create(&videoDecoderThread, NULL, startDecoderThread, this);
}

bool VideoEditor::initEGL() {
    mEGLDisplay = EGL_NO_DISPLAY;
    mEGLContext = EGL_NO_CONTEXT;

    EGLint numConfigs;
    EGLint width;
    EGLint height;
    const EGLint attribs[] = { EGL_BUFFER_SIZE, 32, EGL_ALPHA_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    if((mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(mEGLDisplay, 0 , 0)) {
        ALOGE("eglInitialize error %d", eglGetError());
        return false;
    }
    if (!eglChooseConfig(mEGLDisplay, attribs, &mEGLConfig, 1, &numConfigs)) {
        ALOGE("eglChooseConfig error %d", eglGetError());
        return false;
    }
    EGLint  eglContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    if (!(mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, EGL_NO_CONTEXT, eglContextAttributes ))) {
        ALOGE("eglCreateContext error %d", eglGetError());
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    return true;
}

EGLSurface VideoEditor::createWindowSurface(ANativeWindow *pWindow) {
    EGLSurface surface = NULL;
    EGLint format;
    if (!pWindow) {
        ALOGE("pWindow null");
        return NULL;
    }
    if (!eglGetConfigAttrib(mEGLDisplay, mEGLConfig, EGL_NATIVE_VISUAL_ID, &format)) {
        ALOGE("eglGetConfigAttrib error");
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return NULL;
    }
    ANativeWindow_setBuffersGeometry(pWindow, 0, 0, format);
    if (!(surface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, pWindow, 0))) {
        ALOGE("eglCreateWindowSurface error %d", eglGetError());
        return NULL;
    }
    return surface;
}

EGLSurface VideoEditor::createOffscreenSurface(int width, int height) {
    EGLSurface surface;
    EGLint PbufferAttributes[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE, EGL_NONE };
    if (!(surface = eglCreatePbufferSurface(mEGLDisplay, mEGLConfig, PbufferAttributes))) {
        ALOGE("eglCreatePbufferSurface() returned error %d", eglGetError());
    }
    return surface;
}

void* VideoEditor::prepareThreadCB(void *self) {
    VideoEditor* videoEditor = (VideoEditor*)self;
    videoEditor->prepare_l();
    pthread_exit(0);
}

void* VideoEditor::startDecoderThread(void *ptr) {
    VideoEditor* videoEditor = (VideoEditor*)ptr;
    while (videoEditor->isOnDecoding) {
        videoEditor->decode();
    }
    return 0;
}

void* VideoEditor::upThreadCB(void *myself) {

}

int VideoEditor::videoCallbackGetTex(FrameTexture **frameTex, void *ctx, bool forceGetFrame) {

}

int VideoEditor::audioCallbackFillData(byte *buf, size_t bufSize, void *ctx) {

}





