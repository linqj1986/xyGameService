#include "DataNode.h"


CDataNode::CDataNode(void)
{
	m_pDataSocket = NULL;
	m_pAsynDataChannelPacket = NULL;
}

CDataNode::CDataNode( CDataSocket* pSocket, PASYNDATACHANNELPACKET pPackaet )
{
	m_pDataSocket = pSocket;
	m_pAsynDataChannelPacket = pPackaet;
}

CDataNode::~CDataNode(void)
{
	m_pDataSocket = NULL;
	m_pAsynDataChannelPacket = NULL;
}
