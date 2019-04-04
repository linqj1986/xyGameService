#pragma once
#include "DataService.h"
#include <pthread.h>

class CListenService :
		public CDataService
{
public:
	CListenService(void);
	~CListenService(void);

	virtual int OnNewSocket( int sock, const char* pszIp );
	virtual unsigned int Run( unsigned int uPort, int bAutoAddPort = 0 );
	virtual void Stop( );
};

