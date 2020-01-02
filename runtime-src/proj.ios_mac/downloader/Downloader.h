#include <stdio.h>
#include "FYDC.h"

class Downloader
{
    public:
    static void init();
    static void dispose();
	static bool checkFileExist(const char* url);
    static bool createSimgleTask(const char* url, const char* savePath, long processCallFunc);
    
    static Downloader* getInstance();
    
    FValue createSimgleTask(FValueVector vector){
        string url = vector[0].asString();
        string savePath = vector[1].asString();
        long callFunc = vector[2].asFloat();
        bool success = Downloader::createSimgleTask(url.c_str(),savePath.c_str(),callFunc);
        return FValue(success);
    };
    FValue checkFileExist(FValueVector vector){
        string url = vector[0].asString();
        bool success = Downloader::checkFileExist(url.c_str());
        return FValue(success);
    };
    static void registerFunc(){
        REG_OBJ_FUNC(Downloader::createSimgleTask,Downloader::getInstance(),Downloader::,"Downloader::createSimgleTask")
        REG_OBJ_FUNC(Downloader::checkFileExist,Downloader::getInstance(),Downloader::,"Downloader::checkFileExist")
    };
    ~Downloader(){
        Downloader::dispose();
    }
private:
    static Downloader* __instance;
    Downloader(){
        Downloader::init();
    };
};
