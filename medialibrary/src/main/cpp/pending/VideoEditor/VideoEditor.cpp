#include "VideoEditor.h"

VideoEditor::VideoEditor() {
    mScreenWidth = 0;
    mScreenHeight = 0;
}

VideoEditor::~VideoEditor() {

}

bool VideoEditor::init(char *srcFilePath, JavaVM *jvm, jobject obj, bool isHWDecode) {
    mIsPlaying = false;
    mIsHWDecode = isHWDecode;
    mJvm = jvm;
    mObj = obj;
    pthread_create(&mThreadId, 0, threadCallback, this);
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

bool VideoEditor::startMediaSync() {
    ALOGI("startMediaSync");
    bool ret = false;

    if (mIsUserCancelled) {
        return true;
    }
    if (initMediaSync()) {
        if(mDecoder && !mIsDestroyed && decoder->validAudio()) {
            ret = initAudioOutput();
        }
    }
    return true;
}

bool VideoEditor::initMediaSync() {
    mMediaSync = new MediaSync();
    return mMediaSync->init(mJvm, mObj, mIsHWDecode);
}

void VideoEditor::initVideoOutput(ANativeWindow *window) {

}

bool VideoEditor::initAudioOutput() {

}

void VideoEditor::play() {

}

void VideoEditor::pause() {

}

void* VideoEditor::threadCallback(void *self) {
    VideoEditor* videoEditor = (VideoEditor*)self;
    videoEditor->startMediaSync();
    pthread_exit(0);
}

int VideoEditor::audioCallbackFillData(byte *buf, size_t bufSize, void *ctx) {

}





