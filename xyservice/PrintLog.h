#ifndef PRINTLOG_H
#define PRINTLOG_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
using namespace std;
#include <string>
using namespace std;
#include <string.h>
#include "SingletonObj.h"

enum ENUMLOGTYPE{
	LOG_NORMAL=0,
	LOG_WARNING=1,
	LOG_ERROR=2
};

#define LENLIMIT 65534

class CPrintLog
{
	DECLARE_SINGLETON(CPrintLog);
public:    
	CPrintLog();
	~CPrintLog();
	
	void SetServerPort(int nPort);
	void LogFormat(ENUMLOGTYPE enType,const char *fmt, ...);
	void SetDebugLevel(int nLevel);

private:
	int mnLevel;
	queue<string> m_queNormalLog;
	queue<string> m_queWarningLog;
	queue<string> m_queErrorLog;
	sem_t sem_lock;
	int m_nServerPort;
	string m_strExePath;
	void Log(string strData,ENUMLOGTYPE enType = LOG_NORMAL);
	static void* WriteLogThread( void *param );
	void WriteLog();
};

#endif
