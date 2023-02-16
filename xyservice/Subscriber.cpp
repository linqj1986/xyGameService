#include "Subscriber.h"
#include "ParseIniFile.h"
#include "PrintLog.h"
#include <condition_variable>

subscribe_callback_t Notify;

CSubscriber::CSubscriber()
{
	// 配置
	CParseIniFile ini;

	mstrIP = ini.GetValue("cfg","config","rip");
	string strPort = ini.GetValue("cfg","config","rport");

	mnPort=atoi(strPort.c_str());

	mstrPsw=ini.GetValue("cfg","config","rpwd");

	// 
	sem_init(&moSemLock, 0, 1);
}

CSubscriber::~CSubscriber()
{
}

int CSubscriber::Init(const subscribe_callback_t& fNotify)
{  
	Notify = fNotify;
}

int CSubscriber::Connect()
{
    int nRet = 1;
    if(!msub.is_connected())
    { 
	    msub.connect(mstrIP.c_str(), mnPort, [](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status) 
	    {
		    if (status == cpp_redis::subscriber::connect_state::dropped) 
		    {
		        //std::cout << "client disconnected from " << host << ":" << port << std::endl;
			CPrintLog::GetInstance()->LogFormat( LOG_ERROR,"!!!subscribe client disconnected!!!");
		    }
	    });
	   
	    if(mstrPsw.length()>0) 
	    {
		    msub.auth(mstrPsw.c_str(), [](const cpp_redis::reply& reply) 
		    {
		    	if (reply.is_error()) 
			{ 
				//std::cerr << "Authentication failed: " << reply.as_string() << std::endl; 
				CPrintLog::GetInstance()->LogFormat( LOG_ERROR,"!!!subscribe Authentication failed!!!");
			}
		    	else 
			{
		    		//std::cout << "successful authentication" << std::endl;
				CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"subscribe login success");
		     	}
		    });
	    }  
    }
    return nRet;	
}

int CSubscriber::Subscribe(string strChannel)
{
	sem_wait(&moSemLock);
	if(Connect()==1)
	{
		msub.subscribe(strChannel.c_str(), Notify);
  		msub.commit();
	}
	sem_post(&moSemLock);

}

int CSubscriber::UnSubscribe(string strChannel)
{
	sem_wait(&moSemLock);
	if(Connect()==1)
	{
		msub.unsubscribe(strChannel.c_str());
  		msub.commit();
	}
	sem_post(&moSemLock);
}
