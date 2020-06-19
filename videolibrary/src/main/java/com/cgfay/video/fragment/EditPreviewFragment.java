package com.cgfay.video.fragment;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.cgfay.media.CainMediaPlayer;
import com.cgfay.media.GlenMediaPlayer;
import com.cgfay.media.IMediaPlayer;
import com.cgfay.uitls.utils.FileUtils;
import com.cgfay.uitls.utils.StringUtils;
import com.cgfay.video.R;
import com.cgfay.video.widget.VideoTextureView;

import java.io.File;
import java.io.IOException;

public class EditPreviewFragment extends Fragment implements View.OnClickListener {
    private static final String TAG = "EditPreviewFragment";

    private String mVideoPath;                      // 视频流路径
    private View mContentView;
    // 播放控件
    private VideoTextureView mVideoPlayerView;
    private ImageView mIvVideoPlay;
    private Button mBtnSave;

    private GlenMediaPlayer mCainMediaPlayer;
    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mContentView =  inflater.inflate(R.layout.fragment_edit_preview, container, false);
        return mContentView;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // 播放器显示控件
        mVideoPlayerView = mContentView.findViewById(R.id.video_player_view);
        mVideoPlayerView.setSurfaceTextureListener(mSurfaceTextureListener);
        mVideoPlayerView.setOnClickListener(this);

        mIvVideoPlay = mContentView.findViewById(R.id.iv_video_play);
        mIvVideoPlay.setOnClickListener(this);

        mBtnSave = mContentView.findViewById(R.id.btn_save);
        mBtnSave.setOnClickListener(this);
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mCainMediaPlayer == null) {
            mCainMediaPlayer = new GlenMediaPlayer();
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mCainMediaPlayer != null) {
            mCainMediaPlayer.resume();
            mIvVideoPlay.setVisibility(View.GONE);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mCainMediaPlayer != null) {
            mCainMediaPlayer.pause();
            mIvVideoPlay.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onDestroy() {
        if (mCainMediaPlayer != null) {
            mCainMediaPlayer.reset();
            mCainMediaPlayer = null;
        }
        super.onDestroy();
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.video_player_view) {
            if (mCainMediaPlayer != null) {
                if (mCainMediaPlayer.isPlaying()) {
                    pausePlayer();
                    mIvVideoPlay.setVisibility(View.VISIBLE);
                } else {
                    resumePlayer();
                    mIvVideoPlay.setVisibility(View.GONE);
                }
            }
        } else if (id == R.id.btn_save) {
            storeToPhoto(mVideoPath);
        }
    }

    private void resumePlayer() {
        if (mCainMediaPlayer != null) {
            mCainMediaPlayer.resume();
        }
        mIvVideoPlay.setVisibility(View.GONE);
    }

    private void pausePlayer() {
        if (mCainMediaPlayer != null) {
            mCainMediaPlayer.pause();
        }
        mIvVideoPlay.setVisibility(View.VISIBLE);
    }

    /**
     * 设置视频流路径
     * @param videoPath
     */
    public void setVideoPath(String videoPath) {
        mVideoPath = videoPath;
    }

    // 视频显示监听
    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;
    private TextureView.SurfaceTextureListener mSurfaceTextureListener = new TextureView.SurfaceTextureListener() {
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
            if (mSurfaceTexture == null) {
                mSurfaceTexture = surface;
                openMediaPlayer();
            } else {
                mVideoPlayerView.setSurfaceTexture(mSurfaceTexture);
            }
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {

        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            return mSurfaceTexture == null;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surface) {

        }
    };

    /**
     * 打开视频播放器
     */
    private void openMediaPlayer() {
        mContentView.setKeepScreenOn(true);
        mCainMediaPlayer.setOnPreparedListener(new IMediaPlayer.OnPreparedListener() {
            @Override
            public void onPrepared(final IMediaPlayer mp) {
                mp.start();
            }
        });
        mCainMediaPlayer.setOnVideoSizeChangedListener(new IMediaPlayer.OnVideoSizeChangedListener() {
            @Override
            public void onVideoSizeChanged(IMediaPlayer mediaPlayer, int width, int height) {
                if (mediaPlayer.getRotate() % 180 != 0) {
                    mVideoPlayerView.setVideoSize(height, width);
                } else {
                    mVideoPlayerView.setVideoSize(width, height);
                }
            }
        });
        mCainMediaPlayer.setOnCompletionListener(new IMediaPlayer.OnCompletionListener() {
            @Override
            public void onCompletion(IMediaPlayer mp) {

            }
        });

        mCainMediaPlayer.setOnErrorListener(new IMediaPlayer.OnErrorListener() {
            @Override
            public boolean onError(IMediaPlayer mp, int what, int extra) {
                Log.d(TAG, "onError: what = " + what + ", extra = " + extra);
                return false;
            }
        });

        try {
            mCainMediaPlayer.setDataSource(mVideoPath);
            if (mSurface == null) {
                mSurface = new Surface(mSurfaceTexture);
            }
            mCainMediaPlayer.setSurface(mSurface);
            mCainMediaPlayer.setOption(CainMediaPlayer.OPT_CATEGORY_PLAYER, "vcodec", "h264_mediacodec");
            mCainMediaPlayer.prepare();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void storeToPhoto(String path) {
        ContentResolver localContentResolver = getActivity().getContentResolver();
        ContentValues localContentValues = getVideoContentValues(getActivity(), new File(path), System.currentTimeMillis());
        Uri localUri = localContentResolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, localContentValues);
        Toast.makeText(getActivity(), "保存到相册成功，路径为"+ path, Toast.LENGTH_SHORT).show();
    }

    public static ContentValues getVideoContentValues(Context paramContext, File paramFile, long paramLong) {
        ContentValues localContentValues = new ContentValues();
        localContentValues.put("title", paramFile.getName());
        localContentValues.put("_display_name", paramFile.getName());
        localContentValues.put("datetaken", Long.valueOf(paramLong));
        localContentValues.put("date_modified", Long.valueOf(paramLong));
        localContentValues.put("date_added", Long.valueOf(paramLong));
        localContentValues.put("_data", paramFile.getAbsolutePath());
        localContentValues.put("_size", Long.valueOf(paramFile.length()));
        return localContentValues;
    }
}
