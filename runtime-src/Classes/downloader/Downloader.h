#include <stdio.h>
#include "FYDC.h"

class Downloader
{
    public:
    static void init();
    static void dispose();
	static FValueMap getHttpInfo(const char* url);
    static bool createSimgleTaskInterNal(string url, string savePath, long processCallFunc);
    
    static Downloader* getInstance();
    
    FValue createSimgleTask(FValueVector vector);
    static void registerFunc(){
        REG_OBJ_FUNC(Downloader::createSimgleTask,Downloader::getInstance(),Downloader::,"Downloader::createSimgleTask")
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
