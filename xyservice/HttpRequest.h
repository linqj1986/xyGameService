#ifndef PROJECT_HTTPREQ_H
#define PROJECT_HTTPREQ_H

#endif //PROJECT_HTTPREQ_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <string>
#include <sys/types.h>
using namespace std;

#define BUFSIZE 41000
#define URLSIZE 1024
#define INVALID_SOCKET -1

class CHttpRequest
{
public:
    CHttpRequest();
    ~CHttpRequest();

    int HttpPost(const char* strUrl, const char* strData, string& strResponse,int nRetry=1);

};
