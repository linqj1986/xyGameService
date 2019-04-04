/*************************************************************************
    > File Name: redis_subscriber.h
    > Description: ��װhiredis��ʵ����Ϣ����redis����
 ************************************************************************/

#ifndef REDIS_SUBSCRIBER_H
#define REDIS_SUBSCRIBER_H

#include <stdlib.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <boost/tr1/functional.hpp>
using namespace std;

class CRedisSubscriber
{
public:
    // �ص������������ͣ������յ���Ϣ����ûص�����Ϣ���ͳ�
    typedef std::tr1::function<void (const char *, const char *, int)> NotifyMessageFn;
    typedef std::tr1::function<void (void* p)> NotifyReSubscribeFn;	

    CRedisSubscriber();
    ~CRedisSubscriber();
    
    sem_t moSemConn;
    // ����ص�����
    bool init(const NotifyMessageFn &fn,const NotifyReSubscribeFn &fnReSubscribe);	
    bool uninit();
    bool connect();
    bool disconnect();
    
    // ���Զ�ε��ã����Ķ��Ƶ��
    bool unsubscribe(const std::string &channel_name);
    bool subscribe(const std::string &channel_name);

    bool IsConnected();
    int GetTotalSubNum();	

private:
    // ���������ص�������redis�������
    // ���ӻص�
    static void connect_callback(const redisAsyncContext *redis_context,int status);
	
    // �Ͽ����ӵĻص�
    static void disconnect_callback(const redisAsyncContext *redis_context,int status);

    // ִ������ص�
    static void command_callback(redisAsyncContext *redis_context,void *reply, void *privdata);

    // �¼��ַ��̺߳���
    static void *event_thread(void *data);
    void *event_proc();

private:
    sem_t msemSubLock;
    // libevent�¼�����
    event_base *_event_base;
	// �¼��߳�ID
    pthread_t _event_thread;
	// �¼��̵߳��ź���
    sem_t _event_sem;
	// hiredis�첽����
    redisAsyncContext *_redis_context;
	
	// ֪ͨ���Ļص���������
    NotifyMessageFn _notify_message_fn;
    
    NotifyReSubscribeFn mNotifyReSubscribe;

    bool mbIsConnected;
    
    string m_strIP;
    int m_nPort;
    string m_strPsw;
    int m_nToltalSub;
};

#endif
