#include "WebComm.h"
#include "HttpRequest.h"
#include "EncryptWrapper.h"
#include <json/json.h>
#include "PrintLog.h"
#include "ParseIniFile.h"

// 秘钥组
string GLOBLE_KEY[] = \
{"edca125e-9838-4d3a-babb-60f208245120", "9a70350c-682a-48b3-9d8b-56df29e59402", \
"64974586-39f1-44f6-83ac-a38438abde7b", "760a0de0-9851-4748-9d8a-0499fa64a115", \
"ee7900b7-3c42-45a4-b44f-c3af8308de20", "a076e203-5f0e-4244-be8f-da30b2c5eef3", \
"de634cc0-a81d-444a-8a93-0f2fa9ac89b2", "355d1d61-284a-4b0b-ba04-ea9f534866ee"};

CWebComm::CWebComm()
{
	CParseIniFile ini;
	mstrHostName = ini.GetValue("cfg","config","hostname");
}

CWebComm::~CWebComm()
{

}

string CWebComm::GetRequestData(string strData)
{
	srand((unsigned long)time(NULL));
	
	string strChannelName = "default";
	string strDeviceCode = "0ea1x4d1f0s0df0vc";
	int nDeviceType = 7;
	string strIMEI = "VCIMEI";
	string strPackageType = "main";
	int nR = rand() % 8;

	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	int nTimeStamp = ts_now.tv_sec;

	int nVersion = 1001;
	string strVersionName = "1.0.0.1";

	char* pTemp = new char[500+strData.length()];
	memset(pTemp,0,500+strData.length());
	
	sprintf(pTemp,"%s%s%s%d%s%s%d%d%d%s%s", strChannelName.c_str(), strData.c_str(), strDeviceCode.c_str(), nDeviceType, 
		strIMEI.c_str(), strPackageType.c_str(),
		nR, nTimeStamp, nVersion, strVersionName.c_str(), GLOBLE_KEY[nR].c_str());
	
	std::string strSign = CEncryptWrapper::MD5((const unsigned char*)pTemp, strlen(pTemp));
	
	// ChannelName=default&  D  &DeviceCode=adf&DeviceType=1&IMEI=123456&  R  &TimeStamp=20160110&Version=1001&VersionName=1.99
	sprintf(pTemp,"r=%d&sign=%s&data=%s&ChannelName=%s&DeviceCode=%s&DeviceType=%d&IMEI=%s&PackageType=%s&TimeStamp=%d&Version=%d&VersionName=%s", 
		nR, strSign.c_str(), CEncryptWrapper::UrlEncode(strData.c_str()).c_str(),
		strChannelName.c_str(), strDeviceCode.c_str(), nDeviceType, strIMEI.c_str(), strPackageType.c_str(),
		nTimeStamp, nVersion, strVersionName.c_str());
	string strDst = pTemp;
	delete [] pTemp;

	return strDst;
}

string CWebComm::GetResponseData(string strData)
{
	string strDecodeData="";
#if 0
	int nBeginLenPos = strData.find("Content-Length:");
	if(nBeginLenPos>=0)
	{
		nBeginLenPos += strlen("Content-Length:");
		int nEndLenPos = strData.find("\n",nBeginLenPos);
		if(nEndLenPos>=0)
		{
			string strLen = strData.substr(nBeginLenPos,nEndLenPos-nBeginLenPos);
			int nLen = atoi(strLen.c_str());
		
			string strDest = strData.substr(nEndLenPos,strData.length()-nEndLenPos);
			int i=0;
			for(i=0;i<strDest.length();i++)
			{
				if((strDest[i]!='\r')&&(strDest[i]!='\n'))
					break;
			}
			int nRealLen = (strData.length()-i)<nLen?(strData.length()-i):nLen;
			strDest = strDest.substr(i,nRealLen);
			//printf("nLen=%d,nLeft=%d,nRealLen=%d\n",nLen,(strData.length()-i),nRealLen);
			//printf("%s\n",strDest.c_str());
			strDecodeData=CEncryptWrapper::Decrypt((const char*)(strDest.c_str()));
		}
	}
#else
	strDecodeData=CEncryptWrapper::Decrypt((const char*)(strData.c_str()));
#endif
	//printf("strDecodeData=%s\n",strDecodeData.c_str());
	return strDecodeData;
}

int CWebComm::PostUserExitRoom(int nGameId, int nUserId, bool bGameBegin, int nUserOutType)
{
#if TESTNOWEB
        return 1;
#endif
	int nRet = 0;
	Json::FastWriter jWriter;
	Json::Value jValue;
	jValue["UserID"] = nUserId;
	jValue["GameID"] = nGameId;
	jValue["GameIsBegin"] = bGameBegin;
	jValue["UserOutType"] = nUserOutType;
	string strJson = jWriter.write( jValue );
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostUserExitRoom] %s",strJson.c_str());
	string strEncrypt=CEncryptWrapper::Encrypt(strJson.c_str());
	string strParam = GetRequestData(strEncrypt);
	string strRes="";
	CHttpRequest req;
	string strUrl = "http://" + mstrHostName + URLEXITROOM;
	if(req.HttpPost(strUrl.c_str(),strParam.c_str(),strRes))
	{
		string strData = GetResponseData(strRes);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostUserExitRoom] %s",strData.c_str());
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)(strData.c_str()),jRoot))
		{
			if((!jRoot["Code"].isNull()) && (jRoot["Code"].isInt()))
			{
				int nCode = jRoot["Code"].asInt();
				if(nCode==0)
				{
					nRet = 1;
				}
				else
				{
					nRet = 0-nCode;
				}
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"[PostUserExitRoom] Failed");
		nRet = 0-ERROR0POST0ERROR;
	}

	return nRet;
}

int CWebComm::PostGameStart(int nGameId, int nUserId, int nLiveUserId, int nLiveId)
{
#if TESTNOWEB
        return 1;
#endif
	int nRet = 0;
	Json::FastWriter jWriter;
	Json::Value jValue;
	jValue["UserID"] = nUserId;
	jValue["GameID"] = nGameId;
	jValue["LiveUserID"] = nLiveUserId;
	jValue["LiveID"] = nLiveId;
	string strJson = jWriter.write( jValue );
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostGameStart] %s",strJson.c_str());
	string strEncrypt=CEncryptWrapper::Encrypt(strJson.c_str());
	string strParam = GetRequestData(strEncrypt);
	string strRes="";
	CHttpRequest req;
	string strUrl = "http://" + mstrHostName + URLBEGINGAME;
	if(req.HttpPost(strUrl.c_str(),strParam.c_str(),strRes))
	{
		string strData = GetResponseData(strRes);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostGameStart] %s",strData.c_str());
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)(strData.c_str()),jRoot))
		{
			if((!jRoot["Code"].isNull()) && (jRoot["Code"].isInt()))
			{
				int nCode = jRoot["Code"].asInt();
				if(nCode==0)
				{
					nRet = 1;
				}
				else
				{
					nRet = 0-nCode;
				}
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"[PostGameStart] Failed");
		nRet = 0-ERROR0POST0ERROR;
	}

	return nRet;
}

int CWebComm::PostGameTimeOut(int nGameId, int nUserId, int nType)
{
#if TESTNOWEB
        return 1;
#endif
	int nRet = 0;
	Json::FastWriter jWriter;
	Json::Value jValue;
	jValue["UserID"] = nUserId;
	jValue["GameID"] = nGameId;
	jValue["TimeOutType"] = nType;
	string strJson = jWriter.write( jValue );
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostGameTimeOut] %s",strJson.c_str());
	string strEncrypt=CEncryptWrapper::Encrypt(strJson.c_str());
	string strParam = GetRequestData(strEncrypt);
	string strRes="";
	CHttpRequest req;
	string strUrl = "http://" + mstrHostName + URLGAMETIMEOUT;
	if(req.HttpPost(strUrl.c_str(),strParam.c_str(),strRes))
	{
		string strData = GetResponseData(strRes);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostGameTimeOut] %s",strData.c_str());
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)(strData.c_str()),jRoot))
		{
			if((!jRoot["Code"].isNull()) && (jRoot["Code"].isInt()))
			{
				int nCode = jRoot["Code"].asInt();
				if(nCode==0)
				{
					nRet = 1;
				}
				else
				{
					nRet = 0-nCode;
				}
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"[PostGameTimeOut] Failed");
		nRet = 0-ERROR0POST0ERROR;
	}

	return nRet;
}

string CWebComm::GetUUID(string strData)
{
	std::string strUid;
	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	int nTimeStamp = ts_now.tv_sec;
	
	char szTime[50]={0};
	sprintf(szTime,"%d",nTimeStamp);
	string strTime = strData;
	strTime += szTime;

	strUid = CEncryptWrapper::MD5((const unsigned char*)strTime.c_str(), strTime.length());
	return strUid;
}

int CWebComm::PostBomb(int nGameId, int nUserId, int nType, bool bOver,vector<int> &vecIds,string& strDataInfo)
{
#if TESTNOWEB
        return 1;
#endif
	strDataInfo="";
	int nRet = 0;
	Json::FastWriter jWriter;
	Json::Value jValue,jIds;
	jValue["UserID"] = nUserId;
	jValue["GameID"] = nGameId;
	jValue["BType"] = nType;
	jValue["GameIsOver"] = bOver;

	for(int i=0;i<vecIds.size();i++)
	{
		jIds.append(vecIds[i]);
	}
	jValue["WinUserID"] = jIds;

	string strUData = jWriter.write( jValue );
	strUData = GetUUID(strUData);
	jValue["RequestGuid"] = strUData.c_str();
	//printf("%s\n",strUData.c_str());
	
	string strJson = jWriter.write( jValue );
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostBomb] %s",strJson.c_str());
	string strEncrypt=CEncryptWrapper::Encrypt(strJson.c_str());
	string strParam = GetRequestData(strEncrypt);
	
	string strRes="";
	CHttpRequest req;
	string strUrl = "http://" + mstrHostName + URLBOMB;
	if(req.HttpPost(strUrl.c_str(),strParam.c_str(),strRes,3))
	{
		string strData = GetResponseData(strRes);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostBomb] %s",strData.c_str());
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)(strData.c_str()),jRoot))
		{
			if((!jRoot["Code"].isNull()) && (jRoot["Code"].isInt()))
			{
				int nCode = jRoot["Code"].asInt();
				if(nCode==0)
				{
					nRet = 1;
					Json::FastWriter jWriter;
					strDataInfo = jWriter.write( jRoot["Data"] );
					if (strDataInfo[strDataInfo.length() - 1] == '\n')
					{
						strDataInfo = strDataInfo.substr(0, strDataInfo.length() - 1);
					}
				}
				else
				{
					nRet = 0-nCode;
				}
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"[PostBomb] Failed");
		nRet = 0-ERROR0POST0ERROR;
	}

	return nRet;
}

int CWebComm::PostCheck(int nGameId, int nUserId,int& nJoinUserCount,int& nJoinType,int& nWaitOpenGameTimeOut,string& strDataInfo, int nType)
{
#if TESTNOWEB
        return nGameId;
#endif
	int nRet = 0;
	Json::FastWriter jWriter;
	Json::Value jValue,jIds;
	jValue["UserID"] = nUserId;
	jValue["GameID"] = nGameId;
	jValue["IsCreateOrJoin"] = nType;
	string strJson = jWriter.write( jValue );
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostCheck] %s",strJson.c_str());
	string strEncrypt=CEncryptWrapper::Encrypt(strJson.c_str());
	string strParam = GetRequestData(strEncrypt);
	//printf("strParam=%s,len=%d,sunlen=%d\n",strParam.c_str(),strParam.length(),strEncrypt.length());
	string strRes="";
	CHttpRequest req;
	string strUrl = "http://" + mstrHostName + URLCHECK;
	if(req.HttpPost(strUrl.c_str(),strParam.c_str(),strRes))
	{
		string strData = GetResponseData(strRes);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"[PostCheck] %s",strData.c_str());
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)(strData.c_str()),jRoot))
		{
			if((!jRoot["Code"].isNull()) && (jRoot["Code"].isInt()))
			{
				int nCode = jRoot["Code"].asInt();
				if(nCode==0)
				{
					if(  (!jRoot["Data"].isNull())&&(!jRoot["Data"]["GameInfo"].isNull())
                                           &&(!jRoot["Data"]["JoinType"].isNull()) 
                                          )
					{
						Json::FastWriter jWriter;
						strDataInfo = jWriter.write( jRoot["Data"] );
						if (strDataInfo[strDataInfo.length() - 1] == '\n')
						{
							strDataInfo = strDataInfo.substr(0, strDataInfo.length() - 1);
						}

						nJoinType = jRoot["Data"]["JoinType"].asInt();
						if(nJoinType!=2)//不是观看
						{
							Json::Value jGameInfo;
							jGameInfo = jRoot["Data"]["GameInfo"];
							if(!jGameInfo["GameID"].isNull())
								nRet = jGameInfo["GameID"].asInt();
							//if(!jGameInfo["GameOwnerUserID"].isNull())
							//	nOwerUserId = jGameInfo["GameOwnerUserID"].asInt();
							if(!jGameInfo["WaitOpenGameTimeOut"].isNull())
								nWaitOpenGameTimeOut = jGameInfo["WaitOpenGameTimeOut"].asInt();
							if(!jGameInfo["JoinUserCount"].isNull())
								nJoinUserCount = jGameInfo["JoinUserCount"].asInt();
							

						}
						else
						{
							nRet = 0-9007;
						}						
					}
				}
				else
				{
					nRet = 0-nCode;
				}
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"[PostCheck] Failed");
		nRet = 0-ERROR0POST0ERROR;
	}

	return nRet;
}
