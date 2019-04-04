#ifndef _DATASOCKET_H_
#define _DATASOCKET_H_
#include<sys/types.h>  
#include<sys/socket.h>
#include "DataPacket.h"
#include <vector>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;

typedef int SOCKET;

class CDataSocket
{
public:
	CDataSocket(void);
	~CDataSocket(void);

protected:
	sem_t sem_PacketNo;
	sem_t sem_Socket;
	unsigned short m_wPacketNo;
	unsigned int m_tSocket;
	CDataPacket m_oPacket;
	int m_bExit;

	std::vector<PASYNDATACHANNELPACKET> m_vecRecvWait;

	static void* RecvThread( void *param );
	void RecvProc( );
	int ProcessSendWait( const PDATACHANNELPACKET pPacket );//处理发送等待容器中的指令
	PASYNDATACHANNELPACKET FindRecvWait( const unsigned short dwPacketNo );
	void EraseRecvWait( PASYNDATACHANNELPACKET pPacket );
	PDATACHANNELPACKET Recv( const unsigned int uMilliTimeOut = 0xfffffff0 );
	void InerClose(bool bNotify=false);
	void CloseSocket(bool bNotify=false);
public:
	sem_t semRecvStop;
	pthread_t mhRecvThread;
	sem_t sem_RecvWait;
	int IsValid( );
	int Init( SOCKET sock );
	void Close(bool bNotify=false);
	int AsynSend( const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, const emDataPriority em = EM_DP_NORMAL, const unsigned short wPacketNo = 0 );
	int AsynSendAndWait(  const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, PDATACHANNELPACKET pRetPacket = NULL, const unsigned int dwMSTime = 0xfffffff0, const emDataPriority em = EM_DP_NORMAL );
	int AsynSendAndWait(  const unsigned char byCommandType, const unsigned char* pbyData, const unsigned short wDataLen, string& strRet, const unsigned int dwMSTime = 0xfffffff0, const emDataPriority em = EM_DP_NORMAL );
	virtual PDATACHANNELPACKET Process( const PDATACHANNELPACKET pPacket ){return NULL;};
	virtual void OnClose( ){};

	virtual int Send( const PDATACHANNELPACKET pPacket );

	void PushRecvWait( const PASYNDATACHANNELPACKET pNode );
};


#endif
