package com.cgfay.caincamera.activity;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import com.cgfay.caincamera.R;
import com.cgfay.caincamera.fragment.MediaRecordFragment;

public class MediaRecordActivity extends AppCompatActivity {

    private static final String FRAGMENT_MEDIA_RECORD = "FRAGMENT_MEDIA_RECORD";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_media_record);
        if (null == savedInstanceState) {
            MediaRecordFragment fragment = new MediaRecordFragment();
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.fragment_container, fragment, FRAGMENT_MEDIA_RECORD)
                    .commit();
        }
    }
}
