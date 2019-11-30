package com.mengya.game;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.location.LocationManager;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import com.alibaba.fastjson.JSON;
import com.baidu.location.BDAbstractLocationListener;
import com.baidu.location.BDLocation;
import com.baidu.location.LocationClient;
import com.baidu.location.LocationClientOption;

import org.cocos2dx.lib.Cocos2dxLuaJavaBridge;

import java.util.HashMap;
import java.util.Map;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

class  MyLocationListener extends BDAbstractLocationListener {
    @Override
    public void onReceiveLocation(BDLocation bdLocation) {
        if (bdLocation == null) {
            return;
        }
        Map map = new HashMap();
        map.put("locType", bdLocation.getLocType());
        map.put("latitude", bdLocation.getLatitude());
        map.put("longitude", bdLocation.getLongitude());
        map.put("addr", bdLocation.getAddrStr());
        map.put("country", bdLocation.getCountry());
        map.put("province", bdLocation.getProvince());
        map.put("city", bdLocation.getCity());
        map.put("district", bdLocation.getDistrict());
        map.put("street", bdLocation.getStreet());

        String locationStr = JSON.toJSONString(map);
        BaiduLocationService.invokeCallBack(locationStr);
    }
}

public class BaiduLocationService {
    private static Activity _activity = null;
    private static LocationClient _client = null;
    private static LocationClientOption _customOption;
    private static Object _objLock;
    private static LocationManager _locationManager;
    private static final int BAIDU_READ_PHONE_STATE = 100;//定位权限请求
    private static final int PRIVATE_CODE = 1315;//开启GPS权限
    private static final String[] LOCATIONGPS = new String[]{
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.READ_PHONE_STATE};
    private static int _callBack = 0;
    private static MyLocationListener _listener;

    public static void init(Activity activity){
        _activity = activity;
        _objLock = new Object();
        _customOption = new LocationClientOption();
        synchronized (_objLock) {
            if (_client == null) {
                _client = new LocationClient(_activity);
            }
        }
    }

    public static void initOptions(String str){
        HashMap hashMap = JSON.parseObject(str, HashMap.class);
        _customOption.setLocationMode(LocationClientOption.LocationMode.Hight_Accuracy); // 可选，默认高精度，设置定位模式，高精度，低功耗，仅设备
        _customOption.setCoorType( "bd09ll" ); // 可选，默认gcj02，设置返回的定位结果坐标系，如果配合百度地图使用，建议设置为bd09ll;
        if(hashMap.get("ScanSpan") != null){
            // 可选，默认0，即仅定位一次，设置发起连续定位请求的间隔需要大于等于1000ms才是有效的
            _customOption.setScanSpan((int)hashMap.get("ScanSpan"));
        }
        if(hashMap.get("NeedAddress") != null){
            // 可选，设置是否需要地址信息，默认不需要
            _customOption.setIsNeedAddress((boolean)hashMap.get("NeedAddress"));
        }

        if(hashMap.get("NeedLocationDescribe") != null){
            // 可选，设置是否需要地址描述
            _customOption.setIsNeedLocationDescribe((boolean)hashMap.get("NeedLocationDescribe"));
        }

        if(hashMap.get("NeedDeviceDirect") != null){
            // 可选，设置是否需要设备方向结果
            _customOption.setNeedDeviceDirect((boolean)hashMap.get("NeedDeviceDirect"));
        }

        if(hashMap.get("LocationNotify") != null){
            // 可选，默认false，设置是否当gps有效时按照1S1次频率输出GPS结果
            _customOption.setLocationNotify((boolean)hashMap.get("LocationNotify"));
        }

        if(hashMap.get("IgnoreKillProcess") != null){
            // 可选，默认true，定位SDK内部是一个SERVICE，并放到了独立进程，设置是否在stop
            _customOption.setIgnoreKillProcess((boolean)hashMap.get("LocationNotify"));
        }

        if(hashMap.get("NeedLocationPoiList") != null){
            // 可选，默认false，设置是否需要POI结果，可以在BDLocation
            _customOption.setIsNeedLocationPoiList((boolean)hashMap.get("NeedLocationPoiList"));
        }

        if(hashMap.get("IgnoreCacheException") != null){
            // 可选，默认false，设置是否收集CRASH信息，默认收集
            _customOption.SetIgnoreCacheException((boolean)hashMap.get("IgnoreCacheException"));
        }

        if(hashMap.get("OpenGps") != null) {
            // 可选，默认false，设置是否开启Gps定位
            _customOption.setOpenGps(true);
        }

        if(hashMap.get("NeedAltitude") != null) {
            // 可选，默认false，设置定位时是否需要海拔信息，默认不需要，除基础定位版本都可用
            _customOption.setIsNeedAltitude((boolean)hashMap.get("NeedAltitude"));
        }
        _client.setLocOption(_customOption);
        _listener = new MyLocationListener();
        registerListener(_listener);
    }

    //检查GPS是否开启 6.0还需要判断权限是否开启
    //code 0:开启 1:GPS未开启 2:GPS权限未开启
    public static int isSupportGps() {
        _locationManager = (LocationManager) _activity.getSystemService(_activity.LOCATION_SERVICE);
        boolean ok = _locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER);
        if (!ok) {
            return 1;
        }
        //判断是否为android6.0系统版本，如果是，需要动态添加权限
        if (Build.VERSION.SDK_INT >= 23) {
            if (_activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                    != PERMISSION_GRANTED) {// 没有权限，申请权限。
                return 2;
            }
        }
        return 0;
    }

    //跳转到开启GPS的面板
    public static void jumpEnableGps(){
        Intent intent = new Intent();
        intent.setAction(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        _activity.startActivityForResult(intent, PRIVATE_CODE);
    }

    //跳转到允许GPS权限的面板
    public static void jumpEnableLimitGps(){
        if (Build.VERSION.SDK_INT >= 23) {
            _activity.requestPermissions(LOCATIONGPS, BAIDU_READ_PHONE_STATE);
        }
    }

    //开启GPS服务
    public static void start(int callBack){
        synchronized (_objLock) {
            if (_client != null && !_client.isStarted()) {
                _client.start();
            }
        }
        if(_callBack != callBack){
            _callBack = callBack;
            Cocos2dxLuaJavaBridge.retainLuaFunction(_callBack);
        }
    }
    //关闭GPS服务
    public static void stop(){
        synchronized (_objLock) {
            if (_client != null && _client.isStarted()) {
                _client.stop();
                Cocos2dxLuaJavaBridge.releaseLuaFunction(_callBack);
                _callBack = 0;
            }
        }
    }

    //GPS服务是否已经开启
    public static boolean isStart(){
        return _client.isStarted();
    }

    public static void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case PRIVATE_CODE:
                Log.d("FYD","onActivityResult PRIVATE_CODE");
                break;

        }
    }

    //权限定位请求回调结果
    public static void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
            // requestCode即所声明的权限获取码，在checkSelfPermission时传入
            case BAIDU_READ_PHONE_STATE:
                //如果用户取消，permissions可能为null.
                if (grantResults[0] == PERMISSION_GRANTED && grantResults.length > 0) {  //有权限
                    // 获取到权限，作相应处理
                    Log.d("FYD","GPS权限开启");
                } else {
                    Log.d("FYD","GPS权限没有开启");
                }
                break;
            default:
                break;
        }
    }

    //开始定位
    public static void requestLocation(int callBack) {
        if (_client != null) {
            if(_callBack != callBack){
                _callBack = callBack;
                Cocos2dxLuaJavaBridge.retainLuaFunction(_callBack);
            }
            _client.requestLocation();
        }
    }

    public static void invokeCallBack(String locationStr){
        Cocos2dxLuaJavaBridge.callLuaFunctionWithStringSafe(_callBack,locationStr);
        if(_customOption.getScanSpan() < 1000){
            Cocos2dxLuaJavaBridge.releaseLuaFunction(_callBack);
            _callBack = 0;
        }
    }

    /***
     * 注册定位监听
     *
     * @param listener
     * @return
     */

    public static boolean registerListener(BDAbstractLocationListener listener) {
        boolean isSuccess = false;
        if (isSuccess == false && listener != null) {
            _client.registerLocationListener(listener);
            isSuccess = true;
        }
        return isSuccess;
    }

    public static void unregisterListener(BDAbstractLocationListener listener) {
        if (listener != null) {
            _client.unRegisterLocationListener(listener);
        }
    }
}
