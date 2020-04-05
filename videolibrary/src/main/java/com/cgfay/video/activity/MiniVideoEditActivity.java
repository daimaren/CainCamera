package com.cgfay.video.activity;

import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.view.WindowManager;

import com.cgfay.uitls.bean.Music;
import com.cgfay.uitls.fragment.MusicSelectFragment;
import com.cgfay.video.R;
import com.cgfay.video.fragment.EditPreviewFragment;
import com.cgfay.video.fragment.MiniVideoEditFragment;
import com.cgfay.video.fragment.VideoEditFragment;

public class MiniVideoEditActivity extends AppCompatActivity implements MiniVideoEditFragment.OnSelectMusicListener,
        MusicSelectFragment.OnMusicSelectedListener, MiniVideoEditFragment.OnEditPreviewListener {

    public static final String VIDEO_PATH = "videoPath";

    private static final String FRAGMENT_VIDEO_EDIT = "fragment_video_edit";
    private static final String FRAGMENT_MUSIC_SELECT = "fragment_video_music_select";
    private static final String FRAGMENT_EDIT_PREVIEW = "fragment_edit_preview";
    private String mVideoPath;

    protected void hideNavigationBar() {
        if (Build.VERSION.SDK_INT > 11 && Build.VERSION.SDK_INT < 19) {
            View v = getWindow().getDecorView();
            v.setSystemUiVisibility(View.GONE);
        } else if (Build.VERSION.SDK_INT >= 19) {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
            View decorView = getWindow().getDecorView();
            int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN;
            decorView.setSystemUiVisibility(uiOptions);
            decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
                @Override
                public void onSystemUiVisibilityChange(int visibility) {
                    hideNavigationBar();
                }
            });
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        hideNavigationBar();
        setContentView(R.layout.activity_video_edit);
        if (null == savedInstanceState) {
            mVideoPath = getIntent().getStringExtra(VIDEO_PATH);
            MiniVideoEditFragment fragment = MiniVideoEditFragment.newInstance();
            fragment.setOnSelectMusicListener(this);
            fragment.setOnEditPreviewListener(this);
            fragment.setVideoPath(mVideoPath);
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.fragment_content, fragment, FRAGMENT_VIDEO_EDIT)
                    .addToBackStack(FRAGMENT_VIDEO_EDIT)
                    .commit();
        }
    }

    @Override
    public void onBackPressed() {
        // 判断fragment栈中的个数，如果只有一个，则表示当前只处于视频编辑主页面点击返回
        int backStackEntryCount = getSupportFragmentManager().getBackStackEntryCount();
        if (backStackEntryCount == 1) {
            MiniVideoEditFragment fragment = (MiniVideoEditFragment) getSupportFragmentManager()
                    .findFragmentByTag(FRAGMENT_VIDEO_EDIT);
            if (fragment != null) {
                fragment.onBackPressed();
            }
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        int backStackEntryCount = getSupportFragmentManager().getBackStackEntryCount();
        if (backStackEntryCount == 1) {
            MiniVideoEditFragment fragment = (MiniVideoEditFragment) getSupportFragmentManager()
                    .findFragmentByTag(FRAGMENT_VIDEO_EDIT);
            if (fragment != null) {
                fragment.setOnSelectMusicListener(null);
            }
        }
        super.onDestroy();
    }

    @Override
    public void onOpenMusicSelectPage() {
        MusicSelectFragment fragment = new MusicSelectFragment();
        fragment.addOnMusicSelectedListener(this);
        getSupportFragmentManager()
                .beginTransaction()
                .setCustomAnimations(R.anim.anim_slide_up, 0)
                .add(R.id.fragment_content, fragment)
                .addToBackStack(FRAGMENT_MUSIC_SELECT)
                .commit();
    }

    @Override
    public void onOpenEditPreviewPage() {
        EditPreviewFragment fragment = new EditPreviewFragment();
        fragment.setVideoPath(mVideoPath);
        getSupportFragmentManager()
                .beginTransaction()
                .setCustomAnimations(R.anim.anim_slide_up, 0)
                .add(R.id.fragment_content, fragment)
                .addToBackStack(FRAGMENT_EDIT_PREVIEW)
                .commit();
    }
    @Override
    public void onMusicSelected(Music music) {
        getSupportFragmentManager().popBackStack(FRAGMENT_VIDEO_EDIT, 0);
        MiniVideoEditFragment fragment = (MiniVideoEditFragment) getSupportFragmentManager()
                .findFragmentByTag(FRAGMENT_VIDEO_EDIT);
        if (fragment != null) {
            fragment.setSelectedMusic(music.getSongUrl(), music.getDuration());
        }
    }
}
