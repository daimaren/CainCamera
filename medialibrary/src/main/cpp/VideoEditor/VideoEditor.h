#ifndef VIDEO_EDITOR_H
#define VIDEO_EDITOR_H

#include <pthread.h>
#include <queue>
#include <unistd.h>

#include "utils.h"
#include "VideoOutput.h"
#include "AudioOutput.h"
#include "MediaSync.h"

class VideoEditor {
public:
    VideoEditor();
    virtual ~VideoEditor();

    bool init(char *srcFilePath, JavaVM *jvm, jobject obj, bool isHWDecode);
    void play();
    void seek(float position);
    void pause();
    virtual void destroy();
    float getDuration();

    static int audioCallbackFillData(byte* buf, size_t bufSize, void* ctx);
    int consumeAudioFrames(byte* outData, size_t bufSize);
    static int videoCallbackGetTex();

    bool startMediaSync();
    bool initMediaSync();

    void onSurfaceCreated(ANativeWindow* window, int widht, int height);
    void onSurfaceDestroyed();

    void signalOutputFrameAvailable();

private:
    static void* threadCallback(void* self);
    void initVideoOutput(ANativeWindow* window);
private:
    ANativeWindow* mWindow;
    int mScreenWidth;
    int mScreenHeight;
    bool mIsPlaying;
    bool mIsHWDecode;
    //EglShareContext mSharedEGLCtx;

    JavaVM *mJvm;
    jobject mObj;

    MediaSync*  mMediaSync;
    VideoOutput* mVideoOutput;
    AudioOutput* mAudioOutput;

    bool mIsUserCancelled;
    pthread_t mThreadId;
};

#endif