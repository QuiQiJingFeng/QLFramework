#include "Downloader.h"
#include <curl/curl.h>
#include "LuaCBridge.h"
#include <unistd.h>
#include "cocos2d.h"
#include <thread>
long CONNECT_TIME_OUT = 10L;
struct UserData{
    long luaHandler = 0;
    long alreadyDown = 0;
    long fileLength = 0;
    bool isCancel = false;
};
struct HeaderInfo{
    char * ptr = nullptr;
    long size = 0;
};
enum LUA_CALLBACK_TYPE{
    PROCESS,
    DOWNLOAD_FAILED,
    DOWNLOAD_SUCCESS,
};
Downloader* Downloader::__instance = nullptr;
long global_already_down = 0;
Downloader* Downloader::getInstance()
{
    if(__instance == nullptr){
        __instance = new Downloader();
    }
    
    return __instance;
}

void Downloader::init(){
    curl_global_init(CURL_GLOBAL_ALL);
	curl_version_info_data* version_info = curl_version_info(CURLVERSION_NOW);
    printf("age = %d\n",version_info->age);
    printf("version = %s\n",version_info->version);
    printf("version_num = %d\n",version_info->version_num);
    printf("host = %s\n",version_info->host);
    printf("features = %d\n",version_info->features);
    printf("ssl_version = %s\n",version_info->ssl_version);
}

void Downloader::dispose(){
    curl_global_cleanup();
}

size_t write_data (void * buffer,size_t size,size_t nmemb,void * userp){
    FILE* file = (FILE*)userp;
    size_t r_size = fwrite(buffer, size, nmemb, file);
    return r_size;
}

static size_t header_callback(char* buffer,size_t size,size_t nitems,void* userdata){
    auto headerInfo = (HeaderInfo *)userdata;
    if(headerInfo->ptr == nullptr){
        headerInfo->ptr = new char[nitems * size];
    }else{
        char * temp = new char[headerInfo->size + nitems * size];
        for (long i = 0; i < headerInfo->size; i++) {
            temp[i] = headerInfo->ptr[i];
        }
        delete []headerInfo->ptr;
        headerInfo->ptr = temp;
    }
 
    
    for (long i = 0; i < nitems * size; i++) {
        headerInfo->ptr[headerInfo->size + i] = buffer[i];
    }
    headerInfo->size += nitems * size;

    return nitems * size;
}

FValueMap Downloader::getHttpInfo(const char* url){
    FValueMap map;
    //此句柄不可以在多线程共享
    CURL* curlHandle = curl_easy_init();
    if (nullptr == curlHandle){
        map["errormessage"] = FValue("ERROR: Alloc curl handle failed.");
		return map;
    }
    //设置下载的url
    curl_easy_setopt(curlHandle, CURLOPT_URL,url);
    //是否将http头写到CURLOPT_WRITEFUNCTION方法中 1是写入 0是不写入,默认为0
    curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0);
    //要求libcurl在写回调（CURLOPT_WRITEFUNCTION）中包含标头
    curl_easy_setopt(curlHandle,CURLOPT_HEADERFUNCTION,header_callback);
    HeaderInfo headerInfo;
    curl_easy_setopt(curlHandle, CURLOPT_WRITEHEADER,&headerInfo);
    
    //不需求body
	curl_easy_setopt(curlHandle, CURLOPT_NOBODY, true);

    curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT, CONNECT_TIME_OUT);

    //不验证SSL证书
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, false);

	CURLcode code = curl_easy_perform(curlHandle);
    if(CURLE_OK != code){
        map["errormessage"] = FValue(curl_easy_strerror(code));
        return map;
    }

    long retcode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE , &retcode);
    if(CURLE_OK != code){
        map["errormessage"] = FValue(curl_easy_strerror(code));
        return map;
    }
    if (!(retcode >= 200 && retcode < 300)){
        map["errormessage"] = FValue(curl_easy_strerror(code));
        return map;
    }
    
    long fileLength = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fileLength);
    if(CURLE_OK == code){
        map["fileLength"] = FValue((float)fileLength);
    }else{
        map["errormessage"] = FValue(curl_easy_strerror(code));
        return map;
    }
    
    string header = headerInfo.ptr;
    string::size_type position;
    position = header.find("Accept-Ranges: bytes");
    if (position != std::string::npos)
    {
        map["Accept-Ranges"] = FValue(true);
    }else{
        map["Accept-Ranges"] = FValue(false);
    }
    delete headerInfo.ptr;
    return map;
}

static int progressCURL(void* userdata, curl_off_t TotalToDownload, curl_off_t NowDownloaded, 
     curl_off_t TotalToUpload, curl_off_t NowUploaded)
{
    UserData* data = (UserData*) userdata;
//    if(TotalToDownload == 0) TotalToDownload = data->fileLength;
    if(TotalToDownload != 0){
        NowDownloaded += data->alreadyDown;
        TotalToDownload += data->alreadyDown;
        float process = (NowDownloaded) *1.0 / TotalToDownload * 100;
        int handler = data->luaHandler;
        cocos2d::Scheduler *sched = cocos2d::Director::getInstance()->getScheduler();
        sched->performFunctionInCocosThread( [=](){
            FValueVector vector;
            vector.push_back(FValue((int)LUA_CALLBACK_TYPE::PROCESS));
            FValueMap map;
            map["process"] = FValue(process);
            vector.push_back(FValue(map));
            FValue ret = LuaCBridge::getInstance()->executeFunctionByRetainId(handler, vector);
            FValueMap retMap = ret.asFValueMap();
            if(retMap["errorcode"].asInt() == 0){
                //返回非0值会撤销任务
                if(!retMap["result"].isNull()){
                    bool isCalcel = retMap["result"].asBool();
                    if(isCalcel){
                        data->isCancel = true;
                    }
                }
            }
        });
        
        if(data->isCancel){
            return 1;
        }
    }
  return 0;
}


FValue Downloader::createSimgleTask(FValueVector vector){
    string url = vector[0].asString();
    string savePath = vector[1].asString();
    long callFunc = vector[2].asFloat();
    
    std::thread task(&Downloader::createSimgleTaskInterNal,url,savePath,callFunc);
    task.detach();
    return FValue(true);
};

void Downloader::reportDownloadErrorToLua(const string errormessage,int handler,string url,string savePath){
    cocos2d::Scheduler *sched = cocos2d::Director::getInstance()->getScheduler();
    string message = errormessage;
    string urlPath = url;
    string path = savePath;
    sched->performFunctionInCocosThread( [=](){
        FValueVector vector;
        vector.push_back(FValue((int)LUA_CALLBACK_TYPE::DOWNLOAD_FAILED));
        FValueMap map;
        map["errormessage"] = FValue(message);
        map["url"] = FValue(urlPath);
        map["savePath"] = FValue(path);
        vector.push_back(FValue(map));
        LuaCBridge::getInstance()->executeFunctionByRetainId(handler, vector);
    });
}

void Downloader::reportDownloadSuccessToLua(int handler,string url,string savePath){
    cocos2d::Scheduler *sched = cocos2d::Director::getInstance()->getScheduler();
    sched->performFunctionInCocosThread( [=](){
        FValueVector vector;
        vector.push_back(FValue((int)LUA_CALLBACK_TYPE::DOWNLOAD_SUCCESS));
        FValueMap map;
        map["url"] = FValue(url);
        map["savePath"] = FValue(savePath);
        vector.push_back(FValue(map));
        LuaCBridge::getInstance()->executeFunctionByRetainId(handler, vector);
    });
}

//easy系列接口使您可以通过同步和阻塞功能调用进行单次下载
bool Downloader::createSimgleTaskInterNal(string strUrl,string strPath,long luaCallBack){
    const char * url = strUrl.c_str();
    string tempPath = (strPath + ".download");
    FValueMap info = Downloader::getHttpInfo(url);
    
    if(info.find("errormessage") != info.end()){
        Downloader::reportDownloadErrorToLua(info["errormessage"].asString(),luaCallBack,strUrl,strPath);
        return false;
    }
    
    FILE * file;
    long alreadydownload = 0;
    if(info["Accept-Ranges"].asBool()){
        //断点续传
        int result = access((strPath + ".download").c_str(), F_OK);
        //如果文件已经存在,那么检测一下文件大小,用来做断点续传
        if(result == 0){
            FILE * file = fopen(tempPath.c_str(), "rb");
            fseek(file,0,SEEK_END);
            long size = ftell(file);
            alreadydownload = size;
            global_already_down = size;
            fclose(file);
        }
        file = fopen(tempPath.c_str(),"ab+");
        if (file == NULL) {
            fclose(file);
            Downloader::reportDownloadErrorToLua("Error opening file\n",luaCallBack,strUrl,strPath);
            return false;
        };
    }else{
        file = fopen(tempPath.c_str(),"wb");
        if (file == NULL) {
            fclose(file);
            Downloader::reportDownloadErrorToLua("Error opening file\n",luaCallBack,strUrl,strPath);
            return false;
        };
    }
    //此句柄不可以在多线程共享
    CURL* curlHandle = curl_easy_init();
    if (nullptr == curlHandle){
        curl_easy_cleanup(curlHandle);
        Downloader::reportDownloadErrorToLua("ERROR: Alloc curl handle failed.",luaCallBack,strUrl,strPath);
		return false;
    }
    //设置下载的url
    curl_easy_setopt(curlHandle, CURLOPT_URL,url);
    
    if(info["Accept-Ranges"].asBool()){
        //断点重下载
        /*
         分别测试了三种情况,一种是不支持断点续传的 比如用python 一键搭建的静态服务器
         一种是只支持CURLOPT_RANGE的,比如阿里云的OSS
         一种是两个都支持的 比如我用nodejs的express框架搭建的一个web服务器
         因为要做一个通用的下载器,所以这些情况必须要区分出来,否则很容易导致curl返回一个不支持的错误码
         */
        //方式1
        curl_easy_setopt(curlHandle, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)alreadydownload);
        //方式2
        //char temp[100];
        //sprintf(temp,"%ld-",alreadydownload);
        //curl_easy_setopt(curlHandle,CURLOPT_RANGE,temp);
    }
    
    //设置重定位URL，使用自动跳转，返回的头部中有Location(一般直接请求的url没找到)，则继续请求Location对应的数据
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_MAXREDIRS,5);//查找次数，防止查找太深
    
    //不验证SSL证书
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, false);
    //超时
    curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT, CONNECT_TIME_OUT);
    //去掉超时时间设置,因为大文件下载时间过长,这个普通的HTTP请求不同,所以不能设置这个
    //否则文件一大就超时
//    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, DEFAULT_TIMEOUT);
//    curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1);

    //进度回调方法
    UserData* userdata = new UserData;
    userdata->luaHandler = luaCallBack;
    userdata->alreadyDown = alreadydownload;
    userdata->fileLength = info["fileLength"].asFloat();
    
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFOFUNCTION, progressCURL);
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFODATA, userdata);
    curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 0L);
    
#if(CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
#else
    //设置接收数据的方法如果不设置,那么libcurl将会默认将数据输出到stdout
    curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,write_data);
#endif
    string erromessage;
    //设置write_data方法第四个参数获取的指针
    //使用这个属性可以在应用程序和libcurl调用的函数之间传递自定义数据
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, file);
	CURLcode code = curl_easy_perform(curlHandle);
    if(CURLE_OK != code){
        erromessage = curl_easy_strerror(code);
        Downloader::reportDownloadErrorToLua(erromessage,luaCallBack,strUrl,strPath);
        return false;
    }
 
    long retcode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE , &retcode);
    if ((code != CURLE_OK) || !(retcode >= 200 && retcode < 300) )
    {
        erromessage = curl_easy_strerror(code);
        Downloader::reportDownloadErrorToLua(erromessage,luaCallBack,strUrl,strPath);
        return false;
    }
    curl_easy_cleanup(curlHandle);
    fclose(file);
    
    string oldPath = cocos2d::FileUtils::getInstance()->fullPathForFilename(tempPath);
    string newPath = cocos2d::FileUtils::getInstance()->fullPathForFilename(strPath);
    cocos2d::FileUtils::getInstance()->renameFile(oldPath, newPath);
    Downloader::reportDownloadSuccessToLua(luaCallBack,strUrl,strPath);
    
    cocos2d::Scheduler *sched = cocos2d::Director::getInstance()->getScheduler();
    sched->performFunctionInCocosThread( [userdata](){
        delete userdata;
    });
        
    return true;
}

/*  单文件下载
    Downloader::init();
    char* url = "https://lsjgame.oss-cn-hongkong.aliyuncs.com/1.0.5/package_src_test.zip";
    FILE * pFile;
    pFile = fopen ("package_src_test.zip" , "wb+");
    if (pFile == NULL) perror ("Error opening file");
    Downloader::curlTest(url,pFile);
*/
