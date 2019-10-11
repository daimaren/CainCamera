package com.cgfay.caincamera.activity;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import com.cgfay.caincamera.R;
import com.cgfay.caincamera.fragment.FFMediaRecordFragment;
import com.cgfay.caincamera.fragment.FLVRecordFragment;

public class FLVRecordActivity extends AppCompatActivity {

    private static final String FRAGMENT_FLV_RECORD = "FRAGMENT_FLV_RECORD";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_ffmedia_record);
        if (null == savedInstanceState) {
            FLVRecordFragment fragment = new FLVRecordFragment();
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.fragment_container, fragment, FRAGMENT_FLV_RECORD)
                    .commit();
        }
    }

}
