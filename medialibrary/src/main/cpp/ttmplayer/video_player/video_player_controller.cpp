#include "video_player_controller.h"

#define LOG_TAG "MediaPlayer"
/*
 * class MediaPlayer
 *
 */
MediaPlayer::MediaPlayer() {
    userCancelled = false;
    isEncoding = false;

    videoOutput = NULL;
    audioOutput = NULL;
    synchronizer = NULL;
    messageQueue = NULL;

    screenWidth = 0;
    screenHeight = 0;
    messageQueue = new AVMessageQueue();
    if (messageQueue == nullptr) {
        LOGE("messageQueue create failed");
    }
}

MediaPlayer::~MediaPlayer() {
    audioOutput = NULL;
    synchronizer = NULL;

    if(messageQueue) {
        messageQueue->release();
        delete messageQueue;
        messageQueue = nullptr;
    }
}

void MediaPlayer::signalOutputFrameAvailable() {
//  LOGI("signalOutputFrameAvailable");
    if (NULL != videoOutput){
        videoOutput->signalFrameAvailable();
    }
}

bool MediaPlayer::initAVSynchronizer() {
    synchronizer = new AVSynchronizer();
    return synchronizer->init(requestHeader, g_jvm, obj, messageQueue, minBufferedDuration, maxBufferedDuration);
}

void  MediaPlayer::initVideoOutput(ANativeWindow* window){
    LOGI("MediaPlayer::initVideoOutput beigin width:%d, height:%d", screenWidth, screenHeight);
    if (window == NULL || userCancelled){
        return;
    }
    videoOutput = new VideoOutput();
    videoOutput->initOutput(window, screenWidth, screenHeight,videoCallbackGetTex, this);
}

bool MediaPlayer::startAVSynchronizer() {
    LOGI("enter MediaPlayer::startAVSynchronizer...");
    bool ret = false;

    if (userCancelled) {
        return ret;
    }

    if (this->initAVSynchronizer()) {
        if (synchronizer->validAudio()) {
            ret = this->initAudioOutput();
        }
    }
    if(ret){
        if (NULL != synchronizer && !synchronizer->isValid()) {
            ret = false;
        } else{
            synchronizer->start();
        }
    }

    LOGI("MediaPlayer::startAVSynchronizer() init result:%s", (ret? "success" : "fail"));
    // 准备完成回调
    if (messageQueue) {
        messageQueue->postMessage(MSG_PREPARED);
    }
    return ret;
}

int MediaPlayer::videoCallbackGetTex(FrameTexture** frameTex, void* ctx, bool forceGetFrame){
    MediaPlayer* playerController = (MediaPlayer*) ctx;
    return playerController->getCorrectRenderTexture(frameTex, forceGetFrame);
}

int MediaPlayer::getCorrectRenderTexture(FrameTexture** frameTex, bool forceGetFrame){
    int ret = -1;

    if (!synchronizer->isDestroyed) {
        if(synchronizer->isPlayCompleted()) {
            LOGI("Video Render Thread render Completed We will Render First Frame...");
            (*frameTex) = synchronizer->getFirstRenderTexture();
        } else {
            (*frameTex) = synchronizer->getCorrectRenderTexture(forceGetFrame);
        }
        ret = 0;
    }
    return ret;
}

void MediaPlayer::onSurfaceCreated(ANativeWindow* window, int width, int height) {
    LOGI("enter MediaPlayer::onSurfaceCreated...");

    if (window != NULL){
        this->window = window;
    }

    if (userCancelled){
        return;
    }

    if (width > 0 && height > 0){
        this->screenHeight = height;
        this->screenWidth = width;
    }
    if (!videoOutput) {
        initVideoOutput(window);
    }else{
        videoOutput->onSurfaceCreated(window);
    }
    LOGI("Leave MediaPlayer::onSurfaceCreated...");
}

void MediaPlayer::onSurfaceDestroyed() {
    LOGI("enter MediaPlayer::onSurfaceDestroyed...");
    if (videoOutput) {
        videoOutput->onSurfaceDestroyed();
    }
}

int MediaPlayer::audioCallbackFillData(byte* outData, size_t bufferSize, void* ctx) {
    MediaPlayer* playerController = (MediaPlayer*) ctx;
    return playerController->consumeAudioFrames(outData, bufferSize);
}

status_t MediaPlayer::prepare(char *srcFilenameParam, int* max_analyze_duration, int analyzeCnt, int probesize, bool fpsProbeSizeConfigured,
                              float minBufferedDuration, float maxBufferedDuration){
    mIsPlaying = false;
    synchronizer = NULL;
    audioOutput = NULL;
    videoOutput = NULL;

    requestHeader = new DecoderRequestHeader();
    requestHeader->init(srcFilenameParam, max_analyze_duration, analyzeCnt, probesize, fpsProbeSizeConfigured);
    //this->g_jvm = g_jvm;
    //this->obj = obj;
    this->minBufferedDuration = minBufferedDuration;
    this->maxBufferedDuration = maxBufferedDuration;

    pthread_create(&initThreadThreadId, 0, initThreadCallback, this);
    userCancelled = false;
    return NO_ERROR;
}

int MediaPlayer::getAudioChannels() {
    int channels = -1;
    if (NULL != synchronizer) {
        channels = synchronizer->getAudioChannels();
    }
    return channels;
}

bool MediaPlayer::initAudioOutput() {
    LOGI("MediaPlayer::initAudioOutput");

    int channels = this->getAudioChannels();
    if (channels < 0) {
        LOGI("VideoDecoder get channels failed ...");
        return false;
    }
    int sampleRate = synchronizer->getAudioSampleRate();
    if (sampleRate < 0) {
        LOGI("VideoDecoder get sampleRate failed ...");
        return false;
    }
    audioOutput = new AudioOutput();
    SLresult result = audioOutput->initSoundTrack(channels, sampleRate, audioCallbackFillData, this);
    if (SL_RESULT_SUCCESS != result) {
        LOGI("audio manager failed on initialized...");
        delete audioOutput;
        audioOutput = NULL;
        return false;
    }
    return true;
}

void MediaPlayer::start() {
    LOGI("MediaPlayer::start %d ", (int)mIsPlaying);
    if (this->mIsPlaying)
        return;
    mIsPlaying = true;
    LOGI("call audioOutput start...");
    if (NULL != audioOutput) {
        audioOutput->start();
    }
    LOGI("After call audioOutput start...");
}

void MediaPlayer::pause() {
    LOGI("MediaPlayer::pause");
    if (!this->mIsPlaying)
        return;
    this->mIsPlaying = false;
    if (NULL != audioOutput) {
        audioOutput->pause();
    }
}

void MediaPlayer::resume() {
    LOGI("MediaPlayer::resume %d ", (int)mIsPlaying);
    if (this->mIsPlaying)
        return;
    this->mIsPlaying = true;
    if (NULL != audioOutput) {
        audioOutput->play();
    }
}

int MediaPlayer::isPlaying() {
    return mIsPlaying;
}

int MediaPlayer::isLooping() {

}

void MediaPlayer::resetRenderSize(int left, int top, int width, int height) {
    LOGI("MediaPlayer::resetRenderSize");
    if (NULL != videoOutput) {
        LOGI("MediaPlayer::resetRenderSize NULL != videoOutput width:%d, height:%d", width, height);
        videoOutput->resetRenderSize(left, top, width, height);
    } else {
        LOGI("MediaPlayer::resetRenderSize NULL == videoOutput width:%d, height:%d", width, height);
        screenWidth = width;
        screenHeight = height;
    }
}

int MediaPlayer::consumeAudioFrames(byte* outData, size_t bufferSize) {
    int ret = bufferSize;
    if(this->mIsPlaying &&
       synchronizer && !synchronizer->isDestroyed && !synchronizer->isPlayCompleted()) {
//      LOGI("Before synchronizer fillAudioData...");
        ret = synchronizer->fillAudioData(outData, bufferSize);
//      LOGI("After synchronizer fillAudioData... ");
        signalOutputFrameAvailable();
        if (messageQueue) {
            messageQueue->postMessage(MSG_CURRENT_POSITON, getCurrentPosition() * 1000, getDuration() * 1000);//ms
        }
    } else {
        //LOGI("MediaPlayer::consumeAudioFrames set 0");
        memset(outData, 0, bufferSize);
    }
    return ret;
}

float MediaPlayer::getDuration() {
    if (NULL != synchronizer) {
        return synchronizer->getDuration();
    }
    return 0.0f;
}

int MediaPlayer::getVideoFrameWidth() {
    if (NULL != synchronizer) {
        return synchronizer->getVideoFrameWidth();
    }
    return 0;
}

int MediaPlayer::getVideoFrameHeight() {
    if (NULL != synchronizer) {
        return synchronizer->getVideoFrameHeight();
    }
    return 0;
}

float MediaPlayer::getVideoFPS() {
    float fps = 0.0f;
    if (synchronizer) {
        fps = synchronizer->getVideoFPS();
    }
    return fps;
}

int64_t MediaPlayer::getVideoBitrate() {
    int64_t videoBitrate = 0;
    if (synchronizer) {
        videoBitrate = synchronizer->getVideoBitrate();
    }
    return videoBitrate;
}

int MediaPlayer::getAudioSampleRate() {
    int audioSampleRate = -1;
    if (NULL != synchronizer) {
        audioSampleRate = synchronizer->getAudioSampleRate();
    }
    return audioSampleRate;
}

int64_t MediaPlayer::getAudioBitrate() {
    int64_t audioBitrate = -1;
    if (NULL != synchronizer) {
        audioBitrate = synchronizer->getAudioBitrate();
    }
    return audioBitrate;
}

float MediaPlayer::getBufferedProgress() {
    if (NULL != synchronizer) {
        return synchronizer->getBufferedProgress();
    }
    return 0.0f;
}

float MediaPlayer::getCurrentPosition() {
    if (NULL != synchronizer) {
        return synchronizer->getPlayProgress();
    }
    return 0.0f;
}

void MediaPlayer::seekTo(float position) {
    LOGI("enter MediaPlayer::seekToPosition...");
    if (NULL != synchronizer) {
        return synchronizer->seekToPosition(position);
    }
}

void MediaPlayer::destroy() {
    LOGI("enter MediaPlayer::destroy...");

    userCancelled = true;

    if (synchronizer){
        //中断request
        synchronizer->interruptRequest();
    }

    pthread_join(initThreadThreadId, 0);

    if (NULL != videoOutput) {
        videoOutput->stopOutput();
        delete videoOutput;
        videoOutput = NULL;
    }

    if (NULL != synchronizer) {
        synchronizer->isDestroyed = true;
        this->pause();
        LOGI("stop synchronizer ...");
        synchronizer->destroy();

        LOGI("stop audioOutput ...");
        if (NULL != audioOutput) {
            audioOutput->stop();
            delete audioOutput;
            audioOutput = NULL;
        }
        synchronizer->clearFrameMeta();
        delete synchronizer;
        synchronizer = NULL;
    }
    if(NULL != requestHeader){
        requestHeader->destroy();
        delete requestHeader;
        requestHeader = NULL;
    }

    LOGI("leave MediaPlayer::destroy...");
}

void MediaPlayer::changeFilter(int type, const int id) {

}

void MediaPlayer::changeFilter(int type, const char *name) {

}

void MediaPlayer::beginFilter(int type, const char *name) {
    if (synchronizer) {
        synchronizer->beginFilter(type, name);
    }
}

void MediaPlayer::endFilter(int type, const char *name) {
    if (synchronizer) {
        synchronizer->endFilter(type, name);
    }
}

void MediaPlayer::startEncoding(int width, int height, int videoBitRate, int frameRate,
                                int useHardWareEncoding, int strategy) {
    LOGI("MediaPlayer::startEncoding");
    if (isEncoding) {
        LOGE("is Encoding, do nothing");
    }
    //seekTo(0);
    //start consumer
    LiveCommonPacketPool::GetInstance()->initRecordingVideoPacketQueue();
    LiveCommonPacketPool::GetInstance()->initAudioPacketQueue(getAudioSampleRate());
    LiveAudioPacketPool::GetInstance()->initAudioPacketQueue();

    videoConsumer = new VideoPacketConsumerThread();
    if (videoConsumer) {
        std::map<std::string, int> configMap;
        configMap["adaptiveBitrateWindowSizeInSecs"] = 3;
        configMap["adaptiveBitrateEncoderReconfigInterval"] = 15 * 1000;
        configMap["adaptiveBitrateWarCntThreshold"] = 10;
        configMap["adaptiveMinimumBitrate"] = 300;
        configMap["adaptiveMaximumBitrate"] = 800;

        int ret = videoConsumer->init("/mnt/sdcard/a_songstudio/recording.flv", getVideoFrameWidth(), getVideoFrameHeight(), getVideoFPS(), getVideoBitrate(),
                getAudioSampleRate(), getAudioChannels(), getAudioBitrate(), "libfdk_aac", 0, configMap, NULL, NULL);
        if(ret >= 0){
            videoConsumer->startAsync();
        } else{
            LiveCommonPacketPool::GetInstance()->destoryRecordingVideoPacketQueue();
            LiveCommonPacketPool::GetInstance()->destoryAudioPacketQueue();
            LiveAudioPacketPool::GetInstance()->destoryAudioPacketQueue();
        }
    } else {
        LOGE("videoConsumer create failed");
    }

    if (NULL != videoOutput) {
        videoOutput->startEncoding(getVideoFrameWidth(), getVideoFrameHeight(), getVideoBitrate(), getVideoFPS(), useHardWareEncoding, strategy);
    }
    if (NULL != synchronizer) {
        synchronizer->startEncoding();
    }
    isEncoding = true;
}

void MediaPlayer::stopEncoding() {
    LOGI("MediaPlayer::stopEncoding");
    if (!isEncoding) {
        LOGE("is not Encoding, do nothing");
    }

    if (NULL != videoOutput) {
        videoOutput->stopEncoding();
    }
    if (NULL != synchronizer) {
        synchronizer->stopEncoding();
    }
    if (videoConsumer) {
        videoConsumer->stop();
        delete videoConsumer;
        videoConsumer = NULL;
    }
}

void* MediaPlayer::initThreadCallback(void *myself){
    MediaPlayer *controller = (MediaPlayer*) myself;
    controller->startAVSynchronizer();
    pthread_exit(0);
    return 0;
}

EGLContext MediaPlayer::getUploaderEGLContext() {
    if (NULL != synchronizer) {
        return synchronizer->getUploaderEGLContext();
    }
    return NULL;
}

AVMessageQueue* MediaPlayer::getMessageQueue() {
    return messageQueue;
}