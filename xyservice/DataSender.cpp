#include "DataSender.h"


CDataSender::CDataSender(void)
{
	m_bExit = 0;
	//m_hEventSend = CreateEvent( NULL, FALSE, FALSE, NULL );
	//m_hMutexSend = CreateMutex( NULL, FALSE, NULL );
	//m_hSendThread =  CreateThread(NULL, 0, SendThread, this, 0, NULL);
	sem_init(&sem_SendNotify, 0, 0); // busy
	sem_init(&sem_SendNOdeOp, 0, 1); // 

        pthread_t hSendThread;
	pthread_create(&hSendThread,NULL,SendThread, (void*)this);
	
}


CDataSender::~CDataSender(void)
{
	m_bExit = 1;

	sem_wait(&sem_SendNOdeOp);
	for ( int i = 0; i < EM_DP_COUNT; i++ )
	{
		std::vector<CDataNode*>::iterator itr = m_vecDataNode[i].begin();
		while( itr != m_vecDataNode[i].end() )
		{
			CDataNode* pNode = (CDataNode*)*itr;
			itr = m_vecDataNode[i].erase(itr);
			if ( pNode->m_pAsynDataChannelPacket->emPt == EM_PT_NORMAL )//普通指令，在这里做释放。其它指令，外部调用会自行释放
			{
				pNode->m_pAsynDataChannelPacket->Release();
				free( pNode->m_pAsynDataChannelPacket );
			}
			else if ( pNode->m_pAsynDataChannelPacket->nEventWait != 0 )//需要等待发送完成的
			{
				sem_post(&(pNode->m_pAsynDataChannelPacket->sem_PWait));
			}
			delete pNode;
		}
	}

	sem_post(&sem_SendNOdeOp);
}

void CDataSender::PushSendPacket( CDataSocket* pSocket, PASYNDATACHANNELPACKET pSend, emDataPriority em )
{
	CDataNode* pDataNode = new CDataNode( pSocket, pSend );
	sem_wait(&sem_SendNOdeOp);
	m_vecDataNode[em].push_back( pDataNode );
	sem_post(&sem_SendNotify);
	sem_post(&sem_SendNOdeOp);
}

void* CDataSender::SendThread( void *param )
{
	pthread_detach(pthread_self());
	CDataSender* pParam = (CDataSender*)param;
	if ( pParam )
	{
		pParam->SendProc();
	}
}

void CDataSender::SendProc( )
{
	while( !m_bExit )
	{
		sem_wait(&sem_SendNotify);
		//提取数据
		while( !m_bExit )
		{
			CDataNode* pNode = NULL;
			sem_wait(&sem_SendNOdeOp);
			for ( int i = 0; i < EM_DP_COUNT; i++ )
			{
				std::vector<CDataNode*>::iterator itr = m_vecDataNode[i].begin();
				if ( itr != m_vecDataNode[i].end() )
				{
					pNode = (CDataNode*)*itr;
					m_vecDataNode[i].erase( itr );
				}
				if ( pNode )
				{
					break;
				}
			}
			sem_post(&sem_SendNOdeOp);
			if ( pNode )
			{
				int bNeedWait = 0;
				if ( pNode->m_pAsynDataChannelPacket->emPt != EM_PT_NORMAL )
				{
					bNeedWait = 1;
				}
				if ( pNode->m_pAsynDataChannelPacket->emPt == EM_PT_WAITSENDANDRECV )//需要等待返回的
				{
					pNode->m_pDataSocket->PushRecvWait( pNode->m_pAsynDataChannelPacket );
				}
				if ( bNeedWait )
				{
					//WaitForSingleObject( pNode->m_pDataSocket->m_hMutexRecvWait, INFINITE );
					sem_wait(&(pNode->m_pDataSocket->sem_RecvWait));
				}
				pNode->m_pDataSocket->Send( &pNode->m_pAsynDataChannelPacket->stSend );
				if ( pNode->m_pAsynDataChannelPacket->emPt == EM_PT_NORMAL )//普通指令，在这里做释放。其它指令，外部调用会自行释放
				{
					pNode->m_pAsynDataChannelPacket->Release();
					free( pNode->m_pAsynDataChannelPacket );
				}
				else if ( pNode->m_pAsynDataChannelPacket->emPt == EM_PT_WAITSEND )//需要等待发送完成的
				{
					sem_post(&(pNode->m_pAsynDataChannelPacket->sem_PWait));
				}
				if ( bNeedWait )
				{
					//ReleaseMutex( pNode->m_pDataSocket->m_hMutexRecvWait );
					sem_post(&(pNode->m_pDataSocket->sem_RecvWait));
				}
				delete pNode;
			}
			else
			{
				break;
			}
		}
	}
}

void CDataSender::CloseSocket( const CDataSocket* pSocket )
{
	sem_wait(&sem_SendNOdeOp);
	for ( int i = 0; i < EM_DP_COUNT; i++ )
	{
		std::vector<CDataNode*>::iterator itr = m_vecDataNode[i].begin();
		while( itr != m_vecDataNode[i].end() )
		{
			CDataNode* pNode = (CDataNode*)*itr;
			if ( pNode->m_pDataSocket == pSocket )
			{
				itr = m_vecDataNode[i].erase(itr);
				if ( pNode->m_pAsynDataChannelPacket->emPt == EM_PT_NORMAL )//普通指令，在这里做释放。其它指令，外部调用会自行释放
				{
					pNode->m_pAsynDataChannelPacket->Release();
					free( pNode->m_pAsynDataChannelPacket );
				}
				else if ( pNode->m_pAsynDataChannelPacket->nEventWait != 0 )//需要等待发送完成的
				{
					sem_post(&(pNode->m_pAsynDataChannelPacket->sem_PWait));
				}
				delete pNode;
			}
			else
			{
				itr++;
			}
		}
	}
	sem_post(&sem_SendNOdeOp);
}

int CDataSender::SetSendPacketNormal( const CDataSocket* pSocket, const PASYNDATACHANNELPACKET pSend )
{
	int bRet = 0;

	//判断是否还在Send队列中
	sem_wait(&sem_SendNOdeOp);

	for ( int i = 0; i < EM_DP_COUNT; i++ )
	{
		std::vector<CDataNode*>::iterator itr = m_vecDataNode->begin();
		while( itr != m_vecDataNode->end() )
		{
			CDataNode* pNode = (CDataNode*)*itr;
			if ( pNode->m_pDataSocket == pSocket && pNode->m_pAsynDataChannelPacket == pSend )
			{
				bRet = 1;
				pSend->emPt = EM_PT_NORMAL;
				//SAFE_CLOSEHANDLE( pSend->hEvent );
				pSend->nEventWait=0;
				sem_post(&(pSend->sem_PWait));
				break;
			}
			itr++;
		}
		if ( bRet )
		{
			break;
		}
	}
	sem_post(&sem_SendNOdeOp);
	return bRet;
}
