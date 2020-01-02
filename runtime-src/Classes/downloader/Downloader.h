#include <stdio.h>
class Downloader
{
    public:
    static void init();
    static void dispose();
	static bool checkFileExist(const char* url);
    static bool curlTest(const char* url, const char* savePath, double* length);
};