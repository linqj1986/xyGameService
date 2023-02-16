#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "DataSender.h"
#include "DataClient.h"
CDataSender g_oDataSender;

int main(int argc, char *argv[])
{
        CDataClient oDataClient;
	int nRet = oDataClient.Connect("127.0.0.1",10010);
	printf("connect nRet=%d\n",nRet);
	while(1)
	{
		sleep(60);//ÀØ√ﬂ“ª∑÷÷”
	}
}
