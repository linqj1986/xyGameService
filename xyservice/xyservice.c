#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include "DataSender.h"
#include "ParseIniFile.h"
#include "PrintLog.h"
#include "CustomerService.h"
#include "ListenService.h"

CDataSender g_oDataSender;
CCustomerService g_oDataService;

#define SIZE 5000
void *buffer[SIZE];
void fault_trap(int n, siginfo_t *siginfo,void *myact)
{
        int i, num;
        char **calls;
        printf("Fault address:%p\n",siginfo->si_addr);   
        num = backtrace(buffer, SIZE);
        calls = backtrace_symbols(buffer, num);
        for (i = 0; i < num; i++)
                printf("%s\n", calls[i]);
        exit(1);
}
void setuptrap()
{
    	struct sigaction act;
        sigemptyset(&act.sa_mask);   
        act.sa_flags=SA_SIGINFO;    
        act.sa_sigaction=fault_trap;
        sigaction(SIGSEGV,&act,NULL);
}

int main(int argc, char *argv[])
{
	// curl���ʼ��
	curl_global_init(CURL_GLOBAL_ALL);

	CParseIniFile ini;

	//��ʼ��ΪDaemon
	string strDaemon = ini.GetValue("cfg","config","daemon");
	int nDaemon=atoi(strDaemon.c_str());
	if(nDaemon==1)
	{
		//daemon(1,1);
	}
	//��־�ȼ�
	string strLevel = ini.GetValue("cfg","config","debuglevel");
	int nLevel=atoi(strLevel.c_str());
	CPrintLog::GetInstance()->SetDebugLevel(nLevel);

	signal(SIGPIPE, SIG_IGN);
	//setuptrap();

	// ����
	string strPort = ini.GetValue("cfg","config","port");
	int nPort=atoi(strPort.c_str());
	
	// ���з���
	unsigned int uPort = g_oDataService.Run(nPort,0);
	CPrintLog::GetInstance()->SetServerPort(uPort);
	if(uPort==0)
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"---------------Server Run Failed,port=%d---------------\n",uPort);
		exit(1);
	}
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"---------------Server Run Success,port=%d---------------\n",uPort);
	
	// ���ؾ����������
	string strListenPort = ini.GetValue("cfg","config","lport");
	int nListenPort=atoi(strListenPort.c_str());
	CListenService listenService;
	uPort = listenService.Run(nListenPort,0);
	CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"---------------ListenServer Run Success,port=%d---------------\n",uPort);

	// �ȴ�����
	while(1)
	{
		sleep(60);//˯��һ����
	}
	
	//curl���ͷ�
	curl_global_cleanup();
}
