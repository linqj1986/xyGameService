#include "CustomerClient.h"
#include "CustomerService.h"
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "PrintLog.h"
#include "EncryptWrapper.h"

CCustomerClient::CCustomerClient( void* pServer,const char* szIP )
{
	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	m_nTick = ts_now.tv_sec;

	mbReconn2OtherServer = false;
	mbOffLine = false;
	mbOutRoom = false;
	m_pServer=pServer;
	
	m_nUserType=USERTYPECOUNT;
	mnMaxRemainTime=MAX_REMAIN_TIME;
	mnMinRemainTime=MIN_REMAIN_TIME;
	mnMaxStayTime=MAX_STAY_TIME;
	mnOpenGameUserCount=0;

	memset(m_szRoomId,0,sizeof(m_szRoomId));
	memset(m_szGameType,0,sizeof(m_szGameType));
	memset(m_szGameId,0,sizeof(m_szGameId));
	memset(m_szUserId,0,sizeof(m_szUserId));
	memset(m_szLiveId,0,sizeof(m_szLiveId));
	memset(m_szLiveUserId,0,sizeof(m_szLiveUserId));
	memset(m_szChannelName,0,sizeof(m_szChannelName));	
	memset(m_szUserName,0,sizeof(m_szUserName));
	memset(m_szUserPic,0,sizeof(m_szUserPic));
	if(szIP!=NULL)
	{
		memset(m_szIP,0,sizeof(m_szIP));
		strcat(m_szIP,szIP);
	}
}


CCustomerClient::~CCustomerClient(void)
{
}

string CCustomerClient::GetLiveUserId()
{
	return m_szLiveUserId;
}

string CCustomerClient::GetLiveId()
{
	return m_szLiveId;
}
string CCustomerClient::GetUserId()
{	return m_szUserId;}

string CCustomerClient::GetGameId()
{	return m_szGameId;}

int CCustomerClient::GetUserType()
{	return m_nUserType;}

void CCustomerClient::SetUserType( int nType )
{	m_nUserType=nType;}

int CCustomerClient::GetOpenGameUserCount()
{	return mnOpenGameUserCount;}

string CCustomerClient::GetChannelName()
{	return m_szChannelName;}

string CCustomerClient::GetUserPic()
{
	return m_szUserPic;
}

string CCustomerClient::GetUserName()
{
	return m_szUserName;
}

bool CCustomerClient::IsOffLine()
{
	return mbOffLine;
}

bool CCustomerClient::IsOutRoom()
{
	return mbOutRoom;
}

int CCustomerClient::GetMaxRemainTime()
{
	return mnMaxRemainTime;
}

int CCustomerClient::GetMinRemainTime()
{
	return mnMinRemainTime;
}

int CCustomerClient::GetMaxStayTime()
{
	return mnMaxStayTime;
}

string CCustomerClient::GetLastChannelName()
{
	return m_szLastChannelName;
}

void CCustomerClient::SetGameId(int nGameId)
{
	sprintf(m_szGameId,"%d",nGameId);
}

void CCustomerClient::SetChannelName(int nGameId)
{
	sprintf(m_szChannelName,"%s_%d",m_szGameType,nGameId);
}

void CCustomerClient::SetChannelName(string strChannelName)
{
	memset(m_szChannelName,0,sizeof(m_szChannelName));	
	sprintf(m_szChannelName,"%s",strChannelName.c_str());
}

int CCustomerClient::IsAlive()
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

PDATACHANNELPACKET CCustomerClient::Process( const PDATACHANNELPACKET pPacket )
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
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"Process Cmd(%x): %s,Id=%s,m_tSocket=%d",
						     pPacket->byCommandType,strData.c_str(),m_szUserId,m_tSocket);
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

int CCustomerClient::ProcessHeartBeat( string strData )
{
	// 
 	struct timespec ts_now;
        clock_gettime(CLOCK_REALTIME,&ts_now);
	m_nTick = ts_now.tv_sec;
	return SUCCESS;
}

int CCustomerClient::ProcessCreateRoom( string strData )
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
		    && (!jRoot["LiveUserId"].isNull()) && (!jRoot["LiveId"].isNull()) 
		    && (!jRoot["MaxBombTime"].isNull()) && (!jRoot["MinBombTime"].isNull()) 
		    && (!jRoot["OpenGameUserCount"].isNull()) 
		    && (jRoot["GameType"].isInt())   && (jRoot["GameId"].isInt()) 
                    && (jRoot["UserId"].isInt())     && (jRoot["UserType"].isInt())
		    && (jRoot["LiveUserId"].isInt()) && (jRoot["LiveId"].isInt())
		    && (jRoot["MaxBombTime"].isInt()) && (jRoot["MinBombTime"].isInt())
		    && (jRoot["OpenGameUserCount"].isInt()) 
		  )
		{
			memset(m_szRoomId,0,sizeof(m_szRoomId));
			memset(m_szGameType,0,sizeof(m_szGameType));
			memset(m_szGameId,0,sizeof(m_szGameId));
			memset(m_szUserId,0,sizeof(m_szUserId));
			memset(m_szChannelName,0,sizeof(m_szChannelName));
			memset(m_szLastChannelName,0,sizeof(m_szLastChannelName));
			mnLastGameId = 0;

			m_nUserType = jRoot["UserType"].asInt();
			sprintf(m_szGameType,"%d",jRoot["GameType"].asInt());
			sprintf(m_szGameId,"%d",jRoot["GameId"].asInt());
			sprintf(m_szUserId,"%d",jRoot["UserId"].asInt());
			sprintf(m_szChannelName,"%s_%s",m_szGameType,m_szGameId);
			
			sprintf(m_szLiveUserId,"%d",jRoot["LiveUserId"].asInt());
			sprintf(m_szLiveId,"%d",jRoot["LiveId"].asInt());
			mnMaxRemainTime = jRoot["MaxBombTime"].asInt();
			mnMinRemainTime = jRoot["MinBombTime"].asInt();
			mnOpenGameUserCount = jRoot["OpenGameUserCount"].asInt();

			if(   (!jRoot["LastGameId"].isNull()) 
			   && (jRoot["LastGameId"].isInt()) 
			   && (jRoot["LastGameId"].asInt()!=0)
			  )
			{// 再来一局
				mnLastGameId = jRoot["LastGameId"].asInt();
				sprintf(m_szLastChannelName,"%s_%d",m_szGameType,mnLastGameId);
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
				nRet = ((CCustomerService*)m_pServer)->ClientCreateRoom(this);
			}
		}
	}

	return nRet;
}

int CCustomerClient::ProcessStart( string strData )
{
	int nRet = ERROR0NORMAL;

	nRet = ((CCustomerService*)m_pServer)->ClientGameStart(this);
	
	return nRet;
}

int CCustomerClient::ProcessGameCommand( string strData )
{
	int nRet = 0;
	if( (m_nUserType==OWNER)||(m_nUserType==PLAYER) )
	{
		Json::FastWriter jWriter;
		Json::Value jRoot;
		Json::Reader jReader(Json::Features::strictMode());
		if(jReader.parse((const char*)strData.c_str(),jRoot))
		{
			if(!jRoot["DestUserId"].isNull())
			{
				nRet = ((CCustomerService*)m_pServer)->ClientRoleChg(this,jRoot["DestUserId"].asString());
			}
		}
	}

	return nRet;
}

int CCustomerClient::ProcessHandShake( string strData )
{
	int nRet = ERROR0NORMAL;

	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	if(jReader.parse((const char*)(strData.c_str()),jRoot))
	{
		if(    (!jRoot["GameType"].isNull()) && (!jRoot["GameId"].isNull()) 
                    && (!jRoot["UserId"].isNull())   && (!jRoot["UserType"].isNull()) 
		    && (!jRoot["LiveUserId"].isNull()) && (!jRoot["LiveId"].isNull()) 
		    && (!jRoot["MaxBombTime"].isNull()) && (!jRoot["MinBombTime"].isNull()) 
		    && (!jRoot["OpenGameUserCount"].isNull()) 
		    && (jRoot["GameType"].isInt())   && (jRoot["GameId"].isInt()) 
                    && (jRoot["UserId"].isInt())     && (jRoot["UserType"].isInt())
		    && (jRoot["LiveUserId"].isInt()) && (jRoot["LiveId"].isInt())
		    && (jRoot["MaxBombTime"].isInt()) && (jRoot["MinBombTime"].isInt())
		    && (jRoot["OpenGameUserCount"].isInt()) 
		  )
		{
			memset(m_szRoomId,0,sizeof(m_szRoomId));
			memset(m_szGameType,0,sizeof(m_szGameType));
			memset(m_szGameId,0,sizeof(m_szGameId));
			memset(m_szUserId,0,sizeof(m_szUserId));
			memset(m_szChannelName,0,sizeof(m_szChannelName));

			m_nUserType = jRoot["UserType"].asInt();
			sprintf(m_szGameType,"%d",jRoot["GameType"].asInt());
			sprintf(m_szGameId,"%d",jRoot["GameId"].asInt());
			sprintf(m_szUserId,"%d",jRoot["UserId"].asInt());
			sprintf(m_szChannelName,"%s_%s",m_szGameType,m_szGameId);

			sprintf(m_szLiveUserId,"%d",jRoot["LiveUserId"].asInt());
			sprintf(m_szLiveId,"%d",jRoot["LiveId"].asInt());
			mnMaxRemainTime = jRoot["MaxBombTime"].asInt();
			mnMinRemainTime = jRoot["MinBombTime"].asInt();
			mnOpenGameUserCount = jRoot["OpenGameUserCount"].asInt();

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
			nRet = ((CCustomerService*)m_pServer)->ClientHandShake(this);
		}
	}

	return nRet;
}

int CCustomerClient::ProcessClose( string strData )
{ 
	int nRet = ERROR0UNKNOWN0USER;
	mbOutRoom = true;
	if(m_nUserType<USERTYPECOUNT)
	{
		nRet = ((CCustomerService*)m_pServer)->ClientExitRoom(this);
		if(nRet<=0)
			nRet = 0-nRet;
	}
	return 1;
}

bool CCustomerClient::IsReconn2OtherServer()
{
	return mbReconn2OtherServer;
}

void CCustomerClient::SetReconn2OtherServer(bool bSet)
{
	mbReconn2OtherServer = bSet;
}

void CCustomerClient::OnClose( )
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
			CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"ClientOffLine : Check %s\n",m_szUserId);
			
			sleep(15);
			if( IsReconn2OtherServer())
			{// 有重连到其他服务器
				((CCustomerService*)m_pServer)->ClientOffLine(this,false);
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL," m_szUserId=%s AlreadyReConn other server\n",m_szUserId);
			}
			else if(((CCustomerService*)m_pServer)->IsClientReConn(this) )
			{
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL," m_szUserId=%s AlreadyReConn this server\n",m_szUserId);
			}
			else
			{// 没有重连上来
				CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"ClientOffLine : %s\n",m_szUserId);
				((CCustomerService*)m_pServer)->ClientOffLine(this);
			}
		}		
	}
	else
	{
		return;
	}
}

int CCustomerClient::NotifyClient(enumCustomerClientCommand cmdType,string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(cmdType, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CCustomerClient::NotifyGameOver(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_GAMEOVER, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CCustomerClient::NotifyGameRoleChange(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_GAMECOMMANDRET, (const unsigned char*)strRet.c_str(), strRet.length());
}

int CCustomerClient::NotifyUsers(string strData)
{
	string strRet = CEncryptWrapper::Encrypt(strData);
	return AsynSend(COMMANDTYPE_S2C_USERS, (const unsigned char*)strRet.c_str(), strRet.length());
}

std::string CCustomerClient::BuildStandardResponse( const bool bResult, int nRet, const char* pszResult )
{
	std::string strRet = "";

	Json::FastWriter jsonWriter;
	Json::Value jsonRoot;
	jsonRoot["bResult"] = bResult;
	jsonRoot["code"] = nRet;
	jsonRoot["szResult"] = pszResult;
	return jsonWriter.write( jsonRoot ).c_str();
}

PDATACHANNELPACKET CCustomerClient::BuildDataChannelPacket( const WORD wPacketNo, const BYTE byCommandType, const BYTE* pbyData, const WORD wDataLen )
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

string CCustomerClient::GetErrorMsg(int nRet)
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
