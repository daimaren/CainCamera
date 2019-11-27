//
// Created by 1 on 2019/11/27.
//

#ifndef CAINCAMERA_VIDEODECODER_H
#define CAINCAMERA_VIDEODECODER_H

class VideoDecoder {
public:
    bool validAudio(){ return audioStreamIndex != -1; };
protected:
    int openInput();
    virtual int openFormatInput(char *videoSourceURI);
    virtual void initFFMpegContext();
protected:
    JavaVM* mJvm;
    jobject mObj;

    pthread_mutex_t mLock;
    pthread_cond_t mCondition;

    AVFormatContext *mFormatCtx;

    /** 视频流解码变量 **/
    AVCodecContext *mVideoCodecCtx;
    AVCodec *mVideoCodec;
    AVFrame *mVideoFrame;

    /** 音频流解码变量 **/
    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;
    AVFrame *audioFrame;

    VideoDecoder* decoder;
    bool mIsInitDecode;

};


#endif //CAINCAMERA_VIDEODECODER_H
