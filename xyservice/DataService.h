// 服务端通讯处理的封装类 
#ifndef _DATASERVICE_H_
#define _DATASERVICE_H_
#include<sys/types.h>  
#include<sys/socket.h>
#include <map>

class CDataService
{
public:
    	CDataService();
    	~CDataService(void);

protected:
    	unsigned int m_uPort;
    	unsigned int m_tSocket;
    	pthread_t m_hThreadListen;

protected:
    	static void* ListenThread( void *param );
    	void ListenProc( );
	void CloseSocket( );
	virtual int OnNewSocket( int sock, const char* pszIp ) { return 0; };
public:
    	int IsBusy( );
	virtual unsigned int Run( unsigned int uPort, int bAutoAddPort = 0 );
	virtual void Stop( );
};

#endif
