#include "DataPacket.h"
#include "malloc.h"
#include "deffunc.h"

#define PACKETHEAD		5

CDataPacket::CDataPacket(void)
{
    m_dwSize = 0;
    m_pBuf = NULL;
    m_dwLen = 0;
}


CDataPacket::~CDataPacket(void)
{
    m_dwSize = 0;
    m_dwLen = 0;
    SAFE_RELEASE( m_pBuf );
}

PDATACHANNELPACKET CDataPacket::FilterPacket( const unsigned char* pbyRecv, const unsigned int dwRecvLen )
{
	PDATACHANNELPACKET pRet = NULL;
	const unsigned int dwMaxBufSize = 0xFFFF;
    if ( dwRecvLen > 0 )//有接收到数据，需要对 m_pBuf 追加数据
    {
		if ( m_dwSize == 0 )//初始化第一次数据
		{
			m_dwSize = (PACKET_SIZE*10)>dwRecvLen?(PACKET_SIZE*10):dwRecvLen;
			m_pBuf = (unsigned char*)calloc( m_dwSize, 1 );
		}

        unsigned int dwTmp = m_dwLen + dwRecvLen;
        if ( dwTmp > dwMaxBufSize )
        {
            //清理
            m_dwLen = 0;
            dwTmp = dwRecvLen;
            if ( dwTmp > dwMaxBufSize )//若清理后仍超出，放弃处理，返回NULL
            {
                return NULL;
            }
        }
        if ( dwTmp > m_dwSize && dwTmp < dwMaxBufSize )//超过缓冲，需要重新开辟空间
        {
            dwTmp = (dwTmp + PACKET_SIZE)>dwMaxBufSize?dwMaxBufSize:(dwTmp + PACKET_SIZE);//min( dwTmp + PACKET_SIZE, dwMaxBufSize );
            unsigned char* pbyTmp = m_pBuf;
            m_pBuf = (unsigned char*)calloc( dwTmp, 1 );
            memcpy( m_pBuf, pbyTmp, m_dwLen );
            m_dwSize = dwTmp;
            free( pbyTmp );
            pbyTmp = NULL;
        }
        
        //追加数据
        memcpy( &m_pBuf[m_dwLen], pbyRecv, dwRecvLen );
        m_dwLen += dwRecvLen;
    }
    //解析是否存在有效的数据包
    int nHead = -1;
    int nEnd = -1;
    int bError = 0;
    for ( int i = 0; i < m_dwLen; i++ )
	{
		nHead = i;
        if ( m_pBuf[i] == 0x02 )
        {
			if ( m_dwLen > nHead + PACKETHEAD + 1 )
			{
				//提取包长
				unsigned short wSendLen = 0;
				memcpy( &wSendLen, &m_pBuf[nHead+1+PACKETHEAD-2], 2 );
				//计算结束位置，看是否是0x03
				if ( m_dwLen > nHead + PACKETHEAD + wSendLen + 1 )
				{
					if ( m_pBuf[nHead + PACKETHEAD + wSendLen + 1] == 0x03 )
					{
						nEnd = nHead + PACKETHEAD + wSendLen + 1;
					}
					else
					{
						bError = 1;//出现异常的包了，对应位置没找到正确的结束符，说明该0x02是错误的包头，要清理
					}
				}
			}
			break;
        }
    }
    if ( nEnd > nHead  )
    {
		pRet = (PDATACHANNELPACKET)calloc( 1, sizeof(DATACHANNELPACKET) );
        //提取有效包进行处理
		unsigned int dwIndex = nHead+1;
		//提取包序号
		memcpy( &pRet->wPacketNo, &m_pBuf[dwIndex], 2 );
		dwIndex+=2;
		//提取命令号
		pRet->byCommandType = m_pBuf[dwIndex];
		dwIndex++;
		//提取包长
		memcpy( &pRet->wDataLen, &m_pBuf[dwIndex], 2 );
		dwIndex+=2;
		//提取包数据
		if ( pRet->wDataLen > 0 )
		{
			pRet->pbyData = (unsigned char*)calloc( pRet->wDataLen+2, 1 );//增加2个字节作为结束符'\0'或L'\0'用以方便外部调用处理
			memcpy( pRet->pbyData, &m_pBuf[dwIndex], pRet->wDataLen );
			dwIndex+=pRet->wDataLen;
		}

		//从缓存中清理处理完的指令
        m_dwLen -= (nEnd + 1);
        memcpy( m_pBuf, &m_pBuf[nEnd+1], m_dwLen );
    }
    else if ( nHead >= 0 )//判断并清理冗余的错误数据
    {
		//计算需要清理的冗余数据长度
		int nClear = 0;
		if ( bError )//如果检测到当前包头是错误的，清理nHead+1
		{
			nClear = nHead + 1;
		}
		else if ( nHead > 0 )//包头前的多余数据清理
		{
			nClear = nHead;
		}
        if ( nClear > 0 )//包头前有无效数据
        {
            m_dwLen -= nClear;
            memcpy( m_pBuf, &m_pBuf[nClear], m_dwLen );
        }
    }
    return pRet;
}


unsigned char* CDataPacket::BuildPacket( const PDATACHANNELPACKET pPacket, unsigned int& dwRetLen )
{
    unsigned char* pRet = NULL;
	dwRetLen = pPacket->wDataLen + 2 + PACKETHEAD;
    pRet = (unsigned char*)calloc( dwRetLen, 1 );
	unsigned int dwIndex = 0;
	//包头
    pRet[dwIndex] = 0x02;
	dwIndex++;
	//包序号
	memcpy( &pRet[dwIndex], &pPacket->wPacketNo, 2 );
	dwIndex+=2;
	//包命令号
	pRet[dwIndex] = pPacket->byCommandType;
	dwIndex++;
	//包长
	memcpy( &pRet[dwIndex], &pPacket->wDataLen, 2 );
	dwIndex+=2;
	//包数据
    if ( pPacket->wDataLen > 0 )
    {
        memcpy( &pRet[dwIndex], pPacket->pbyData, pPacket->wDataLen );
		dwIndex+=pPacket->wDataLen;
    }
	//包尾
    pRet[dwIndex] = 0x03;
	dwIndex++;
    return pRet;
}

void CDataPacket::Clear( )
{
    m_dwLen = 0;
}
