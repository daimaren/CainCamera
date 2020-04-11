package com.timeapp.shawn.recorder.pro;

import android.os.Build;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Log;

import com.timeapp.shawn.recorder.pro.audioeffect.AudioEffect;
import com.timeapp.shawn.recorder.pro.audioeffect.AudioEffectFilterType;
import com.timeapp.shawn.recorder.pro.recording.RecordingImplType;
import com.timeapp.shawn.recorder.pro.recording.camera.preview.PreviewScheduler;
import com.timeapp.shawn.recorder.pro.recording.exception.InitPlayerFailException;
import com.timeapp.shawn.recorder.pro.recording.exception.InitRecorderFailException;
import com.timeapp.shawn.recorder.pro.recording.exception.RecordingStudioException;
import com.timeapp.shawn.recorder.pro.recording.exception.RecordingStudioNullArgumentException;
import com.timeapp.shawn.recorder.pro.recording.exception.StartRecordingException;
import com.timeapp.shawn.recorder.pro.recording.service.PlayerService;
import com.timeapp.shawn.recorder.pro.recording.service.factory.PlayerServiceFactory;
import com.timeapp.shawn.recorder.pro.recording.service.impl.AudioRecordRecorderServiceImpl;
import com.timeapp.shawn.recorder.pro.recording.video.VideoRecordingStudio;
import com.timeapp.shawn.recorder.pro.recording.video.service.MediaRecorderService;
import com.timeapp.shawn.recorder.pro.recording.video.service.factory.MediaRecorderServiceFactory;

import java.util.ArrayList;

/**
 * 自研移动端录制框架
 */
public final class MediaRecorder {
    private static final String TAG = "MediaRecorder";

    public static final int VIDEO_FRAME_RATE = 24;
    public static final int COMMON_VIDEO_BIT_RATE = 520 * 1000;
    public static int initializeVideoBitRate = COMMON_VIDEO_BIT_RATE;
    protected static int SAMPLE_RATE_IN_HZ = 44100;

    private long handle;
    private String dstPath;

    public static final int audioChannels = 2;
    public static final int audioBitRate = 64 * 1000;
    //伴奏播放
    protected PlayerService playerService = null;
    protected MediaRecorderService recorderService = null;
    // 输出video的路径
    protected RecordingImplType recordingImplType;
    protected Handler mTimeHandler;
    protected PlayerService.OnCompletionListener onComletionListener;

    private MediaRecorder() {

    }

    public MediaRecorder(RecordingImplType recordingImplType, Handler mTimeHandler,
                          PlayerService.OnCompletionListener onComletionListener) {
        this.recordingImplType = recordingImplType;
        this.onComletionListener = onComletionListener;
        this.mTimeHandler = mTimeHandler;
        handle = nativeInit();
    }

    public void initRecordingResource(PreviewScheduler scheduler, AudioEffect audioEffect) throws RecordingStudioException {
        /**
         * 这里一定要注意顺序，先初始化record在初始化player，因为player中要用到recorder中的samplerateSize
         **/
        if (scheduler == null) {
            throw new RecordingStudioNullArgumentException("null argument exception in initRecordingResource");
        }

        scheduler.resetStopState();
        recorderService = MediaRecorderServiceFactory.getInstance().getRecorderService(scheduler, recordingImplType);
        if (recorderService != null) {
            recorderService.initMetaData();
        }
        if (recorderService != null && !recorderService.initMediaRecorderProcessor(audioEffect)) {
            throw new InitRecorderFailException();
        }
        // 初始化伴奏带额播放器 实例化以及init播放器
        playerService = PlayerServiceFactory.getInstance().getPlayerService(onComletionListener,
                RecordingImplType.ANDROID_PLATFORM, mTimeHandler);
        if (playerService != null) {
            boolean result = playerService.setAudioDataSource(AudioRecordRecorderServiceImpl.SAMPLE_RATE_IN_HZ);
            if (!result) {
                throw new InitPlayerFailException();
            }
        }
    }

    public void destroyRecordingResource() {
        // 销毁伴奏播放器
        if (playerService != null) {
            playerService.stop();
            playerService = null;
        }
        if (recorderService != null) {
            recorderService.stop();
            recorderService.destoryMediaRecorderProcessor();
            recorderService = null;
        }
    }

    // here define use MediaCodec or not
    public void startVideoRecording(final String outputPath, final int bitRate, final int videoWidth, final int videoHeight,
                                    final int audioSampleRate,
                                    final int qualityStrategy, final int adaptiveBitrateWindowSizeInSecs, final int adaptiveBitrateEncoderReconfigInterval,
                                    final int adaptiveBitrateWarCntThreshold, final int adaptiveMinimumBitrate,
                                    final int adaptiveMaximumBitrate, final boolean useHardWareEncoding) {
        //设置初始编码率
        if (bitRate > 0){
            initializeVideoBitRate = bitRate*1000;
            Log.i("problem", "initializeVideoBitRate:" + initializeVideoBitRate + ",useHardWareEncoding:" + useHardWareEncoding);
        }

        new Thread() {
            public void run() {
                try {
                    //这里面不应该是根据Producer是否选用硬件编码器再去看Consumer端, 这里应该对于Consumer端是透明的
                    int ret = startConsumer(outputPath, videoWidth, videoHeight, audioSampleRate,
                            qualityStrategy,adaptiveBitrateWindowSizeInSecs,
                            adaptiveBitrateEncoderReconfigInterval, adaptiveBitrateWarCntThreshold,adaptiveMinimumBitrate, adaptiveMaximumBitrate);
                    if (ret < 0) {
                        destroyRecordingResource();
                    } else {
                        Videostudio.getInstance().connectSuccess();
                        startProducer(videoWidth, videoHeight, useHardWareEncoding, qualityStrategy);
                    }
                } catch (StartRecordingException exception) {
                    //启动录音失败，需要把资源销毁，并且把消费者线程停止掉
                    stopRecording();
                }
            }
        }.start();
    }

    protected int startConsumer(final String outputPath, final int videoWidth, final int videoHeight, final int audioSampleRate,
                                int qualityStrategy, int adaptiveBitrateWindowSizeInSecs, int adaptiveBitrateEncoderReconfigInterval,
                                int adaptiveBitrateWarCntThreshold, int adaptiveMinimumBitrate,
                                int adaptiveMaximumBitrate) {

        qualityStrategy = ifQualityStrayegyEnable(qualityStrategy);
        return Videostudio.getInstance().startVideoRecord(outputPath,
                videoWidth, videoHeight, VIDEO_FRAME_RATE, COMMON_VIDEO_BIT_RATE,
                audioSampleRate, audioChannels, audioBitRate,
                qualityStrategy, adaptiveBitrateWindowSizeInSecs, adaptiveBitrateEncoderReconfigInterval,
                adaptiveBitrateWarCntThreshold, adaptiveMinimumBitrate, adaptiveMaximumBitrate, null);
    }

    public void stopRecording() {
        destroyRecordingResource();
        Videostudio.getInstance().stopRecord();
    }

    protected boolean startProducer(final int videoWidth, int videoHeight, boolean useHardWareEncoding, int strategy) throws StartRecordingException {
        if (playerService != null) {
            playerService.start();
        }
        if (recorderService != null) {
            return recorderService.start(videoWidth, videoHeight, VideoRecordingStudio.getInitializeVideoBitRate(), VIDEO_FRAME_RATE, useHardWareEncoding, strategy);
        }

        return false;
    }

    private int ifQualityStrayegyEnable(int qualityStrategy) {
        qualityStrategy =  (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) ? 0 : qualityStrategy;
        return qualityStrategy;
    }

    /**
     * 获取录音器的参数
     **/
    public int getRecordSampleRate() {
        if (recorderService != null) {
            return recorderService.getSampleRate();
        }
        return SAMPLE_RATE_IN_HZ;
    }

    public int getAudioBufferSize() {
        if (recorderService != null) {
            return recorderService.getAudioBufferSize();
        }
        return SAMPLE_RATE_IN_HZ;
    }

    public void setAudioEffect(AudioEffect audioEffect) {
        if (null != recorderService) {
            recorderService.setAudioEffect(audioEffect);
        }
    }

    /**
     * 播放一个新的伴奏
     **/
    public void startAccompany(String musicPath) {
        if (null != playerService) {
            playerService.startAccompany(musicPath);
        }
    }

    /**
     * 停止播放
     **/
    public void stopAccompany() {
        if (null != playerService) {
            playerService.stopAccompany();
        }
    }

    /**
     * 暂停播放
     **/
    public void pauseAccompany() {
        if (null != playerService) {
            playerService.pauseAccompany();
        }
    }

    /**
     * 继续播放
     **/
    public void resumeAccompany() {
        if (null != playerService) {
            playerService.resumeAccompany();
        }
    }

    /**
     * 设置伴奏的音量大小
     **/
    public void setAccompanyVolume(float volume, float accompanyMax) {
        if (null != playerService) {
            playerService.setVolume(volume, accompanyMax);
        }
    }

    public void setAccompanyEffect(AudioEffect audioEffect) {
        if (null != playerService) {
            ArrayList<Integer> accompanyEffectFilterChain = new ArrayList<Integer>();
            accompanyEffectFilterChain.add(AudioEffectFilterType.AccompanyPitchShiftFilter.getId());
            audioEffect.setAccompanyEffectFilterChain(accompanyEffectFilterChain);
            playerService.setAudioEffect(audioEffect);
        }
    }

    // 初始化
    private native long nativeInit();
    // 释放资源
    private native void nativeRelease(long handle);
    // 设置录制监听回调
    private native void setRecordListener(long handle, Object listener);
    // 设置录制输出文件
    private native void setOutput(long handle, String dstPath);
    // 指定音频编码器名称
    private native void setAudioEncoder(long handle, String encoder);
    // 指定视频编码器名称
    private native void setVideoEncoder(long handle, String encoder);
    // 指定音频AVFilter描述
    private native void setAudioFilter(long handle, String filter);
    // 指定视频AVFilter描述
    private native void setVideoFilter(long handle, String filter);
    // 设置录制视频的旋转角度
    private native void setVideoRotate(long handle, int rotate);
    // 设置录制视频参数
    private native void setVideoParams(long handle, int width, int height, int frameRate,
                                       int pixelFormat, long maxBitRate, int quality);
    // 设置录制音频参数
    private native void setAudioParams(long handle, int sampleRate, int sampleFormat, int channels);
    // 录制一帧视频帧
    private native int recordVideoFrame(long handle, byte[] data, int length, int width, int height,
                                        int pixelFormat);
    // 录制一帧音频帧
    private native int recordAudioFrame(long handle, byte[] data, int length);
    // 开始录制
    private native void startRecord(long handle);
    // 停止录制
    private native void stopRecord(long handle);

    @Override
    protected void finalize() throws Throwable {
        release();
        super.finalize();
    }

    /**
     * 释放资源
     */
    public void release() {
        if (handle != 0) {
            nativeRelease(handle);
            handle = 0;
        }
    }

    /**
     * 设置录制监听器
     * @param listener
     */
    public void setRecordListener(OnRecordListener listener) {
        setRecordListener(handle, listener);
    }

    /**
     * 设置输出文件
     * @param dstPath
     */
    public void setOutput(String dstPath) {
        this.dstPath = dstPath;
        setOutput(handle, dstPath);
    }

    /**
     * 获取输出文件
     * @return
     */
    public String getOutput() {
        return dstPath;
    }

    /**
     * 指定音频编码器名称
     * @param encoder
     */
    public void setAudioEncoder(String encoder) {
        setAudioEncoder(handle, encoder);
    }

    /**
     * 指定视频编码器名称
     * @param encoder
     */
    public void setVideoEncoder(String encoder) {
        setVideoEncoder(handle, encoder);
    }

    /**
     * 设置音频AVFilter描述
     * @param filter
     */
    public void setAudioFilter(String filter) {
        setAudioFilter(handle, filter);
    }

    /**
     * 设置视频AVFilter描述
     * @param filter
     */
    public void setVideoFilter(String filter) {
        setVideoFilter(handle, filter);
    }

    /**
     * 设置旋转角度
     * @param rotate
     */
    public void setVideoRotate(int rotate) {
        setVideoRotate(handle, rotate);
    }

    /**
     * 设置视频参数
     * @param width
     * @param height
     * @param frameRate
     * @param pixelFormat
     * @param maxBitRate
     * @param quality
     */
    public void setVideoParams(int width, int height, int frameRate, int pixelFormat,
                               long maxBitRate, int quality) {
        setVideoParams(handle, width, height, frameRate, pixelFormat, maxBitRate, quality);
    }

    /**
     * 设置音频参数
     * @param sampleRate
     * @param sampleForamt
     * @param channels
     */
    public void setAudioParams(int sampleRate, int sampleForamt, int channels) {
        setAudioParams(handle, sampleRate, sampleForamt, channels);
    }

    /**
     * 录制一帧视频帧
     * @param data
     * @param length
     * @param width
     * @param height
     * @param pixelFormat
     */
    public void recordVideoFrame(byte[] data, int length, int width, int height, int pixelFormat) {
        recordVideoFrame(handle, data, length, width, height, pixelFormat);
    }

    /**
     * 录制一帧音频帧
     * @param data
     * @param length
     */
    public void recordAudioFrame(byte[] data, int length) {
        recordAudioFrame(handle, data, length);
    }

    /**
     * 开始录制
     */
    public void startRecord() {
        startRecord(handle);
    }

    /**
     * 停止录制
     */
    public void stopRecord() {
        stopRecord(handle);
    }

    /**
     * 使用Builder模式创建录制器
     */
    public static class RecordBuilder {

        private String mDstPath;        // 文件输出路径

        // 视频参数
        private int mWidth;             // 视频宽度
        private int mHeight;            // 视频高度
        private int mRotate;            // 旋转角度
        private int mPixelFormat;       // 像素格式
        private int mFrameRate;         // 帧率，默认30fps
        private long mMaxBitRate;       // 最大比特率，当无法达到设置的quality是，quality会自动调整
        private int mQuality;           // 视频质量系数，默认为23, 推荐17~28之间，值越小，质量越高
        private String mVideoEncoder;   // 指定视频编码器名称
        private String mVideoFilter;    // 视频AVFilter描述

        // 音频参数
        private int mSampleRate;        // 采样率
        private int mSampleFormat;      // 采样格式
        private int mChannels;          // 声道数
        private String mAudioEncoder;   // 指定音频编码器名称
        private String mAudioFilter;    // 音频AVFilter描述

        public RecordBuilder(@NonNull String dstPath) {
            mDstPath = dstPath;

            mWidth = -1;
            mHeight = -1;
            mRotate = 0;
            mPixelFormat = -1;
            mFrameRate = -1;
            mMaxBitRate = -1;
            mQuality = 23;
            mVideoEncoder = null;
            mVideoFilter = "null";

            mSampleRate = -1;
            mSampleFormat = -1;
            mChannels = -1;
            mAudioEncoder = null;
            mAudioFilter = "anull";
        }

        /**
         * 设置录制视频参数
         * @param width
         * @param height
         * @return
         */
        public RecordBuilder setVideoParams(int width, int height, int pixelFormat, int frameRate) {
            mWidth = width;
            mHeight = height;
            mPixelFormat = pixelFormat;
            mFrameRate = frameRate;
            return this;
        }

        /**
         * 设置录制音频参数
         * @param sampleRate
         * @param sampleFormat
         * @param channels
         * @return
         */
        public RecordBuilder setAudioParams(int sampleRate, int sampleFormat, int channels) {
            mSampleRate = sampleRate;
            mSampleFormat = sampleFormat;
            mChannels = channels;
            return this;
        }

        /**
         * 设置视频AVFilter描述
         * @param videoFilter
         * @return
         */
        public RecordBuilder setVideoFilter(String videoFilter) {
            if (!TextUtils.isEmpty(videoFilter)) {
                mVideoFilter = videoFilter;
            }
            return this;
        }

        /**
         * 设置音频AVFilter描述
         * @param audioFilter
         * @return
         */
        public RecordBuilder setAudioFilter(String audioFilter) {
            if (!TextUtils.isEmpty(audioFilter)) {
                mAudioFilter = audioFilter;
            }
            return this;
        }
        /**
         * 设置旋角度
         * @param rotate
         * @return
         */
        public RecordBuilder setRotate(int rotate) {
            mRotate = rotate;
            return this;
        }
        
        /**
         * 设置最大比特率
         * @param maxBitRate
         * @return
         */
        public RecordBuilder setMaxBitRate(long maxBitRate) {
            mMaxBitRate = maxBitRate;
            return this;
        }

        /**
         * 设置质量系数
         * @param quality
         * @return
         */
        public RecordBuilder setQuality(int quality) {
            mQuality = quality;
            return this;
        }

        /**
         * 设置视频编码器名称
         * @param encoder
         * @return
         */
        public RecordBuilder setVideoEncoder(String encoder) {
            mVideoEncoder = encoder;
            return this;
        }

        /**
         * 指定音频编码器名称
         * @param encoder
         * @return
         */
        public RecordBuilder setAudioEncoder(String encoder) {
            mAudioEncoder = encoder;
            return this;
        }

        /**
         * 创建一个录制器
         * @return
         */
        public MediaRecorder create() {
            MediaRecorder recorder = new MediaRecorder();
            recorder.setOutput(mDstPath);
            recorder.setVideoParams(mWidth, mHeight, mFrameRate, mPixelFormat, mMaxBitRate, mQuality);
            recorder.setVideoRotate(mRotate);
            recorder.setAudioParams(mSampleRate, mSampleFormat, mChannels);
            if (!TextUtils.isEmpty(mVideoEncoder)) {
                recorder.setVideoEncoder(mVideoEncoder);
            }
            if (!TextUtils.isEmpty(mAudioEncoder)) {
                recorder.setAudioEncoder(mAudioEncoder);
            }
            if (!mVideoFilter.equalsIgnoreCase("null")) {
                recorder.setVideoFilter(mVideoFilter);
            }
            if (!mAudioFilter.equalsIgnoreCase("anull")) {
                recorder.setAudioFilter(mAudioFilter);
            }
            return recorder;
        }
    }

    /**
     * 录制监听回调
     */
    public interface OnRecordListener {
        // 准备完成回调
        void onRecordStart();
        // 正在录制
        void onRecording(float duration);
        // 录制完成回调
        void onRecordFinish(boolean success, float duration);
        // 录制出错回调
        void onRecordError(String msg);
    }
}
