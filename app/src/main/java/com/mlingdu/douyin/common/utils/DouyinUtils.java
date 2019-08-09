package com.mlingdu.douyin.common.utils;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.util.Log;

import com.ss.android.common.applog.UserInfo;

import java.util.HashMap;
import java.util.Map.Entry;

public class DouyinUtils {
    //as/cp算法只能用于169版本之前，新版mas算法先不处理
    public static String appVersionCode = "168";
    public static String appVersionName = "1.6.8";

    public static final String PKGNAME = "com.ss.android.ugc.aweme";

    private final static String GETDATA_JSON_URL = "https://api.amemv.com/aweme/v1/feed/";
    private final static String URL = "https://aweme-eagle.snssdk.com/aweme/v1/feed/";

    /**
     * 下拉数据规律：min_cursor=max_cursor=0
     * <p>
     * 上拉数据规律：
     * 第二次请求取第一次请求返回的json数据中的min_cursor字段，max_cursor不需要携带。
     * 第三次以及后面所有的请求都只带max_cursor字段，值为第一次请求返回的json数据中的max_cursor字段
     *
     * @return
     */
    public static String getEncryptUrl(Activity act, long minCursor, long maxCursor) {
        String url = null;
        int time = (int) (System.currentTimeMillis() / 1000);
        try {
            HashMap<String, String> paramsMap = getCommonParams(act);
            String[] paramsAry = new String[paramsMap.size() * 2];
            int i = 0;
            for (Entry<String, String> entry : paramsMap.entrySet()) {
                paramsAry[i] = entry.getKey();
                i++;
                paramsAry[i] = entry.getValue();
                i++;
            }

            paramsMap.put("count", "6");
            paramsMap.put("type", "0");
            paramsMap.put("retry_type", "no_retry");
            paramsMap.put("ts", "" + time);

            if (minCursor >= 0) {
                paramsMap.put("max_cursor", minCursor + "");
            }
            if (maxCursor >= 0) {
                paramsMap.put("min_cursor", maxCursor + "");
            }

            StringBuilder paramsSb = new StringBuilder();
            for (String key : paramsMap.keySet()) {
                paramsSb.append(key + "=" + paramsMap.get(key) + "&");
            }
            String urlStr = GETDATA_JSON_URL + "?" + paramsSb.toString();
            if (urlStr.endsWith("&")) {
                urlStr = urlStr.substring(0, urlStr.length() - 1);
            }

            String as_cp = UserInfo.getUserInfo(time, urlStr, paramsAry);

            String asStr = as_cp.substring(0, as_cp.length() / 2);
            String cpStr = as_cp.substring(as_cp.length() / 2, as_cp.length());

            url = urlStr + "&as=" + asStr + "&cp=" + cpStr;

        } catch (Exception e) {
            Log.i("jw", "get url err:" + Log.getStackTraceString(e));
        }
        return url;
    }
    /**
     * 公共参数
     *
     * @return
     */
    @SuppressLint("DefaultLocale")
    public static HashMap<String, String> getCommonParams(Activity act) {
        HashMap<String, String> params = new HashMap<String, String>();
        params.put("iid", "58496817843");
        params.put("channel", "xiaomi");
        params.put("aid", "1128");
        params.put("uuid", Utils.getDeviceUUID(act));
        params.put("openudid", "b3fed31de11972de");
        params.put("app_name", "aweme");
        params.put("version_code", appVersionCode);
        params.put("version_name", appVersionName);
        params.put("ssmix", "a");
        params.put("manifest_version_code", appVersionCode);
        params.put("device_type", Utils.getDeviceName());
        params.put("device_brand", Utils.getDeviceFactory());
        params.put("os_api", Utils.getOSSDK());
        params.put("os_version", Utils.getOSRelease());
        params.put("resolution", Utils.getDeviceWidth(act) + "*" + Utils.getDeviceHeight(act));
        params.put("dpi", Utils.getDeviceDpi(act) + "");
//        params.put("device_id", "34971691517");
        params.put("device_id", "52047126588");
//		params.put("ac", NetworkUtil.getNetworkType(GlobalContext.getContext()).toLowerCase());
        params.put("ac", "wifi");
        params.put("device_platform", "android");
        params.put("update_version_code", "1682");
        params.put("app_type", "normal");
        return params;
    }

    @SuppressLint("DefaultLocale")
    public static HashMap<String, String> getHuaweiParams(Activity act) {
        int time = (int) (System.currentTimeMillis() / 1000);
        HashMap<String, String> params = new HashMap<String, String>();
        params.put("js_sdk_version", "");
        params.put("ts", "" + time);
        params.put("is_cold_start", "1");
        params.put("ssmix", "a");
        params.put("volume", "0.5333333333333333");
        params.put("pull_type", "0");
        params.put("need_relieve_aweme", "0");
        params.put("filter_warn", "0");
        params.put("req_from", "enter_auto");
        params.put("_rticket", "1550304035356");
        params.put("mcc_mnc", "");
        params.put("longitude", "0");
        params.put("latitude", "0");
        params.put("iid", "63518423481");
        params.put("channel", "huawei");
        params.put("aid", "1128");
        params.put("uuid", "862917031706577");
        params.put("openudid", "8b4cfaef4096931");
        params.put("app_name", "aweme");
        params.put("version_code", "500");
        params.put("version_name", "5.0.0");
        params.put("ssmix", "a");
        params.put("manifest_version_code", "500");
        params.put("device_type", "NEM-AL10");
        params.put("device_brand", "HONOR");
        params.put("os_api", "23");
        params.put("os_version", "6.0");
        params.put("resolution", "1080*1812");
        params.put("dpi", "480");
        params.put("device_id", "66331521630");
        params.put("ac", "wifi");
        params.put("device_platform", "android");
        params.put("update_version_code", "5002");
        params.put("app_type", "normal");
        params.put("language", "zh");
        return params;
    }

}
