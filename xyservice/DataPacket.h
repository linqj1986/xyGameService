// 通讯数据包处理的封装类 主要用于封包、解包
#ifndef _DATAPACKET_H_
#define _DATAPACKET_H_

#include "DataChannelDefine.h"

#define PACKET_SIZE			4096

class CDataPacket
{
public:
    CDataPacket(void);
    ~CDataPacket(void);

protected:
    unsigned char* m_pBuf;
    unsigned int m_dwSize;
    unsigned int m_dwLen;

public:
    //注意：以指针 BYTE* 返回的这两个函数，返回值需要通过 free 释放
    PDATACHANNELPACKET FilterPacket( const unsigned char* pbyRecv, const unsigned int dwRecvLen );
    unsigned char* BuildPacket( const PDATACHANNELPACKET pPacket, unsigned int& dwRetLen );

    void Clear( );
};


#endif
