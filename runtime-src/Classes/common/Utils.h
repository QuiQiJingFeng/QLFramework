//
//  Utils.hpp
//  aam2_client
//
//  Created by JingFeng on 2017/10/1.
//
//

#ifndef Utils_h
#define Utils_h

#include <string>
#include "FYDC.h"
class Utils {
    
public:
    //need full_path
    bool unzipFile(std::string path,std::string outpath);
    bool versionGreater(const std::string& version1, const std::string& version2);
    long xxteaDecrypt(unsigned char* bytes,long size,char* xxteaSigin,char* xxteaKey);
    FValue unzipFile(FValueVector vector);
    static Utils* getInstance();
    void registerFunc(){
        REG_OBJ_FUNC(Utils::unzipFile,Utils::getInstance(),Utils::,"Utils::unzipFile")
    }

private:
    static Utils* __instance;
    Utils(){};
    
};

#endif /* Utils_h */
