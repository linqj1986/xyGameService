#ifndef _DATASENDER_H_
#define _DATASENDER_H_
#include "DataNode.h"
#include <vector>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

class CDataSender
{
public:
	CDataSender(void);
	~CDataSender(void);

private:
	int m_bExit;
	std::vector<CDataNode*> m_vecDataNode[EM_DP_COUNT];

	sem_t sem_SendNotify;
	sem_t sem_SendNOdeOp;
	static void* SendThread( void *param );
	void SendProc( );
public:
	void CloseSocket( const CDataSocket* pSocket );
	void PushSendPacket( CDataSocket* pSocket, PASYNDATACHANNELPACKET pSend, emDataPriority em );
	int SetSendPacketNormal( const CDataSocket* pSocket, const PASYNDATACHANNELPACKET pSend );
};
#endif
