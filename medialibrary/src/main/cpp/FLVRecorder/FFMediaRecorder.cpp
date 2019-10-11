//
// Created by CainHuang on 2019/8/17.
//

#include "FFMediaRecorder.h"
#include "aw_all.h"


FFMediaRecorder::FFMediaRecorder() : mRecordListener(nullptr), mAbortRequest(true),
                                     mStartRequest(false), mExit(true), mRecordThread(nullptr),
                                     mYuvConvertor(nullptr), mFrameQueue(nullptr){
    mRecordParams = new RecordParams();
}

FFMediaRecorder::~FFMediaRecorder() {
    release();
    if (mRecordParams != nullptr) {
        delete mRecordParams;
        mRecordParams = nullptr;
    }
}

/**
 * 设置录制监听器
 * @param listener
 */
void FFMediaRecorder::setOnRecordListener(OnRecordListener *listener) {
    mRecordListener = listener;
}

/**
 * 释放资源
 */
void FFMediaRecorder::release() {
    stopRecord();
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
    if (mFrameQueue != nullptr) {
        delete mFrameQueue;
        mFrameQueue = nullptr;
    }
    if (mRecordThread != nullptr) {
        delete mRecordThread;
        mRecordThread = nullptr;
    }
    aw_sw_encoder_close_faac_encoder();
    aw_sw_encoder_close_x264_encoder();
}

RecordParams* FFMediaRecorder::getRecordParams() {
    return mRecordParams;
}

/**
 * 初始化录制器
 * @param params
 * @return
 */
int FFMediaRecorder::prepare() {

    RecordParams *params = mRecordParams;
    if (params->rotateDegree % 90 != 0) {
        LOGE("invalid rotate degree: %d", params->rotateDegree);
        return -1;
    }

    int ret;

    mFrameQueue = new SafetyQueue<AVMediaData *>();

    LOGI("Record to file: %s, width: %d, height: %d", params->dstFile, params->width,
         params->height);

    // yuv转换
    int outputWidth = params->width;
    int outputHeight = params->height;

    // yuv转换器
    mYuvConvertor = new YuvConvertor();
    mYuvConvertor->setInputParams(params->width, params->height, params->pixelFormat);
    mYuvConvertor->setCrop(params->cropX, params->cropY, params->cropWidth, params->cropHeight);
    mYuvConvertor->setRotate(params->rotateDegree);
    mYuvConvertor->setScale(params->scaleWidth, params->scaleHeight);
    mYuvConvertor->setMirror(params->mirror);
    // 准备
    if (mYuvConvertor->prepare() < 0) {
        delete mYuvConvertor;
        mYuvConvertor = nullptr;
    } else {
        outputWidth = mYuvConvertor->getOutputWidth();
        outputHeight = mYuvConvertor->getOutputHeight();
        if (outputWidth == 0 || outputHeight == 0) {
            outputWidth = params->rotateDegree % 180 == 0 ? params->width : params->height;
            outputHeight = params->rotateDegree % 180 == 0 ? params->height : params->width;
        }
    }

    // 准备好A/V编码器, 复用器
    aw_faac_config faacConfig;
    faacConfig.sample_rate = params->sampleRate;
    faacConfig.bitrate = 100000;
    faacConfig.channel_count = params->channels;
    faacConfig.sample_size = params->sampleFormat;

    aw_sw_encoder_open_faac_encoder(&faacConfig);

    aw_x264_config *pX264_config = alloc_aw_x264_config();
    pX264_config->width = outputWidth;
    pX264_config->height = outputHeight;
    pX264_config->bitrate = 1000000;
    pX264_config->fps = params->frameRate;
    pX264_config->input_data_format = getX264PixelFormat((PixelFormat)params->pixelFormat);

    aw_sw_encoder_open_x264_encoder(pX264_config);

    // todo 打开文件 params->dstFile
    return 0;
}

/**
 * 录制一帧数据
 * @param data
 * @return
 */
int FFMediaRecorder::recordFrame(AVMediaData *data) {
    if (mAbortRequest || mExit) {
        LOGE("Recoder is not recording.");
        delete data;
        return -1;
    }

    // 录制的是音频数据并且不允许音频录制，则直接删除
    if (!mRecordParams->enableAudio && data->getType() == MediaAudio) {
        delete data;
        return -1;
    }

    // 录制的是视频数据并且不允许视频录制，直接删除
    if (!mRecordParams->enableVideo && data->getType() == MediaVideo) {
        delete data;
        return -1;
    }

    // 将媒体数据入队
    if (mFrameQueue != nullptr) {
        mFrameQueue->push(data);
    } else {
        delete data;
    }
    return 0;
}

/**
 * 开始录制
 */
void FFMediaRecorder::startRecord() {
    mMutex.lock();
    mAbortRequest = false;
    mStartRequest = true;
    mCondition.signal();
    mMutex.unlock();

    if (mRecordThread == nullptr) {
        mRecordThread = new Thread(this);
        mRecordThread->start();
        mRecordThread->detach();
    }
}

/**
 * 停止录制
 */
void FFMediaRecorder::stopRecord() {
    mMutex.lock();
    mAbortRequest = true;
    mCondition.signal();
    mMutex.unlock();
    if (mRecordThread != nullptr) {
        mRecordThread->join();
        delete mRecordThread;
        mRecordThread = nullptr;
    }
}

/**
 * 判断是否正在录制
 * @return
 */
bool FFMediaRecorder::isRecording() {
    bool recording = false;
    mMutex.lock();
    recording = !mAbortRequest && mStartRequest && !mExit;
    mMutex.unlock();
    return recording;
}

void FFMediaRecorder::run() {
    int ret = 0;
    int64_t start = 0;
    int64_t current = 0;
    mExit = false;

    // 录制回调监听器
    if (mRecordListener != nullptr) {
        mRecordListener->onRecordStart();
    }

    LOGD("waiting to start record");
    while (!mStartRequest) {
        if (mAbortRequest) { // 停止请求则直接退出
            break;
        } else { // 睡眠10毫秒继续
            av_usleep(10 * 1000);
        }
    }

    // 开始录制编码流程
    if (!mAbortRequest && mStartRequest) {
        LOGD("start record");
        // 正在运行，并等待frameQueue消耗完
        while (!mAbortRequest || !mFrameQueue->empty()) {
            if (!mFrameQueue->empty()) {

                // 从帧对列里面取出媒体数据
                auto data = mFrameQueue->pop();
                if (!data) {
                    continue;
                }
                if (start == 0) {
                    start = data->getPts();
                }
                if (data->getPts() >= current) {
                    current = data->getPts();
                }

                // yuv转码
                if (data->getType() == MediaVideo && mYuvConvertor != nullptr) {
                    // 将数据转换成Yuv数据，处理失败则开始处理下一帧
                    if (mYuvConvertor->convert(data) < 0) {
                        LOGE("Failed to convert video data to yuv420");
                        delete data;
                        continue;
                    }
                }
                if (data->getType() == MediaVideo) {
                    // x264编码
                    aw_flv_video_tag *video_tag = aw_sw_encoder_encode_x264_data((int8_t*)data->image, data->length, data->pts);
                    if (video_tag) {
                        saveFlvVideoTag(video_tag);
                    }
                } else if (data->getType() == MediaAudio) {
                    // faac编码
                    int timestamp = aw_sw_faac_encoder_max_input_sample_count() * 1000 / mRecordParams->sampleRate;
                    aw_flv_audio_tag *audio_tag = aw_sw_encoder_encode_faac_data((int8_t *)data->sample, data->sample_size, timestamp);
                    if (audio_tag) {
                        saveFlvAudioTag(audio_tag);
                    }
                }

                if (ret < 0) {
                    LOGE("Failed to encoder media data： %s", data->getName());
                } else {
                    LOGD("recording time: %f", (float)(current - start));
                    if (mRecordListener != nullptr) {
                        mRecordListener->onRecording((float)(current - start));
                    }
                }
                // 释放资源
                delete data;
            }
        }

        // 停止文件写入器
        ret = mMediaWriter->stop();
    }

    // 通知退出成功
    mExit = true;
    mCondition.signal();

    // 录制完成回调
    if (mRecordListener != nullptr) {
        mRecordListener->onRecordFinish(ret == 0, (float)(current - start));
    }
}

/**
 * Save Flv AudioTag
 */
void FFMediaRecorder::saveFlvAudioTag(aw_flv_audio_tag *audio_tag) {
    if(!audio_tag){
        LOGE("audio_tag null pointer");
        return;;
    }
    if(!isSpsPpsAndAudioSpecificConfigSent){
        sendSpsPpsAndAudioSpecificConfigTag();
        free_aw_flv_audio_tag(&audio_tag);
    } else{
        save_audio_data(audio_tag);
    }
}

/**
 * Save Flv VideoTag
 */
void FFMediaRecorder::saveFlvVideoTag(aw_flv_video_tag *video_tag) {

    if(!video_tag){
        LOGE("video_tag null pointer");
        return;;
    }
    if(!isSpsPpsAndAudioSpecificConfigSent){
        sendSpsPpsAndAudioSpecificConfigTag();
        free_aw_flv_video_tag(&video_tag);
    } else{
        save_video_data(video_tag);
    }
}

/**
 * 根据flv，h264，aac协议，提供首帧需要发送的tag
 */
void FFMediaRecorder::sendSpsPpsAndAudioSpecificConfigTag() {
    aw_flv_video_tag *spsPpsTag = NULL;
    aw_flv_audio_tag *audioSpecificConfigTag = NULL;
    if(isSpsPpsAndAudioSpecificConfigSent){
        LOGE("[AVCapture] already send sps pps or not capture");
        return;
    }
    //create video sps pps tag
    spsPpsTag = aw_sw_encoder_create_x264_sps_pps_tag();
    if(spsPpsTag) {
        saveFlvVideoTag(spsPpsTag);
        LOGD("send sps pps success");
    }else{
        LOGE("spsPpsTag null pointer");
    }

    //audio specific config tag
    audioSpecificConfigTag = aw_sw_encoder_create_faac_specific_config_tag();
    if(audioSpecificConfigTag){
        saveFlvAudioTag(audioSpecificConfigTag);
        LOGD("send audio Specific Tag success");
    }else{
        LOGE("audioSpecificConfigTag null pointer");
    }

    isSpsPpsAndAudioSpecificConfigSent = spsPpsTag || audioSpecificConfigTag;
    LOGD("is sps pps and audio sepcific config sent=%d", isSpsPpsAndAudioSpecificConfigSent);
}

void FFMediaRecorder::save_audio_data(aw_flv_audio_tag *audio_tag){
    aw_streamer_send_flv_tag_to_rtmp(&audio_tag->common_tag);
}

void FFMediaRecorder::save_video_data(aw_flv_video_tag *video_tag) {

}