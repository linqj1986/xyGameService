#include "RedisClient.h"
#include<boost/shared_ptr.hpp>
#include "PrintLog.h"

CRedisClient::CRedisClient()
{
}

CRedisClient::~CRedisClient()
{
    sem_wait(&semlock);
    while(!m_clients.empty())
    {
        redisContext *ctx = m_clients.front();
        redisFree(ctx);
        m_clients.pop();
    }
    sem_post(&semlock);
}

void CRedisClient::Init(string ip, int port,string psw,string db,  int timeout)
{
    m_timeout = timeout;
    m_serverPort = port;
    m_setverIp = ip;
    mstrPsw = psw;
    mstrDb  = db;
    m_beginInvalidTime = 0;
    sem_init(&semlock, 0, 1);
}

bool CRedisClient::ExecuteCmd(string &response,const char *fmt, vector<string>& vec)
{
    bool bRet = false;
    redisReply *reply = ExecuteCmd(fmt, vec);
    if(reply == NULL) return false;

    boost::shared_ptr<redisReply> autoFree(reply, freeReplyObject);
    if(reply->type == REDIS_REPLY_INTEGER)
    {
        char szVal[20]={0};
        sprintf(szVal,"%lld",reply->integer);
        response = szVal;
        bRet = true;
    }
    else if(reply->type == REDIS_REPLY_STRING)
    {
        response.assign(reply->str, reply->len);
        bRet = true;
    }
    else if(reply->type == REDIS_REPLY_STATUS)
    {
        response.assign(reply->str, reply->len);
        bRet = true;
    }
    else if(reply->type == REDIS_REPLY_NIL)
    {
        response = "";
        bRet = true;
    }
    else if(reply->type == REDIS_REPLY_ERROR)
    {
        response.assign(reply->str, reply->len);
        bRet = false;
    }
    else if(reply->type == REDIS_REPLY_ARRAY)
    {
        response = "Not Support Array Result!!!";
        bRet = false;
    }
    else
    {
        response = "Undefine Reply Type";
        bRet = false;
    }
    //printf("%s,%s\n",fmt,response.c_str());
    return bRet;
}

bool CRedisClient::ExecuteArrayCmd(vector<string>& vecKeys, vector<string>& vecValues,const char *fmt, vector<string>& vec)
{
    redisReply *reply = ExecuteCmd(fmt, vec);
    if(reply == NULL) return false;

    boost::shared_ptr<redisReply> autoFree(reply, freeReplyObject);
    if(reply->type == REDIS_REPLY_ARRAY)
    {
    	for (int i=0;i<reply->elements;i++)
	{
		if (i%2==0)
		{
			vecKeys.push_back(reply->element[i]->str);
		}
		else
			vecValues.push_back(reply->element[i]->str);
	}
        return true;
    }
    else
    {
        return false;
    }
}

redisReply* CRedisClient::ExecuteCmd(const char *fmt, vector<string>& vec)
{
    redisContext *ctx = CreateContext();
    if(ctx == NULL) return NULL;

    redisReply *reply = NULL;
    switch(vec.size())
    {
	case 1:
              	reply=(redisReply*)redisCommand(ctx, fmt, vec[0].c_str());
		break;
	case 2:
              	reply=(redisReply*)redisCommand(ctx, fmt, vec[0].c_str(), vec[1].c_str());
		break;
        case 3:
              	reply=(redisReply*)redisCommand(ctx, fmt, vec[0].c_str(), vec[1].c_str(), vec[2].c_str());
		break;
	case 4:
              	reply=(redisReply*)redisCommand(ctx, fmt, vec[0].c_str(), vec[1].c_str(), vec[2].c_str(), vec[3].c_str());
		break;
	default:
              	break;
        
    }
    

    ReleaseContext(ctx, reply != NULL);

    return reply;
}

redisContext* CRedisClient::CreateContext()
{
    sem_wait(&semlock);//
    if(!m_clients.empty())
    {
        redisContext *ctx = m_clients.front();
        m_clients.pop();
	sem_post(&semlock);//
        if(CheckStatus(ctx))
        	return ctx;
    }
    else
    	sem_post(&semlock);//

    time_t now = time(NULL);
    if(now < m_beginInvalidTime + m_maxReconnectInterval) return NULL;

    struct timeval tv;
    tv.tv_sec = m_timeout / 1000;
    tv.tv_usec = (m_timeout % 1000) * 1000;;
    redisContext *ctx = redisConnectWithTimeout(m_setverIp.c_str(), m_serverPort, tv);
    if(ctx == NULL || ctx->err != 0)
    {
        if(ctx != NULL) redisFree(ctx);

        m_beginInvalidTime = time(NULL);
        
        return NULL;
    }
	
    if (mstrPsw.length()>0)
    {
	string strCommandLogin = "AUTH ";
	strCommandLogin += mstrPsw;
	redisReply* r = (redisReply*)redisCommand(ctx, strCommandLogin.c_str());
        boost::shared_ptr<redisReply> autoFree(r, freeReplyObject);
	if( NULL == r)
	{//失败
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:Login failure\n");
		redisFree(ctx);
		m_beginInvalidTime = time(NULL);
		return NULL;
	}
	if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))
	{//失败
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:Login failure:%s\n",r->str);
		redisFree(ctx);
		m_beginInvalidTime = time(NULL);
		return NULL;
	}	
    }

    // 选择DB
    if(mstrDb!="0")
    {
	string strCommandSelect = "SELECT ";
	strCommandSelect += mstrDb;
	redisReply* r = (redisReply*)redisCommand(ctx, strCommandSelect.c_str());
	boost::shared_ptr<redisReply> autoFree(r, freeReplyObject);
	if( NULL == r)
	{//失败
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:SELECT failure\n");
		redisFree(ctx);
		m_beginInvalidTime = time(NULL);
		return NULL;
	}
	if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))
	{//失败
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisPublisher:SELECT failure:%s\n",r->str);
		redisFree(ctx);
		m_beginInvalidTime = time(NULL);
		return NULL;
	}	
    }

    return ctx;
}

void CRedisClient::ReleaseContext(redisContext *ctx, bool active)
{
    if(ctx == NULL) return;
    if(!active) {redisFree(ctx); return;}

    sem_wait(&semlock);
    m_clients.push(ctx);
    sem_post(&semlock);
}

bool CRedisClient::CheckStatus(redisContext *ctx)
{
    redisReply *reply = (redisReply*)redisCommand(ctx, "ping");
    if(reply == NULL) return false;

    boost::shared_ptr<redisReply> autoFree(reply, freeReplyObject);

    if(reply->type != REDIS_REPLY_STATUS) return false;
    if(strcasecmp(reply->str,"PONG") != 0) return false;

    return true;
}
