#include "DataClient.h"
#include "malloc.h"
#include "TSocket.h"

CDataClient::CDataClient(void)
{
    m_uPort = 0;
    TFC::CTSocket::Startup();
}

CDataClient::~CDataClient(void)
{
}

BOOL CDataClient::Connect( const char* pszIp, unsigned int uPort )
{
	BOOL bRet = 0;
	if ( !IsValid() )
	{
		SOCKET sock = TFC::CTSocket::ConnectToServer( pszIp, uPort );
		if ( sock )
		{
			m_uPort = uPort;
			bRet = Init( sock );
		}
	}
    return bRet;
}
