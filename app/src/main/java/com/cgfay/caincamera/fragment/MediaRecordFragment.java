package com.cgfay.caincamera.fragment;

import android.app.Activity;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

import com.cgfay.caincamera.R;
import com.cgfay.caincamera.widget.GLRecordView;
import com.cgfay.camera.engine.model.CameraInfo;
import com.cgfay.camera.utils.PathConstraints;
import com.cgfay.camera.widget.RecordProgressView;
import com.cgfay.media.recorder.AVFormatter;
import com.cgfay.media.recorder.FFMediaRecorder;
import com.cgfay.media.recorder.MediaRecorder;
import com.cgfay.uitls.utils.NotchUtils;
import com.cgfay.uitls.utils.StatusBarUtils;

public class MediaRecordFragment extends Fragment implements View.OnClickListener, SurfaceHolder.Callback,
        Camera.PreviewCallback, SurfaceTexture.OnFrameAvailableListener, MediaRecorder.OnRecordListener{

    private static final String TAG = "FFMediaRecordFragment";
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

    //todo
    private MediaRecorder mMediaRecorder;
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
        mMediaRecorder = new MediaRecorder.RecordBuilder(generateOutputPath())
                .create();
        mMediaRecorder.setRecordListener(this);
        initView();
        bindListener();
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
            mRecordButton.setEnabled(false);
            //todo
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

    private void bindListener() {

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
        if (mMediaRecorder != null)
            mMediaRecorder.prepareEGLContext(surfaceHolder.getSurface(), mSVRecordView.getWidth(), mSVRecordView.getHeight(), CAMERA_FACING_FRONT);
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    // camera callback
    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {

    }

    @Override
    public void onPreviewFrame(byte[] bytes, Camera camera) {

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
