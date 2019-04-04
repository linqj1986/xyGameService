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
    if ( dwRecvLen > 0 )//�н��յ����ݣ���Ҫ�� m_pBuf ׷������
    {
		if ( m_dwSize == 0 )//��ʼ����һ������
		{
			m_dwSize = (PACKET_SIZE*10)>dwRecvLen?(PACKET_SIZE*10):dwRecvLen;
			m_pBuf = (unsigned char*)calloc( m_dwSize, 1 );
		}

        unsigned int dwTmp = m_dwLen + dwRecvLen;
        if ( dwTmp > dwMaxBufSize )
        {
            //����
            m_dwLen = 0;
            dwTmp = dwRecvLen;
            if ( dwTmp > dwMaxBufSize )//��������Գ�����������������NULL
            {
                return NULL;
            }
        }
        if ( dwTmp > m_dwSize && dwTmp < dwMaxBufSize )//�������壬��Ҫ���¿��ٿռ�
        {
            dwTmp = (dwTmp + PACKET_SIZE)>dwMaxBufSize?dwMaxBufSize:(dwTmp + PACKET_SIZE);//min( dwTmp + PACKET_SIZE, dwMaxBufSize );
            unsigned char* pbyTmp = m_pBuf;
            m_pBuf = (unsigned char*)calloc( dwTmp, 1 );
            memcpy( m_pBuf, pbyTmp, m_dwLen );
            m_dwSize = dwTmp;
            free( pbyTmp );
            pbyTmp = NULL;
        }
        
        //׷������
        memcpy( &m_pBuf[m_dwLen], pbyRecv, dwRecvLen );
        m_dwLen += dwRecvLen;
    }
    //�����Ƿ������Ч�����ݰ�
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
				//��ȡ����
				unsigned short wSendLen = 0;
				memcpy( &wSendLen, &m_pBuf[nHead+1+PACKETHEAD-2], 2 );
				//�������λ�ã����Ƿ���0x03
				if ( m_dwLen > nHead + PACKETHEAD + wSendLen + 1 )
				{
					if ( m_pBuf[nHead + PACKETHEAD + wSendLen + 1] == 0x03 )
					{
						nEnd = nHead + PACKETHEAD + wSendLen + 1;
					}
					else
					{
						bError = 1;//�����쳣�İ��ˣ���Ӧλ��û�ҵ���ȷ�Ľ�������˵����0x02�Ǵ���İ�ͷ��Ҫ����
					}
				}
			}
			break;
        }
    }
    if ( nEnd > nHead  )
    {
		pRet = (PDATACHANNELPACKET)calloc( 1, sizeof(DATACHANNELPACKET) );
        //��ȡ��Ч�����д���
		unsigned int dwIndex = nHead+1;
		//��ȡ�����
		memcpy( &pRet->wPacketNo, &m_pBuf[dwIndex], 2 );
		dwIndex+=2;
		//��ȡ�����
		pRet->byCommandType = m_pBuf[dwIndex];
		dwIndex++;
		//��ȡ����
		memcpy( &pRet->wDataLen, &m_pBuf[dwIndex], 2 );
		dwIndex+=2;
		//��ȡ������
		if ( pRet->wDataLen > 0 )
		{
			pRet->pbyData = (unsigned char*)calloc( pRet->wDataLen+2, 1 );//����2���ֽ���Ϊ������'\0'��L'\0'���Է����ⲿ���ô���
			memcpy( pRet->pbyData, &m_pBuf[dwIndex], pRet->wDataLen );
			dwIndex+=pRet->wDataLen;
		}

		//�ӻ��������������ָ��
        m_dwLen -= (nEnd + 1);
        memcpy( m_pBuf, &m_pBuf[nEnd+1], m_dwLen );
    }
    else if ( nHead >= 0 )//�жϲ���������Ĵ�������
    {
		//������Ҫ������������ݳ���
		int nClear = 0;
		if ( bError )//�����⵽��ǰ��ͷ�Ǵ���ģ�����nHead+1
		{
			nClear = nHead + 1;
		}
		else if ( nHead > 0 )//��ͷǰ�Ķ�����������
		{
			nClear = nHead;
		}
        if ( nClear > 0 )//��ͷǰ����Ч����
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
	//��ͷ
    pRet[dwIndex] = 0x02;
	dwIndex++;
	//�����
	memcpy( &pRet[dwIndex], &pPacket->wPacketNo, 2 );
	dwIndex+=2;
	//�������
	pRet[dwIndex] = pPacket->byCommandType;
	dwIndex++;
	//����
	memcpy( &pRet[dwIndex], &pPacket->wDataLen, 2 );
	dwIndex+=2;
	//������
    if ( pPacket->wDataLen > 0 )
    {
        memcpy( &pRet[dwIndex], pPacket->pbyData, pPacket->wDataLen );
		dwIndex+=pPacket->wDataLen;
    }
	//��β
    pRet[dwIndex] = 0x03;
	dwIndex++;
    return pRet;
}

void CDataPacket::Clear( )
{
    m_dwLen = 0;
}
