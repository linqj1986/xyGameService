#include "ListenService.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include "PrintLog.h"

CListenService::CListenService(void)
{
}

CListenService::~CListenService(void)
{
	Stop();
}

int CListenService::OnNewSocket( int sock, const char* pszIp )
{
	return 0;
}

unsigned int CListenService::Run( unsigned int uPort, int bAutoAddPort )
{
	return CDataService::Run(uPort, bAutoAddPort);
}

void CListenService::Stop()
{
	CDataService::Stop();
}
