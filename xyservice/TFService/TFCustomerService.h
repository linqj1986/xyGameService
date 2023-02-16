#pragma once
#include "../DataService.h"
#include "TFCustomerClient.h"
#include <pthread.h>
#include "TFRedis.h"
#include <map>
#include <list>
#include <queue>
#include <algorithm>
#include <json/json.h>
#include "../WebComm.h"
#include "../Subscriber.h"
//US
#define CKBOOM_SLEEP_US  1*30*1000

typedef struct TAG_CHG
{
	int nType;
	int nSrcTowerId;
	int nDstTowerId;
	int nBotNum;
	int nUserId;
	int nSecs;
	int nBeginTime;
	int nPassSecs;
}TagChg;

typedef struct TAG_SUBMSG
{
	string strChannel;
	string strMsg;
}TagSubMsg;

typedef struct TAG_GAME
{
	int nGameOverTime;
	int nGameStatus;
	int nCreateTime;
	//int nRemainTime;
	//int nMaxTime;
	//int nMinTime;
	//int nMaxStayTime;
	int nGameId;
	int nOwnerId;
	string strLastChannelName;
	list<CTFCustomerClient*> clientList;
	list<string> chgList;
	
}TagGame;

typedef map<string,TagGame> CHANNELMAP;
typedef map<string,TagGame>::iterator CHANNELIT;

typedef map< string,map<CTFCustomerClient*,CTFCustomerClient*> > VIEWERMAP;
typedef map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator VIEWERIT;

class CTFCustomerService : public CDataService
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
	std::vector<CTFCustomerClient*> m_vecCustomerClient;

	// 观众
	sem_t semViewerLock;
	map< string,map<CTFCustomerClient*,CTFCustomerClient*> > m_mapViewer; 

public:
	CTFCustomerService(void);
	~CTFCustomerService(void);

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
	int ClientCreateRoom(CTFCustomerClient*pClient);
	int ClientHandShake(CTFCustomerClient*pClient);
	int ClientGameStart(CTFCustomerClient*pClient);
	int ClientBotMoving(CTFCustomerClient*pClient,TagChg tag);
	int ClientExitRoom(CTFCustomerClient*pClient);
	bool ClientOffLine(CTFCustomerClient*pClient,bool bNotify=true);
	
	bool IsClientReConn(CTFCustomerClient*pClient);
	
private:
	// 线程 检查各sock心跳
	void CheckAlive();
	static void* CheckAliveThread( void *param );

	// 线程 检查递增
	void CheckBirth();
	static void* CheckBirthThread( void *param );

	// 线程 取出订阅消息
	static void* PopSubMsgThread(void *param);
	// 处理订阅消息
	void DoSubscriberMsg(const char *channel_name,const char *message);
	void DealUserIdsMsg(const char *channel_name,const char *message);

	// 取消订阅
	static void* PopUnsubMsgThread(void *param);

	// 订阅
	void SubscribeC(const char* szChannelName);

	// 玩家
	void AddUser(CTFCustomerClient*pClient);

	// 观众
	void BroadcastViewer(enumCustomerClientCommand cmdType,string strData, const char* szChannelName);
	void AddViewer(CTFCustomerClient*pClient,const char* szChannelName);
	int DelViewer(CTFCustomerClient*pClient,const char* szChannelName);
	int DelViewerInAll(CTFCustomerClient*pClient,const char* szChannelName);

	// 发布广播
	void PublishUserIds(int nUserId,const char *szChannelName,const char *szLastChannelName,int nGameId,string strDataInfo,int nGameStatus=GAMESTATUSWAIT);
	void PublishTowerIds(const char *szChannelName,string strIds="");
	void PublishGameOver(const char *szChannelName, int nOverUserId,string strDataInfo);
	void PublishBotMoving(const char *szChannelName,TagChg tag);

	string GetServerId();
	bool IsInThisServer(string strTable,string strKey, string strUserId);
	bool PraseJsonToChgTag(string strJson,TagChg* pTag);
	string PraseChgTagToJson(TagChg tag);
	string GetChgList(string strChannelName);
	int DoBotMoving(CTFCustomerClient*pClient,TagChg tag);
};

