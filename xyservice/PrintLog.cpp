#include "PrintLog.h"
#include "../include/fileoperator.h"
#include <sys/types.h> 
#include <sys/stat.h> 

#define PRINTFLOG 1

CPrintLog::CPrintLog()
{
	m_nServerPort = 0;
	mnLevel = LOG_WARNING;
	char szPath[256]={0};
	int cnt=readlink("/proc/self/exe",szPath,256);
	if(cnt>0)
	{
		for(int i=cnt-1;i>=0;i--)
		{
			if(szPath[i]=='/')
			{
				szPath[i+1]=0x00;
				break;
			}
		}
	}
	m_strExePath = szPath;

	sem_init(&sem_lock, 0, 1); // 空闲的
	pthread_t hWriteLogThread;
	pthread_create(&hWriteLogThread,NULL,WriteLogThread, (void*)this);
}

void CPrintLog::SetDebugLevel(int nLevel)
{
	mnLevel = nLevel;
}

void* CPrintLog::WriteLogThread( void *param )
{
	CPrintLog* pParam = (CPrintLog*)param;
	pParam->WriteLog();
}

void CPrintLog::WriteLog()
{
	while(1)
	{
		sem_wait(&sem_lock);
		
		if(!m_queNormalLog.empty())
		{
			string strLog = m_queNormalLog.front();
			Log(strLog,LOG_NORMAL);
			m_queNormalLog.pop();
		}
		if(!m_queWarningLog.empty())
		{
			string strLog = m_queWarningLog.front();
			Log(strLog,LOG_WARNING);
			m_queWarningLog.pop();
		}
		if(!m_queErrorLog.empty())
		{
			string strLog = m_queErrorLog.front();
			Log(strLog,LOG_ERROR);
			m_queErrorLog.pop();
		}

		sem_post(&sem_lock);
		usleep(10*1000);
	}
}

CPrintLog::~CPrintLog()
{

}

void CPrintLog::SetServerPort(int nPort)
{
	m_nServerPort = nPort;
}

void CPrintLog::Log(string strData,ENUMLOGTYPE enType)
{
	char szLogPath[256]={0};
	sprintf(szLogPath,"%slog/",m_strExePath.c_str());
	mkdir(szLogPath,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	char szSubPath[256]={0};
	sprintf(szSubPath,"%sserver_%d/",szLogPath,m_nServerPort);
	mkdir(szSubPath,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	struct tm* pTime;
	time_t t;
	time(&t);
	pTime = localtime(&t);
	char szDate[30]={0};
	sprintf(szDate,"%d-%d-%d",1900+pTime->tm_year,1+pTime->tm_mon,pTime->tm_mday);

	char szDatePath[256]={0};
	sprintf(szDatePath,"%s%s/",szSubPath,szDate);
	mkdir(szDatePath,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	// level
	if( enType < mnLevel )
		return;

	char szFileName[256]={0};
	sprintf(szFileName,"%slog_%d.log",szDatePath,enType);
	
	FILE* fn = MyOpenFile( szFileName, BIN_APPEND );
	if ( fn)
	{
		char szTime[30]={0};
		sprintf(szTime,"%d-%d-%d %d:%d:%d",1900+pTime->tm_year,1+pTime->tm_mon,pTime->tm_mday,pTime->tm_hour,pTime->tm_min,pTime->tm_sec);

		string strLog = szTime;
		strLog += " : ";
		strLog += strData;
		strLog += "\n";

		//if(PRINTFLOG)
			//printf("%s",strLog.c_str());
		MyWriteFile( fn, strLog.c_str(), strLog.length() );
		MyCloseFile( fn );
	}
}

void CPrintLog::LogFormat(ENUMLOGTYPE enType,const char *fmt, ...)
{
	va_list args;
	int iSize=512;
	char* pBuf = (char*)malloc(iSize+1);
	memset(pBuf, 0, (iSize+1)*sizeof(char));
	while(true)
	{
		va_start(args, fmt);
		int iRet = vsnprintf(pBuf, iSize, fmt, args);
		va_end(args);
		if((iRet > -1 && iRet < iSize) || iSize >= LENLIMIT )
		{
			break;
		}
		if(iSize > LENLIMIT/2)
		{		
			iSize = LENLIMIT;
		}
		else
			iSize *= 2;
		pBuf = (char*)realloc(pBuf,iSize+1);
		if(pBuf == NULL)
		{
			return;
		}
		memset(pBuf,0,(iSize+1)*sizeof(char));
	}

	sem_wait(&sem_lock);
	//Log(pBuf,enType);
	
	string strLog=pBuf;
	free(pBuf);
	switch(enType)
	{
		case LOG_NORMAL:
		{
			m_queNormalLog.push(strLog);
		}
		break;
		case LOG_WARNING:
		{
			m_queWarningLog.push(strLog);
		}
		break;
		case LOG_ERROR:
		{
			m_queErrorLog.push(strLog);
		}
		break;
		default:
		break;
	}
	sem_post(&sem_lock);
}
