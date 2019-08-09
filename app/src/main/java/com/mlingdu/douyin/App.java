package com.mlingdu.douyin;

import android.content.Context;
import android.graphics.Bitmap;
import com.mlingdu.douyin.common.utils.hookpms.ServiceManagerWraper;
import com.mlingdu.douyin.common.utils.SpUtils;
import com.mlingdu.douyin.common.utils.Utils;
import com.mlingdu.douyin.common.utils.CommonUtils;
import com.mlingdu.douyin.common.base.CygApplication;
import com.ss.android.common.applog.GlobalContext;
import com.ss.android.common.applog.UserInfo;
/**
 * Created by jack on 2017/6/13
 */
public class App extends CygApplication {

    public static String IMEI;
    public static String PACKAGE_NAME;
    public static String VERSION_NAME;
    public static String CHANNEL_ID;
    public static String ANDROID_ID;
    public static String SERIAL_NO;

    @Override
    public void onCreate() {
        super.onCreate();
        ServiceManagerWraper.hookPMS(this.getApplicationContext());

        SpUtils.init(this);
        //缓存起来防止每次网络请求都去拿
        PACKAGE_NAME = CommonUtils.getProcessName();
        VERSION_NAME = Utils.getHasDotVersion(App.getInstance());
        CHANNEL_ID = CommonUtils.getMetaData(App.getInstance(), "BaiduMobAd_CHANNEL");
        IMEI = Utils.getDeviceIMEI(App.getInstance());
        ANDROID_ID = Utils.getDeviceAndroidId(App.getInstance());
        SERIAL_NO = Utils.getSerialNo();

        GlobalContext.setContext(getApplicationContext()); //Hook 抖音

        try {
            System.loadLibrary("userinfo");//抖音&火山
        } catch (Exception e) {
            e.printStackTrace();
        }

        UserInfo.setAppId(2);
        int result = UserInfo.initUser("a3668f0afac72ca3f6c1697d29e0e1bb1fef4ab0285319b95ac39fa42c38d05f");
    }

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
    }

}
