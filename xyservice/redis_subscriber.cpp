#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "redis_subscriber.h"
#include "ParseIniFile.h"
#include "PrintLog.h"
#include <event2/thread.h>

CRedisSubscriber *g_pThis = NULL;

CRedisSubscriber::CRedisSubscriber():_event_base(0), _event_thread(0),
_redis_context(0)
{
	m_nToltalSub=0;
	mbIsConnected = false;
	g_pThis = this;
	
    	CParseIniFile ini;
	m_strIP = ini.GetValue("cfg","config","rip");
	string strPort = ini.GetValue("cfg","config","rport");
	m_nPort=atoi(strPort.c_str());
	m_strPsw=ini.GetValue("cfg","config","rpwd");

	// 创建libevent对象
	int nCode = evthread_use_pthreads();
 	_event_base = event_base_new();    
	if (NULL == _event_base)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: Create redis event failed.");
	}
	
	// 初始化_event_sem
	sem_init(&_event_sem, 0, 0);// 忙的

	// 初始化连接结果lock
	sem_init(&moSemConn, 0, 0); 
}

CRedisSubscriber::~CRedisSubscriber()
{
	
}

bool CRedisSubscriber::init(const NotifyMessageFn &fn,const NotifyReSubscribeFn &fnReSubscribe)
{
    _notify_message_fn = fn;
    mNotifyReSubscribe = fnReSubscribe;
    return true;
}

bool CRedisSubscriber::uninit()
{    
    sem_destroy(&_event_sem);
    sem_destroy(&moSemConn);
    return true;
}

bool CRedisSubscriber::connect()
{
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CRedisSubscriber: AsynConn Redis.");

	// connect redis
	_redis_context = redisAsyncConnect(m_strIP.c_str(), m_nPort);    // 异步连接到redis服务器上，使用默认端口
	if (NULL == _redis_context)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: Connect redis failed.");
		return false;
	}

	if (_redis_context->err)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_ERROR,"CRedisSubscriber: Connect redis error: %d, %s\n", 
						     _redis_context->err, _redis_context->errstr);    // 输出错误信息
		return false;
	}

	// attach the event
	redisLibeventAttach(_redis_context, _event_base);    // 将事件绑定到redis context上，使设置给redis的回调跟事件关联

	// 创建事件处理线程
	int ret = pthread_create(&_event_thread, 0, &CRedisSubscriber::event_thread, this);
	if (ret != 0)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: create event thread failed.");
		disconnect();
		return false;
	}

	// 设置连接回调，当异步调用连接后，服务器处理连接请求结束后调用，通知调用者连接的状态
    	redisAsyncSetConnectCallback(_redis_context, &CRedisSubscriber::connect_callback);

	// 设置断开连接回调，当服务器断开连接后，通知调用者连接断开，调用者可以利用这个函数实现重连
    	redisAsyncSetDisconnectCallback(_redis_context,&CRedisSubscriber::disconnect_callback);

	// 启动事件线程
    	sem_post(&_event_sem);
    	return true;
}

bool CRedisSubscriber::disconnect()
{
    //struct timeval delay = {2,20*1000};
    event_base_loopexit(_event_base,NULL);
   
    if(mbIsConnected)
    {
	if (_redis_context)
	{
		redisAsyncDisconnect(_redis_context);
		_redis_context = NULL;
	}
    }

    mbIsConnected = false;
    return true;
}

bool CRedisSubscriber::subscribe(const std::string &channel_name)
{
    int ret = redisAsyncCommand(_redis_context, &CRedisSubscriber::command_callback, this, "SUBSCRIBE %s", channel_name.c_str());
    if (REDIS_ERR == ret)
    {
	CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: subscribe failed: %d", ret);
        return false;
    }
    
    return true;
}

bool CRedisSubscriber::unsubscribe(const std::string &channel_name)
{
	int ret = redisAsyncCommand(_redis_context, NULL, this, "UNSUBSCRIBE %s", channel_name.c_str());
 	if (REDIS_ERR == ret)
    	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: unsubscribe failed: %d", ret);
		return false;
    	}
    
    return true;
}

void CRedisSubscriber::connect_callback(const redisAsyncContext *redis_context,int status)
{
    if (status != REDIS_OK)
    {
	CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber(connect_callback) : Error: %s", redis_context->errstr);
    }
    else
    {
	if(g_pThis->m_strPsw.length()>0)
	{
		string strCommandLogin = "AUTH ";
		strCommandLogin += g_pThis->m_strPsw;
		int ret = redisAsyncCommand(g_pThis->_redis_context, &CRedisSubscriber::command_callback, g_pThis, strCommandLogin.c_str());
		if (REDIS_ERR == ret)
		{
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: Login failed: %d,Psw=%s", ret,g_pThis->m_strPsw.c_str());
			sem_post(&(g_pThis->moSemConn));
		}   
	}   
	else
	{
	    g_pThis->mbIsConnected = true;
    	    sem_post(&(g_pThis->moSemConn));
	}		
    }
}

void CRedisSubscriber::disconnect_callback(const redisAsyncContext *redis_context, int status)
{
    if (status != REDIS_OK)
    {
	g_pThis->mbIsConnected = false;
	// 这里异常退出，可以尝试重连
	CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber(disconnect_callback): Error: %s", redis_context->errstr);
       
    }
}

// 消息接收回调函数
void CRedisSubscriber::command_callback(redisAsyncContext *redis_context,void *reply, void *privdata)
{
    if (NULL == reply || NULL == privdata) 
    {
        //printf("CRedisSubscriber : reply = %p\n",reply);
        return ;
    }

    // 静态函数中，要使用类的成员变量，把当前的this指针传进来，用this指针间接访问
    CRedisSubscriber *self_this = reinterpret_cast<CRedisSubscriber *>(privdata);
    redisReply *redis_reply = reinterpret_cast<redisReply *>(reply);
    
    // 订阅接收到的消息是一个带三元素的数组
    if ( (redis_reply->type == REDIS_REPLY_ARRAY) 
      && (redis_reply->elements == 3)
       )
    {
	// 调用函数对象把消息通知给外层
	if(strcmp(redis_reply->element[0]->str,"message")==0)
	{
		self_this->_notify_message_fn(redis_reply->element[1]->str,redis_reply->element[2]->str, redis_reply->element[2]->len);
	}

	if(strcmp(redis_reply->element[0]->str,"unsubscribe")==0)
		self_this->m_nToltalSub = redis_reply->element[2]->integer;
    }
    // 登录
    else if( (redis_reply->type == REDIS_REPLY_STATUS) )
    {
            //printf("CRedisSubscriber: REDIS_REPLY_STATUS\n");
	    if((strcasecmp(redis_reply->str,"OK")==0))
	    {
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CRedisSubscriber : Login Success.");
		g_pThis->mbIsConnected = true;
	    }
	    else
	    {
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber : Login failed: err=%s", redis_reply->str);
	    }
    	    sem_post(&(g_pThis->moSemConn));
    }
}

void *CRedisSubscriber::event_thread(void *data)
{
    pthread_detach(pthread_self());
    if (NULL == data)
    {
	CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber: Error!");
        assert(false);
        return NULL;
    }

    CRedisSubscriber *self_this = reinterpret_cast<CRedisSubscriber *>(data);
    return self_this->event_proc();
}

void *CRedisSubscriber::event_proc()
{
    sem_wait(&_event_sem);
	
    // 开启事件分发，event_base_dispatch会阻塞
    event_base_dispatch(_event_base);

    CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"CRedisSubscriber::event_proc()!");
    g_pThis->mNotifyReSubscribe((void*)0);
    return NULL;
}

bool CRedisSubscriber::IsConnected()
{
	return mbIsConnected;
}

int CRedisSubscriber::GetTotalSubNum()
{
	return m_nToltalSub;
}

