#include "Downloader.h"
#include <curl/curl.h>
#include "LuaCBridge.h"
#include <unistd.h>
#include "cocos2d.h"
long DEFAULT_TIMEOUT = 100L;

struct UserData{
    long luaHandler = 0;
    long alreadyDown = 0;
    bool isCancel = false;
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

bool Downloader::checkFileExist(const char* url){
    //此句柄不可以在多线程共享
    CURL* curlHandle = curl_easy_init();
    if (nullptr == curlHandle){
		printf("ERROR: Alloc curl handle failed.");
		return false;
    }
    //设置下载的url
    curl_easy_setopt(curlHandle, CURLOPT_URL,url);

    //不需求body
	curl_easy_setopt(curlHandle, CURLOPT_NOBODY, true);

    curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT, DEFAULT_TIMEOUT);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, DEFAULT_TIMEOUT);

    //不验证SSL证书
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, false);

	int code = curl_easy_perform(curlHandle);
    if(CURLE_OK != code){
        printf("curl easy perform faild code = %d\n",code);
        return false;
    }

    long retcode = 0;
    if(CURLE_OK != curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE , &retcode)){
        return printf("CURLINFO_RESPONSE_CODE failed\n");
    }
    if (retcode != 200){
        printf("retcode = %ld\n",retcode);
        return false;
    }
    return true;
}

static int progressCURL(void* userdata, curl_off_t TotalToDownload, curl_off_t NowDownloaded, 
     curl_off_t TotalToUpload, curl_off_t NowUploaded)
{
    UserData* data = (UserData*)userdata;
    if(TotalToDownload != 0){
        NowDownloaded += data->alreadyDown;
        TotalToDownload += data->alreadyDown;
        float process = (NowDownloaded) *1.0 / TotalToDownload * 100;
        
        cocos2d::Scheduler *sched = cocos2d::Director::getInstance()->getScheduler();
        sched->performFunctionInCocosThread( [=](){
            FValueVector vector;
            vector.push_back(FValue(process));
            FValue ret = LuaCBridge::getInstance()->executeFunctionByRetainId(data->luaHandler, vector);
            //返回非0值会撤销任务
            bool isCalcel = ret.asBool();
            if(isCalcel){
                data->isCancel = true;
            }
        });
        
        if(data->isCancel){
            return 1;
        }
    }
  return 0;
}

//https://curl.haxx.se/libcurl/c/getinmemory.html
//easy系列接口使您可以通过同步和阻塞功能调用进行单次下载
bool Downloader::createSimgleTask(const char* url,const char* savePath,long luaCallBack){
    
    long alreadydownload = 0;
    //断点续传
    int result = access(savePath, F_OK);
    //如果文件已经存在,那么检测一下文件大小,用来做断点续传
    if(result == 0){
        FILE * file = fopen(savePath, "rb");
        fseek(file,0,SEEK_END);
        long size = ftell(file);
        alreadydownload = size;
        global_already_down = size;
        fclose(file);
    }
    
    
    FILE * file;
	file = fopen (savePath,"ab+");
    if (file == NULL) {
        printf("Error opening file\n");
        fclose(file);
        return false;
    };
    
    //此句柄不可以在多线程共享
    CURL* curlHandle = curl_easy_init();
    if (nullptr == curlHandle){
		printf("ERROR: Alloc curl handle failed.");
        curl_easy_cleanup(curlHandle);
		return false;
    }
    //设置下载的url
    curl_easy_setopt(curlHandle, CURLOPT_URL,url);
    //断点重下载
    //阿里云不支持这种方式的断点续传
    //curl_easy_setopt(curlHandle, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)alreadydownload);
    //改用这种方式的断点续传
    char temp[100];
    sprintf(temp,"%ld-",alreadydownload);
    curl_easy_setopt(curlHandle,CURLOPT_RANGE,temp);
    
    //设置重定位URL，使用自动跳转，返回的头部中有Location(一般直接请求的url没找到)，则继续请求Location对应的数据
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_MAXREDIRS,5);//查找次数，防止查找太深
    
    //不验证SSL证书
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, false);
    //超时
    curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT, DEFAULT_TIMEOUT);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, DEFAULT_TIMEOUT);

    //https://curl.haxx.se/libcurl/c/progressfunc.html
    //进度回调方法
    UserData userdata;
    userdata.luaHandler = luaCallBack;
    userdata.alreadyDown = alreadydownload;
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFOFUNCTION, progressCURL);
    curl_easy_setopt(curlHandle, CURLOPT_XFERINFODATA, &userdata);
    curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 0L);
    
#if(CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
#else
    //设置接收数据的方法如果不设置,那么libcurl将会默认将数据输出到stdout
    curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,write_data);
#endif

    //设置write_data方法第四个参数获取的指针
    //使用这个属性可以在应用程序和libcurl调用的函数之间传递自定义数据
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, file);
	int code = curl_easy_perform(curlHandle);
    if(CURLE_OK != code){
        printf("curl easy perform faild code = %d\n",code);
        fclose(file);
        curl_easy_cleanup(curlHandle);
        return false;
    }

    long retcode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE , &retcode); 
    if ( (code == CURLE_OK) && retcode != 200 )
    {
        curl_easy_cleanup(curlHandle);
        fclose(file);
        return false;
    }
    printf("download success\n");
    curl_easy_cleanup(curlHandle);
    fclose(file);
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
