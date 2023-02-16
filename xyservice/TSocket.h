#pragma once

typedef unsigned char BYTE;
typedef int BOOL;

namespace TFC{

#ifndef SOCKET
typedef int SOCKET;
#endif

#ifdef _UNICODE
#define ConnectToServer			ConnectToServerW
#define ConnectWithTimeout		ConnectWithTimeoutW
#else
#define ConnectToServer			ConnectToServerA
#define ConnectWithTimeout		ConnectWithTimeoutA
#endif

	
	class CTSocket{
	public:
		CTSocket();
		virtual ~CTSocket();

		static void Startup( );
		static SOCKET ConnectToServerA( const  char* pcsIp, const unsigned short uPort );

		static SOCKET CreateServer( const unsigned short uPort );
		static SOCKET Accept( SOCKET sock, BYTE* pbyIp = 0, const unsigned int uSize = 0 );
		static int Send( SOCKET sock, const BYTE* pbyBuf, const int nSize );
		static int Recv( SOCKET sock, BYTE* pbyRecv, const int nSize, const unsigned int uMilliTimeOut );
        static int Recv( SOCKET sock, BYTE* pbyRecv, const int nSize );
		static void Close( SOCKET sock );

		static void Cleanup( );
	};

}
