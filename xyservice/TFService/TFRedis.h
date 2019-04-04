/*************************************************************************
    > File Name: redis_publisher.h
    > Description: 封装hiredis，实现消息发布给redis功能
 ************************************************************************/

#ifndef REDIS_PUBLISHER_H
#define REDIS_PUBLISHER_H

#include <stdlib.h>
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>
#include "../SingletonObj.h"
#include "../RedisClient.h"
using namespace std;

//MS
#define MAX_REMAIN_TIME     30*1000
#define MIN_REMAIN_TIME     15*1000

#define MAX_STAY_TIME       5*1000
#define MAX0USER0COUNT 	    18

// 用户游戏状态
typedef enum EN_USERSTATUS
{
	USERBOOM=0,
	USERIDLE=1,
	USERROLE=2,
	USERSTATUSCOUNT
}ENUSERSTATUS;

// 用户身份类型
typedef enum EN_USERTYPE
{
	OWNER=0,
	PLAYER=1,
	VIEWER=2,
	USERTYPECOUNT
}ENUSERTYPE;

// 用户断开类型
typedef enum EN_USEREXITTYPE
{
	EXIT_SOCKET=1,
	EXIT_ACTION=2
}ENUSEREXITTYPE;

// 游戏状态
typedef enum EN_GAMESTATUS
{
	GAMESTATUSWAIT=0,
	GAMESTATUSBEGIN=1,
	GAMESTATUSOVER=2,
	GAMESTATUSCOUNT
}ENGAMESTATUS;

typedef struct TAG_REDIS_GAME
{
	bool bThisServer;
	int nGameId;
	int nCreateTime;
	int nRemainTime;
	int nMaxTime;
	int nMinTime;	
	int nWaitOpenGameTimeOut;
	int nGameStatus;
	int nCurUserId;
}TagRedisGame;

typedef struct TAG_REDIS_USER
{
	int nStatus;
	int nType;
}TagRedisUser;

class CTFRedis
{
    DECLARE_SINGLETON(CTFRedis);
public:    
    CTFRedis();
    ~CTFRedis();

    // 发布
    bool Publish(const string &channel_name, const string &message);
    
    // 游戏用户表-表名为频道
    bool GetUserIds(string strTable,string& strValue);
    bool AddUserId(string strTable, string strKey, int nUserId,int nUserType, string strUserPic, string strUserName);
    int RemoveUserId(string strTable, string strKey, int nUserId);
    bool DeleteUserInfo(string strTable);
    bool DeleteLocalUserInfo(string strTable,string strKey);
    bool UpdateUserStatus(string strTable,string strKey, int nUserId,int nStatus,int nInRoom,int nOnLine=-1);
    int GetUserStatus(string strTable,string strKey, string strUserId);
    int GetIdleUser(string strTable,string strExcept,string& strIdleUserId);
    bool GetUserInfo(string strTable, string strKey,int nUserId,TagRedisUser& tag);
    int ChangeOwner(string strTable,int nLastOwnerId);
    bool IsAllOffLine(string strTable);

    bool SetTowerIds(string strTable,string strValue);
    bool GetTowerIds(string strTable,string& strValue);

    // 游戏信息表
    bool CreateGameInfo(string strKey, int nGameId,int nWaitOpenGameTimeOut);
    bool UpdateGameInfo(string strKey, int nStatus);
    bool GetOrderList(string strKey,vector<int>& vec);
    ENGAMESTATUS  GetGameStatus(string strKey);
    int  GetGameTag(string strKey,TagRedisGame& tag);
    void UpdateGameCreateTime(string strKey);
    bool DeleteGameInfo(string strKey);
	
private:
    CRedisClient mRedisClient;
    string m_strDb;
    string m_strIP;
    int m_nPort;
    string m_strPsw;
    sem_t sem_lock;
    //sem_t semGameInfolock;
    //void* m_pContext;
    void* Connect( string strIP, int nPort, string strPwd, string& strResult );
    bool Close( void* pContext, string& strResult );
    bool HGetAllValues( string strTable,vector<string>& vecKeys, vector<string>& vecValues,  string& strResult );
    bool HSet( string strTable, string strKey, string strValue, string& strResult );
    bool HGet( string strTable, string strKey,  string& strValue,  string& strResult );
    bool HDel( string strTable, string strKey, string& strResult );
    bool DeleteTable( string strTable, string& strResult );
    bool InnerPublish(const string &channel_name, const string &message);

    bool Set( string strTable, string strValue, string& strResult);
    bool Get( string strTable, string& strValue, string& strResult);
public:
    bool Lock(string strTable);
    bool Unlock(string strTable);
};

#endif
