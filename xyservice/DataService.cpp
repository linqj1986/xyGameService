#include "DataService.h"
#include "DataPacket.h"
#include "TSocket.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

typedef struct{
    CDataService* pService;
    int sockClient;
}RECVPARAMS;

CDataService::CDataService(void)
{
    m_uPort = 0;
    m_tSocket = 0;
    m_hThreadListen = 0;
    TFC::CTSocket::Startup();
}


CDataService::~CDataService(void)
{
    Stop();
}

void CDataService::CloseSocket( )
{
    if ( m_tSocket )
    {
        TFC::CTSocket::Close( m_tSocket );
        m_tSocket = 0;
    }
}

unsigned int CDataService::Run( unsigned int uPort, BOOL bAutoAddPort )
{
    if ( uPort != m_uPort )
    {
        CDataService::Stop();
    }

    if ( m_tSocket == 0 )
    {
        m_tSocket = TFC::CTSocket::CreateServer( uPort );
        if ( bAutoAddPort )
        {
            while( m_tSocket == 0 )
            {
                m_tSocket = TFC::CTSocket::CreateServer( ++uPort );
            }
        }
        if ( m_tSocket )
        {
            m_uPort = uPort;
            //m_hThreadListen = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)ListenThread, (LPVOID)(this), 0, NULL );
	    pthread_create(&m_hThreadListen,NULL,ListenThread, (void*)this);
	    
        }
    }
    return m_uPort;
}

void CDataService::Stop()
{
    m_uPort = 0;
    CloseSocket();
    //WAIT_TERMINATE_THREAD( m_hThreadListen, INFINITE );
}

void CDataService::ListenProc( )
{
    while( m_tSocket )
    {
	unsigned char byIp[4];
        int sock = TFC::CTSocket::Accept( m_tSocket, byIp, sizeof(byIp) );
	char szIp[100];
	memset( szIp, 0, sizeof(szIp) );
	sprintf( szIp, "%d.%d.%d.%d", byIp[0],byIp[1],byIp[2],byIp[3] );
        if ( sock )
        {
		if ( !OnNewSocket(sock, szIp) )
		{
                        //printf("new client error\n");
			TFC::CTSocket::Close( sock );
		}
        }
        else // 套接字出现错误，退出
        {
           TFC::CTSocket::Close( m_tSocket );
           m_tSocket = 0;

	   if ( m_uPort == 0 )
	   {
		break;
	   }
	   else
	   {
		usleep(100*1000);
		m_tSocket = TFC::CTSocket::CreateServer( m_uPort );
	   }
        }
    }
}

void* CDataService::ListenThread( void *param )
{
    CDataService* pParam = (CDataService*)param;
    if ( pParam )
    {
        pParam->ListenProc();
    }
}

BOOL CDataService::IsBusy( )
{
    return m_hThreadListen != 0;
}
