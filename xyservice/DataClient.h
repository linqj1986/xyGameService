// 客户端通讯处理的封装类 
#ifndef _DATACLIENT_H_
#define _DATACLIENT_H_
#include "DataPacket.h"
#include "DataSocket.h"

class CDataClient:
	public CDataSocket
{
public:
    CDataClient(void);
    ~CDataClient(void);

protected:
    unsigned int m_uPort;

public:
	int Connect( const char* pszIp, unsigned int uPort  );
	virtual void OnClose(){};
};


#endif
