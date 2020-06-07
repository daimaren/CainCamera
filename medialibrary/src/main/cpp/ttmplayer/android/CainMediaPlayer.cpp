//
// Created by cain on 2019/2/1.
//

#include "CainMediaPlayer.h"

CainMediaPlayer::CainMediaPlayer() {
    msgThread = nullptr;
    abortRequest = true;
    mediaPlayer = nullptr;
    mListener = nullptr;
    mPrepareSync = false;
    mPrepareStatus = NO_ERROR;
    mAudioSessionId = 0;
    mSeeking = false;
    mSeekingPosition = 0;
}

CainMediaPlayer::~CainMediaPlayer() {

}

void CainMediaPlayer::init() {
    if (mediaPlayer == nullptr) {
        mediaPlayer = new MediaPlayer();
    }
    mMutex.lock();
    abortRequest = false;
    mCondition.signal();
    mMutex.unlock();

    mMutex.lock();

    if (msgThread == nullptr) {
        msgThread = new CainThread(this);
        msgThread->start();
    }
    mMutex.unlock();
}

void CainMediaPlayer::disconnect() {

    mMutex.lock();
    abortRequest = true;
    mCondition.signal();
    mMutex.unlock();

    reset();

    if (msgThread != nullptr) {
        msgThread->join();
        delete msgThread;
        msgThread = nullptr;
    }

    if (mediaPlayer != nullptr) {
        delete mediaPlayer;
        mediaPlayer = nullptr;
    }
    if (mListener != nullptr) {
        delete mListener;
        mListener = nullptr;
    }

}

status_t CainMediaPlayer::setDataSource(const char *url, int64_t offset, const char *headers) {
    if (url == nullptr) {
        return BAD_VALUE;
    }
    mUrl = av_strdup(url);
    return NO_ERROR;
}

status_t CainMediaPlayer::setMetadataFilter(char **allow, char **block) {
    // do nothing
    return NO_ERROR;
}

status_t
CainMediaPlayer::getMetadata(bool update_only, bool apply_filter, AVDictionary **metadata) {
    if (mediaPlayer != nullptr) {
        //return mediaPlayer->getMetadata(metadata);
    }
    return NO_ERROR;
}

status_t CainMediaPlayer::setVideoSurface(ANativeWindow *native_window) {
    if (mediaPlayer == nullptr) {
        return NO_INIT;
    }
    if (native_window != nullptr) {
        mediaPlayer->onSurfaceCreated(native_window, ANativeWindow_getWidth(native_window), ANativeWindow_getHeight(native_window));
        return NO_ERROR;
    }
    return NO_ERROR;
}

status_t CainMediaPlayer::setListener(MediaPlayerListener *listener) {
    if (mListener != nullptr) {
        delete mListener;
        mListener = nullptr;
    }
    mListener = listener;
    return NO_ERROR;
}

status_t CainMediaPlayer::prepare() {
    //////LOGI("CainMediaPlayer::prepare");
    if (mediaPlayer == nullptr) {
        return NO_INIT;
    }
    if (mPrepareSync) {
        return -EALREADY;
    }
    if (mUrl == nullptr) {
        return NO_INIT;
    }
    mPrepareSync = true;

    int max_analyze_duration[] = {-1, -1, -1};
    int cnt = 3;
    status_t ret = mediaPlayer->prepare(mUrl, max_analyze_duration, cnt, -1, true, 0.5f, 0.5f);
    if (ret != NO_ERROR) {
        return ret;
    }
    if (mPrepareSync) {
        mPrepareSync = false;
    }
    return mPrepareStatus;
}

status_t CainMediaPlayer::prepareAsync() {
    if (mediaPlayer != nullptr) {
        //return mediaPlayer->prepareAsync();
    }
    return INVALID_OPERATION;
}

void CainMediaPlayer::start() {
    if (mediaPlayer != nullptr) {
        mediaPlayer->start();
    }
}

void CainMediaPlayer::stop() {
    if (mediaPlayer) {
        //mediaPlayer->stop();
    }
}

void CainMediaPlayer::pause() {
    if (mediaPlayer) {
        mediaPlayer->pause();
    }
}

void CainMediaPlayer::resume() {
    if (mediaPlayer) {
        mediaPlayer->resume();
    }
}

bool CainMediaPlayer::isPlaying() {
    if (mediaPlayer) {
        return mediaPlayer->isPlaying();
    }
    return false;
}

int CainMediaPlayer::getRotate() {
    if (mediaPlayer != nullptr) {
        //return mediaPlayer->getRotate();
    }
    return 0;
}

int CainMediaPlayer::getVideoWidth() {
    if (mediaPlayer != nullptr) {
       //return mediaPlayer->getVideoWidth();
    }
    return 0;
}

int CainMediaPlayer::getVideoHeight() {
    if (mediaPlayer != nullptr) {
        //return mediaPlayer->getVideoHeight();
    }
    return 0;
}

status_t CainMediaPlayer::seekTo(float msec) {
    if (mediaPlayer != nullptr) {
        // if in seeking state, put seek message in queue, to process after preview seeking.
        if (mSeeking) {
            mediaPlayer->getMessageQueue()->postMessage(MSG_REQUEST_SEEK, msec);
        } else {
            mediaPlayer->seekTo(msec * 1000);
            mSeekingPosition = (long)msec;
            mSeeking = true;
        }
    }
    return NO_ERROR;
}

long CainMediaPlayer::getCurrentPosition() {
    if (mediaPlayer != nullptr) {
        if (mSeeking) {
            return mSeekingPosition;
        }
        return mediaPlayer->getCurrentPosition();
    }
    return 0;
}

long CainMediaPlayer::getDuration() {
    if (mediaPlayer != nullptr) {
        return mediaPlayer->getDuration();
    }
    return -1;
}

status_t CainMediaPlayer::reset() {
    mPrepareSync = false;
    if (mediaPlayer != nullptr) {
        //mediaPlayer->reset();
        delete mediaPlayer;
        mediaPlayer = nullptr;
    }
    return NO_ERROR;
}

status_t CainMediaPlayer::setAudioStreamType(int type) {
    // TODO setAudioStreamType
    return NO_ERROR;
}

status_t CainMediaPlayer::setLooping(bool looping) {
    if (mediaPlayer != nullptr) {
        //mediaPlayer->setLooping(looping);
    }
    return NO_ERROR;
}

bool CainMediaPlayer::isLooping() {
    if (mediaPlayer != nullptr) {
        //return (mediaPlayer->isLooping() != 0);
    }
    return false;
}

status_t CainMediaPlayer::setVolume(float leftVolume, float rightVolume) {
    if (mediaPlayer != nullptr) {
        //mediaPlayer->setVolume(leftVolume, rightVolume);
    }
    return NO_ERROR;
}

void CainMediaPlayer::setMute(bool mute) {
    if (mediaPlayer != nullptr) {
        //mediaPlayer->setMute(mute);
    }
}

void CainMediaPlayer::setRate(float speed) {
    if (mediaPlayer != nullptr) {
        //mediaPlayer->setRate(speed);
    }
}

void CainMediaPlayer::setPitch(float pitch) {
    if (mediaPlayer != nullptr) {
        //mediaPlayer->setPitch(pitch);
    }
}

status_t CainMediaPlayer::setAudioSessionId(int sessionId) {
    if (sessionId < 0) {
        return BAD_VALUE;
    }
    mAudioSessionId = sessionId;
    return NO_ERROR;
}

int CainMediaPlayer::getAudioSessionId() {
    return mAudioSessionId;
}

void CainMediaPlayer::changeFilter(int type, const char *name) {

}

void CainMediaPlayer::changeFilter(int type, const int id) {

}

void CainMediaPlayer::beginFilter(int type, const char *name) {
    LOGD("beginFilter");
    if (mediaPlayer) {
        mediaPlayer->beginFilter(type, name);
    }
}

void CainMediaPlayer::endFilter(int type, const char *name) {
    LOGD("endFilter");
    if (mediaPlayer) {
        mediaPlayer->endFilter(type, name);
    }
}

void CainMediaPlayer::startEncoding(int width, int height, int videoBitRate, int frameRate,
                                    int useHardWareEncoding, int strategy) {
    LOGD("startEncoding");
    if (mediaPlayer) {
        mediaPlayer->startEncoding(width, height, videoBitRate, frameRate, useHardWareEncoding, strategy);
    }
}

void CainMediaPlayer::stopEncoding() {
    LOGD("stopEncoding");
    if (mediaPlayer) {
        mediaPlayer->stopEncoding();
    }
}

void CainMediaPlayer::setOption(int category, const char *type, const char *option) {

}

void CainMediaPlayer::setOption(int category, const char *type, int64_t option) {

}

void CainMediaPlayer::notify(int msg, int ext1, int ext2, void *obj, int len) {
    if (mediaPlayer != nullptr) {
        mediaPlayer->getMessageQueue()->postMessage(msg, ext1, ext2, obj, len);
    }
}

void CainMediaPlayer::postEvent(int what, int arg1, int arg2, void *obj) {
    if (mListener != nullptr) {
        mListener->notify(what, arg1, arg2, obj);
    }
}

void CainMediaPlayer::run() {
    LOGD("CainMediaPlayer::run msg thread");
    int retval;
    while (true) {

        if (abortRequest) {
            break;
        }

        // 如果此时播放器还没准备好，则睡眠10毫秒，等待播放器初始化
        if (!mediaPlayer || !mediaPlayer->getMessageQueue()) {
            av_usleep(10 * 1000);
            continue;
        }

        AVMessage msg;
        retval = mediaPlayer->getMessageQueue()->getMessage(&msg);
        if (retval < 0) {
            LOGD("getMessage error");
            break;
        }

        assert(retval > 0);

        switch (msg.what) {
            case MSG_FLUSH: {
                LOGD("CainMediaPlayer is flushing.\n");
                postEvent(MEDIA_NOP, 0, 0);
                break;
            }

            case MSG_ERROR: {
                LOGD("CainMediaPlayer occurs error: %d\n", msg.arg1);
                if (mPrepareSync) {
                    mPrepareSync = false;
                    mPrepareStatus = msg.arg1;
                }
                postEvent(MEDIA_ERROR, msg.arg1, 0);
                break;
            }

            case MSG_PREPARED: {
                LOGD("CainMediaPlayer is prepared.\n");
                if (mPrepareSync) {
                    mPrepareSync = false;
                    mPrepareStatus = NO_ERROR;
                }
                postEvent(MEDIA_PREPARED, 0, 0);
                break;
            }

            case MSG_STARTED: {
                LOGD("CainMediaPlayer is started!");
                postEvent(MEDIA_STARTED, 0, 0);
                break;
            }

            case MSG_COMPLETED: {
                LOGD("CainMediaPlayer is playback completed.\n");
                mediaPlayer->stopEncoding();
                postEvent(MEDIA_PLAYBACK_COMPLETE, 0, 0);
                break;
            }

            case MSG_VIDEO_SIZE_CHANGED: {
                LOGD("CainMediaPlayer is video size changing: %d, %d\n", msg.arg1, msg.arg2);
                postEvent(MEDIA_SET_VIDEO_SIZE, msg.arg1, msg.arg2);
                break;
            }

            case MSG_SAR_CHANGED: {
                LOGD("CainMediaPlayer is sar changing: %d, %d\n", msg.arg1, msg.arg2);
                postEvent(MEDIA_SET_VIDEO_SAR, msg.arg1, msg.arg2);
                break;
            }

            case MSG_VIDEO_RENDERING_START: {
                LOGD("CainMediaPlayer is video playing.\n");
                break;
            }

            case MSG_AUDIO_RENDERING_START: {
                LOGD("CainMediaPlayer is audio playing.\n");
                break;
            }

            case MSG_VIDEO_ROTATION_CHANGED: {
                LOGD("CainMediaPlayer's video rotation is changing: %d\n", msg.arg1);
                break;
            }

            case MSG_AUDIO_START: {
                LOGD("CainMediaPlayer starts audio decoder.\n");
                break;
            }

            case MSG_VIDEO_START: {
                LOGD("CainMediaPlayer starts video decoder.\n");
                break;
            }

            case MSG_OPEN_INPUT: {
                LOGD("CainMediaPlayer is opening input file.\n");
                break;
            }

            case MSG_FIND_STREAM_INFO: {
                LOGD("CanMediaPlayer is finding media stream info.\n");
                break;
            }

            case MSG_BUFFERING_START: {
                LOGD("CanMediaPlayer is buffering start.\n");
                postEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_START, msg.arg1);
                break;
            }

            case MSG_BUFFERING_END: {
                LOGD("CainMediaPlayer is buffering finish.\n");
                postEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END, msg.arg1);
                break;
            }

            case MSG_BUFFERING_UPDATE: {
                LOGD("CainMediaPlayer is buffering: %d, %d", msg.arg1, msg.arg2);
                postEvent(MEDIA_BUFFERING_UPDATE, msg.arg1, msg.arg2);
                break;
            }

            case MSG_BUFFERING_TIME_UPDATE: {
                LOGD("CainMediaPlayer time text update");
                break;
            }

            case MSG_SEEK_COMPLETE: {
                LOGD("CainMediaPlayer seeks completed!\n");
                mSeeking = false;
                postEvent(MEDIA_SEEK_COMPLETE, 0, 0);
                break;
            }

            case MSG_PLAYBACK_STATE_CHANGED: {
                LOGD("CainMediaPlayer's playback state is changed.");
                break;
            }

            case MSG_TIMED_TEXT: {
                LOGD("CainMediaPlayer is updating time text");
                postEvent(MEDIA_TIMED_TEXT, 0, 0, msg.obj);
                break;
            }

            case MSG_REQUEST_PREPARE: {
                LOGD("CainMediaPlayer is preparing...");
                break;
            }

            case MSG_REQUEST_START: {
                LOGD("CainMediaPlayer is waiting to start.");
                break;
            }

            case MSG_REQUEST_PAUSE: {
                LOGD("CainMediaPlayer is pausing...");
                pause();
                break;
            }

            case MSG_REQUEST_SEEK: {
                LOGD("CainMediaPlayer is seeking...");
                mSeeking = true;
                mSeekingPosition = (long)msg.arg1;
                if (mediaPlayer != nullptr) {
                    mediaPlayer->seekTo(mSeekingPosition * 1000);
                    //mediaPlayer->seekTo(mSeekingPosition);
                }
                break;
            }

            case MSG_CURRENT_POSITON: {
                postEvent(MEDIA_CURRENT, msg.arg1, msg.arg2);
                break;
            }
            case MSG_FRAME_AVAILABLE: {
                if (mediaPlayer) {
                    mediaPlayer->signalOutputFrameAvailable();
                }
                break;
            }
            default: {
                LOGD("CainMediaPlayer unknown MSG_xxx(%d)\n", msg.what);
                break;
            }
        }
        message_free_resouce(&msg);
    }
}
