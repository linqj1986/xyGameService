#ifndef _DATANODE_H_
#define _DATANODE_H_
#include "DataPacket.h"
#include "DataSocket.h"

class CDataNode
{
public:
	CDataNode(void);
	CDataNode( CDataSocket* pSocket, PASYNDATACHANNELPACKET pPackaet );
	~CDataNode(void);

public:
	CDataSocket* m_pDataSocket;
	PASYNDATACHANNELPACKET m_pAsynDataChannelPacket;
};

#endif