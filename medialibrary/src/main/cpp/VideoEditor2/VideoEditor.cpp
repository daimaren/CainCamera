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
    initCopier();
    initPassRender();
    initCircleQueue(width, height);
    openFile();

    if (mIsHWDecode) {

    } else {
        //先实现软解
    }

    startUploader();
    initAudioOutput();
    initDecoderThread();
    if (NULL != audioOutput) {
        audioOutput->start();
    }
    return true;
}

void VideoEditor::initCopier() {
    mCopierVertexShader = NO_FILTER_VERTEX_SHADER;
    mCopierFragmentShader = YUV_FRAME_FRAGMENT_SHADER;

    mCopierProgId = loadProgram(mCopierVertexShader, mCopierFragmentShader);
    if (!mCopierProgId) {
        ALOGE("Could not create program.");
        return;
    }
    mCopierVertexCoords = glGetAttribLocation(mCopierProgId, "vPosition");
    checkGlError("glGetAttribLocation vPosition");
    mCopierTextureCoords = glGetAttribLocation(mCopierProgId, "vTexCords");
    checkGlError("glGetAttribLocation vTexCords");
    glUseProgram(mCopierProgId);
    _uniformSamplers[0] = glGetUniformLocation(mCopierProgId, "s_texture_y");
    _uniformSamplers[1] = glGetUniformLocation(mCopierProgId, "s_texture_u");
    _uniformSamplers[2] = glGetUniformLocation(mCopierProgId, "s_texture_v");

    mCopierUniformTransforms = glGetUniformLocation(mCopierProgId, "trans");
    checkGlError("glGetUniformLocation mUniformTransforms");

    mCopierUniformTexMatrix = glGetUniformLocation(mCopierProgId, "texMatrix");
    checkGlError("glGetUniformLocation mUniformTexMatrix");

    mIsCopierInitialized = true;
}

void VideoEditor::initPassRender() {
    mPassRenderVertexShader = OUTPUT_VIEW_VERTEX_SHADER;
    mPassRenderFragmentShader = OUTPUT_VIEW_FRAG_SHADER;

    mPassRenderProgId = loadProgram(mPassRenderVertexShader, mPassRenderFragmentShader);
    if (!mPassRenderProgId) {
        ALOGE("Could not create program.");
        return;
    }
    mPassRenderVertexCoords = glGetAttribLocation(mPassRenderProgId, "position");
    checkGlError("glGetAttribLocation vPosition");
    mPassRenderTextureCoords = glGetAttribLocation(mPassRenderProgId, "texcoord");
    checkGlError("glGetAttribLocation vTexCords");
    mPassRenderUniformTexture = glGetUniformLocation(mPassRenderProgId, "yuvTexSampler");
    checkGlError("glGetAttribLocation yuvTexSampler");

    mIsPassRenderInitialized = true;
}

void VideoEditor::initCircleQueue(int videoWidth, int videoHeight) {
    int queueSize = (LOCAL_MAX_BUFFERED_DURATION + 1.0) * fps;
    circleFrameTextureQueue = new CircleFrameTextureQueue(
            "decode frame texture queue");
    circleFrameTextureQueue->init(videoWidth, videoHeight, queueSize);
    audioFrameQueue = new std::queue<AudioFrame*>();
    pthread_mutex_init(&audioFrameQueueMutex, NULL);
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
    avStreamFPSTimeBase(videoStream, 0.04, &fps, &videoTimeBase);
    if(fps > 30.0f || fps < 5.0f){
        fps = 24.0f;
    }
    ALOGI("video codec size: fps: %.3f tb: %f", fps, videoTimeBase);
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

void VideoEditor::avStreamFPSTimeBase(AVStream *st, float defaultTimeBase, float *pFPS, float *pTimeBase) {
    float fps, timebase;

    if (st->time_base.den && st->time_base.num)
        timebase = av_q2d(st->time_base);
    else if (st->codec->time_base.den && st->codec->time_base.num)
        timebase = av_q2d(st->codec->time_base);
    else
        timebase = defaultTimeBase;

    if (st->codec->ticks_per_frame != 1) {
        ALOGI("WARNING: st.codec.ticks_per_frame=%d", st->codec->ticks_per_frame);
    }

    if (st->avg_frame_rate.den && st->avg_frame_rate.num){
        fps = av_q2d(st->avg_frame_rate);
        ALOGI("Calculate By St avg_frame_rate : fps is %.3f", fps);
    } else if (st->r_frame_rate.den && st->r_frame_rate.num){
        fps = av_q2d(st->r_frame_rate);
        ALOGI("Calculate By St r_frame_rate : fps is %.3f", fps);
    } else {
        fps = 1.0 / timebase;
        ALOGI("Calculate By 1.0 / timebase : fps is %.3f", fps);
    }
    if (pFPS) {
        ALOGI("assign fps value fps is %.3f", fps);
        *pFPS = fps;
    }
    if (pTimeBase) {
        ALOGI("assign pTimeBase value");
        *pTimeBase = timebase;
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
    mIsCopierInitialized = false;
    if (mCopierProgId) {
        glDeleteProgram(mCopierProgId);
        mCopierProgId = 0;
    }
    mIsPassRenderInitialized = false;
    if (mPassRenderProgId) {
        glDeleteProgram(mPassRenderProgId);
        mPassRenderProgId = 0;
    }
    if (circleFrameTextureQueue) {
        delete circleFrameTextureQueue;
        circleFrameTextureQueue = NULL;
    }
    if (audioFrameQueue) {
        delete audioFrameQueue;
        audioFrameQueue = NULL;
    }
    pthread_mutex_destroy(&audioFrameQueueMutex);

    if (textures[0]) {
        glDeleteTextures(3, textures);
    }
    if(-1 != outputTexId){
        glDeleteTextures(1, &outputTexId);
    }
    if (mFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &mFBO);
    }
    eglDestroySurface(mEGLDisplay, copyTexSurface);
    copyTexSurface = EGL_NO_SURFACE;
    if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
        eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        ALOGI("after eglMakeCurrent...");
        eglDestroyContext(mEGLDisplay, mEGLContext);
        ALOGI("after eglDestroyContext...");
    }
    mEGLDisplay = EGL_NO_DISPLAY;
    mEGLContext = EGL_NO_CONTEXT;
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
    return NULL;
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
                //todo avpicture_deinterlace
            }
            uploadTexture();
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

void VideoEditor::startUploader() {
    initTexture();
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
}

void VideoEditor::initTexture() {
    textures[0] = 0;
    textures[1] = 0;
    textures[2] = 0;
    glGenTextures(3, textures);
    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        if (checkGlError("glBindTexture")) {
            return;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (checkGlError("glTexParameteri")) {
            return;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        if (checkGlError("glTexParameteri")) {
            return;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        if (checkGlError("glTexParameteri")) {
            return;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (checkGlError("glTexParameteri")) {
            return;
        }
    }
}
void VideoEditor::uploadTexture() {
    //直接上传纹理，不做多线程同步
    eglMakeCurrent(mEGLDisplay, copyTexSurface, copyTexSurface, mEGLContext);
    drawFrame();
}

void VideoEditor::drawFrame() {
    updateTexImage();
    glBindFramebuffer(GL_FRAMEBUFFER,mFBO);
    renderWithCoords(outputTexId, vertexCoords, textureCoords);
    pushToVideoQueue(outputTexId, width, height, position);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VideoEditor::updateTexImage() {
    yuvFrame = handleVideoFrame();
    if (yuvFrame) {
        updateYUVTexImage();
        position = yuvFrame->position;
        delete yuvFrame;
    }
}

void VideoEditor::updateYUVTexImage() {
    if (yuvFrame) {
        int frameWidth = yuvFrame->width;
        int frameHeight = yuvFrame->height;
        if (frameWidth % 16 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        uint8_t *pixels[3] = {yuvFrame->luma, yuvFrame->chromaB, yuvFrame->chromaR};
        int widths[3] = {frameWidth, frameWidth >> 1, frameWidth >> 1};
        int heights[3] = {frameHeight, frameHeight >> 1, frameHeight >> 1};
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i]);
            if (checkGlError("glBindTexture")) {
                return;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, widths[i], heights[i], 0, GL_LUMINANCE,
                         GL_UNSIGNED_BYTE, pixels[i]);
        }
    }
}

VideoFrame* VideoEditor::handleVideoFrame() {
    if(!videoFrame->data[0]) {
        ALOGE("videoFrame->data[0] is null");
        return NULL;
    }
    VideoFrame *yuvFrame = new VideoFrame();
    int width = MIN(videoFrame->linesize[0], videoCodecCtx->width);
    int height = videoCodecCtx->height;
    int lumaLength = width * height;
    uint8_t * luma = new uint8_t[lumaLength];
    copyFrameData(luma, videoFrame->data[0], width, height, videoFrame->linesize[0]);
    yuvFrame->luma = luma;

    width = MIN(videoFrame->linesize[1], videoCodecCtx->width / 2);
    height = videoCodecCtx->height / 2;
    int chromaBLength = width * height;
    uint8_t * chromaB = new uint8_t[chromaBLength];
    copyFrameData(chromaB, videoFrame->data[1], width, height, videoFrame->linesize[1]);
    yuvFrame->chromaB = chromaB;

    width = MIN(videoFrame->linesize[2], videoCodecCtx->width / 2);
    height = videoCodecCtx->height / 2;
    int chromaRLength = width * height;
    uint8_t * chromaR = new uint8_t[chromaRLength];
    copyFrameData(chromaR, videoFrame->data[2], width, height, videoFrame->linesize[2]);
    yuvFrame->chromaR = chromaR;

    yuvFrame->width = videoCodecCtx->width;
    yuvFrame->height = videoCodecCtx->height;
    yuvFrame->position = av_frame_get_best_effort_timestamp(videoFrame) * videoTimeBase;

    const int64_t frameDuration = av_frame_get_pkt_duration(videoFrame);
    if (frameDuration) {
        yuvFrame->duration = frameDuration * videoTimeBase;
        yuvFrame->duration += videoFrame->repeat_pict * videoTimeBase * 0.5;
    } else {
        yuvFrame->duration = 1.0 / fps;
    }
//	ALOGI("VFD: %.4f %.4f | %lld ", yuvFrame->position, yuvFrame->duration, av_frame_get_pkt_pos(videoFrame));
//	ALOGI("leave VideoDecoder::handleVideoFrame()...");
    return yuvFrame;
}

void VideoEditor::pushToVideoQueue(GLuint inputTexId, int width, int height, float position) {
    if (!mIsPassRenderInitialized){
        ALOGE("renderToVideoQueue::passThorughRender is NULL");
        return;
    }

    if (!circleFrameTextureQueue) {
        ALOGE("renderToVideoQueue::circleFrameTextureQueue is NULL");
        return;
    }

    bool isFirstFrame = circleFrameTextureQueue->getIsFirstFrame();
    FrameTexture* frameTexture = circleFrameTextureQueue->lockPushCursorFrameTexture();
    if (NULL != frameTexture) {
        frameTexture->position = position;
//		ALOGI("Render To TextureQueue texture Position is %.3f ", position);
       renderToTexture(inputTexId, frameTexture->texId);
        circleFrameTextureQueue->unLockPushCursorFrameTexture();
        // backup the first frame
        if (isFirstFrame) {
            FrameTexture* firstFrameTexture = circleFrameTextureQueue->getFirstFrameFrameTexture();
            if (firstFrameTexture) {
                //cpy input texId to target texId
                renderToTexture(inputTexId, firstFrameTexture->texId);
            }
        }
    }
}

void VideoEditor::renderToTexture(GLuint inputTexId, GLuint outputTexId) {
    glViewport(0, 0, GLsizei(width), GLsizei(height));

    if (!mIsPassRenderInitialized) {
        ALOGE("ViewRenderEffect::renderEffect effect not initialized!");
        return;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexId, 0);
    checkGlError("PassThroughRender::renderEffect glFramebufferTexture2D");
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGI("failed to make complete framebuffer object %x", status);
    }

    glUseProgram (mPassRenderProgId);
    static const GLfloat _vertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f };
    glVertexAttribPointer(mPassRenderVertexCoords, 2, GL_FLOAT, 0, 0, _vertices);
    glEnableVertexAttribArray(mPassRenderVertexCoords);
    static const GLfloat texCoords[] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    glVertexAttribPointer(mPassRenderTextureCoords, 2, GL_FLOAT, 0, 0, texCoords);
    glEnableVertexAttribArray(mPassRenderTextureCoords);

    /* Binding the input texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexId);
    glUniform1i(mPassRenderUniformTexture, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mPassRenderVertexCoords);
    glDisableVertexAttribArray(mPassRenderTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

void VideoEditor::copyFrameData(uint8_t * dst, uint8_t * src, int width, int height, int linesize) {
    for (int i = 0; i < height; ++i) {
        memcpy(dst, src, width);
        dst += width;
        src += linesize;
    }
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

GLuint VideoEditor::loadProgram(char *pVertexSource, char *pFragmentSource) {
    GLuint programId;
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        ALOGE("vertexShader null");
        return 0;
    }
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        ALOGE("vertexShader null");
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

GLuint VideoEditor::loadShader(GLenum shaderType, const char *pSource) {
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
                    ALOGE("%s %S glCompileShader error %d\n %s\n", __FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            } else {
                ALOGE("Guessing at GL_INFO_LOG_LENGTH size\n");
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

bool VideoEditor::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        ALOGE("after %s() glError (0x%x)\n", op, error);
        return true;
    }
    return false;
}

void VideoEditor::matrixSetIdentityM(float *m)
{
    memset((void*)m, 0, 16*sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void VideoEditor::renderWithCoords(GLuint texId, GLfloat* vertexCoords, GLfloat* textureCoords) {
    glBindTexture(GL_TEXTURE_2D, texId);
    checkGlError("glBindTexture");

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texId, 0);
    checkGlError("glFramebufferTexture2D");

    glUseProgram(mCopierProgId);
    if (!mIsCopierInitialized) {
        return;
    }
    glVertexAttribPointer(mCopierVertexCoords, 2, GL_FLOAT, GL_FALSE, 0, vertexCoords);
    glEnableVertexAttribArray (mCopierVertexCoords);
    glVertexAttribPointer(mCopierTextureCoords, 2, GL_FLOAT, GL_FALSE, 0, textureCoords);
    glEnableVertexAttribArray (mCopierTextureCoords);

    /* Binding the input texture */
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        if (checkGlError("glBindTexture")) {
            return;
        }
        glUniform1i(_uniformSamplers[i], i);
    }

    float texTransMatrix[4 * 4];
    matrixSetIdentityM(texTransMatrix);
    glUniformMatrix4fv(mCopierUniformTexMatrix, 1, GL_FALSE, (GLfloat *) texTransMatrix);

    float rotateMatrix[4 * 4];
    matrixSetIdentityM(rotateMatrix);
    glUniformMatrix4fv(mCopierUniformTransforms, 1, GL_FALSE, (GLfloat *) rotateMatrix);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mCopierVertexCoords);
    glDisableVertexAttribArray(mCopierTextureCoords);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
//  ALOGI("draw waste time is %ld", (getCurrentTime() - startDrawTimeMills));
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

int VideoEditor::videoCallbackGetTex(FrameTexture **frameTex, void *ctx, bool forceGetFrame) {

}

int VideoEditor::audioCallbackFillData(byte *buf, size_t bufSize, void *ctx) {

}





