#include "HttpRequest.h"
#include "PrintLog.h"
#include <curl/curl.h>
#include <curl/easy.h>

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) 
{
    string data((const char*) ptr, (size_t) size * nmemb);

    *((std::string*) stream) += data;

    return size * nmemb;
}

CHttpRequest::CHttpRequest()
{
}

CHttpRequest::~CHttpRequest()
{
}

int CHttpRequest::HttpPost(const char* strUrl, const char* strData, string& strResponse,int nRetry)
{
	int nRet = 0;
	for(int i=0;i<nRetry;i++)
	{
		CURL *curl;	
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	
		// url
		curl_easy_setopt(curl, CURLOPT_URL, strUrl);

		// head 
		struct curl_slist* headers = NULL;	
		//headers=curl_slist_append(headers, "Content-Type: application/json;charset=UTF-8");
		headers=curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		// data
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strData);

		//设置回调函数
		string out="";
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

		// perform
		CURLcode res = curl_easy_perform(curl);
		if( res == CURLE_OK )
		{
			strResponse = out;
			nRet = 1;
			curl_easy_cleanup(curl);
			break;
		}
		else
		{
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"curl_easy_perform() failed: %s",curl_easy_strerror(res));
		}
		curl_easy_cleanup(curl);
	}
	return nRet;
}
