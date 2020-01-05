package com.cgfay.caincamera.fragment;

import android.app.Activity;
import android.content.Context;
import android.media.AudioFormat;
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
import com.cgfay.media.recorder.AudioRecorder;
import com.cgfay.media.recorder.MiniRecorder;
import com.cgfay.uitls.utils.NotchUtils;
import com.cgfay.uitls.utils.StatusBarUtils;

public class MiniRecordFragment extends Fragment implements View.OnClickListener, SurfaceHolder.Callback,
        MiniRecorder.OnRecordListener, AudioRecorder.OnRecordCallback{

    private static final String TAG = "MediaRecordFragment";
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

    private MiniRecorder mMiniRecorder;
    private AudioRecorder mAudioRecorder;

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
        mMiniRecorder = new MiniRecorder.RecordBuilder(generateOutputPath())
                .create();
        mMiniRecorder.setRecordListener(this);
        initView();
        bindListener();
        initAudioRecorder();
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
            //mRecordButton.setEnabled(false);
            if (!mIsRecording) {
                mMiniRecorder.startRecord();
                mAudioRecorder.start();
                mIsRecording = true;
                mRecordButton.setText(R.string.btn_record_cancel);
            } else {
                mMiniRecorder.stopRecord();
                mAudioRecorder.stop();
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

    private void bindListener() {

    }

    /**
     * 录制一帧音频帧数据
     * @param data
     */
    @Override
    public void onRecordSample(byte[] data) {
        if (mIsRecording) {
            if (mMiniRecorder != null) {
                mMiniRecorder.recordAudioFrame(data, data.length);
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
}
