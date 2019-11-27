#include <list>
#include "VideoEditor.h"

VideoEditor::VideoEditor()
    : isMediaCodecInit(false), inputBuffer(NULL){
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
    pthread_create(&mThreadId, 0, threadCB, this);
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
    //todo
}

void VideoEditor::onSurfaceDestroyed() {

}

bool VideoEditor::prepare_l() {
    ALOGI("prepare_l");
    bool ret = false;

    if (mIsUserCancelled) {
        return true;
    }
    //init ffmpeg
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
    std::list<int> *ma = new std::list<int>();
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (AVMEDIA_TYPE_VIDEO == pFormatCtx->streams[i]->codec->codec_type) {
            ma->push_back(i);
        }
    }
    pthread_create(&upThreadId, 0, upThreadCB, this);
    return true;
}

void VideoEditor::initVideoOutput(ANativeWindow *window) {

}

bool VideoEditor::initAudioOutput() {

}

void VideoEditor::play() {

}

void VideoEditor::pause() {

}

void* VideoEditor::threadCB(void *self) {
    VideoEditor* videoEditor = (VideoEditor*)self;
    videoEditor->prepare_l();
    pthread_exit(0);
}

void* VideoEditor::upThreadCB(void *myself) {

}

int VideoEditor::audioCallbackFillData(byte *buf, size_t bufSize, void *ctx) {

}





