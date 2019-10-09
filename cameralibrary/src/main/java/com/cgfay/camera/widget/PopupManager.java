package com.cgfay.camera.widget;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import com.cgfay.cameralibrary.R;

import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.List;

/**
 * description:
 * Created by aserbao on 2018/5/15.
 */
public class PopupManager {
    private Context mContext;
    public String mV = "30";
    private boolean isClick = false;

    public PopupManager(Context context) {
        mContext = context;
    }
    //显示倒计时
    public void showCountDown(Resources res, int mStartTime, final SelTimeBackListener pop) {
        View showCountDown = LayoutInflater.from(mContext).inflate(R.layout.pop_window_count_down, null);
        final PopupWindow mShowCountDownPW = new PopupWindow(showCountDown.getRootView(), ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mShowCountDownPW.setOutsideTouchable(true);
        mShowCountDownPW.setFocusable(true);
        mShowCountDownPW.setAnimationStyle(R.style.expression_dialog_anim_style);
        mShowCountDownPW.setBackgroundDrawable(new BitmapDrawable());
        mShowCountDownPW.showAtLocation(showCountDown.getRootView(), Gravity.BOTTOM, 0, 0);
        final ThumbnailCountDownTimeView thumbnailCountDownTimeView = (ThumbnailCountDownTimeView) showCountDown.findViewById(R.id.thumb_count_view);
        final TextView selTimeTv = (TextView) showCountDown.findViewById(R.id.sel_time_tv);
        selTimeTv.setText("30s");

        DisplayMetrics displayMetrics = mContext.getApplicationContext().getResources().getDisplayMetrics();
        int min = mStartTime * (displayMetrics.widthPixels - (int) res.getDimension(R.dimen.dp20)) / 30;
        selTimeTv.layout(min, 0, selTimeTv.getHeight(), selTimeTv.getWidth());
        selTimeTv.setTranslationX((displayMetrics.widthPixels - (int) res.getDimension(R.dimen.dp40)));
        thumbnailCountDownTimeView.setMinWidth(min);
        final DecimalFormat fnum = new DecimalFormat("##0.0");
        mShowCountDownPW.setOnDismissListener(new PopupWindow.OnDismissListener() {
            @Override
            public void onDismiss() {
                pop.selTime(mV, isClick);
            }
        });
        thumbnailCountDownTimeView.setOnScrollBorderListener(new ThumbnailCountDownTimeView.OnScrollBorderListener() {
            @Override
            public void OnScrollBorder(float start, float end) {
                selTimeTv.setTranslationX(start - 10);
                mV = fnum.format((end * 30 / thumbnailCountDownTimeView.getWidth()));
                selTimeTv.setText(mV + "s");
            }

            @Override
            public void onScrollStateChange() {

            }
        });
        showCountDown.findViewById(R.id.count_down_btn).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                isClick = true;
                mShowCountDownPW.dismiss();
            }
        });
    }

    public interface SelTimeBackListener {
        void selTime(String selTime, boolean isDismiss);
    }

}
