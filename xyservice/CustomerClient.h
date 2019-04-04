#pragma once
#include "DataSocket.h"
#include "redis_publisher.h"
#include <json/json.h>
#include <json/value.h>

#ifndef WORD
#define WORD unsigned short
#endif
#ifndef BYTE
#define BYTE unsigned char
#endif

#define MAX_CHANNELNAME_LEN 500

enum enumCustomerClientCommand{
	COMMANDTYPE_S2C_GAMECOMMANDRET 	= 0x01,
	COMMANDTYPE_S2C_GAMEOVER 	= 0x02,
	COMMANDTYPE_S2C_USERS	 	= 0x03,
	COMMANDTYPE_C2S_CREATEROOM      = 0x80,
	COMMANDTYPE_C2S_START 		= 0x81,
	COMMANDTYPE_C2S_GAMECOMMAND 	= 0x82,

	COMMANDTYPE_C2S_CLOSE 		= 0xFD,
	COMMANDTYPE_C2S_HEARTBEAT	= 0xFE,
	COMMANDTYPE_C2S_HANDSHAKE	= 0xFF
};

class CCustomerClient : public CDataSocket
{
private:
	char m_szIP[20];
	char m_szUserId[100];
	char m_szUserPic[100];
	char m_szUserName[100];
	char m_szLiveId[100];
	char m_szLiveUserId[100];
	bool mbOffLine;
	bool mbOutRoom;
	bool mbReconn2OtherServer;
	void* m_pServer;
	int m_nTick;
	char m_szRoomId[100];
	char m_szGameType[100];
	char m_szGameId[100];
	char m_szChannelName[MAX_CHANNELNAME_LEN];
	int m_nUserType;
	int mnMaxRemainTime;
	int mnMinRemainTime;
	int mnMaxStayTime;
	int mnLastGameId;
	int mnOpenGameUserCount;
	char m_szLastChannelName[MAX_CHANNELNAME_LEN];

private:
	string BuildStandardResponse( const bool bResult, int nRet, const char* pszResult );
	PDATACHANNELPACKET BuildDataChannelPacket( const WORD wPacketNo, const BYTE byCommandType, const BYTE* pbyData, const WORD wDataLen );
	string GetErrorMsg(int nRet);

public:
	CCustomerClient( void* pServer,const char* szIP );
	~CCustomerClient(void);

	virtual void OnClose( );
	virtual PDATACHANNELPACKET Process( const PDATACHANNELPACKET pPacket );

	int ProcessHeartBeat( string strData );
	int ProcessCreateRoom( string strData );
	int ProcessStart( string strData );
	int ProcessGameCommand( string strData );
	int ProcessHandShake( string strData );
	int ProcessClose( string strData );

	bool IsOffLine();
	bool IsOutRoom();
	int IsAlive();
	string GetChannelName();
	string GetUserId();
	string GetUserPic();
	string GetUserName();
	string GetLiveUserId();
	string GetLiveId();
	string GetGameId();
	bool IsReconn2OtherServer();
	void SetReconn2OtherServer(bool bSet);
	int GetUserType();
	void SetUserType( int nType );
	int GetMaxRemainTime();
	int GetMinRemainTime();
	int GetMaxStayTime();
	int GetOpenGameUserCount();
	string GetLastChannelName();
	void SetGameId(int nGameId);
	void SetChannelName(int nGameId);
	void SetChannelName(string strChannelName);
	int NotifyClient(enumCustomerClientCommand cmdType,string strData);
	int NotifyGameRoleChange(string strData);
	int NotifyGameOver(string strData);
	int NotifyUsers(string strData);
};


