#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "redis_publisher.h"
#include <unistd.h>
#include "ParseIniFile.h"
#include "PrintLog.h"

#define REDISPOOL 1

CRedisPublisher::CRedisPublisher()
{
	CParseIniFile ini;
	string strPort = ini.GetValue("cfg","config","rport");
	m_nPort=atoi(strPort.c_str());
    	m_strIP = ini.GetValue("cfg","config","rip");
	m_strPsw = ini.GetValue("cfg","config","rpwd");
	m_strDb = ini.GetValue("cfg","config","rdb");
	if(m_strDb.length()==0)
		m_strDb="0";
	sem_init(&sem_lock, 0, 1);

        mRedisClient.Init(m_strIP,m_nPort,m_strPsw,m_strDb);
	//sem_init(&semGameInfolock, 0, 1);
}

CRedisPublisher::~CRedisPublisher()
{
}

bool CRedisPublisher::Publish(const string &channel_name, const string &message)
{
	bool bRet = false;
	sem_wait(&sem_lock);
	bRet = InnerPublish(channel_name, message);
	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::InnerPublish(const string &channel_name, const string &message)
{
	bool bRet = false;
#if REDISPOOL
	string strCommand = "PUBLISH ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";

	string strResponse="";
	vector<string> vec;
	vec.push_back(channel_name);
	vec.push_back(message);
	bRet = mRedisClient.ExecuteCmd(strResponse,strCommand.c_str(),vec);
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "PUBLISH ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),channel_name.c_str(),message.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut PUBLISH failure\n");
		Close(pContext,strRet);
		return bRet;
	}

	if( !(r->type == REDIS_REPLY_INTEGER) )
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Failed to execute PUBLISH command\n");
		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}	
        bRet = true;
	freeReplyObject(r);

	Close(pContext,strRet);
#endif
	return bRet;
}

void* CRedisPublisher::Connect( string strIP, int nPort, string strPwd, string& strResult )
{
	strResult = "";
	redisContext* pContext = redisConnect(strIP.c_str(), nPort);
	if ( pContext->err )
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:Connect to redisServer faile:%s\n",pContext->errstr);
		strResult = pContext->errstr;
		redisFree(pContext);
		return NULL;
	}
	if (strPwd.length()>0)
	{
		string strCommandLogin = "AUTH ";
		strCommandLogin += strPwd;
		redisReply* r = (redisReply*)redisCommand(pContext, strCommandLogin.c_str());
		if( NULL == r)
		{//ʧ��
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:Login failure\n");
			Close(pContext,strResult);
			return NULL;
		}
		if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))
		{//ʧ��
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:Login failure:%s\n",r->str);
			strResult = r->str;
			freeReplyObject(r);
			Close(pContext,strResult);
			return NULL;
		}
		//�ɹ�
		freeReplyObject(r);	
	}
	
	// ѡ��DB
	if(m_strDb!="0")
	{
		string strCommandSelect = "SELECT ";
		strCommandSelect += m_strDb;
		redisReply* r = (redisReply*)redisCommand(pContext, strCommandSelect.c_str());
		if( NULL == r)
		{//ʧ��
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:SELECT failure\n");
			Close(pContext,strResult);
			return NULL;
		}
		if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))
		{//ʧ��
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:SELECT failure:%s\n",r->str);
			strResult = r->str;
			freeReplyObject(r);
			Close(pContext,strResult);
			return NULL;
		}
		//�ɹ�
		freeReplyObject(r);	
	}

	return (void*)pContext;
}

bool CRedisPublisher::Close( void* pContext, string& strResult )
{
	strResult = "";
	bool bRet = false;
	if (pContext==NULL)
	{
		return bRet;
	}
	redisFree((redisContext*)pContext);
	return bRet;
}

bool CRedisPublisher::HSet( string strTable, string strKey, string strValue, string& strResult )
{
	strResult = "";
	bool bRet = false;
#if REDISPOOL
	string strCommand = "hset ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";

	string strResponse="";
	vector<string> vec;
	vec.push_back(strTable);
	vec.push_back(strKey);
	vec.push_back(strValue);
	bRet = mRedisClient.ExecuteCmd(strResponse,strCommand.c_str(),vec);
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "hset ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),strTable.c_str(),strKey.c_str(),strValue.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut hset failure\n");
		Close(pContext,strRet);
		return bRet;
	}
	if( !(r->type == REDIS_REPLY_INTEGER) )
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Failed to execute hset command\n");
		strResult = "Failed to execute hset command";
		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}	
	freeReplyObject(r);


	bRet = true;
	Close(pContext,strRet);
#endif
	return bRet;
}

bool CRedisPublisher::HGet( string strTable, string strKey,  string& strValue,  string& strResult )
{
	strResult = "";
	bool bRet = false;
#if REDISPOOL
	string strCommand = "hget ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";

	string strResponse="";
	vector<string> vec;
	vec.push_back(strTable);
	vec.push_back(strKey);
	bRet = mRedisClient.ExecuteCmd(strResponse,strCommand.c_str(),vec);
	strValue = strResponse;
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "hget ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),strTable.c_str(),strKey.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut hget failure\n");
		Close(pContext,strRet);
		return bRet;
	}

	// ���ط��ַ���
	if ( r->type != REDIS_REPLY_STRING)
	{
		// ��������Ϊ��
		if (r->type == REDIS_REPLY_NIL)
		{
			strResult = "the value is null!";
			freeReplyObject(r);
			Close(pContext,strRet);
			return bRet;
		}
		strResult = r->str;
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"HGet Failed %s\n",strResult.c_str());
		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}	
	strValue = r->str;
	freeReplyObject(r);

	bRet = true;
	Close(pContext,strRet);
#endif
	return bRet;
}

bool CRedisPublisher::HDel( string strTable, string strKey, string& strResult )
{
	strResult = "";
	bool bRet = false;
#if REDISPOOL
	string strCommand = "hdel ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";

	string strResponse="";
	vector<string> vec;
	vec.push_back(strTable);
	vec.push_back(strKey);
	bRet = mRedisClient.ExecuteCmd(strResponse,strCommand.c_str(),vec);
	strResult = strResponse;
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "hdel ";
	strCommand += "%s";
	strCommand += " ";
	strCommand += "%s";
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),strTable.c_str(),strKey.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut hdel failure\n");
		Close(pContext,strRet);
		return bRet;
	}
	if( !(r->type == REDIS_REPLY_INTEGER ))
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Failed to execute hdel command\n");
		strResult = "Failed to execute hdel command";
		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}
	// ������0ʱ����ʾ������ڣ���strResult�������֪ͨ�ⲿ
	char szCount[20]={0};
	sprintf(szCount,"%lld",r->integer);
	strResult = szCount;

	freeReplyObject(r);

	bRet = true;
	Close(pContext,strRet);
#endif
	return bRet;
}

bool CRedisPublisher::HGetAllValues( string strTable,vector<string>& vecKeys, vector<string>& vecValues,  string& strResult )
{
	strResult = "";
	bool bRet = false;
#if REDISPOOL
	string strCommand = "hgetall ";
	strCommand += "%s";

	vector<string> vec;
	vec.push_back(strTable);
	bRet = mRedisClient.ExecuteArrayCmd(vecKeys,vecValues,strCommand.c_str(),vec);
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "hgetall ";
	strCommand += "%s";
		
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),strTable.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut hgetall failure\n");
		Close(pContext,strRet);
		return bRet;
	}

	// ���ط��ַ���
	if ( r->type != REDIS_REPLY_ARRAY)
	{
		// ��������Ϊ��
		if (r->type == REDIS_REPLY_NIL)
		{
			strResult = "the value is null!";
			freeReplyObject(r);
			Close(pContext,strRet);
			return bRet;
		}

		strResult = "Failed to execute hgetall command";
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"hgetall Failed %s\n",strResult.c_str());

		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}	
	for (int i=0;i<r->elements;i++)
	{
		if (i%2==0)
		{
			vecKeys.push_back(r->element[i]->str);
		}
		else
			vecValues.push_back(r->element[i]->str);
	}
	
	freeReplyObject(r);

	bRet = true;
	Close(pContext,strRet);
#endif
	return bRet;
}

bool CRedisPublisher::DeleteTable( string strTable, string& strResult )
{
	strResult = "";
	bool bRet = false;
#if REDISPOOL
	string strCommand = "DEL ";
	strCommand += "%s";

	string strResponse="";
	vector<string> vec;
	vec.push_back(strTable);
	bRet = mRedisClient.ExecuteCmd(strResponse,strCommand.c_str(),vec);
	strResult = strResponse;
#else
	void* pContext = NULL;
	string strRet;
	if(NULL==(pContext=Connect(m_strIP.c_str(),m_nPort,m_strPsw.c_str(),strRet)))
	{
		return bRet;
	}

	string strCommand = "DEL ";
	strCommand += "%s";
	redisReply* r = (redisReply*)redisCommand((redisContext*)pContext, strCommand.c_str(),strTable.c_str());

	if( NULL == r)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Execut DEL failure\n");
		Close(pContext,strRet);
		return bRet;
	}
	if( !(r->type == REDIS_REPLY_INTEGER ))
	{
		strResult = r->str;
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"DEL Failed %s\n",strResult.c_str());
		freeReplyObject(r);
		Close(pContext,strRet);
		return bRet;
	}	
	freeReplyObject(r);

	bRet = true;
	Close(pContext,strRet);
#endif
	return bRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool CRedisPublisher::DeleteUserInfo(string strTable)
{
	sem_wait(&sem_lock);
	string strRet;
	bool bRet = DeleteTable(strTable,strRet);
	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::GetUserIds(string strTable,string& strValue)
{
	sem_wait(&sem_lock);
	bool bRet = false;
	strValue = "";
	string strResult = "";
	vector<string> vecKeys;
	vector<string> vecValues;
	vecKeys.empty();
	vecValues.empty();
	
	Json::Reader jReader(Json::Features::strictMode());
	Json::Value jValue,jTotalValue;
	Json::FastWriter jWriter;
	bRet = HGetAllValues(strTable,vecKeys,vecValues,strResult);
	if(bRet)
	{
		for(int i=0;i<vecKeys.size();i++)
		{
			if(jReader.parse(vecValues[i].c_str(),jValue))
			{
				Json::Value jValueIdArray;
				jValueIdArray = jValue["UserIds"];
				for(int j=0;j<jValueIdArray.size();j++)
				{
					jTotalValue.append(jValueIdArray[j]);
				}
			}
		}	
		
		Json::Value jValueRet;
		jValueRet["UserIds"].resize(0);
		if(jTotalValue.size()>0)
		{
			map<int,int> mapUser; 
			// �����ظ�����ʱ�����µĽڵ�
			for(int h=0;h<jTotalValue.size();h++)
			{
				int nKeepIt = h;
				for(int k=0;k<jTotalValue.size();k++)
				{
					if( (jTotalValue[h]["UserId"].asInt() == jTotalValue[k]["UserId"].asInt()) && (k!=h) )
					{
						//printf("---------remove user=%d\n",jTotalValue[h]["UserId"].asInt());
						if(jTotalValue[nKeepIt]["InTime"].asInt()<jTotalValue[k]["InTime"].asInt())
						{
							nKeepIt=k;
						}
					}
				}
				mapUser.insert(map<int,int>::value_type(nKeepIt,nKeepIt));
			}

			Json::Value jValueKeep;
			map<int,int>::iterator itr = mapUser.begin();
			while( itr!=mapUser.end() )
			{
				jValueKeep.append( jTotalValue[itr->first] );
				itr++;
			}

			// ����������û�����������
			if(jValueKeep.size()>0)
			{
				bool bOwnerFound=false;
				for(int i=0;i<jValueKeep.size();i++)
				{
					if( jValueKeep[i]["UserType"].asInt() == 0 )//����
					{
						if(bOwnerFound == true)
						{
							jValueKeep[i]["UserType"] = 1;
							string strTemp = jWriter.write(jValueKeep);
							CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"--UserType RESET 1,data=%s\n",strTemp.c_str());
						}
						bOwnerFound = true;
					}
				}

				if(bOwnerFound==false)
				{
					int nTime=0x7ffffffd;
					int nOldest=0;
					for(int i=0;i<jValueKeep.size();i++)
					{
						int nIndexTime=jTotalValue[i]["InTime"].asInt();
						if(nIndexTime<nTime)
						{
							nTime=nIndexTime;
							nOldest=i;
						}
					}
					jValueKeep[nOldest]["UserType"] = 0;
					string strTemp = jWriter.write(jValueKeep);
					CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"--UserType RESET 0,data=%s\n",strTemp.c_str());
				}
			}
			jValueRet["UserIds"]=jValueKeep;			
		}
		strValue = jWriter.write(jValueRet);
	}

	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::AddUserId(string strTable, string strKey, string strUserId,int nUserType, string strUserPic, string strUserName)
{
	// ����λ���ֶ�
	int aPosArray[MAX0USER0COUNT];
	for(int i=0;i<MAX0USER0COUNT;i++)
	{
		aPosArray[i]=-1;
	}
	bool bFoundInAll = false;
	int nPos=-1;
	int nStatus=USERIDLE;
	string strValue="";
	bool bRet = GetUserIds(strTable,strValue);
	sem_wait(&sem_lock);
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
        	Json::Value jUserIdsArray,jGetRoot;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				aPosArray[jUserIdsArray[j]["Pos"].asInt()]=jUserIdsArray[j]["Pos"].asInt();
				// ����ҵ��û�ID,�����þ�pos
				if(jUserIdsArray[j]["UserId"].asInt()==atoi(strUserId.c_str()))
				{
					bFoundInAll = true;
					nPos = jUserIdsArray[j]["Pos"].asInt();
					nStatus = jUserIdsArray[j]["Status"].asInt();
				  	nUserType = jUserIdsArray[j]["UserType"].asInt();
					break;
				}
			}
		}	
	}
	// ���û���ҵ���pos�����·���
	if(nPos==-1)
	{
		for(int i=0;i<MAX0USER0COUNT;i++)
		{
			if(aPosArray[i]<0)
			{
				nPos=i;
				break;
			}
		}
	}
	
	//sem_post(&sem_lock);
	////////////////////////////
	
	//sem_wait(&sem_lock);
	string strRet;
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
        Json::Value jUserIdsArray;
	Json::Value jGetRoot;
	bool bFound=false;
	bRet = HGet(strTable,strKey,strValue,strRet);		
	if(bRet)
	{
		//printf("GetUserId=%s,ret=%s",strValue.c_str(),strRet.c_str());
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if(( atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt()))
				{ 
					jUserIdsArray[j]["InRoom"] = 1;
					jUserIdsArray[j]["OnLine"] = 1;
					bFound = true;
					break;
				}
			}
		}	
	}

	if(!bFound)
	{
		Json::Value jNewUserId;
		jNewUserId["UserId"] = atoi(strUserId.c_str());
		jNewUserId["UserType"] = nUserType;
		jNewUserId["Status"] = nStatus;
		jNewUserId["InRoom"] = 1;
		jNewUserId["OnLine"] = 1;
		jNewUserId["UserPic"] = strUserPic.c_str();
		jNewUserId["UserName"] = strUserName.c_str();
		jNewUserId["Pos"] = nPos;
		jNewUserId["Key"] = strKey.c_str();
		struct timespec ts_now;
        	clock_gettime(CLOCK_REALTIME,&ts_now);
		jNewUserId["InTime"] = (int)ts_now.tv_sec;
		
		jUserIdsArray.append(jNewUserId);
	}

	// ���±�
	jGetRoot["UserIds"] = jUserIdsArray;
	strValue = jWriter.write(jGetRoot);
	HSet(strTable,strKey,strValue,strRet);	

	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::UpdateUserStatus(string strTable,string strKey, string strUserId,int nStatus,int nInRoom,int nOnLine)
{
	sem_wait(&sem_lock);
	bool bUpdate=true;
	string strNewData="";
	string strIdleUserId="";
	string strRet;
	string strValue="";
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
        Json::Value jUserIdsArray,jGetRoot,jTemp;
	bool bRet = HGet(strTable,strKey,strValue,strRet);		
	if(bRet)
	{
		//printf("GetUserId=%s,nStatus=%d\n",strValue.c_str(),nStatus);
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if((atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt()))
				{ 
					if(jUserIdsArray[j]["Status"]==0)
					{
						// Ŀ���û�����Ѿ����˾Ͳ�������
						//bUpdate = false;
						//CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"UpdateUserStatus : Already Boom UserId=%s,status=%d",strUserId.c_str(),nStatus);
					}
					else if(nStatus!=-1)
					{
						jUserIdsArray[j]["Status"] = nStatus;
					}
					if(nInRoom==0)
					{
						jUserIdsArray[j]["InRoom"]=nInRoom;
						if(jUserIdsArray[j]["UserType"]==0)
							jUserIdsArray[j]["UserType"]=1;
					}
					if(nOnLine!=-1)
						jUserIdsArray[j]["OnLine"]=nOnLine;
				}
			}

			if(bUpdate)
			{
				
				jTemp["UserIds"] = jUserIdsArray;
				strNewData = jWriter.write(jTemp);
				HSet(strTable,strKey,strNewData,strRet);
			}
	
		}	
	}
	
	//printf("UpdateUserStatus=%s,nStatus=%d, bUpdate=%d \n",strNewData.c_str(),nStatus,bUpdate);
	sem_post(&sem_lock);
	return bUpdate;
}

int CRedisPublisher::GetIdleUser(string strTable,string strExcept,string& strIdleUserId)
{
	strIdleUserId="";
	string strValue="";
	bool bRet = GetUserIds(strTable,strValue);
	sem_wait(&sem_lock);
	vector<int> vecIdls;
	vecIdls.clear();
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
        	Json::Value jUserIdsArray,jGetRoot;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if((jUserIdsArray[j]["Status"]!=USERBOOM))
				{
					if(atoi(strExcept.c_str())!=jUserIdsArray[j]["UserId"].asInt())// ���ڻ�ȡ��һ�������û����������Լ�
						vecIdls.push_back(jUserIdsArray[j]["UserId"].asInt());
				}
			}
		}
	}
	if(vecIdls.size()>0)
	{
		srand((int)time(0));
		int nRan = rand()%(vecIdls.size());
		char szIdleUser[20]={0};
		sprintf(szIdleUser,"%d",vecIdls[nRan]);
		strIdleUserId=szIdleUser;
	}
	sem_post(&sem_lock);
	return vecIdls.size();
}

bool CRedisPublisher::GetUserInfo(string strTable, string strKey,string strUserId,TagRedisUser& tag)
{
	tag.nType   = USERTYPECOUNT;
	tag.nStatus = USERSTATUSCOUNT;

	sem_wait(&sem_lock);
	string strRet;
	string strValue="";
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
        Json::Value jUserIdsArray,jGetRoot,jTemp;
	bool bRet = HGet(strTable,strKey,strValue,strRet);		
	if(bRet)
	{
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if(atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt())
				{ 
					tag.nType   = jUserIdsArray[j]["UserType"].asInt();
					tag.nStatus = jUserIdsArray[j]["Status"].asInt();
					break;
				}
			}
		}	
	}
	sem_post(&sem_lock);
	return bRet;
}

int CRedisPublisher::ChangeOwner(string strTable,string strLastOwnerId)
{
	int nNewUserId=0;
	int nBeginTime = 0x7FFFFFFF;
	sem_wait(&sem_lock);
	bool bRet = false;
	
	string strResult = "";
	vector<string> vecKeys;
	vector<string> vecValues;
	vecKeys.empty();
	vecValues.empty();
	
	Json::Reader jReader(Json::Features::strictMode());
	Json::Value jValue;
	Json::FastWriter jWriter;
	bRet = HGetAllValues(strTable,vecKeys,vecValues,strResult);
	if(bRet)
	{
		int nNewIndex=0;
		
		for(int i=0;i<vecKeys.size();i++)
		{
			if(jReader.parse(vecValues[i].c_str(),jValue))
			{
				
				Json::Value jValueIdArray;
				jValueIdArray = jValue["UserIds"];
				for(int j=0;j<jValueIdArray.size();j++)
				{
					if((jValueIdArray[j]["InRoom"].asInt()==1)
					&&(jValueIdArray[j]["InTime"].asInt()<nBeginTime)
					&&(atoi(strLastOwnerId.c_str()) != jValueIdArray[j]["UserId"].asInt()))
					{
						nBeginTime = jValueIdArray[j]["InTime"].asInt();
						nNewIndex=i;
						nNewUserId = jValueIdArray[j]["UserId"].asInt();
						//printf("newuser %d\n",nNewUserId);
					}
				}
			}
		}	

		if(jReader.parse(vecValues[nNewIndex].c_str(),jValue))
		{
				
			Json::Value jValueIdArray;
			jValueIdArray = jValue["UserIds"];
			for(int j=0;j<jValueIdArray.size();j++)
			{
				if(jValueIdArray[j]["UserId"].asInt()==nNewUserId)
				{
					jValueIdArray[j]["UserType"] = 0;
					break;
				}
			}

			Json::Value jGetRoot;
			jGetRoot["UserIds"] = jValueIdArray;
			string strValue = jWriter.write(jGetRoot);
			HSet(strTable,vecKeys[nNewIndex],strValue,strResult);	
		}
		
	}

	sem_post(&sem_lock);
	return nNewUserId;
}

int CRedisPublisher::GetUserStatus(string strTable,string strKey, string strUserId)
{
	int nStatus=USERSTATUSCOUNT;
	if(strKey.length()==0)
	{
		string strValue="";
		bool bRet = GetUserIds(strTable,strValue);
		sem_wait(&sem_lock);
		if(bRet)
		{
			Json::Reader jReader(Json::Features::strictMode());
			Json::FastWriter jWriter;
			Json::Value jUserIdsArray,jGetRoot;
			if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
			{
				jUserIdsArray = jGetRoot["UserIds"];
				for(int j=0;j<jUserIdsArray.size();j++)
				{
					if(atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt())
					{
						nStatus = jUserIdsArray[j]["Status"].asInt();
						break;
					}
				}
			}
		}
		sem_post(&sem_lock);
	}
	else
	{
		sem_wait(&sem_lock);
		string strRet;
		string strValue="";
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
		Json::Value jUserIdsArray,jGetRoot;
		bool bRet = HGet(strTable,strKey,strValue,strRet);		
		if(bRet)
		{
			if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
			{
				jUserIdsArray = jGetRoot["UserIds"];
				for(int j=0;j<jUserIdsArray.size();j++)
				{
					if((atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt()))
					{ 
						nStatus = jUserIdsArray[j]["Status"].asInt();
						break;
					}
				}
			}		
		}
	
		sem_post(&sem_lock);
	}
	
	return nStatus;
}

bool CRedisPublisher::IsAllOffLine(string strTable)
{
	bool bAllOff = false;
	string strValue="";
	bool bRet = GetUserIds(strTable,strValue);
	sem_wait(&sem_lock);
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
		Json::Value jUserIdsArray,jGetRoot;
	
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			if(jUserIdsArray.size()==0)
				bAllOff = true;
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if( 1 == jUserIdsArray[j]["OnLine"].asInt())
				{
					bAllOff = false;
					break;
				}
			}
		}
		else if(strValue.length()==0)
		{
			bAllOff = true;
		}
	}
	sem_post(&sem_lock);
	return bAllOff;
}

int CRedisPublisher::RemoveUserId(string strTable, string strKey, string strUserId)
{
	int nUserNum=-1;
	sem_wait(&sem_lock);
	string strRet;
	string strValue="";
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
        Json::Value jUserIdsArray,jGetRoot,jTemp;
	bool bRet = HGet(strTable,strKey,strValue,strRet);		
	if(bRet)
	{
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if(atoi(strUserId.c_str()) == jUserIdsArray[j]["UserId"].asInt())
				{ 
					jUserIdsArray.removeIndex(j,&jTemp); 
				}
			}
			
		}	
		nUserNum = jUserIdsArray.size();
		jGetRoot["UserIds"] = jUserIdsArray;
		strValue = jWriter.write(jGetRoot);
		HSet(strTable,strKey,strValue,strRet);	
	}

	sem_post(&sem_lock);
	return nUserNum;
}

bool CRedisPublisher::CreateGameInfo(string strKey, string strCurUserId, string strGameId,int nWaitOpenGameTimeOut, int nRemainTime, int nMaxStayTime)
{
	sem_wait(&sem_lock);
	Json::FastWriter jWriter;
        Json::Value jGetRoot;
	jGetRoot["RemainTime"] = nRemainTime;
	jGetRoot["MaxStayTime"] = nMaxStayTime;
	jGetRoot["CurUserId"] = strCurUserId.c_str();
	jGetRoot["GameId"] = strGameId.c_str();
	jGetRoot["Status"] =GAMESTATUSWAIT;
	jGetRoot["WaitOpenGameTimeOut"] =nWaitOpenGameTimeOut;
	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	jGetRoot["CreateTime"] = (int)ts_now.tv_sec;

	jGetRoot["OrderList"].resize(0);
	string strValue = jWriter.write(jGetRoot);
	string strRet;
	bool bRet = HSet("GameInfo",strKey,strValue,strRet);		
	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::UpdateGameInfo( string strKey, string strCurUserId,string strBoomUserId, int nRemainTime,int nStatus)
{
	sem_wait(&sem_lock);
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	Json::Value jGetRoot;
	string strValue="";
	string strRet="";
	bool bRet = HGet("GameInfo",strKey,strValue,strRet);	
	if(bRet)
	{
		// �������û�
		//printf("UpdateGameInfo,GetInfo=%s,ret=%s\n",strValue.c_str(),strRet.c_str());
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jGetRoot["CurUserId"] = strCurUserId.c_str();
			if(nRemainTime!=-1)
				jGetRoot["RemainTime"] = nRemainTime;
			if(nStatus!=-1)
				jGetRoot["Status"] = nStatus;
			if(strBoomUserId.length()>0)
			{
				jGetRoot["OrderList"].append(strBoomUserId.c_str());
			}
			string strNewData = jWriter.write(jGetRoot);
			HSet("GameInfo",strKey,strNewData,strRet);		
			//printf("UpdateGameInfo true=%s\n",strNewData.c_str());
		}
	}
	else
	{
		//printf("UpdateGameInfo GetInfo false=%s\n",strRet.c_str());
	}
	sem_post(&sem_lock);
	return bRet;
}

void CRedisPublisher::UpdateGameCreateTime(string strKey)
{
	sem_wait(&sem_lock);
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	Json::Value jGetRoot;
	string strValue="";
	string strRet="";
	bool bRet = HGet("GameInfo",strKey,strValue,strRet);	
	if(bRet)
	{
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			struct timespec ts_now;
			clock_gettime(CLOCK_REALTIME,&ts_now);
			jGetRoot["CreateTime"] = (int)ts_now.tv_sec;

			string strNewData = jWriter.write(jGetRoot);
			HSet("GameInfo",strKey,strNewData,strRet);		
			
		}
	}
	else
	{
		
	}
	sem_post(&sem_lock);
	return ;
}

ENGAMESTATUS CRedisPublisher::GetGameStatus(string strKey)
{
	sem_wait(&sem_lock);
	ENGAMESTATUS enStatus = GAMESTATUSCOUNT;//GAMESTATUSWAIT GAMESTATUSCOUNT
	string strRet="";
	string strValue="";
	bool bRet = HGet("GameInfo",strKey,strValue,strRet);
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::Value jGetRoot;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			enStatus = (ENGAMESTATUS)(jGetRoot["Status"].asInt());
		}
	}
	sem_post(&sem_lock);
	return enStatus;
}

int  CRedisPublisher::GetGameTag(string strKey,TagRedisGame& tag)
{
	sem_wait(&sem_lock);
	memset(&tag,0,sizeof(TagRedisGame));
	tag.nGameStatus=GAMESTATUSCOUNT;
	int nRet = 1;
	string strRet="";
	string strValue="";
	bool bRet = HGet("GameInfo",strKey,strValue,strRet);
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::Value jGetRoot;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			tag.nCreateTime = (jGetRoot["CreateTime"].asInt());
			tag.nWaitOpenGameTimeOut = (jGetRoot["WaitOpenGameTimeOut"].asInt());
			tag.nGameStatus = (jGetRoot["Status"].asInt());
			tag.nCurUserId  = atoi(jGetRoot["CurUserId"].asCString());
			tag.nGameId = atoi(jGetRoot["GameId"].asCString());
			tag.nRemainTime = jGetRoot["RemainTime"].asInt();
		}
	}
	sem_post(&sem_lock);
	return nRet;
}

bool CRedisPublisher::GetOrderList(string strKey,vector<int>& vec)
{
	sem_wait(&sem_lock);
	string strRet="";
	string strValue="";
	bool bRet = HGet("GameInfo",strKey,strValue,strRet);
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::Value jGetRoot;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			int nSize = jGetRoot["OrderList"].size();
			for(int i=nSize-1;i>=0;i--)
			{
				vec.push_back(atoi(jGetRoot["OrderList"][i].asCString()));
			}
		}
	}
	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::DeleteLocalUserInfo(string strTable,string strKey)
{
	sem_wait(&sem_lock);
	string strRet;
	bool bRet = HDel(strTable,strKey,strRet);
	sem_post(&sem_lock);
	return bRet;
}

bool CRedisPublisher::DeleteGameInfo(string strKey)
{
	sem_wait(&sem_lock);
	string strRet;
	bool bRet = HDel("GameInfo",strKey,strRet);
	sem_post(&sem_lock);
	return bRet;
}
