#ifndef _DATACHANNELDEFINE_H_
#define _DATACHANNELDEFINE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>

typedef struct _tagDataChannelPacket{
	unsigned short wPacketNo;
	unsigned char byCommandType;
	unsigned char* pbyData;
	unsigned short wDataLen;
	void Init( ){ memset(&wPacketNo, 0, sizeof(struct _tagDataChannelPacket)); }
	void Release( ) { if ( pbyData ){ free(pbyData);  pbyData = NULL; } }
}DATACHANNELPACKET,*PDATACHANNELPACKET;


enum emPacketType{
	EM_PT_NORMAL = 0,		//正常指令，不需等待
	EM_PT_WAITSEND,			//需要等待发送出去
	EM_PT_WAITSENDANDRECV	//需要等待发送且需要等待接收
};

enum emDataPriority
{
	EM_DP_HIGH = 0,
	EM_DP_NORMAL,
	EM_DP_LOW,
	EM_DP_COUNT
};

typedef struct _tagAsynDataChannelPacket{
	DATACHANNELPACKET stSend;
	emPacketType emPt;
        int nEventWait;
	sem_t sem_PWait;
	DATACHANNELPACKET stRecv;
	void Init(){ stSend.Init(); stRecv.Init();  nEventWait=0;}
	void Release(){ stSend.Release(); stRecv.Release(); sem_post(&sem_PWait);sem_destroy(&sem_PWait);}
}ASYNDATACHANNELPACKET,*PASYNDATACHANNELPACKET;

#endif
