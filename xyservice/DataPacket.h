// ͨѶ���ݰ�����ķ�װ�� ��Ҫ���ڷ�������
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
    //ע�⣺��ָ�� BYTE* ���ص�����������������ֵ��Ҫͨ�� free �ͷ�
    PDATACHANNELPACKET FilterPacket( const unsigned char* pbyRecv, const unsigned int dwRecvLen );
    unsigned char* BuildPacket( const PDATACHANNELPACKET pPacket, unsigned int& dwRetLen );

    void Clear( );
};


#endif
