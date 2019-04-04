#include "TFCustomerClient.h"
#include "TFCustomerService.h"
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "../PrintLog.h"
#include "../EncryptWrapper.h"

CTFCustomerClient::CTFCustomerClient( void* pServer,const char* szIP )
{
	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	m_nTick = ts_now.tv_sec;

	mbReconn2OtherServer = false;
	mbOffLine = false;
	mbOutRoom = false;
	m_pServer=pServer;
	
	m_nUserType=USERTYPECOUNT;
	mnOpenGameUserCount=0;

	mnTowerCount=10;
	m_nGameType=0;
	m_nGameId=0;
	m_nUserId=0;
	m_nLiveId=0;
	m_nLiveUserId=0;
	memset(m_szChannelName,0,sizeof(m_szChannelName));	
	memset(m_szUserName,0,sizeof(m_szUserName));
	memset(m_szUserPic,0,sizeof(m_szUserPic));
	if(szIP!=NULL)
	{
		memset(m_szIP,0,sizeof(m_szIP));
		strcat(m_szIP,szIP);
	}
}


CTFCustomerClient::~CTFCustomerClient(void)
{
}

int CTFCustomerClient::GetLiveUserId()
{
	return m_nLiveUserId;
}

int CTFCustomerClient::GetLiveId()
{
	return m_nLiveId;
}
int CTFCustomerClient::GetTowerCount()
{	return mnTowerCount;}

int CTFCustomerClient::GetUserId()
{	return m_nUserId;}

int CTFCustomerClient::GetGameId()
{	return m_nGameId;}

int CTFCustomerClient::GetUserType()
{	return m_nUserType;}

void CTFCustomerClient::SetUserType( int nType )
{	m_nUserType=nType;}

int CTFCustomerClient::GetOpenGameUserCount()
{	return mnOpenGameUserCount;}

string CTFCustomerClient::GetChannelName()
{	return m_szChannelName;}

string CTFCustomerClient::GetUserPic()
{
	return m_szUserPic;
}

string CTFCustomerClient::GetUserName()
{
	return m_szUserName;
}

bool CTFCustomerClient::IsOffLine()
{
	return mbOffLine;
}

bool CTFCustomerClient::IsOutRoom()
{
	return mbOutRoom;
}

string CTFCustomerClient::GetLastChannelName()
{
	return m_szLastChannelName;
}

void CTFCustomerClient::SetGameId(int nGameId)
{
	m_nGameId=nGameId;
}

void CTFCustomerClient::SetChannelName(int nGameId)
{
	sprintf(m_szChannelName,"%d_%d",m_nGameType,nGameId);
}

void CTFCustomerClient::SetChannelName(string strChannelName)
{
	memset(m_szChannelName,0,sizeof(m_szChannelName));	
	sprintf(m_szChannelName,"%s",strChannelName.c_str());
}

int CTFCustomerClient::IsAlive()
{
	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	int nTick = ts_now.tv_sec;
	if( (nTick-m_nTick) < 40)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

PDATACHANNELPACKET CTFCustomerClient::Process( const PDATACHANNELPACKET pPacket )
{
	PDATACHANNELPACKET pRet = NULL;
	int nRet = 0 ;
	//printf("byCommandType=[0x%x]\n",(unsigned int)(pPacket->byCommandType));
	
	string strData = "";
	if((pPacket->pbyData != NULL))
	{
		strData=CEncryptWrapper::Decrypt((const char*)(pPacket->pbyData));
	}

	//if(pPacket->byCommandType!=COMMANDTYPE_C2S_HEARTBEAT)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"Process Cmd(%x): %s,Id=%d,m_tSocket=%d",
						     pPacket->byCommandType,strData.c_str(),m_nUserId,m_tSocket);
	}	

	switch( pPacket->byCommandType )
	{
	case COMMANDTYPE_C2S_HEARTBEAT:
		nRet = ProcessHeartBeat(strData );
		break;
	case COMMANDTYPE_C2S_CREATEROOM:
		{
			nRet = ProcessCreateRoom( strData );
		}
		break;
	case COMMANDTYPE_C2S_START:
		{
			nRet = ProcessStart( strData );
		}
		break;
	case COMMANDTYPE_C2S_GAMECOMMAND:
		{
			nRet = ProcessGameCommand( strData );					
     		}
		break;
	case COMMANDTYPE_C2S_HANDSHAKE:
		{
			nRet = ProcessHandShake( strData );
		}
		break;
	case COMMANDTYPE_C2S_CLOSE:
		{
			nRet = ProcessClose( strData );
		}
		break;
	}
	//printf("byCommandType=[0x%x] Process end\n",(unsigned int)(pPacket->byCommandType));
	if(nRet!=SUCCESS)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"<Process>Error: %d,Msg:%s\n",nRet,GetErrorMsg(nRet).c_str());
		string strErrorMsg = GetErrorMsg(nRet);
		string strRet = BuildStandardResponse(false,nRet,strErrorMsg.c_str());
		string strData = CEncryptWrapper::Encrypt(strRet);
		pRet = BuildDataChannelPacket( pPacket->wPacketNo,pPacket->byCommandType,
					       (const BYTE*)(strData.c_str()),strData.length());
	}
	return pRet;
}

int CTFCustomerClient::ProcessHeartBeat( string strData )
{
	// 
 	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	m_nTick = ts_now.tv_sec;
	return SUCCESS;
}

int CTFCustomerClient::ProcessCreateRoom( string strData )
{
	int nRet = ERROR0NORMAL;
	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	bool bRet = false;
	if(jReader.parse((const char*)(strData.c_str()),jRoot))
	{
		if(    (!jRoot["GameType"].isNull()) && (!jRoot["GameId"].isNull()) 
                    && (!jRoot["UserId"].isNull())   && (!jRoot["UserType"].isNull()) 
                     && (!jRoot["TowerCount"].isNull())
		    //&& (!jRoot["LiveUserId"].isNull()) && (!jRoot["LiveId"].isNull()) 

		    && (jRoot["GameType"].isInt())   && (jRoot["GameId"].isInt()) 
                    && (jRoot["UserId"].isInt())     && (jRoot["UserType"].isInt())
  		    && (jRoot["TowerCount"].isInt())
		    //&& (jRoot["LiveUserId"].isInt()) && (jRoot["LiveId"].isInt())
		  )
		{
			mnTowerCount=jRoot["TowerCount"].asInt();
			memset(m_szChannelName,0,sizeof(m_szChannelName));
			memset(m_szLastChannelName,0,sizeof(m_szLastChannelName));
			mnLastGameId = 0;

			m_nUserType = jRoot["UserType"].asInt();
			m_nGameType=jRoot["GameType"].asInt();
			m_nGameId=jRoot["GameId"].asInt();
			m_nUserId=jRoot["UserId"].asInt();
			sprintf(m_szChannelName,"%d_%d",m_nGameType,m_nGameId);
			
			//m_nLiveUserId=jRoot["LiveUserId"].asInt();
			//m_nLiveId=jRoot["LiveId"].asInt();

			if(   (!jRoot["LastGameId"].isNull()) 
			   && (jRoot["LastGameId"].isInt()) 
			   && (jRoot["LastGameId"].asInt()!=0)
			  )
			{// 再来一局
				mnLastGameId = jRoot["LastGameId"].asInt();
				sprintf(m_szLastChannelName,"%d_%d",m_nGameType,mnLastGameId);
			}
			
			if(   (!jRoot["UserPic"].isNull()) 
			   && (jRoot["UserPic"].isString()) 
			  )
			{
				memset(m_szUserPic,0,sizeof(m_szUserPic));
				strcat(m_szUserPic,jRoot["UserPic"].asCString());
			}
			if(   (!jRoot["UserName"].isNull()) 
			   && (jRoot["UserName"].isString()) 
			  )
			{
				memset(m_szUserName,0,sizeof(m_szUserName));
				strcat(m_szUserName,jRoot["UserName"].asCString());
			}
			if(m_nUserType==OWNER)
			{// 是房主才进入创建流程
				nRet = ((CTFCustomerService*)m_pServer)->ClientCreateRoom(this);
			}
		}
	}

	return nRet;
}

int CTFCustomerClient::ProcessStart( string strData )
{
	int nRet = ERROR0NORMAL;

	nRet = ((CTFCustomerService*)m_pServer)->ClientGameStart(this);
	
	return nRet;
}

int CTFCustomerClient::ProcessGameCommand( string strData )
{
	int nRet = 0;
	if( (m_nUserType==OWNER)||(m_nUserType==PLAYER) )
	{
		Json::FastWriter jWriter;
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)strData.c_str(),jRoot))
		{
			if( !jRoot["Type"].isNull() && !jRoot["SrcTowerId"].isNull()
			    && !jRoot["DstTowerId"].isNull() && !jRoot["BotNum"].isNull()
			    && !jRoot["UserId"].isNull()	
                          )
			{
				TagChg tag;
				tag.nType=jRoot["Type"].asInt();
				tag.nSrcTowerId=jRoot["SrcTowerId"].asInt();
				tag.nDstTowerId=jRoot["DstTowerId"].asInt();
				tag.nBotNum=jRoot["BotNum"].asInt();
				tag.nUserId=jRoot["UserId"].asInt();
				struct timespec ts_now;
				clock_gettime(CLOCK_REALTIME,&ts_now);
				tag.nBeginTime=ts_now.tv_sec;
				tag.nPassSecs=0;
				nRet = ((CTFCustomerService*)m_pServer)->ClientBotMoving(this,tag);
			}
		}
	}

	return nRet;
}

int CTFCustomerClient::ProcessHandShake( string strData )
{
	int nRet = ERROR0NORMAL;

	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	if(jReader.parse((const char*)(strData.c_str()),jRoot))
	{
		if(    (!jRoot["GameType"].isNull()) && (!jRoot["GameId"].isNull()) 
                    && (!jRoot["UserId"].isNull())   && (!jRoot["UserType"].isNull()) 
		    && (!jRoot["TowerCount"].isNull())	
		    //&& (!jRoot["LiveUserId"].isNull()) && (!jRoot["LiveId"].isNull()) 

		    && (jRoot["GameType"].isInt())   && (jRoot["GameId"].isInt()) 
                    && (jRoot["UserId"].isInt())     && (jRoot["UserType"].isInt())
		    && (jRoot["TowerCount"].isInt())
		    //&& (jRoot["LiveUserId"].isInt()) && (jRoot["LiveId"].isInt())

		  )
		{
			mnTowerCount=jRoot["TowerCount"].asInt();
			memset(m_szChannelName,0,sizeof(m_szChannelName));

			m_nUserType = jRoot["UserType"].asInt();
			m_nGameType=jRoot["GameType"].asInt();
			m_nGameId=jRoot["GameId"].asInt();
			m_nUserId=jRoot["UserId"].asInt();
			sprintf(m_szChannelName,"%d_%d",m_nGameType,m_nGameId);

			//m_nLiveUserId=jRoot["LiveUserId"].asInt();
			//m_nLiveId=jRoot["LiveId"].asInt();

			if(   (!jRoot["UserPic"].isNull()) 
			   && (jRoot["UserPic"].isString()) 
			  )
			{
				memset(m_szUserPic,0,sizeof(m_szUserPic));
				strcat(m_szUserPic,jRoot["UserPic"].asCString());
			}
			if(   (!jRoot["UserName"].isNull()) 
			   && (jRoot["UserName"].isString()) 
			  )
			{
				memset(m_szUserName,0,sizeof(m_szUserName));
				strcat(m_szUserName,jRoot["UserName"].asCString());
			}
			nRet = ((CTFCustomerService*)m_pServer)->ClientHandShake(this);
		}
	}

	return nRet;
}

int CTFCustomerClient::ProcessClose( string strData )
{ 
	int nRet = ERROR0UNKNOWN0USER;
	mbOutRoom = true;
	if(m_nUserType<USERTYPECOUNT)
	{
		nRet = ((CTFCustomerService*)m_pServer)->ClientExitRoom(this);
		if(nRet<=0)
			nRet = 0-nRet;
	}
	return 1;
}

bool CTFCustomerClient::IsReconn2OtherServer()
{
	return mbReconn2OtherServer;
}

void CTFCustomerClient::SetReconn2OtherServer(bool bSet)
{
	mbReconn2OtherServer = bSet;
}

void CTFCustomerClient::OnClose( )
{	
	// 底层sock断开通知
	mbOffLine = true;
	if(m_nUserType<USERTYPECOUNT)
	{
		if(mbOutRoom==true)
		{// 已经处理过退出房间了
			return;
		}
		else
		{
			CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"ClientOffLine : Check %d\n",m_nUserId);
			
			sleep(15);
			if( IsReconn2OtherServer())
			{// 有重连到其他服务器
				((CTFCustomerService*)m_pServer)->ClientOffLine(this,false);
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL," m_nUserId=%d AlreadyReConn other server\n",m_nUserId);
			}
			else if(((CTFCustomerService*)m_pServer)->IsClientReConn(this) )
			{
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL," m_nUserId=%d AlreadyReConn this server\n",m_nUserId);
			}
			else
			{// 没有重连上来
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"ClientOffLine : %d\n",m_nUserId);
				((CTFCustomerService*)m_pServer)->ClientOffLine(this);
			}
		}		
	}
	else
	{
		return;
	}
}

int CTFCustomerClient::NotifyClient(enumCustomerClientCommand cmdType,string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(cmdType, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CTFCustomerClient::NotifyGameOver(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_GAMEOVER, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CTFCustomerClient::NotifyGameRoleChange(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_GAMECOMMANDRET, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CTFCustomerClient::NotifyUsers(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_USERS, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CTFCustomerClient::NotifyTowers(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_TOWERS, (const unsigned char*)strRet.c_str(), strRet.length());
}

std::string CTFCustomerClient::BuildStandardResponse( const bool bResult, int nRet, const char* pszResult )
{
	std::string strRet = "";

	Json::FastWriter jsonWriter;
	Json::Value jsonRoot;
	jsonRoot["bResult"] = bResult;
	jsonRoot["code"] = nRet;
	jsonRoot["szResult"] = pszResult;
	return jsonWriter.write( jsonRoot ).c_str();
}

PDATACHANNELPACKET CTFCustomerClient::BuildDataChannelPacket( const WORD wPacketNo, const BYTE byCommandType, const BYTE* pbyData, const WORD wDataLen )
{
	PDATACHANNELPACKET pPacket = (PDATACHANNELPACKET)calloc( 1, sizeof(DATACHANNELPACKET) );
	pPacket->Init();
	pPacket->wPacketNo = wPacketNo;
	pPacket->byCommandType = byCommandType;
	if ( wDataLen>0 )
	{
		pPacket->pbyData = (BYTE*)calloc( wDataLen, 1 );
		memcpy( pPacket->pbyData, pbyData, wDataLen );
		pPacket->wDataLen = wDataLen;
	}
	return pPacket;
}

string CTFCustomerClient::GetErrorMsg(int nRet)
{
	switch(nRet)
	{
		case 500:
			return "未知错误";
		case ERROR0UNKNOWN0USER://9000
			return "未知身份用户退出";
		case ERROR0Exit0Check:
			return "玩家退出,列表未找到该用户";
		case ERROR0GameStart0IDLE://9002
			return "人数异常不能开始";
		case ERROR0GameStart0OWER://9003
			return "非房主身份";
		case ERROR0RoleChg0Check://9004
			return "扔出失败";
		case ERROR0Role0Viewer://9007
			return "游戏人数已满";
		case ERROR0POST0ERROR://9008
			return "提交web接口失败";
		default:
			return "";
	}
}
