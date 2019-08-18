package com.mlingdu.douyin.ui.act;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import com.bumptech.glide.Glide;
import com.cgfay.caincamera.R;
import com.cgfay.caincamera.activity.MainActivity;
import com.cgfay.camera.engine.PreviewEngine;
import com.cgfay.camera.engine.model.AspectRatio;
import com.cgfay.camera.engine.model.GalleryType;
import com.cgfay.camera.listener.OnGallerySelectedListener;
import com.cgfay.camera.listener.OnPreviewCaptureListener;
import com.cgfay.image.activity.ImageEditActivity;
import com.cgfay.video.activity.VideoEditActivity;
import com.dueeeke.videoplayer.player.IjkVideoView;
import com.dueeeke.videoplayer.player.PlayerConfig;
import com.mlingdu.douyin.App;

import com.mlingdu.douyin.common.okhttp.http.OkHttpClientManager;
import com.mlingdu.douyin.common.utils.DouyinUtils;
import com.mlingdu.douyin.common.utils.StatusBarCompat;
import com.mlingdu.douyin.common.utils.ToastUtil;
import com.mlingdu.douyin.ui.widget.DouYinController;
import com.mlingdu.douyin.entity.LevideoData;
import com.mlingdu.douyin.ui.widget.VerticalViewPager;
import com.mlingdu.douyin.ui.adapter.DouYinAdapter;
import com.mlingdu.douyin.entity.DouyinVideoListData;

import java.io.IOException;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.List;

import de.hdodenhof.circleimageview.CircleImageView;
import okhttp3.Request;


/**
 * Created by hackest on 2018/3/5.
 */

public class DouyinActivity extends AppCompatActivity {
    private VerticalViewPager mVerticalViewpager;

    private List<LevideoData> mList = new ArrayList<>();

    private int mCurrentItem;

    private IjkVideoView mIjkVideoView;
    private DouYinController mDouYinController;
    private DouYinAdapter mDouYinAdapter;
    private List<View> mViews = new ArrayList<>();

    private ImageView mCover;
    private ImageView mPlay;
    private ImageView mThumb;
    private CircleImageView mAuthorImg;
    private TextView mPlayCount;
    private TextView mLikeCount;
    private TextView mShareCount;
    private ImageView mStartCamera;

    private int mPlayingPosition;
    private int position;
    private long max_cursor = 0;
    private int maxListCount = 20;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_vertical_video);
        initView();
        initListener();
        initData();
    }

    protected void initView() {
        mStartCamera = (ImageView) findViewById(R.id.iv_camera);
        mVerticalViewpager = (VerticalViewPager)findViewById(R.id.verticalviewpager);
        StatusBarCompat.translucentStatusBar(this, true);

        mIjkVideoView = new IjkVideoView(this);
        PlayerConfig config = new PlayerConfig.Builder().setLooping().build();
        mIjkVideoView.setPlayerConfig(config);

        mDouYinController = new DouYinController(this);
        mIjkVideoView.setVideoController(mDouYinController);
    }

    private void initListener() {
        mStartCamera.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                previewCamera();
            }
        });
    }

    private void startPlay() {
        View view = mViews.get(mCurrentItem);
        FrameLayout frameLayout = view.findViewById(R.id.container);
        mCover = view.findViewById(R.id.cover_img);

        mDouYinController.setSelect(false);

        if (mCover != null && mCover.getDrawable() != null) {
            mDouYinController.getThumb().setImageDrawable(mCover.getDrawable());
        }

        ViewGroup parent = (ViewGroup) mIjkVideoView.getParent();

        if (parent != null) {
            parent.removeAllViews();
        }

        frameLayout.addView(mIjkVideoView);
        mIjkVideoView.setUrl(mList.get(mCurrentItem).getVideoPlayUrl());
        mIjkVideoView.setScreenScale(IjkVideoView.SCREEN_SCALE_DEFAULT);
        mIjkVideoView.start();

        mPlayingPosition = mCurrentItem;
    }

    @Override
    protected void onPause() {
        super.onPause();
        mIjkVideoView.pause();
        if (mDouYinController != null) {
            mDouYinController.getIvPlay().setVisibility(View.GONE);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mIjkVideoView.resume();

        if (mDouYinController != null) {
            mDouYinController.setSelect(false);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mIjkVideoView.release();
    }


    public void initData() {
        String url = DouyinUtils.getEncryptUrl(this, 0, max_cursor);
        Log.e("DouyinActivity",url);
        OkHttpClientManager.getAsyn(url, new OkHttpClientManager.StringCallback() {
            @Override
            public void onResponse(String response) {
                try {
                    DouyinVideoListData listData = DouyinVideoListData.fromJSONData(response);
                    max_cursor = listData.getMaxCursor();

                    if (listData.getVideoDataList() == null || listData.getVideoDataList().size() == 0) {
                        return;
                    }

                    mList = listData.getVideoDataList();
                    for (LevideoData item : mList) {
                        View view = LayoutInflater.from(DouyinActivity.this).inflate(R.layout.item_video_view, null);

                        mCover = view.findViewById(R.id.cover_img);
                        mAuthorImg = view.findViewById(R.id.iv_avatar);
                        mLikeCount = view.findViewById(R.id.tv_like_count);
                        mPlayCount = view.findViewById(R.id.tv_play_count);
                        mShareCount = view.findViewById(R.id.tv_share_count);
                        /*填充UI组件*/
                        Glide.with(App.getInstance()).load(item.getCoverImgUrl()).into(mCover);
                        Glide.with(App.getInstance()).load(item.getAuthorImgUrl()).into(mAuthorImg);
                        mLikeCount.setText(new DecimalFormat("0.0").format((float)item.getLikeCount()/10000) + "w");
                        mPlayCount.setText(new DecimalFormat("0.0").format((float)item.getPlayCount()/10000) + "w");
                        mShareCount.setText(new DecimalFormat("0.0").format(1.3) + "w");
                        mViews.add(view);
                    }

                    mDouYinAdapter = new DouYinAdapter(mViews);
                    mVerticalViewpager.setAdapter(mDouYinAdapter);

                    if (position != -1) {
                        mVerticalViewpager.setCurrentItem(position);
                    }

                    mVerticalViewpager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener() {

                        @Override
                        public void onPageSelected(int position) {
                            mCurrentItem = position;
                            mIjkVideoView.pause();

                            if (mCurrentItem == mList.size() - 1) {
                                ToastUtil.showToast("加载中，请稍后");
                                getDouyinListData();
                            }
                        }

                        @Override
                        public void onPageScrollStateChanged(int state) {

                            if (mPlayingPosition == mCurrentItem) return;
                            if (state == VerticalViewPager.SCROLL_STATE_IDLE) {
                                mIjkVideoView.release();
                                ViewParent parent = mIjkVideoView.getParent();
                                if (parent != null && parent instanceof FrameLayout) {
                                    ((FrameLayout) parent).removeView(mIjkVideoView);
                                }
                                startPlay();
                            }
                        }
                    });

                    mVerticalViewpager.post(new Runnable() {
                        @Override
                        public void run() {
                            startPlay();
                        }
                    });
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void onFailure(Request request, IOException e) {
                ToastUtil.showToast("网络连接失败");
            }
        });
    }

    /**
     * 下拉数据规律：min_cursor=max_cursor=0
     * 上拉数据规律：
     * 第二次请求取第一次请求返回的json数据中的min_cursor字段，max_cursor不需要携带。
     * 第三次以及后面所有的请求都只带max_cursor字段，值为第一次请求返回的json数据中的max_cursor字段
     */
    public void getDouyinListData() {
        String url = DouyinUtils.getEncryptUrl(this, 0, max_cursor);
        OkHttpClientManager.getAsyn(url, new OkHttpClientManager.StringCallback() {
            @Override
            public void onResponse(String response) {
                try {
                    DouyinVideoListData listData = DouyinVideoListData.fromJSONData(response);
                    max_cursor = listData.getMaxCursor();

                    if (listData.getVideoDataList() == null || listData.getVideoDataList().size() == 0) {
                        return;
                    }

                    List<LevideoData> list = listData.getVideoDataList();

                    mList.addAll(list);

                    mViews.clear();//加载更多需要先清空原来的view

                    for (LevideoData item : mList) {
                        View view = LayoutInflater.from(DouyinActivity.this).inflate(R.layout.item_video_view, null);

                        mCover = view.findViewById(R.id.cover_img);
                        mAuthorImg = view.findViewById(R.id.iv_avatar);
                        mLikeCount = view.findViewById(R.id.tv_like_count);
                        mPlayCount = view.findViewById(R.id.tv_play_count);
                        /*填充UI组件*/
                        Glide.with(App.getInstance()).load(item.getCoverImgUrl()).into(mCover);
                        Glide.with(App.getInstance()).load(item.getAuthorImgUrl()).into(mAuthorImg);
                        mLikeCount.setText(new DecimalFormat("0.0").format((float)item.getLikeCount()/10000) + "w");
                        mPlayCount.setText(new DecimalFormat("0.0").format((float)item.getPlayCount()/10000) + "w");

                        mViews.add(view);
                    }
                    mDouYinAdapter.setmViews(mViews);
                    mDouYinAdapter.notifyDataSetChanged();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void onFailure(Request request, IOException e) {
                ToastUtil.showToast("网络连接失败");
            }
        });
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }

    /**
     * 打开预览页面
     */
    private void previewCamera() {
        PreviewEngine.from(this)
                .setCameraRatio(AspectRatio.Ratio_16_9)
                .showFacePoints(false)
                .showFps(true)
                .backCamera(true)
                .setGalleryListener(new OnGallerySelectedListener() {
                    @Override
                    public void onGalleryClickListener(GalleryType type) {
                        //scanMedia(type == GalleryType.ALL);
                    }
                })
                .setPreviewCaptureListener(new OnPreviewCaptureListener() {
                    @Override
                    public void onMediaSelectedListener(String path, GalleryType type) {
                        if (type == GalleryType.PICTURE) {
                            Intent intent = new Intent(DouyinActivity.this, ImageEditActivity.class);
                            intent.putExtra(ImageEditActivity.IMAGE_PATH, path);
                            intent.putExtra(ImageEditActivity.DELETE_INPUT_FILE, true);
                            startActivity(intent);
                        } else if (type == GalleryType.VIDEO) {
                            Intent intent = new Intent(DouyinActivity.this, VideoEditActivity.class);
                            intent.putExtra(VideoEditActivity.VIDEO_PATH, path);
                            startActivity(intent);
                        }
                    }
                })
                .startPreview();
    }
}