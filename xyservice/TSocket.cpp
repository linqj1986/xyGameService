#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include "deffunc.h"
#include "TSocket.h"
#include<unistd.h> 

namespace TFC{
	CTSocket::CTSocket()
	{
	}

	CTSocket::~CTSocket()
	{
	}
	
	void CTSocket::Startup( )
	{
	}

	SOCKET CTSocket::ConnectToServerA( const char* pcsIp, const unsigned short uPort )
	{

		SOCKET sockRet = 0;

		if ( pcsIp != NULL && uPort < 65536 )
		{
			SOCKET sock;
			struct sockaddr_in stSockAddrInfo;
			int nSize = sizeof( struct sockaddr_in );
			memset( &stSockAddrInfo, 0, sizeof( stSockAddrInfo ));
			stSockAddrInfo.sin_family = AF_INET;
			stSockAddrInfo.sin_port = htons( uPort );
			stSockAddrInfo.sin_addr.s_addr = inet_addr( (const char*)pcsIp );
			sock = socket( AF_INET, SOCK_STREAM, 0 );
			if ( sock > 0 )
			{
				// ¿ªÆôKeepAlive
				BOOL bKeepAlive = 1;
				::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(bKeepAlive));


				if( connect( sock, (struct sockaddr *)&stSockAddrInfo, nSize ) < 0 )
				{
					//printf("connect error\n");
					CTSocket::Close( sock );
				}
				else
				{
					sockRet = sock;
				}
			}
		}
		return sockRet;
	}

	SOCKET CTSocket::CreateServer( const unsigned short uPort )
	{

		SOCKET sockRet = 0;
		if ( uPort < 65536 )
		{
			struct sockaddr_in addr;
			BOOL  bDontLinger = 0;
			SOCKET sock = socket( AF_INET,SOCK_STREAM,IPPROTO_IP );
			if ( sock > 0 )
			{
				int err,sock_reuse=1; 
				err=setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&sock_reuse,sizeof(sock_reuse)); 

				addr.sin_family = AF_INET;
				addr.sin_port = htons( (short)uPort );
				addr.sin_addr.s_addr= htonl(INADDR_ANY);
				if ( -1 == bind( sock, (struct sockaddr*)&addr, sizeof(struct sockaddr) ) )
				{
					close( sock );
				}
				else
				{
					if ( -1 == listen(sock, SOMAXCONN) )
					{
						close( sock );
					}
					else
					{
						sockRet = sock;
					}
				}
			}
		}
		return sockRet;
	}

	SOCKET CTSocket::Accept( SOCKET sock, BYTE* pbyIp, const unsigned int uSize )
	{
		SOCKET sockRet = 0;
		if ( sock != 0 )
		{
			struct sockaddr_in addrclient;
			socklen_t sin_size=sizeof(struct sockaddr_in); 
			sockRet = accept( sock, (struct sockaddr*)&(addrclient), &sin_size );

			// ¿ªÆôKeepAlive
			BOOL bKeepAlive = 1;
			::setsockopt(sockRet, SOL_SOCKET, SO_KEEPALIVE, (char*)&bKeepAlive, sizeof(bKeepAlive));

			if ( pbyIp != NULL )
			{
				strcpy((char*)pbyIp,inet_ntoa(addrclient.sin_addr));
			}
		}
		return sockRet;
	}

	int CTSocket::Send( SOCKET sock, const BYTE* pbyBuf, const int nSize )
	{

		int nSendLen = -1;
		if ( sock != 0 )
		{
			nSendLen = 0;
			while ( nSendLen < nSize )
			{
				int nTemp = send( sock, (char*)&pbyBuf[nSendLen], nSize - nSendLen, 0 );
				if ( nTemp <= 0 )
				{
					break;
				}
				nSendLen += nTemp;
			}
		}
		return nSendLen;
	}

    int CTSocket::Recv( SOCKET sock, BYTE* pbyRecv, const int nSize )
    {
        int nReaded = -1;
        if ( sock )
        {
            nReaded = recv( sock, (char*)pbyRecv, nSize, 0 );
            if ( nReaded == 0 )
            {
                nReaded = -1;
            }
        }
        return nReaded;
    }


	int CTSocket::Recv ( SOCKET sock, BYTE* pbyRecv, const int nSize, const unsigned int uMilliTimeOut )
	{
		int nReaded = -1;
		if ( sock )
		{
			nReaded = 0;
			int nSelect = 0;
			fd_set fdRead;
			struct timeval stTimeOut;
			FD_ZERO( &fdRead );
			FD_SET( sock, &fdRead );
			stTimeOut.tv_sec = uMilliTimeOut/1000;
			stTimeOut.tv_usec = uMilliTimeOut%1000;

			nSelect = select( sock+1, &fdRead, 0, 0, &stTimeOut );
			if ( nSelect > 0 && FD_ISSET( sock, &fdRead ) )
			{
				int nTemp = recv( sock, (char*)pbyRecv, nSize, 0 );
				if ( nTemp <= 0 )
				{
					nReaded = -1;
				}
				else
				{
					nReaded += nTemp;
				}
			}
		}
		return nReaded;
	}

	void CTSocket::Close( SOCKET sock )
	{
		if ( sock != 0 )
		{
			//shutdown( sock, 0x02 );
                        //printf("close socket=%d\n",sock);
			close( sock );
		}
	}

	void CTSocket::Cleanup()
	{
		//WSACleanup();
	}
}
