#include <list>
#include <common/opengl_media/movie_frame.h>
#include "VideoEditor.h"

/**
 * 主要到事情说三遍，为了吃透核心代码，先不要做封装，所有核心代码写在这一个cpp里
 * 串联整个音视频播放流程，完成文件的解析、音视频的解码、渲染。
 */
VideoEditor::VideoEditor()
    : isMediaCodecInit(false), inputBuffer(NULL), audioFrameQueue(NULL), circleFrameTextureQueue(NULL), currentAudioFrame(NULL){
    mFormatCtx = nullptr;
    videoOutput = NULL;
    audioOutput = NULL;
    mScreenWidth = 0;
    mScreenHeight = 0;
    minBufferedDuration = LOCAL_MIN_BUFFERED_DURATION;
    maxBufferedDuration = LOCAL_MAX_BUFFERED_DURATION;
    syncMaxTimeDiff = LOCAL_AV_SYNC_MAX_TIME_DIFF;
    currentAudioFramePos = 0;
    moviePosition = 0;
    bufferedDuration = 0;
    seek_req = false;
    position = 0.0f;

    mIsPlaying = false;
    isOnDecoding = false;
    isDestroyed = false;
    isCompleted = false;
    isInitializeDecodeThread = false;
    vertexCoords = new GLfloat[OPENGL_VERTEX_COORDNATE_CNT];
    textureCoords = new GLfloat[OPENGL_VERTEX_COORDNATE_CNT];

    mMsgQueue = new MessageQueue();
    mHandler = new VideoEditorHandler(this, mMsgQueue);

    mCbMsgQueue = new AVMessageQueue();
}

VideoEditor::~VideoEditor() {
    delete[] vertexCoords;
    delete[] textureCoords;

    if (mCbMsgQueue) {
        mCbMsgQueue->release();
        delete mCbMsgQueue;
        mCbMsgQueue = nullptr;
    }

    if (mHandler) {
        mHandler->postMessage(new Message(MESSAGE_QUEUE_LOOP_QUIT_FLAG));
    }
    pthread_join(mThreadId, 0);
    if (mMsgQueue) {
        mMsgQueue->abort();
        delete mMsgQueue;
        mMsgQueue = NULL;
    }
    delete mHandler;
    mHandler = NULL;
    LOGI("MediaRecorder thread stopped");
}

bool VideoEditor::prepare(char *srcFilePath,bool isHWDecode) {
    mIsPlaying = false;
    mIsHWDecode = isHWDecode;
    mUri = strdup(srcFilePath);
    if (mHandler)
        mHandler->postMessage(new Message(MSG_PREPARE));
    pthread_create(&mThreadId, 0, threadStartCallback, this);
    mIsUserCancelled = false;
    return true;
}

void VideoEditor::onSurfaceCreated(ANativeWindow *window, int widht, int height) {
    LOGI("onSurfaceCreated");
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
    if (videoOutput) {
        videoOutput->onSurfaceDestroyed();
    }
}

bool VideoEditor::prepare_l() {
    LOGI("prepare_l");
    bool ret = false;

    if (mIsUserCancelled) {
        return true;
    }
    if (mUri == nullptr) {
        LOGE("mUri is nullptr");
        return false;
    }
    ret = initFFmpeg();
    if (false == ret) {
        LOGE("initFFmpeg failed");
        return false;
    }

    if (mIsHWDecode) {

    } else {
        //先实现软解
    }

    initAudioOutput();
    initDecoderThread();
    if (mCbMsgQueue) {
        mCbMsgQueue->postMessage(MSG_PREPARED);
    }
    return true;
}

void VideoEditor::start() {
    LOGI("VideoEditor::start");
    if (NULL != audioOutput) {
        audioOutput->start();
    }
    mIsPlaying = true;
}

void VideoEditor::pause() {
    LOGI("VideoEditor::pause");
    if (!mIsPlaying)
        return;
    mIsPlaying = false;
    if (NULL != audioOutput) {
        audioOutput->pause();
    }
}

void VideoEditor::resume() {
    if (this->mIsPlaying)
        return;
    this->mIsPlaying = true;
    if (NULL != audioOutput) {
        audioOutput->play();
    }
}

void VideoEditor::seekTo(float position) {
    //todo
    //moviePosition = position;
}

bool VideoEditor::isPlaying() {
    return mIsPlaying;
}

float VideoEditor::getCurrentPosition() {
    return moviePosition;
}

float VideoEditor::getDuration() {
    if(!mFormatCtx){
        return 0;
    }
    if (mFormatCtx->duration == AV_NOPTS_VALUE){
        return -1;
    }
    return (float)mFormatCtx->duration / AV_TIME_BASE;
}

void VideoEditor::initCopier() {
    mCopierVertexShader = NO_FILTER_VERTEX_SHADER;
    mCopierFragmentShader = YUV_FRAME_FRAGMENT_SHADER;

    mCopierProgId = loadProgram(mCopierVertexShader, mCopierFragmentShader);
    if (!mCopierProgId) {
        LOGE("Could not create program.");
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
        LOGE("Could not create program.");
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
    circleFrameTextureQueue->setIsFirstFrame(true);
}

bool VideoEditor::initFFmpeg() {
    avcodec_register_all();
    av_register_all();
    mFormatCtx = avformat_alloc_context();
    if ((avformat_open_input(&mFormatCtx, mUri, NULL, NULL) != 0)) {
        LOGE("avformat_open_input failed");
        return false;
    }
    //mFormatCtx->probesize = 50 * 1024;
    //mFormatCtx->max_analyze_duration = 75000;
    if (avformat_find_stream_info(mFormatCtx, NULL) < 0) {
        LOGE("Video decoder Stream info not found...");
        return false;
    }
    mVideoStreamIndex = -1;
    for (int i = 0; i < mFormatCtx->nb_streams; i++) {
        if (AVMEDIA_TYPE_VIDEO == mFormatCtx->streams[i]->codec->codec_type) {
            mVideoStreamIndex = i;
        }
    }
    AVStream* videoStream = mFormatCtx->streams[mVideoStreamIndex];
    mVideoCodecCtx = videoStream->codec;
    videoCodec = avcodec_find_decoder(mVideoCodecCtx->codec_id);
    if (!videoCodec) {
        LOGE("videoCodec null");
        return false;
    }
    if (avcodec_open2(mVideoCodecCtx, videoCodec, NULL) < 0) {
        LOGE("avcodec_open2 failed");
        return false;
    }
    videoFrame = av_frame_alloc();
    if (!videoFrame) {
        LOGE("videoFrame null");
        avcodec_close(mVideoCodecCtx);
        return false;
    }
    avStreamFPSTimeBase(videoStream, 0.04, &fps, &videoTimeBase);
    if(fps > 30.0f || fps < 5.0f){
        fps = 24.0f;
    }
    LOGI("video codec size: fps: %.3f tb: %f", fps, videoTimeBase);
    LOGI("videoCodecCtx->pix_fmt is %d {%d, %d}", mVideoCodecCtx->pix_fmt, AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUVJ420P);
    mWidth = mVideoCodecCtx->width;
    mHeight = mVideoCodecCtx->height;
    LOGI("width=%d height=%d", mWidth, mHeight);

    audioStreamIndex = -1;
    for (int i = 0; i < mFormatCtx->nb_streams; i++) {
        if (AVMEDIA_TYPE_AUDIO == mFormatCtx->streams[i]->codec->codec_type) {
            audioStreamIndex = i;
        }
    }
    AVStream *audioStream = mFormatCtx->streams[audioStreamIndex];
    mAudioCodecCtx = audioStream->codec;
    mAudioCodec = avcodec_find_decoder(mAudioCodecCtx->codec_id);
    if (!mAudioCodec) {
        LOGE("mAudioCodec null");
        avcodec_close(mVideoCodecCtx);
        av_frame_free(&videoFrame);
        return false;
    }
    if (avcodec_open2(mAudioCodecCtx, mAudioCodec, NULL) < 0) {
        LOGE("open audio codec failed...");
        avcodec_close(mAudioCodecCtx);
        avcodec_close(mVideoCodecCtx);
        av_frame_free(&videoFrame);
        return false;
    }
    if (mAudioCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16) {
        LOGI("because of audio Codec Is Not Supported so we will init swresampler...");
        swrContext = swr_alloc_set_opts(NULL, av_get_default_channel_layout(mAudioCodecCtx->channels), AV_SAMPLE_FMT_S16,
                                        mAudioCodecCtx->sample_rate, av_get_default_channel_layout(mAudioCodecCtx->channels), mAudioCodecCtx->sample_fmt, mAudioCodecCtx->sample_rate, 0, NULL);
        if (!swrContext || swr_init(swrContext)) {
            if (swrContext)
                swr_free(&swrContext);
            avcodec_close(mAudioCodecCtx);
            avcodec_close(mVideoCodecCtx);
            av_frame_free(&videoFrame);
            LOGE("init resampler failed...");
            return false;
        }
    }
    audioFrame = av_frame_alloc();
    if (audioFrame == NULL) {
        LOGE("alloc audio frame failed...");
        if (swrContext)
            swr_free(&swrContext);
        avcodec_close(mAudioCodecCtx);
        avcodec_close(mVideoCodecCtx);
        av_frame_free(&videoFrame);
        return -1;
    }
    avStreamFPSTimeBase(audioStream, 0.025, 0, &audioTimeBase);
    LOGI("sample rate is %d channels is %d audioCodecCtx->sample_fmt is %d and AV_SAMPLE_FMT_S16 is %d audioTimeBase: %f",
            mAudioCodecCtx->sample_rate, mAudioCodecCtx->channels, mAudioCodecCtx->sample_fmt, AV_SAMPLE_FMT_S16, audioTimeBase);
    return true;
}

void VideoEditor::initVideoOutput(ANativeWindow *window) {
    if (window == NULL || mIsUserCancelled){
        return;
    }
    videoOutput = new VideoOutput();
    videoOutput->initOutput(window, mScreenWidth, mScreenHeight, videoCallbackGetTex, this);
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
        LOGI("WARNING: st.codec.ticks_per_frame=%d", st->codec->ticks_per_frame);
    }

    if (st->avg_frame_rate.den && st->avg_frame_rate.num){
        fps = av_q2d(st->avg_frame_rate);
        LOGI("Calculate By St avg_frame_rate : fps is %.3f", fps);
    } else if (st->r_frame_rate.den && st->r_frame_rate.num){
        fps = av_q2d(st->r_frame_rate);
        LOGI("Calculate By St r_frame_rate : fps is %.3f", fps);
    } else {
        fps = 1.0 / timebase;
        LOGI("Calculate By 1.0 / timebase : fps is %.3f", fps);
    }
    if (pFPS) {
        LOGI("assign fps value fps is %.3f", fps);
        *pFPS = fps;
    }
    if (pTimeBase) {
        LOGI("assign pTimeBase value");
        *pTimeBase = timebase;
    }
}

bool VideoEditor::initAudioOutput() {
    LOGI("initAudioOutput");
    audioOutput = new AudioOutput();
    SLresult result = audioOutput->initSoundTrack(mAudioCodecCtx->channels, mAudioCodecCtx->sample_rate, audioCallbackFillData, this);
    if (SL_RESULT_SUCCESS != result) {
        LOGI("audio manager failed on initialized...");
        delete audioOutput;
        audioOutput = NULL;
        return false;
    }
    return true;
}

void VideoEditor::destroy() {
    isDestroyed = true;

    if (NULL != videoOutput) {
        videoOutput->stopOutput();
        delete videoOutput;
        videoOutput = NULL;
    }
    if (NULL != audioOutput) {
        audioOutput->stop();
        delete audioOutput;
        audioOutput = NULL;
    }

    destroyDecoderThread();
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
    if (NULL != audioFrameQueue) {
        clearAudioFrameQueue();
        pthread_mutex_lock(&audioFrameQueueMutex);
        delete audioFrameQueue;
        audioFrameQueue = NULL;
        pthread_mutex_unlock(&audioFrameQueueMutex);
        pthread_mutex_destroy(&audioFrameQueueMutex);
    }

    closeFile();
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
    float duration = 0.1f;
    while (good) {
        good = false;
        if (canDecode()) {
            processDecodingFrame(good, duration);
        } else {
            break;
        }
    }
}

void VideoEditor::processDecodingFrame(bool &good, float duration) {
    int decodeVideoErrorState;
    std::list<MovieFrame*>* frames = decodeFrames(duration, &decodeVideoErrorState);
    if (NULL != frames) {
        if (!frames->empty()) {
            good = addFrames(0.8, frames);
        } else {
            LOGI("frames is empty %d", (int )good);
        }
        delete frames;
    } else {
        LOGI("why frames is NULL tell me why?");
    }
}

std::list<MovieFrame*>* VideoEditor::decodeFrames(float minDuration, int *decodeVideoErrorState) {
    bool finished = false;
    if (-1 == mVideoStreamIndex && -1 == audioStreamIndex) {
        LOGE("decodeFrames error");
        return NULL;
    }
    std::list<MovieFrame*> *result = new std::list<MovieFrame*>();

    int ret = 0;
    char errString[128];
    AVPacket packet;
    float decodedDuration = 0.0f;
    while (!finished) {
        ret = av_read_frame(mFormatCtx, &packet);
        if (ret < 0) {
            LOGE("av_read_frame return an error");
            if (ret != AVERROR_EOF) {
                av_strerror(ret, errString, 128);
                LOGE("av_read_frame return an not AVERROR_EOF error : %s", errString);
            } else {
                LOGI("input EOF");
                is_eof = true;
            }
            av_free_packet(&packet);
            break;
        }
        if (packet.stream_index == mVideoStreamIndex) {
            decodeVideoFrame(packet, decodeVideoErrorState);
        } else if (packet.stream_index == audioStreamIndex) {
            finished = decodeAudioFrames(&packet, result, decodedDuration, minDuration, decodeVideoErrorState);
        }
        av_free_packet(&packet);
    }
    if (is_eof) {
        //todo
    }
    return result;
}

bool VideoEditor::decodeVideoFrame(AVPacket packet, int *decodeVideoErrorState) {
    int pktSize = packet.size;
    int gotFrame = 0;
    while (pktSize > 0) {
        int len = avcodec_decode_video2(mVideoCodecCtx, videoFrame, &gotFrame, &packet);
        if (len < 0) {
            LOGI("decode video error, skip packet");
            *decodeVideoErrorState = 1;
            break;
        }
        if (gotFrame) {
            if (videoFrame->interlaced_frame) {
                //avpicture_deinterlace((AVPicture*) videoFrame, (AVPicture*) videoFrame, videoCodecCtx->pix_fmt, videoCodecCtx->width, videoCodecCtx->height);
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
                                    float &decodedDuration, float minDuration, int *decodeVideoErrorState) {
    bool finished = false;
    int pktSize = packet->size;

    while (pktSize > 0) {
        int gotframe = 0;
        int len = avcodec_decode_audio4(mAudioCodecCtx, audioFrame, &gotframe, packet);
        if (len < 0) {
            LOGE("decode audio error, skip packet");
            *decodeVideoErrorState = 1;
            break;
        }
        if (gotframe) {
            AudioFrame* frame = handleAudioFrame();
            if(frame) {
                result->push_back(frame);
                position = frame->position;
                decodedDuration += frame->duration;
                if (decodedDuration > minDuration) {
                    finished = true;
                }
            } else {
                LOGI("skip audio");
            }
        }
        if (0 == len) {
            break;
        }
        pktSize -= len;
    }
    return finished;
}

AudioFrame * VideoEditor::handleAudioFrame() {
    if (!audioFrame->data[0]) {
        LOGI("audioFrame->data[0] is 0... why...");
        return NULL;
    }
    int numChannels = mAudioCodecCtx->channels;
    int numFrames = 0;
    void * audioData;
    if (swrContext) {
        const int ratio = 2;
        const int bufSize = av_samples_get_buffer_size(NULL, numChannels, audioFrame->nb_samples * ratio, AV_SAMPLE_FMT_S16, 1);
        if (!swrBuffer || swrBufferSize < bufSize) {
//			LOGI("start realloc buffer and bufSize is %d...", bufSize);
            swrBufferSize = bufSize;
            swrBuffer = realloc(swrBuffer, swrBufferSize);
//			LOGI("realloc buffer success");
        }
//		LOGI("define and assign outbuf");
        byte *outbuf[2] = { (byte*) swrBuffer, NULL };
//		LOGI("start swr_convert");
        numFrames = swr_convert(swrContext, outbuf, audioFrame->nb_samples * ratio, (const uint8_t **) audioFrame->data, audioFrame->nb_samples);
//		LOGI("swr_convert success and numFrames is %d", numFrames);
        if (numFrames < 0) {
            LOGI("fail resample audio");
            return NULL;
        }
        audioData = swrBuffer;
    } else {
        if (mAudioCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16) {
            LOGI("bucheck, audio format is invalid");
            return NULL;
        }
        audioData = audioFrame->data[0];
        numFrames = audioFrame->nb_samples;
    }
//	LOGI("start process audioData and numFrames is %d numChannels is %d", numFrames, numChannels);
    const int numElements = numFrames * numChannels;
    float position = av_frame_get_best_effort_timestamp(audioFrame) * audioTimeBase;
//	LOGI("begin processAudioData...");
//	LOGI("decode audio bufferSize expected 1024  actual is %d audio position is %.4f", numElements, position);
    byte* buffer = NULL;

    int actualSize = -1;
    actualSize = processAudioData((short*)audioData, numElements, position, &buffer);
    if (actualSize  < 0) {
        return NULL;
    }

    AudioFrame *frame = new AudioFrame();
    /** av_frame_get_best_effort_timestamp 瀹為檯涓婅幏鍙朅VFrame鐨?int64_t best_effort_timestamp; 杩欎釜Filed **/
    frame->position = position;
    frame->samples = buffer;
    frame->size = actualSize;
    frame->duration = av_frame_get_pkt_duration(audioFrame) * audioTimeBase;
    if (frame->duration == 0) {
        // sometimes ffmpeg can't determine the duration of audio frame
        // especially of wma/wmv format
        // so in this case must compute duration
        frame->duration = frame->size / (sizeof(float) * numChannels * 2 * mAudioCodecCtx->sample_rate);
    }
//	LOGI("AFD: %.4f %.4f | %.4f ", frame->position, frame->duration, frame->size / (8.0 * 44100.0));
//	LOGI("leave VideoDecoder::handleAudioFrame()...");
    return frame;
}

int VideoEditor::processAudioData(short *sample, int size, float position, byte** buffer) {
    int bufferSize = size * 2;
    (*buffer) = new byte[bufferSize];
    convertByteArrayFromShortArray(sample, size, *buffer);
    return bufferSize;
}

bool VideoEditor::addFrames(float thresholdDuration, std::list<MovieFrame *> *frames) {
    if (audioStreamIndex != -1) {
        pthread_mutex_lock(&audioFrameQueueMutex);
        std::list<MovieFrame*>::iterator i;
        for (i = frames->begin(); i != frames->end(); ++i) {
            MovieFrame* frame = *i;
            if (frame->getType() == MovieFrameTypeAudio) {
                AudioFrame* audioFrame = (AudioFrame*) frame;
                audioFrameQueue->push(audioFrame);
//				LOGI("audioFrameQueue->push(audioFrame) position is %.4f", audioFrame->position);
                bufferedDuration += audioFrame->duration;
            }
        }
        pthread_mutex_unlock(&audioFrameQueueMutex);
    }
//	LOGI("bufferDuration is %.3f thresholdDuration is %.3f buffered is %d", bufferedDuration, thresholdDuration, buffered);
    bool isBufferedDurationIncreasedToThreshold = (bufferedDuration >= thresholdDuration) &&
                                                  (circleFrameTextureQueue->getValidSize()  >= thresholdDuration * fps);
    return  !isBufferedDurationIncreasedToThreshold;
}

bool VideoEditor::canDecode() {
    return !pauseDecodeThreadFlag && !isDestroyed && (mVideoStreamIndex != -1 || audioStreamIndex != -1);
}

void VideoEditor::initDecoderThread() {
    isInitializeDecodeThread = true;
    isDecodingFrames = false;
    isOnDecoding = true;
    pthread_mutex_init(&videoDecoderLock, NULL);
    pthread_cond_init(&videoDecoderCondition, NULL);
    pthread_create(&videoDecoderThread, NULL, startDecoderThread, this);
}

void VideoEditor::signalDecodeThread() {
    if (isDestroyed) {
        LOGI("NULL == decoder || isDestroyed == true");
        return;
    }

    bool isBufferedDurationDecreasedToMin = bufferedDuration <= minBufferedDuration ||
                                            (circleFrameTextureQueue->getValidSize() <= minBufferedDuration * fps);

    if (!isDestroyed && (seek_req) || ((!isDecodingFrames) && isBufferedDurationDecreasedToMin)) {
        int getLockCode = pthread_mutex_lock(&videoDecoderLock);
        pthread_cond_signal(&videoDecoderCondition);
        pthread_mutex_unlock(&videoDecoderLock);
    }
}

void VideoEditor::destroyDecoderThread() {
//	LOGI("AVSynchronizer::destroyDecoderThread ...");
    isOnDecoding = false;
    if (!isInitializeDecodeThread) {
        return;
    }
    void* status;
    int getLockCode = pthread_mutex_lock(&videoDecoderLock);
    pthread_cond_signal(&videoDecoderCondition);
    pthread_mutex_unlock(&videoDecoderLock);
    pthread_join(videoDecoderThread, &status);
    pthread_mutex_destroy(&videoDecoderLock);
    pthread_cond_destroy(&videoDecoderCondition);
}
void VideoEditor::initConverter() {
    initTexture();
    memcpy(vertexCoords, DECODER_COPIER_GL_VERTEX_COORDS,
           sizeof(GLfloat) * OPENGL_VERTEX_COORDNATE_CNT);
    memcpy(textureCoords, DECODER_COPIER_GL_TEXTURE_COORDS_NO_ROTATION,
           sizeof(GLfloat) * OPENGL_VERTEX_COORDNATE_CNT);
    copyTexSurface = createOffscreenSurface(mWidth, mHeight);
    eglMakeCurrent(mEGLDisplay, copyTexSurface, copyTexSurface, mEGLContext);

    glGenFramebuffers(1, &mFBO);
    glGenTextures(1, &outputTexId);
    glBindTexture(GL_TEXTURE_2D, outputTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VideoEditor::closeFile() {
    LOGI("closeAudioStream...");
    closeAudioStream();
    LOGI("closeVideoStream...");
    closeVideoStream();
    LOGI("closeSubtitleStream...");

    if (NULL != mFormatCtx) {
        mFormatCtx->interrupt_callback.opaque = NULL;
        mFormatCtx->interrupt_callback.callback = NULL;
        LOGI("avformat_close_input(&mFormatCtx)");
        avformat_close_input(&mFormatCtx);
        avformat_free_context(mFormatCtx);
        mFormatCtx = NULL;
    }
}

void VideoEditor::closeVideoStream() {
    mVideoStreamIndex = -1;

    if (NULL != videoFrame) {
        av_free(videoFrame);
        videoFrame = NULL;
    }
    if (NULL != mVideoCodecCtx) {
        avcodec_close(mVideoCodecCtx);
        mVideoCodecCtx = NULL;
    }
}

void VideoEditor::closeAudioStream() {
    audioStreamIndex = -1;
    if (NULL != swrBuffer) {
        free(swrBuffer);
        swrBuffer = NULL;
        swrBufferSize = 0;
    }
    if (NULL != swrContext) {
        swr_free(&swrContext);
        swrContext = NULL;
    }
    if (NULL != audioFrame) {
        av_free(audioFrame);
        audioFrame = NULL;
    }
    if (NULL != mAudioCodecCtx) {
        avcodec_close(mAudioCodecCtx);
        mAudioCodecCtx = NULL;
    }
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
    //直接上传纹理，不做多线程同步，注意当前是在解码线程里
    eglMakeCurrent(mEGLDisplay, copyTexSurface, copyTexSurface, mEGLContext);
    drawFrame();
}

void VideoEditor::drawFrame() {
    float position = updateTexImage();
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    renderWithCoords(outputTexId, vertexCoords, textureCoords);
    pushToVideoQueue(outputTexId, mWidth, mHeight, position);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

float VideoEditor::updateTexImage() {
    float position = -1;
    VideoFrame* yuvFrame = handleVideoFrame();
    if (yuvFrame) {
        updateYUVTexImage(yuvFrame);
        position = yuvFrame->position;
        delete yuvFrame;
    }
    return position;
}

void VideoEditor::updateYUVTexImage(VideoFrame* yuvFrame) {
    if (yuvFrame) {
        int frameWidth = yuvFrame->width;
        int frameHeight = yuvFrame->height;
        if (frameWidth % 16 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        uint8_t *pixels[3] = {yuvFrame->luma, yuvFrame->chromaB, yuvFrame->chromaR};
        int widths[3] = {frameWidth, frameWidth >> 1, frameWidth >> 1};
        int heights[3] = {frameHeight, frameHeight >> 1, frameHeight >> 1};
        LOGI("updateYUVTexImage()...");
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i]);
            if (checkGlError("glBindTexture1")) { //error in here
                return;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, widths[i], heights[i], 0, GL_LUMINANCE,
                         GL_UNSIGNED_BYTE, pixels[i]);
        }
    }
}

VideoFrame* VideoEditor::handleVideoFrame() {
    LOGI("enter VideoEditor::handleVideoFrame()...");
    if(!videoFrame->data[0]) {
        LOGE("videoFrame->data[0] is null");
        return NULL;
    }
    VideoFrame *yuvFrame = new VideoFrame();
    int width = MIN(videoFrame->linesize[0], mVideoCodecCtx->width);
    int height = mVideoCodecCtx->height;
    int lumaLength = width * height;
    uint8_t * luma = new uint8_t[lumaLength];
    copyFrameData(luma, videoFrame->data[0], width, height, videoFrame->linesize[0]);
    yuvFrame->luma = luma;

    width = MIN(videoFrame->linesize[1], mVideoCodecCtx->width / 2);
    height = mVideoCodecCtx->height / 2;
    int chromaBLength = width * height;
    uint8_t * chromaB = new uint8_t[chromaBLength];
    copyFrameData(chromaB, videoFrame->data[1], width, height, videoFrame->linesize[1]);
    yuvFrame->chromaB = chromaB;

    width = MIN(videoFrame->linesize[2], mVideoCodecCtx->width / 2);
    height = mVideoCodecCtx->height / 2;
    int chromaRLength = width * height;
    uint8_t * chromaR = new uint8_t[chromaRLength];
    copyFrameData(chromaR, videoFrame->data[2], width, height, videoFrame->linesize[2]);
    yuvFrame->chromaR = chromaR;

    yuvFrame->width = mVideoCodecCtx->width;
    yuvFrame->height = mVideoCodecCtx->height;
    yuvFrame->position = av_frame_get_best_effort_timestamp(videoFrame) * videoTimeBase;

    const int64_t frameDuration = av_frame_get_pkt_duration(videoFrame);
    if (frameDuration) {
        yuvFrame->duration = frameDuration * videoTimeBase;
        yuvFrame->duration += videoFrame->repeat_pict * videoTimeBase * 0.5;
    } else {
        yuvFrame->duration = 1.0 / fps;
    }
	LOGI("VFD: %.4f %.4f | %lld ", yuvFrame->position, yuvFrame->duration, av_frame_get_pkt_pos(videoFrame));
	LOGI("leave VideoEditor::handleVideoFrame()...");
	//todo dump yuv data
    return yuvFrame;
}

void VideoEditor::pushToVideoQueue(GLuint inputTexId, int width, int height, float position) {
    if (!mIsPassRenderInitialized){
        LOGE("renderToVideoQueue::passThorughRender is NULL");
        return;
    }

    if (!circleFrameTextureQueue) {
        LOGE("renderToVideoQueue::circleFrameTextureQueue is NULL");
        return;
    }

    bool isFirstFrame = circleFrameTextureQueue->getIsFirstFrame();
    FrameTexture* frameTexture = circleFrameTextureQueue->lockPushCursorFrameTexture();
    if (NULL != frameTexture) {
        frameTexture->position = position;
		LOGI("Render To TextureQueue texture Position is %.3f ", position);
       renderToTexture(inputTexId, frameTexture->texId);
        circleFrameTextureQueue->unLockPushCursorFrameTexture();
        // backup the first frame
        if (isFirstFrame) {
            FrameTexture* firstFrameTexture = circleFrameTextureQueue->getFirstFrameFrameTexture();
            if (firstFrameTexture) {
                //cpy input texId to target texId
                //todo
                //renderToTexture(inputTexId, firstFrameTexture->texId);
            }
        }
    }
}

FrameTexture* VideoEditor::getCorrectRenderTexture(bool forceGetFrame) {
    FrameTexture *texture = NULL;
    if (!circleFrameTextureQueue) {
        LOGE("getCorrectRenderTexture::circleFrameTextureQueue is NULL");
        return texture;
    }
    int leftVideoFrames = circleFrameTextureQueue->getValidSize();
    if (leftVideoFrames == 1) {
        return texture;
    }
    while (true) {
        int ret = circleFrameTextureQueue->front(&texture);
        if(ret > 0){
            if (forceGetFrame) {
                return texture;
            }
            const float delta = (moviePosition - DEFAULT_AUDIO_BUFFER_DURATION_IN_SECS) - texture->position;
            if (delta < (0 - syncMaxTimeDiff)) {
                texture = NULL;
                break;
            }
            circleFrameTextureQueue->pop();
            if (delta > syncMaxTimeDiff) {
                continue;
            } else {
                break;
            }
        } else{
            texture = NULL;
            break;
        }
    }
    return texture;
}

int VideoEditor::consumeAudioFrames(byte* outData, size_t bufferSize) {
    int ret = bufferSize;
    if(mIsPlaying && !isDestroyed && !isCompleted) {
        ret = fillAudioData(outData, bufferSize);
        signalOutputFrameAvailable();
    } else {
        memset(outData, 0, bufferSize);
    }
    return ret;
}

int VideoEditor::fillAudioData(byte* outData, int bufferSize) {
    signalDecodeThread();

    //checkPlayState();

    int needBufferSize = bufferSize;
    while (bufferSize > 0) {
        if (NULL == currentAudioFrame) {
            pthread_mutex_lock(&audioFrameQueueMutex);
            int count = audioFrameQueue->size();
			//LOGI("audioFrameQueue->size() is %d", count);
            if (count > 0) {
                AudioFrame *frame = audioFrameQueue->front();
                bufferedDuration -= frame->duration;
                audioFrameQueue->pop();
                if (!seek_req) {
                    //resolve when drag seek bar position changed Frequent
                    moviePosition = frame->position;
                }
                currentAudioFrame = new AudioFrame();
                currentAudioFramePos = 0;
                int frameSize = frame->size;
                currentAudioFrame->samples = new byte[frameSize];
                memcpy(currentAudioFrame->samples, frame->samples, frameSize);
                currentAudioFrame->size = frameSize;
                delete frame;
            }
            pthread_mutex_unlock(&audioFrameQueueMutex);
        }

        if (NULL != currentAudioFrame) {
            byte* bytes = currentAudioFrame->samples + currentAudioFramePos;
            int bytesLeft = currentAudioFrame->size - currentAudioFramePos;
            int bytesCopy = std::min(bufferSize, bytesLeft);
            memcpy(outData, bytes, bytesCopy);
            bufferSize -= bytesCopy;
            outData += bytesCopy;

            if (bytesCopy < bytesLeft)
                currentAudioFramePos += bytesCopy;
            else {
                delete currentAudioFrame;
                currentAudioFrame = NULL;
            }
        } else {
            LOGI("fillAudioData NULL == currentAudioFrame");
            memset(outData, 0, bufferSize);
            bufferSize = 0;
            break;
        }
    }
    return needBufferSize - bufferSize;
}

void VideoEditor::clearAudioFrameQueue() {
    pthread_mutex_lock(&audioFrameQueueMutex);
    while (!audioFrameQueue->empty()) {
        AudioFrame* frame = audioFrameQueue->front();
        audioFrameQueue->pop();
        delete frame;
    }

    bufferedDuration = 0;
    pthread_mutex_unlock(&audioFrameQueueMutex);
}

void VideoEditor::signalOutputFrameAvailable() {
    if (NULL != videoOutput){
        videoOutput->signalFrameAvailable();
    }
}

void VideoEditor::renderToTexture(GLuint inputTexId, GLuint outputTexId) {
    glViewport(0, 0, GLsizei(mWidth), GLsizei(mHeight));

    if (!mIsPassRenderInitialized) {
        LOGE("ViewRenderEffect::renderEffect effect not initialized!");
        return;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexId, 0);
    checkGlError("PassThroughRender::renderEffect glFramebufferTexture2D");
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGI("failed to make complete framebuffer object %x", status);
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
        LOGE("eglGetDisplay error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(mEGLDisplay, 0 , 0)) {
        LOGE("eglInitialize error %d", eglGetError());
        return false;
    }
    if (!eglChooseConfig(mEGLDisplay, attribs, &mEGLConfig, 1, &numConfigs)) {
        LOGE("eglChooseConfig error %d", eglGetError());
        return false;
    }
    EGLint  eglContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, EGL_NO_CONTEXT, eglContextAttributes);
    checkGlError("eglCreateContext");
    if (eglGetError() == EGL_SUCCESS) {
        LOGI("eglCreateContext success");
    }
    if (!mEGLContext) {
        LOGE("eglCreateContext error %d", eglGetError());
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            LOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    return true;
}

bool VideoEditor::releaseEGL() {
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
        LOGI("after eglMakeCurrent...");
        eglDestroyContext(mEGLDisplay, mEGLContext);
        LOGI("after eglDestroyContext...");
    }
    mEGLDisplay = EGL_NO_DISPLAY;
    mEGLContext = EGL_NO_CONTEXT;
}

EGLSurface VideoEditor::createWindowSurface(ANativeWindow *pWindow) {
    EGLSurface surface = NULL;
    EGLint format;
    if (!pWindow) {
        LOGE("pWindow null");
        return NULL;
    }
    if (!eglGetConfigAttrib(mEGLDisplay, mEGLConfig, EGL_NATIVE_VISUAL_ID, &format)) {
        LOGE("eglGetConfigAttrib error");
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            LOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return NULL;
    }
    ANativeWindow_setBuffersGeometry(pWindow, 0, 0, format);
    if (!(surface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, pWindow, 0))) {
        LOGE("eglCreateWindowSurface error %d", eglGetError());
        return NULL;
    }
    return surface;
}

EGLSurface VideoEditor::createOffscreenSurface(int width, int height) {
    EGLSurface surface;
    EGLint PbufferAttributes[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE, EGL_NONE };
    if (!(surface = eglCreatePbufferSurface(mEGLDisplay, mEGLConfig, PbufferAttributes))) {
        LOGE("eglCreatePbufferSurface() returned error %d", eglGetError());
    }
    return surface;
}

GLuint VideoEditor::loadProgram(char *pVertexSource, char *pFragmentSource) {
    GLuint programId;
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        LOGE("vertexShader null");
        return 0;
    }
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        LOGE("vertexShader null");
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
                    LOGI("Could not link program:\n%s\n", buf);
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
    //LOGE("%s", pSource);
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
                    LOGE("%s %S glCompileShader error %d\n %s\n", __FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            } else {
                LOGE("Guessing at GL_INFO_LOG_LENGTH size\n");
                char* buf = (char*) malloc(0x1000);
                if (buf) {
                    glGetShaderInfoLog(shader, 0x1000, NULL, buf);
                    LOGE("%s %S Could not compile shader %d:\n%s\n",__FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            }
            glDeleteShader(shader);
            shader = 0;
        }
    } else {
        LOGE("glCreateShader error");
    }
    return shader;
}

bool VideoEditor::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGE("after %s() glError (0x%x)\n", op, error);
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
    checkGlError("glBindTexture2");

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texId, 0);
    checkGlError("glFramebufferTexture2D");

    glUseProgram(mCopierProgId);
    if (!mIsCopierInitialized) {
        LOGE("mIsCopierInitialized false");
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
        if (checkGlError("glBindTexture3")) {
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
//  LOGI("draw waste time is %ld", (getCurrentTime() - startDrawTimeMills));
}

void VideoEditor::processMessage() {
    bool renderingEnabled = true;
    while (renderingEnabled) {
        Message *msg = NULL;
        if (mMsgQueue->dequeueMessage(&msg, true) > 0) {
            if (MESSAGE_QUEUE_LOOP_QUIT_FLAG == msg->execute()) {
                renderingEnabled = false;
            }
            delete msg;
        }
    }
}

void* VideoEditor::threadStartCallback(void *self) {
    LOGI("startPreviewThread");
    VideoEditor* videoEditor = (VideoEditor*)self;
    videoEditor->processMessage();
    pthread_exit(0);
    return 0;
}

void* VideoEditor::startDecoderThread(void *ptr) {
    VideoEditor* videoEditor = (VideoEditor*)ptr;
    videoEditor->initCircleQueue(videoEditor->mWidth, videoEditor->mHeight);
    int ret = videoEditor->initEGL();
    if (false == ret) {
        LOGE("initEGL failed");
        return 0;
    }
    videoEditor->initConverter();
    videoEditor->initCopier();
    videoEditor->initPassRender();

    while (videoEditor->isOnDecoding) {
        videoEditor->decode();
    }
    videoEditor->releaseEGL();
    return 0;
}

int VideoEditor::videoCallbackGetTex(FrameTexture **frameTex, void *ctx, bool forceGetFrame) {
    int ret = -1;
    VideoEditor* videoEditor = (VideoEditor*) ctx;
    if (!videoEditor->isDestroyed) {
        if (videoEditor->isCompleted) {
            //do nothing
        } else {
            (*frameTex) = videoEditor->getCorrectRenderTexture(forceGetFrame);
        }
        ret = 0;
    }
    return ret;
}

int VideoEditor::audioCallbackFillData(byte *buf, size_t bufSize, void *ctx) {
    VideoEditor* videoEditor = (VideoEditor*) ctx;
    return videoEditor->consumeAudioFrames(buf, bufSize);
}

AVMessageQueue* VideoEditor::getMessageQueue() {
    return mCbMsgQueue;
}



