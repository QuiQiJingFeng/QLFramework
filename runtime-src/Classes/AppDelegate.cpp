/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "AppDelegate.h"
#include "scripting/lua-bindings/manual/CCLuaEngine.h"
#include "cocos2d.h"
#include "scripting/lua-bindings/manual/lua_module_register.h"

// #define USE_AUDIO_ENGINE 1
// #define USE_SIMPLE_AUDIO_ENGINE 1

#if USE_AUDIO_ENGINE && USE_SIMPLE_AUDIO_ENGINE
#error "Don't use AudioEngine and SimpleAudioEngine at the same time. Please just select one in your game!"
#endif

#if USE_AUDIO_ENGINE
#include "audio/include/AudioEngine.h"
using namespace cocos2d::experimental;
#elif USE_SIMPLE_AUDIO_ENGINE
#include "audio/include/SimpleAudioEngine.h"
using namespace CocosDenshion;
#endif

USING_NS_CC;
using namespace std;
//FYD BEGIN
#if (CC_TARGET_PLATFORM == CCPLATFORM_MAC || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include "runtime/Runtime.h"
#endif

extern "C" {
    int luaopen_pblib(lua_State *L);
    int luaopen_crypt(lua_State *L);
    int luaopen_md5_core(lua_State *L);
	int luaopen_bit(lua_State *L);
}

#include "JSONManager.h"
#include "Utils.h"
#include "FYDC.h"
#include "Downloader.h"
//FYD ENDED

AppDelegate::AppDelegate()
{
}

AppDelegate::~AppDelegate()
{
#if USE_AUDIO_ENGINE
    AudioEngine::end();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::end();
#endif

#if (COCOS2D_DEBUG > 0) && (CC_CODE_IDE_DEBUG_SUPPORT > 0)
    // NOTE:Please don't remove this call if you want to debug with Cocos Code IDE
    RuntimeEngine::getInstance()->end();
#endif

}

// if you want a different context, modify the value of glContextAttrs
// it will affect all platforms
void AppDelegate::initGLContextAttrs()
{
    // set OpenGL context attributes: red,green,blue,alpha,depth,stencil,multisamplesCount
    GLContextAttrs glContextAttrs = {8, 8, 8, 8, 24, 8, 0 };

    GLView::setGLContextAttrs(glContextAttrs);
}

// if you want to use the package manager to install more packages, 
// don't modify or remove this function
static int register_all_packages()
{
    return 0; //flag for packages manager
}

bool AppDelegate::applicationDidFinishLaunching()
{
    // set default FPS
    Director::getInstance()->setAnimationInterval(1.0 / 60.0f);

    // register lua module
    auto engine = LuaEngine::getInstance();
    ScriptEngineManager::getInstance()->setScriptEngine(engine);
    lua_State* L = engine->getLuaStack()->getLuaState();
    lua_module_register(L);

    register_all_packages();

	//FYD
    luaopen_pblib(L);
    luaopen_bit(L);
    luaopen_crypt(L);
    luaopen_md5_core(L);
    luaopen_FYDC(L);

    LuaStack* stack = engine->getLuaStack();
    stack->setXXTEAKeyAndSign("10cc4fdee2fcd047", strlen("10cc4fdee2fcd047"), "gclR3cu9", strlen("gclR3cu9"));

    //register custom function
    //LuaStack* stack = engine->getLuaStack();
    //register_custom_function(stack->getLuaState());
    
    //如果是IOS/ANDROID 第一次启动 需要解压缩文件
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS
    string path = FileUtils::getInstance()->getWritablePath();
    printf("writePath = %s\n",path.c_str());
    bool exsit = FileUtils::getInstance()->isFileExist(path + "project.manifest");
    bool decompress = false;
    if(!exsit){
        decompress = true;
    }else{
        //沙盒目录的版本文件
        string content = FileUtils::getInstance()->getStringFromFile(path + "project.manifest");
        JSONManager json;
        json.parseValueMapFromJSON(content);
        auto data = json.getDataMap();
        
        string version2 = data["version"].asString();
        //程序包中的版本文件
        content = FileUtils::getInstance()->getStringFromFile("package/project.manifest");
        json.parseValueMapFromJSON(content);
        data = json.getDataMap();
        string version1 = data["version"].asString();
        bool greater = Utils::getInstance()->versionGreater(version1,version2);
        //覆盖安装会出现这种情况,程序包中的版本号比可写目录中的版本号大
        if(greater){
            decompress = true;
        }
    }

    if(decompress){
        FileUtils::getInstance()->removeDirectory(path + "package/");
        //获取程序包中的版本文件
        string assets_info = FileUtils::getInstance()->getStringFromFile("package/project.manifest");
        JSONManager json;
        json.parseValueMapFromJSON(assets_info);
        auto data = json.getDataMap();
        auto assets = data["assets"].asValueMap();
        //解压ZIP包到沙盒目录
        // for(auto iter = assets.begin();iter != assets.end();++iter){
        //     auto fileName = "package/"+ iter->first;
        //     printf("unzip=>%s\n",fileName.c_str());
        //     Utils::getInstance()->unzipFile(fileName, path);
        // }
        //解压一部分资源
        Utils::getInstance()->unzipFile("package/package_src.zip", path);
        Utils::getInstance()->unzipFile("package/package_src_app_common.zip", path);
        Utils::getInstance()->unzipFile("package/package_src_uncompress.zip", path);
        Utils::getInstance()->unzipFile("package/package_res_ui_uncompress.zip", path);
        FILE * p_fd = fopen((path + "project.manifest").c_str(), "wb");
        fwrite(assets_info.c_str(), assets_info.size(), 1, p_fd);
        fclose(p_fd);
    }
    FileUtils::getInstance()->addSearchPath(path + "package/src");
    FileUtils::getInstance()->addSearchPath(path + "package/res");
    FileUtils::getInstance()->addSearchPath(path + "package/res/ui");
#endif



#if CC_64BITS
    FileUtils::getInstance()->addSearchPath("src/64bit");
#endif
    FileUtils::getInstance()->addSearchPath("src");
    FileUtils::getInstance()->addSearchPath("res");
    string str = "main.lua";
#if (CC_TARGET_PLATFORM == CCPLATFORM_MAC || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    auto file = RuntimeEngine::getInstance()->getProjectConfig().getScriptFile();
    //file "$(PROJDIR)/main.lua"
	replace(file.begin(), file.end(), '\\', '/');
    int pos = file.find_last_of("/");
    str = file.substr(pos+1);
#endif
    if (engine->executeScriptFile(str.c_str()))
    {
        return false;
    }

    return true;
}

// This function will be called when the app is inactive. Note, when receiving a phone call it is invoked.
void AppDelegate::applicationDidEnterBackground()
{
    Director::getInstance()->stopAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::pauseAll();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::getInstance()->pauseBackgroundMusic();
    SimpleAudioEngine::getInstance()->pauseAllEffects();
#endif
}

// this function will be called when the app is active again
void AppDelegate::applicationWillEnterForeground()
{
    Director::getInstance()->startAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::resumeAll();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::getInstance()->resumeBackgroundMusic();
    SimpleAudioEngine::getInstance()->resumeAllEffects();
#endif
}
