package com.cgfay.caincamera.fragment;

import android.app.Activity;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

import com.cgfay.caincamera.R;
import com.cgfay.camera.utils.PathConstraints;
import com.cgfay.camera.widget.RecordProgressView;
import com.cgfay.media.NativeMp3Player;
import com.cgfay.media.recorder.AVFormatter;
import com.cgfay.media.recorder.AudioRecorder;
import com.cgfay.media.recorder.MiniVideoRecorder;
import com.cgfay.uitls.utils.NotchUtils;
import com.cgfay.uitls.utils.StatusBarUtils;

public class MiniVideoRecorderFragment extends Fragment implements View.OnClickListener, SurfaceHolder.Callback,
        MiniVideoRecorder.OnRecordListener, AudioRecorder.OnRecordCallback{

    private static final String TAG = "MiniRecorderFragment";
    private static final int CAMERA_FACING_BACK = 0;
    private static final int CAMERA_FACING_FRONT = 1;

    private Activity mActivity;
    private Handler mMainHandler;

    private View mContentView;
    private SurfaceView mSVRecordView;
    private RecordProgressView mProgressView;
    private Button mRecordButton;

    private Button mBtnNext;
    private Button mBtnDelete;

    private MiniVideoRecorder mMiniRecorder;
    private AudioRecorder mAudioRecorder;
    public NativeMp3Player mediaPlayer;

    private boolean mIsRecording = false;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mActivity = getActivity();
        mMainHandler = new Handler(Looper.getMainLooper());
    }

    @Override
    public void onDetach() {
        mActivity = null;
        super.onDetach();
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mContentView = inflater.inflate(R.layout.fragment_media_record, container, false);
        return mContentView;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        initView();
        bindListener();
        initAudioRecorder();
        initMusicPlayer();
        mMiniRecorder = new MiniVideoRecorder.RecordBuilder("/storage/emulated/0/a_songstudio/mini_recorder.flv") //generateOutputPath()
                .setVideoParams(720, 1280,  0, 900)
                .setAudioParams(mAudioRecorder.getSampleRate(), AVFormatter.getSampleFormat(mAudioRecorder.getSampleFormat()), mAudioRecorder.getChannels())
                .create();
        mMiniRecorder.setRecordListener(this);
    }

    @Override
    public void onStop() {
        super.onStop();
        try {
            if (mediaPlayer != null) {
                mediaPlayer.stop();
            }
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    private void initView() {
        // 录制预览
        mSVRecordView = (SurfaceView) mContentView.findViewById(R.id.sv_record_view);
        mSVRecordView.getHolder().addCallback(this);
        // 进度条
        mProgressView = (RecordProgressView) mContentView.findViewById(R.id.record_progress_view);
        // 录制按钮
        mRecordButton = (Button) mContentView.findViewById(R.id.record_button);
        mRecordButton.setOnClickListener(v -> {
            if (!mIsRecording) {
                mMiniRecorder.startRecord();
                mAudioRecorder.start();
                startMusicPlayer();
                mIsRecording = true;
                mRecordButton.setText(R.string.btn_record_cancel);
            } else {
                stopMusicPlayer();
                mAudioRecorder.stop();
                mMiniRecorder.stopRecord();
                mIsRecording = false;
                mRecordButton.setText(R.string.btn_record_start);
            }

        });

        // 下一步
        mBtnNext = mContentView.findViewById(R.id.btn_next);
        mBtnNext.setOnClickListener(this);

        // 删除按钮
        mBtnDelete = mContentView.findViewById(R.id.btn_delete);
        mBtnDelete.setOnClickListener(this);

        // 判断是否存在刘海屏
        if (NotchUtils.hasNotchScreen(mActivity)) {
            View view = mContentView.findViewById(R.id.view_safety_area);
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) view.getLayoutParams();
            params.height = StatusBarUtils.getStatusBarHeight(mActivity);
            view.setLayoutParams(params);
        }
    }

    private void initAudioRecorder() {
        mAudioRecorder = new AudioRecorder();
        mAudioRecorder.setOnRecordCallback(this);
        mAudioRecorder.setSampleFormat(AudioFormat.ENCODING_PCM_16BIT);
    }

    private void initMusicPlayer() {
        try {
            if (mediaPlayer == null) {
                mediaPlayer = new NativeMp3Player();
                mediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void startMusicPlayer() {
        if (mediaPlayer != null) {
            mediaPlayer.prepare(44100);
            mediaPlayer.startAccompany("/storage/emulated/0/a_songstudio/101.mp3");
            mediaPlayer.start();
        }
    }

    private void stopMusicPlayer() {
        if (mediaPlayer != null) {
            mediaPlayer.stop();
        }
    }

    private void bindListener() {

    }

    /**
     * 录制一帧音频帧数据
     * @param audioSamples
     * @param audioSampleSize
     */
    @Override
    public void onRecordSample(short[] audioSamples, int audioSampleSize) {
        if (mIsRecording) {
            if (mMiniRecorder != null) {
                mMiniRecorder.recordAudioFrame(audioSamples, audioSampleSize);
            }
        }
    }

    @Override
    public void onRecordFinish() {
        Log.d(TAG, "onRecordFinish: audio record finish");
    }

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    public void onClick(View view) {

    }

    // surface callback
    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        if (mMiniRecorder != null)
            mMiniRecorder.prepareEGLContext(surfaceHolder.getSurface(), mSVRecordView.getWidth(), mSVRecordView.getHeight(), CAMERA_FACING_FRONT);
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
        mMiniRecorder.destroyEGLContext();
    }

    //record callback
    @Override
    public void onRecordStart() {

    }

    @Override
    public void onRecording(float duration) {

    }

    @Override
    public void onRecordFinish(boolean success, float duration) {

    }

    @Override
    public void onRecordError(String msg) {

    }

    /**
     * 创建合成的视频文件名
     * @return
     */
    public String generateOutputPath() {
        return PathConstraints.getVideoCachePath(mActivity);
    }

    @Override
    public void onRecordSample(byte[] data) {

    }
}
