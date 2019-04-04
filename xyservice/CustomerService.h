#pragma once
#include "DataService.h"
#include "CustomerClient.h"
#include <pthread.h>
#include "redis_publisher.h"
//#include "redis_subscriber.h"
#include <map>
#include <list>
#include <queue>
#include <algorithm>
#include <json/json.h>
#include "WebComm.h"
#include "Subscriber.h"
//US
#define CKBOOM_SLEEP_US  1*30*1000


typedef struct TAG_SUBMSG
{
	string strChannel;
	string strMsg;
}TagSubMsg;

typedef struct TAG_GAME
{
	int nGameOverTime;
	bool bThisServer;
	int nCreateTime;
	int nRemainTime;
	int nMaxTime;
	int nMinTime;
	int nMaxStayTime;
	int nGameId;
	string strCurUserId;
	string strLastChannelName;
	list<CCustomerClient*> clientList;
	
}TagGame;

typedef map<string,TagGame> CHANNELMAP;
typedef map<string,TagGame>::iterator CHANNELIT;

typedef map< string,map<CCustomerClient*,CCustomerClient*> > VIEWERMAP;
typedef map< string,map<CCustomerClient*,CCustomerClient*> >::iterator VIEWERIT;

class CCustomerService : public CDataService
{
protected:

	sem_t sem_lock;
	CHANNELMAP m_mapChannel;
	CWebComm m_oWebComm;
	//CRedisSubscriber m_oSubscriber;
	CSubscriber m_oSubscriber;
	char m_szMac[MAX_CHANNELNAME_LEN];

	// 所有sock
	sem_t semAllSockLock;
	std::vector<CCustomerClient*> m_vecCustomerClient;

	// 观众
	sem_t semViewerLock;
	map< string,map<CCustomerClient*,CCustomerClient*> > m_mapViewer; 

public:
	CCustomerService(void);
	~CCustomerService(void);

	sem_t semQueLock;
	std::queue<TagSubMsg> m_queSubMsg;

	sem_t semUnSubQueLock;
	std::queue<string> m_queUnSubMsg;

	// 继承
	virtual int OnNewSocket( int sock, const char* pszIp );
	virtual unsigned int Run( unsigned int uPort, int bAutoAddPort = 0 );
	virtual void Stop( );

	// 订阅短线重连
	void ReSubscribeAll();
	static void* ReSubscribeAllThread( void *param );

	// 客户端业务处理
	int ClientCreateRoom(CCustomerClient*pClient);
	int ClientHandShake(CCustomerClient*pClient);
	int ClientGameStart(CCustomerClient*pClient);
	int ClientRoleChg(CCustomerClient*pClient,string strDestUserId);
	int ClientExitRoom(CCustomerClient*pClient);
	bool ClientOffLine(CCustomerClient*pClient,bool bNotify=true);
	
	bool IsClientReConn(CCustomerClient*pClient);
private:
	// 线程 检查各sock心跳
	void CheckAlive();
	static void* CheckAliveThread( void *param );

	// 线程 检查炸弹是否爆炸
	void CheckBoom();
	static void* CheckBoomThread( void *param );

	// 线程 取出订阅消息
	static void* PopSubMsgThread(void *param);
	// 处理订阅消息
	void DoSubscriberMsg(const char *channel_name,const char *message);

	// 取消订阅
	static void* PopUnsubMsgThread(void *param);

	// 订阅
	void AddChannel(const char* szChannelName);
	void SubscribeC(const char* szChannelName);

	// 玩家
	void AddUser(CCustomerClient*pClient);

	// 观众
	void BroadcastViewer(enumCustomerClientCommand cmdType,string strData, const char* szChannelName);
	void AddViewer(CCustomerClient*pClient,const char* szChannelName);
	int DelViewer(CCustomerClient*pClient,const char* szChannelName);
	int DelViewerInAll(CCustomerClient*pClient,const char* szChannelName);

	// 发布广播
	void PublishUserIds(int nUserId,const char *szChannelName,const char *szLastChannelName,int nGameId,string strDataInfo,int nGameStatus=GAMESTATUSWAIT);
	void PublishRoleChg(const char *szChannelName, const char *szLastUserId, const char *szCurUserId);
	void PublishBoom(const char *szChannelName, const char *szOverUserId, const char *szNextUserId,string strDataInfo);

	// 
	int GetTimeLeftAndStopBoomCheck(const char *channel_name);
	string GetServerId();
	bool IsBoom(string strTable,string strKey, string strUserId);
	bool IsTheRole(string strTable,string strUserId);
	bool IsValidConnect(string strTable,string strUserId);
	bool IsInThisServer(string strTable,string strKey, string strUserId);
};

