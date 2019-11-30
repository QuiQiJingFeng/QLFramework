//
//  BaiduLocationService.mm
//  QLClient-mobile
//
//  Created by JingFeng on 2019/11/30.
//

#import "BaiduLocationService.h"
#include "scripting/lua-bindings/manual/platform/ios/CCLuaObjcBridge.h"
#include "cocos2d.h"
static BaiduLocationService * _instance;
static BMKLocationManager  * _locationManager;
static CLLocationManager* _cclLocationManager;
static int _updateCallBack = 0;
@implementation BaiduLocationService
+(BaiduLocationService*)getInstance{
    //线程锁
    @synchronized(self){
        if(_instance == nil){
            _instance = [[BaiduLocationService alloc] init];
        }
    }
    return _instance;
}
-(void)initOptions:(NSDictionary*)options{
    if(_locationManager == nil){
        _locationManager = [[BMKLocationManager alloc] init];
        _locationManager.delegate = self;
    }
    
    _locationManager.coordinateType = BMKLocationCoordinateTypeBMK09LL;
    _locationManager.distanceFilter = [options[@"distanceFilter"] doubleValue];
    _locationManager.desiredAccuracy = [options[@"desiredAccuracy"] doubleValue];
    _locationManager.activityType = CLActivityTypeAutomotiveNavigation;
    _locationManager.pausesLocationUpdatesAutomatically = [options[@"pausesLocationUpdatesAutomatically"] boolValue];
    _locationManager.allowsBackgroundLocationUpdates = [options[@"allowsBackgroundLocationUpdates"] boolValue];
    _locationManager.locationTimeout = [options[@"locationTimeout"] intValue];
    _locationManager.reGeocodeTimeout = [options[@"reGeocodeTimeout"] intValue];
}
//初始化百度SDK
+(void)initOptions:(NSDictionary*)options{
    // 需要注意的是请在 SDK 任何类的初始化以及方法调用之前设置正确的 AK
    [[BMKLocationAuth sharedInstance] checkPermisionWithKey:options[@"appKey"] authDelegate:self];
    [[BaiduLocationService getInstance] initOptions:options];
}
/*
 kCLAuthorizationStatusNotDetermined                  //用户尚未对该应用程序作出选择
 kCLAuthorizationStatusRestricted                     //应用程序的定位权限被限制
 kCLAuthorizationStatusAuthorizedAlways               //一直允许获取定位
 kCLAuthorizationStatusAuthorizedWhenInUse            //在使用时允许获取定位
 kCLAuthorizationStatusAuthorized                     //已废弃，相当于一直允许获取定位
 kCLAuthorizationStatusDenied                         //拒绝获取定位
 */
//是否支持GPS 0:开启 1:GPS未开启 2:GPS权限未开启
+(int)isSupportGps{
    BOOL isEnable = [CLLocationManager locationServicesEnabled];
    if(!isEnable){
        return 1;
    }

    int code = [CLLocationManager authorizationStatus];
    switch (code) {
        case kCLAuthorizationStatusNotDetermined:
            return 2;
        case kCLAuthorizationStatusRestricted:
            return 2;
        case kCLAuthorizationStatusAuthorizedAlways:
            return 0;
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            return 0;
        case kCLAuthorizationStatusDenied:
            return 2;
        default:
            break;
    }
    return 2;
}
//跳转到开启GPS的面板
+(void)jumpEnableGps{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]];
}
//动态申请GPS权限
+(void)jumpEnableLimitGps{
    if(_cclLocationManager == nil){
        _cclLocationManager = [[CLLocationManager alloc] init];
    }
    [_cclLocationManager requestWhenInUseAuthorization];
}
//开始定位
+(void)start:(NSDictionary*)options{
    //单次定位
    int callBack = [options[@"callBack"] intValue];
    cocos2d::LuaObjcBridge::retainLuaFunctionById(callBack);
    [_locationManager requestLocationWithReGeocode:YES withNetworkState:YES completionBlock:^(BMKLocation * _Nullable location, BMKLocationNetworkState state, NSError * _Nullable error) {
        if (error)
        {
            NSLog(@"locError:{%ld - %@};", (long)error.code, error.localizedDescription);
            NSDictionary* dict = [[NSDictionary alloc] initWithDictionary:@{@"errcode":[NSNumber numberWithInteger: error.code],@"descript":error.localizedDescription}];
            [BaiduLocationService callBackWithFuncID:callBack andParams:dict];
            cocos2d::LuaObjcBridge::releaseLuaFunctionById(callBack);
        }else{
            if (location) {//得到定位信息，添加annotation
                double longitude = location.location.coordinate.longitude;
                double latitude = location.location.coordinate.latitude;
                NSString* addr = [location.rgcData locationDescribe];
                NSDictionary* dict = [[NSDictionary alloc] initWithDictionary:@{@"latitude":[NSNumber numberWithDouble:latitude],@"longitude":[NSNumber numberWithDouble:longitude],@"addr":addr}];
                [BaiduLocationService callBackWithFuncID:callBack andParams:dict];
                cocos2d::LuaObjcBridge::releaseLuaFunctionById(callBack);
            }else{
                NSLog(@"error not location");
            }
        }
    }];
}
/*
 由于苹果系统的首次定位结果为粗定位，其可能无法满足需要高精度定位的场景。
 百度提供了 kCLLocationAccuracyBest 参数，设置该参数可以获取到精度在10m左右的定位结果，但是相应的需要付出比较长的时间（10s左右），越高的精度需要持续定位时间越长。
 
 推荐使用kCLLocationAccuracyHundredMeters，一次还不错的定位，偏差在百米左右，超时时间设置在2s-3s左右即可。
 */
+(void)startUpdate:(NSDictionary*)options{
    if(_updateCallBack != 0){
        [self stopUpdate];
    }
    _updateCallBack = [options[@"callBack"] intValue];
    cocos2d::LuaObjcBridge::retainLuaFunctionById(_updateCallBack);
    //如果需要持续定位返回地址信息（需要联网），请设置如下：
    [_locationManager setLocatingWithReGeocode:YES];
    [_locationManager startUpdatingLocation];
}

- (void)BMKLocationManager:(BMKLocationManager * _Nonnull)manager didUpdateLocation:(BMKLocation * _Nullable)location orError:(NSError * _Nullable)error

{
    if (error)
    {
        NSLog(@"locError:{%ld - %@};", (long)error.code, error.localizedDescription);
        NSDictionary* dict = [[NSDictionary alloc] initWithDictionary:@{@"errcode":[NSNumber numberWithInteger: error.code],@"descript":error.localizedDescription}];
        [BaiduLocationService callBackWithFuncID:_updateCallBack andParams:dict];
    }else{
        if (location) {//得到定位信息，添加annotation
            double longitude = location.location.coordinate.longitude;
            double latitude = location.location.coordinate.latitude;
            NSString* addr = [location.rgcData locationDescribe];
            NSDictionary* dict = [[NSDictionary alloc] initWithDictionary:@{@"latitude":[NSNumber numberWithDouble:latitude],@"longitude":[NSNumber numberWithDouble:longitude],@"addr":addr}];
            [BaiduLocationService callBackWithFuncID:_updateCallBack andParams:dict];
        }else{
            NSLog(@"error not location");
        }
    }
}

+(void)stopUpdate{
    cocos2d::LuaObjcBridge::releaseLuaFunctionById(_updateCallBack);
    _updateCallBack = 0;
    [_locationManager stopUpdatingLocation];
}
//返回的时候不要返回bool类型,并且返回值是一个table
+(void) callBackWithFuncID:(int) funcId andParams:(NSDictionary*) dic
{
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([funcId,dic]{
        cocos2d::LuaObjcBridge::pushLuaFunctionById(funcId);
        cocos2d::LuaValueDict item;
        for (NSString *key in dic) {
            if([dic[key] isKindOfClass:[NSString class]]){
                item[[key UTF8String]] = cocos2d::LuaValue::stringValue([dic[key] UTF8String]);
            }else if([dic[key] isKindOfClass:[NSNumber class]]){
                item[[key UTF8String]] = cocos2d::LuaValue::floatValue([dic[key] floatValue]);
            }
        }
        
        cocos2d::LuaObjcBridge::getStack()->pushLuaValueDict(item);
        cocos2d::LuaObjcBridge::getStack()->executeFunction(1);
        int count = (int)[dic retainCount];
        for (int i = 0; i < count; i++) {
            [dic release];
        }
    });
    
}

@end
