//
//  BaiduLocationService.h
//  QLClient-mobile
//
//  Created by JingFeng on 2019/11/30.
//
#import <Foundation/Foundation.h>
#import <BMKLocationKit/BMKLocationComponent.h>
#import <CoreLocation/CLLocationManager.h>
@interface BaiduLocationService : NSObject <BMKLocationManagerDelegate>

+(BaiduLocationService*)getInstance;
//初始化百度SDK
+(void)initOptions:(NSDictionary*)options;
-(void)initOptions:(NSDictionary*)options;
//是否支持GPS 0:开启 1:GPS未开启 2:GPS权限未开启
+(int)isSupportGps;
//跳转到开启GPS的面板
+(void)jumpEnableGps;
//动态申请GPS权限
+(void)jumpEnableLimitGps;
//开始定位 单次定位
+(void)start:(NSDictionary*)options;
//开始定位 连续定位
+(void)startUpdate:(NSDictionary*)options;
//停止连续定位回调
+(void)stopUpdate;
+(void) callBackWithFuncID:(int) funcId andParams:(NSDictionary*) dic;
//连续定位回调
- (void)BMKLocationManager:(BMKLocationManager * _Nonnull)manager didUpdateLocation:(BMKLocation * _Nullable)location orError:(NSError * _Nullable)error;
@end
