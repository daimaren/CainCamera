package com.cgfay.caincamera.activity;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import com.cgfay.caincamera.R;
import com.cgfay.caincamera.fragment.MiniRecordFragment;

public class MiniRecordActivity extends AppCompatActivity {

    private static final String FRAGMENT_MEDIA_RECORD = "FRAGMENT_MEDIA_RECORD";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_media_record);
        if (null == savedInstanceState) {
            MiniRecordFragment fragment = new MiniRecordFragment();
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.fragment_container, fragment, FRAGMENT_MEDIA_RECORD)
                    .commit();
        }
    }
}
