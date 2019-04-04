#include "DataSocket.h"
#include "DataSender.h"
#include "TSocket.h"
#include <time.h>
#include <unistd.h>
extern CDataSender g_oDataSender;

CDataSocket::CDataSocket(void)
{
	m_bExit = 0;
	m_tSocket = 0;
	//m_hRecvThread = CreateThread(NULL, 0, RecvThread, this, 0, NULL);

	
	pthread_create(&mhRecvThread,NULL,RecvThread, (void*)this);
	

	m_wPacketNo = 1;
	//m_hMutexRecvWait = CreateMutex( NULL, FALSE, NULL );
	//m_hMutexPacketNo = CreateMutex( NULL, FALSE, NULL );
	//m_hMutexSocket = CreateMutex( NULL, FALSE, NULL );
        sem_init(&sem_RecvWait, 0, 1); // 空闲的
	sem_init(&sem_PacketNo, 0, 1); // 空闲的
	sem_init(&sem_Socket, 0, 1); // 空闲的
}

CDataSocket::~CDataSocket(void)
{
	CloseSocket();
	Close();
	sem_destroy(&sem_RecvWait);
	sem_destroy(&sem_PacketNo);
	sem_destroy(&sem_Socket);
}

BOOL CDataSocket::Init( SOCKET sock )
{
	BOOL bRet = 0;
	sem_wait(&sem_Socket);
	if ( m_tSocket == 0 )
	{
		bRet = 1;
		m_tSocket = sock;
	}
	sem_post(&sem_Socket);
	return bRet;
}

BOOL CDataSocket::IsValid( )
{
	return m_tSocket != 0;
}

void CDataSocket::CloseSocket( bool bNotify )
{
	m_bExit = 1;
	BOOL bNeedClose = 0;

	sem_wait(&sem_Socket);
	if ( m_tSocket )
	{
		TFC::CTSocket::Close( m_tSocket );
		m_tSocket = 0;
		bNeedClose = 1;
	}
	sem_post(&sem_Socket);
	if ( bNeedClose )
	{
		if(bNotify)
			OnClose( );
	}
}

void CDataSocket::Close( bool bNotify )
{
	InerClose();

	//pthread_cancel(mhRecvThread);

	g_oDataSender.CloseSocket( this );

	sem_wait(&sem_RecvWait);
	std::vector<PASYNDATACHANNELPACKET>::iterator itr = m_vecRecvWait.begin();
	while( itr != m_vecRecvWait.end() )
	{
		PASYNDATACHANNELPACKET pNode = (PASYNDATACHANNELPACKET)*itr;
		itr = m_vecRecvWait.erase(itr);
		if ( pNode->nEventWait )
		{
			sem_post(&(pNode->sem_PWait));
		}
	}
	std::vector<PASYNDATACHANNELPACKET>().swap( m_vecRecvWait );
	sem_post(&sem_RecvWait);
}

void CDataSocket::InerClose(bool bNotify)
{
	if(m_tSocket)
	{
		CloseSocket(bNotify);	
	}
}

PDATACHANNELPACKET CDataSocket::Recv( const unsigned int uMilliTimeOut )
{
	PDATACHANNELPACKET pRet = NULL;
	while( m_tSocket )
	{
		pRet = m_oPacket.FilterPacket( NULL, 0 );
		if ( pRet )
		{
			break;
		}

		unsigned char byBuf[PACKET_SIZE];
		memset( byBuf, 0, sizeof(byBuf) );
		int nRecv = 0;
 		if ( uMilliTimeOut == 0xfffffff0 )
 		{
 			nRecv = TFC::CTSocket::Recv( m_tSocket, byBuf, sizeof(byBuf) );
 		}
 		else
		{
			//这里的超时最低只能精确到1秒，若低于1000，它无法判断超时。
			nRecv = TFC::CTSocket::Recv( m_tSocket, byBuf, sizeof(byBuf), uMilliTimeOut>1000?uMilliTimeOut:1000 );
		}
		if ( nRecv < 0 )
		{
			//超时或套接字异常
			InerClose(true);
			break;
		}
		else if ( nRecv > 0 )
		{
			pRet = m_oPacket.FilterPacket( byBuf, nRecv );
			if ( pRet )
			{
				break;
			}
		}
	}
	return pRet;
}

void CDataSocket::PushRecvWait( const PASYNDATACHANNELPACKET pNode )
{
	sem_wait(&sem_RecvWait);
	if ( pNode->nEventWait )
	{
		m_vecRecvWait.push_back( pNode );
	}
	sem_post(&sem_RecvWait);
}

BOOL CDataSocket::Send( const PDATACHANNELPACKET pPacket )
{
	BOOL bRet = 0;
	if ( m_tSocket )
	{
		unsigned int dwRetLen = 0;
		unsigned char* pbyPacket = m_oPacket.BuildPacket( pPacket, dwRetLen );
		if ( dwRetLen > 0 && pbyPacket )
		{
			if ( dwRetLen == TFC::CTSocket::Send( m_tSocket, pbyPacket, dwRetLen ) )
			{
				bRet = 1;
			}
			free( pbyPacket );
			pbyPacket = NULL;
		}
	}
	return bRet;
}

BOOL CDataSocket::AsynSend(  const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, const emDataPriority em, const unsigned short wPacketNo )
{
	BOOL bRet = 0;
	if ( m_tSocket )
	{
		PASYNDATACHANNELPACKET pAsynPacket = (PASYNDATACHANNELPACKET)calloc( 1, sizeof(ASYNDATACHANNELPACKET) );
		pAsynPacket->Init();
		pAsynPacket->stSend.byCommandType = byCommandType;
		pAsynPacket->stSend.wPacketNo = wPacketNo;
		if ( pbyData && wDataLen > 0 )
		{
			pAsynPacket->stSend.wDataLen = wDataLen;
			pAsynPacket->stSend.pbyData = (unsigned char*)calloc( pAsynPacket->stSend.wDataLen+2, 1 );
			memcpy( pAsynPacket->stSend.pbyData, pbyData, pAsynPacket->stSend.wDataLen );
		}
		pAsynPacket->emPt = EM_PT_NORMAL;
		g_oDataSender.PushSendPacket( this, pAsynPacket, em );
		bRet = 1;
	}
	return bRet;
}

BOOL CDataSocket::AsynSendAndWait(  const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, std::string& strRet, const unsigned int dwMSTime, const emDataPriority em )
{
	PDATACHANNELPACKET pstRet = (PDATACHANNELPACKET)calloc( 1, sizeof(DATACHANNELPACKET) );
	pstRet->Init();
	BOOL bRet = AsynSendAndWait( byCommandType, pbyData, wDataLen, pstRet, dwMSTime, em );
	if ( pstRet->wDataLen > 0 && pstRet->pbyData )
	{
		strRet = (char*)pstRet->pbyData;
	}
	else
	{
		strRet = "";
	}
	pstRet->Release();
	free( pstRet );
	return bRet;
}

BOOL CDataSocket::AsynSendAndWait(  const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, PDATACHANNELPACKET pRetPacket, const unsigned int dwMSTime, const emDataPriority em )
{
	BOOL bRet = 0;
	if ( m_tSocket )
	{
		PASYNDATACHANNELPACKET pAsynPacket = (PASYNDATACHANNELPACKET)calloc( 1, sizeof(ASYNDATACHANNELPACKET) );
		pAsynPacket->Init();
		pAsynPacket->stSend.byCommandType = byCommandType;
		if ( pbyData && wDataLen > 0 )
		{
			pAsynPacket->stSend.wDataLen = wDataLen;
			pAsynPacket->stSend.pbyData = (unsigned char*)calloc( pAsynPacket->stSend.wDataLen+2, 1 );
			memcpy( pAsynPacket->stSend.pbyData, pbyData, pAsynPacket->stSend.wDataLen );
		}

		pAsynPacket->nEventWait = 1;
		sem_init(&(pAsynPacket->sem_PWait), 0, 0);

		if ( pRetPacket )
		{
			pAsynPacket->emPt = EM_PT_WAITSENDANDRECV;
		}
		else
		{
			pAsynPacket->emPt = EM_PT_WAITSEND;
		}

		sem_wait(&sem_PacketNo);
		pAsynPacket->stSend.wPacketNo = m_wPacketNo++;
		if ( m_wPacketNo == 0 )
		{
			m_wPacketNo = 1;
		}
		sem_post(&sem_PacketNo);
		BOOL bNeedFree = 1;
		g_oDataSender.PushSendPacket( this, pAsynPacket, em );
		int   nTimeout=-1;
		//unsigned int dwStartTick = GetTickCount();
                struct timespec ts_start;
   		clock_gettime(CLOCK_REALTIME,&ts_start);
		
		while( IsValid() )
		{
                        struct timespec ts_now;
                        clock_gettime(CLOCK_REALTIME,&ts_now);
			unsigned int dwTotalsWaitTicks = (ts_now.tv_sec - ts_start.tv_sec)*1000;
			if ( dwTotalsWaitTicks >= dwMSTime && dwMSTime != 0xfffffff0 )
			{
				break;
			}
			unsigned int dwCurWaitTick = (dwMSTime-dwTotalsWaitTicks)< 5000 ?(dwMSTime-dwTotalsWaitTicks):5000;
			/*dwWaitRet = WaitForSingleObject( pAsynPacket->hEvent, dwCurWaitTick );
			if ( dwWaitRet != WAIT_TIMEOUT )
			{
				break;
			}*/
                        ts_now.tv_sec+=(dwCurWaitTick/1000);
			nTimeout=sem_timedwait(&(pAsynPacket->sem_PWait),&ts_now);
			if(nTimeout!=-1)
			{
				break;
			}
		}

		if ( -1 == nTimeout )
		{
			BOOL bFindSend = 0;

			bFindSend = g_oDataSender.SetSendPacketNormal( this, pAsynPacket );
			if ( bFindSend )
			{
				bNeedFree = 0;
			}

			if ( pAsynPacket->emPt == EM_PT_WAITSENDANDRECV
				&& !bFindSend )//未在待发送队列中找到，那应该已经进入到了接收等待数据中。检索接收等待数据中是否存在
			{
				sem_wait(&sem_RecvWait);
				EraseRecvWait( pAsynPacket );
				//SAFE_CLOSEHANDLE( pAsynPacket->hEvent );
				pAsynPacket->nEventWait=0;
				sem_post(&(pAsynPacket->sem_PWait));
				sem_post(&sem_RecvWait);
			}
		}
		else
		{
			bRet = 1;
			if ( pRetPacket )
			{
				memcpy( pRetPacket, &pAsynPacket->stRecv, sizeof(DATACHANNELPACKET) );
				if ( pRetPacket->wDataLen > 0 )
				{
					pRetPacket->pbyData = (unsigned char*)calloc( pRetPacket->wDataLen+2, 1 );
					memcpy( pRetPacket->pbyData, pAsynPacket->stRecv.pbyData, pRetPacket->wDataLen );
				}
			}
		}
		if ( bNeedFree )
		{
			pAsynPacket->Release();
			free( pAsynPacket );
			pAsynPacket = NULL;
		}
	}
	return bRet;
}


PASYNDATACHANNELPACKET CDataSocket::FindRecvWait( const unsigned short wPacketNo )
{
	for ( int i = 0; i < m_vecRecvWait.size(); i++ )
	{
		if ( m_vecRecvWait[i]->stSend.wPacketNo == wPacketNo )
		{
			return m_vecRecvWait[i];
		}
	}
	return NULL;
}

void CDataSocket::EraseRecvWait( PASYNDATACHANNELPACKET pPacket )
{
	std::vector<PASYNDATACHANNELPACKET>::iterator itr = m_vecRecvWait.begin();
	while( itr != m_vecRecvWait.end() )
	{
		if ( *itr == pPacket )
		{
			m_vecRecvWait.erase( itr );
			break;
		}
		itr++;
	}
}


void* CDataSocket::RecvThread( void *param )
{
	pthread_detach(pthread_self());
	CDataSocket* pParam = (CDataSocket*)param;
	if ( pParam )
	{
		pParam->RecvProc();
	}
}


void CDataSocket::RecvProc( )
{
	while( !m_bExit )
	{
		pthread_testcancel();
		PDATACHANNELPACKET pPacket = Recv( );
		if ( pPacket )
		{
			ProcessSendWait( pPacket );
			PDATACHANNELPACKET pSendPacket = Process( pPacket );
			if ( pSendPacket )
			{
				AsynSend( pSendPacket->byCommandType, pSendPacket->pbyData, pSendPacket->wDataLen, EM_DP_HIGH, pSendPacket->wPacketNo );//应答统一已高优先级回复
				pSendPacket->Release();
				free( pSendPacket );
			}
			pPacket->Release();
			free( pPacket );
		}
		else
		{
			usleep(100*1000);
		}
		pthread_testcancel();
	}
}


BOOL CDataSocket::ProcessSendWait( const PDATACHANNELPACKET pPacket )
{
	BOOL bRet = 0;
	sem_wait(&sem_RecvWait);
	PASYNDATACHANNELPACKET pRecvWait = FindRecvWait( pPacket->wPacketNo );
	if ( pRecvWait )
	{
		//赋值并激活等待的事件
		memcpy( &pRecvWait->stRecv, pPacket, sizeof(DATACHANNELPACKET) );
		if ( pRecvWait->stRecv.wDataLen > 0 )
		{
			pRecvWait->stRecv.pbyData = (unsigned char*)calloc( pRecvWait->stRecv.wDataLen+2, 1 );
			memcpy( pRecvWait->stRecv.pbyData, pPacket->pbyData, pRecvWait->stRecv.wDataLen );
		}
		if ( pRecvWait->nEventWait )
		{
			sem_post(&(pRecvWait->sem_PWait));
		}
		EraseRecvWait( pRecvWait );
		bRet = 1;
	}
	sem_post(&sem_RecvWait);
	return bRet;
}
