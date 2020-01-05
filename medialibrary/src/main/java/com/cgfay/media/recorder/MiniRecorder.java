package com.cgfay.media.recorder;

import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * 基于FFmpeg的音频/视频录制器
 */
public final class MiniRecorder {
    private static final String TAG = "MiniRecorder";
    private int defaultCameraFacingId = Camera.CameraInfo.CAMERA_FACING_FRONT;
    public static final String MIME_TYPE = "video/avc"; // H.264 Advanced Video
    private static final int IFRAME_INTERVAL = 1; // sync frame every second
    private static final int TIMEOUT_USEC = 5000;

    //Camera
    private Camera mCamera;
    private SurfaceTexture mCameraSurfaceTexture;
    //Encoder
    private Surface mInputSurface;
    private MediaCodec mEncoder;
    private MediaCodec.BufferInfo mBufferInfo;
    private long mLastPresentationTimeUs = -1;

    static {
        System.loadLibrary("soundtouch");
        System.loadLibrary("yuv");
        System.loadLibrary("mini_recorder");
    }

    // 初始化
    private native long nativeInit();
    // prepareEGLContext
    public native void prepareEGLContext(long handle, Surface surface, int width, int height, int cameraFacingId);
    public native void destroyEGLContext(long handle);
    // notifyFrameAvailable
    public native void notifyFrameAvailable(long handle);
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

    private long handle;
    private String dstPath;

    private MiniRecorder() {
        handle = nativeInit();
    }

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

    public void startPreviewFromNative(int textureId) {
        mCamera = Camera.open(defaultCameraFacingId);
        Camera.Parameters parameters = mCamera.getParameters();
        parameters.setPreviewSize(720, 1280);
        parameters.setPictureSize(720, 1280);
        mCameraSurfaceTexture = new SurfaceTexture(textureId);
        try {
            mCamera.setPreviewTexture(mCameraSurfaceTexture);
            mCameraSurfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
                @Override
                public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                    notifyFrameAvailable();
                }
            });
            mCamera.startPreview();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void updateTexImageFromNative() {
        if (null != mCameraSurfaceTexture) {
            mCameraSurfaceTexture.updateTexImage();
        }
    }
    /***************************************MediaCodec functions***********************************************/
    public void createMediaCodecSurfaceEncoderFromNative(int width, int height, int bitRate, int frameRate) {
        try {
            Log.i(TAG, String.format("MediaCodecSurfaceEncoder width:%d, height:%d", width, height));
            MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, width, height);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);

            mEncoder = MediaCodec.createEncoderByType(MIME_TYPE);
            mEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            mInputSurface = mEncoder.createInputSurface();
            mEncoder.start();
        } catch (Exception e) {
            Log.e(TAG, "createMediaCodecSurfaceEncoder failed");
        }
    }

    public Surface getEncodeSurfaceFromNative() {
        return mInputSurface;
    }

    public void closeMediaCodecCalledFromNative() {
        try {
            if (null != mEncoder) {
                mEncoder.stop();
                mEncoder.release();
                mEncoder = null;
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public long pullH264StreamFromDrainEncoderFromNative(byte[] returnedData) {
        long val = 0;
        try {
            mBufferInfo = new MediaCodec.BufferInfo();
            ByteBuffer[] encoderOutputBuffers = mEncoder.getOutputBuffers();
            int encoderStatus = mEncoder.dequeueOutputBuffer(mBufferInfo, TIMEOUT_USEC);
            if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                Log.i(TAG, "no output available yet");
            } else if(encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                Log.i(TAG, " not expected for an encoder");
                encoderOutputBuffers = mEncoder.getOutputBuffers();
            }else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                MediaFormat mEncodedFormat = mEncoder.getOutputFormat();
                Log.d(TAG, "encoder output format changed: " + mEncodedFormat);
            }else if (encoderStatus < 0) {
                Log.w(TAG, "unexpected result from encoder.dequeueOutputBuffer: " + encoderStatus);
            } else {
                //normal case
                ByteBuffer encodedData = encoderOutputBuffers[encoderStatus];
                if (encodedData == null) {
                    Log.e(TAG, "output buffer is null");
                }
                if((mBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    if (mBufferInfo.size != 0) {
                        encodedData.position(mBufferInfo.offset);
                        encodedData.limit(mBufferInfo.offset + mBufferInfo.size);
                        val = mBufferInfo.size;
                        encodedData.get(returnedData, 0, mBufferInfo.size);
                    } else {
                        Log.i(TAG, "why mBufferInfo.size is equals 0");
                    }
                    mBufferInfo.size = 0;
                }
                if (mBufferInfo.size != 0) {
                    encodedData.position(mBufferInfo.offset);
                    encodedData.limit(mBufferInfo.offset + mBufferInfo.size);

                    val = mBufferInfo.size;
                    encodedData.get(returnedData, 0, mBufferInfo.size);
                } else {
                    Log.i(TAG, "why mBufferInfo.size is equals 0");
                }
                mEncoder.releaseOutputBuffer(encoderStatus, false);
                if ((mBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    Log.w(TAG, "reached end of stream unexpectedly");
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return val;
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
     * prepareEGLContext
     */
    public void prepareEGLContext(Surface surface, int width, int height, int cameraFacingId) {
        prepareEGLContext(handle, surface, width, height, cameraFacingId);
    }
    /**
     * destroyEGLContext
     */
    public void destroyEGLContext() {
        destroyEGLContext(handle);
    }
    /**
     * notifyFrameAvailable
     */
    public void notifyFrameAvailable() {
        notifyFrameAvailable(handle);
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
        public MiniRecorder create() {
            MiniRecorder recorder = new MiniRecorder();
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
