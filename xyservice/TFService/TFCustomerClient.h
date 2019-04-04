#pragma once
#include "../DataSocket.h"
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
	COMMANDTYPE_S2C_TOWERS          = 0X04,
	COMMANDTYPE_C2S_CREATEROOM      = 0x80,
	COMMANDTYPE_C2S_START 		= 0x81,
	COMMANDTYPE_C2S_GAMECOMMAND 	= 0x82,

	COMMANDTYPE_C2S_CLOSE 		= 0xFD,
	COMMANDTYPE_C2S_HEARTBEAT	= 0xFE,
	COMMANDTYPE_C2S_HANDSHAKE	= 0xFF
};

class CTFCustomerClient : public CDataSocket
{
private:
	char m_szIP[20];
	int m_nUserId;
	char m_szUserPic[100];
	char m_szUserName[100];
	int m_nLiveId;
	int m_nLiveUserId;
	bool mbOffLine;
	bool mbOutRoom;
	bool mbReconn2OtherServer;
	void* m_pServer;
	int m_nTick;
	int mnTowerCount;
	int m_nGameType;
	int m_nGameId;
	char m_szChannelName[MAX_CHANNELNAME_LEN];
	int m_nUserType;
	int mnLastGameId;
	int mnOpenGameUserCount;
	char m_szLastChannelName[MAX_CHANNELNAME_LEN];

private:
	string BuildStandardResponse( const bool bResult, int nRet, const char* pszResult );
	PDATACHANNELPACKET BuildDataChannelPacket( const WORD wPacketNo, const BYTE byCommandType, const BYTE* pbyData, const WORD wDataLen );
	string GetErrorMsg(int nRet);

public:
	CTFCustomerClient( void* pServer,const char* szIP );
	~CTFCustomerClient(void);

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
	int GetUserId();
	string GetUserPic();
	string GetUserName();
	int GetLiveUserId();
	int GetLiveId();
	int GetGameId();
	bool IsReconn2OtherServer();
	void SetReconn2OtherServer(bool bSet);
	int GetUserType();
	void SetUserType( int nType );
	int GetTowerCount();
	int GetOpenGameUserCount();
	string GetLastChannelName();
	void SetGameId(int nGameId);
	void SetChannelName(int nGameId);
	void SetChannelName(string strChannelName);
	int NotifyClient(enumCustomerClientCommand cmdType,string strData);
	int NotifyGameRoleChange(string strData);
	int NotifyGameOver(string strData);
	int NotifyUsers(string strData);
	int NotifyTowers(string strData);
};


