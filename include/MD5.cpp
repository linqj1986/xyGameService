// MD5.cpp
// 
#include "MD5.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


	int CMD5::MD5_Hash(const BYTE* pbInputBuffer,
		UINT uiInputCount, BYTE* pbOutputBuffer )
	{
		st_MD5CTX_t stContext;

		if( pbInputBuffer == NULL )
		{
			uiInputCount = 0;
		}
		// 进行MD5变换  
		_MD5_Init(stContext);
		_MD5_Update(stContext, pbInputBuffer, uiInputCount);
		_MD5_Final(stContext);
		// 获取哈希值  
		if( pbOutputBuffer != NULL )
		{
			memcpy( pbOutputBuffer, stContext.dwState, 16 );
		}   
		return 16;
	}

	int CMD5::HMAC_MD5_Hash(const BYTE* pbInputBuffer,
		UINT uiInputCount, const BYTE* pbUserKey,
		UINT uiUserKeyLen, BYTE* pbOutputBuffer)
	{   
		BYTE baHmacKey[64] = {0};
		BYTE baKipad[64];
		BYTE baKopad[64];
		st_MD5CTX_t stContext;

		if( pbInputBuffer == NULL )
		{
			uiInputCount = 0;
		}

		if( pbUserKey == NULL )
		{
			uiUserKeyLen = 0;
		}
		// 保证密钥长度不超过64字节  
		if( uiUserKeyLen > 64 )
		{
			MD5_Hash(pbUserKey, uiUserKeyLen, baHmacKey);
		}
		else
		{
			memcpy(baHmacKey, pbUserKey, uiUserKeyLen);
		}

		for( UINT i = 0; i < 64; i++ )
		{
			baKipad[i] = baHmacKey[i] ^ 0x36;
			baKopad[i] = baHmacKey[i] ^ 0x5C;
		}
		// 内圈MD5运算  
		_MD5_Init(stContext);
		_MD5_Update(stContext, baKipad, 64);
		_MD5_Update(stContext, pbInputBuffer, uiInputCount);
		_MD5_Final(stContext);
		memcpy(baHmacKey, stContext.dwState, 16);
		// 外圈MD5运算  
		_MD5_Init(stContext);
		_MD5_Update(stContext, baKopad, 64);
		_MD5_Update(stContext, baHmacKey, 16);
		_MD5_Final(stContext);
		// 获取哈希值  
		if( pbOutputBuffer != NULL )
		{
			memcpy(pbOutputBuffer, stContext.dwState, 16);
		}

		return 16;
	}

	CMD5::CMD5()
	{
		st_Md5Data_t stMd5Data = {
			{
				0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476
			},  
			{// 变换操作偏移量表
				0xD76AA478,0xE8C7B756,0x242070DB,0xC1BDCEEE,  
					0xF57C0FAF,0x4787C62A,0xA8304613,0xFD469501,  
					0x698098D8,0x8B44F7AF,0xFFFF5BB1,0x895CD7BE,  
					0x6B901122,0xFD987193,0xA679438E,0x49B40821,  

					0xF61E2562,0xC040B340,0x265E5A51,0xE9B6C7AA,  
					0xD62F105D,0x02441453,0xD8A1E681,0xE7D3FBC8,  
					0x21E1CDE6,0xC33707D6,0xF4D50D87,0x455A14ED,  
					0xA9E3E905,0xFCEFA3F8,0x676F02D9,0x8D2A4C8A,  

					0xFFFA3942,0x8771F681,0x6D9D6122,0xFDE5380C,  
					0xA4BEEA44,0x4BDECFA9,0xF6BB4B60,0xBEBFBC70,  
					0x289B7EC6,0xEAA127FA,0xD4EF3085,0x04881D05,  
					0xD9D4D039,0xE6DB99E5,0x1FA27CF8,0xC4AC5665,  

					0xF4292244,0x432AFF97,0xAB9423A7,0xFC93A039,  
					0x655B59C3,0x8F0CCC92,0xFFEFF47D,0x85845DD1,  
					0x6FA87E4F,0xFE2CE6E0,0xA3014314,0x4E0811A1,  
					0xF7537E82,0xBD3AF235,0x2AD7D2BB,0xEB86D391
				}
		};
		memcpy(&m_stMd5Arguments, &stMd5Data, sizeof(st_Md5Data_t));

		BYTE bMd5ShiftTable[][4] = {
			{7,12,17,22},{5,9,14,20},
			{4,11,16,23},{6,10,15,21}
		};
		memcpy(m_bMd5ShiftTable, bMd5ShiftTable, sizeof(m_bMd5ShiftTable));

		BYTE bMd5Padding[] = {
			0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		memcpy(m_bMd5Padding, bMd5Padding, sizeof(m_bMd5Padding));
	}

	void CMD5::_MD5_Init( st_MD5CTX_t& pstruContext )
	{
		const DWORD *pdwOriginState = m_stMd5Arguments.dwaP;
		pstruContext.dwState[0] = pdwOriginState[0];
		pstruContext.dwState[1] = pdwOriginState[1];
		pstruContext.dwState[2] = pdwOriginState[2];
		pstruContext.dwState[3] = pdwOriginState[3];
		pstruContext.dwCount[0] = pstruContext.dwCount[1] = 0;
	}

	void CMD5::_MD5_Transform(PDWORD pState, PDWORD px)
	{
		const DWORD (*MDOffTable)[16];
		DWORD a,b,c,d;

		MDOffTable = m_stMd5Arguments.dwaQ;
		a = pState[0], b = pState[1], c = pState[2], d = pState[3];

		// 第一轮变换  
		__MD5_FF(a, b, c, d, px[ 0], m_bMd5ShiftTable[0][0], MDOffTable[0][ 0]);
		__MD5_FF(d, a, b, c, px[ 1], m_bMd5ShiftTable[0][1], MDOffTable[0][ 1]);
		__MD5_FF(c, d, a, b, px[ 2], m_bMd5ShiftTable[0][2], MDOffTable[0][ 2]);
		__MD5_FF(b, c, d, a, px[ 3], m_bMd5ShiftTable[0][3], MDOffTable[0][ 3]);
		__MD5_FF(a, b, c, d, px[ 4], m_bMd5ShiftTable[0][0], MDOffTable[0][ 4]);
		__MD5_FF(d, a, b, c, px[ 5], m_bMd5ShiftTable[0][1], MDOffTable[0][ 5]);
		__MD5_FF(c, d, a, b, px[ 6], m_bMd5ShiftTable[0][2], MDOffTable[0][ 6]);
		__MD5_FF(b, c, d, a, px[ 7], m_bMd5ShiftTable[0][3], MDOffTable[0][ 7]);
		__MD5_FF(a, b, c, d, px[ 8], m_bMd5ShiftTable[0][0], MDOffTable[0][ 8]);
		__MD5_FF(d, a, b, c, px[ 9], m_bMd5ShiftTable[0][1], MDOffTable[0][ 9]);
		__MD5_FF(c, d, a, b, px[10], m_bMd5ShiftTable[0][2], MDOffTable[0][10]);
		__MD5_FF(b, c, d, a, px[11], m_bMd5ShiftTable[0][3], MDOffTable[0][11]);
		__MD5_FF(a, b, c, d, px[12], m_bMd5ShiftTable[0][0], MDOffTable[0][12]);
		__MD5_FF(d, a, b, c, px[13], m_bMd5ShiftTable[0][1], MDOffTable[0][13]);
		__MD5_FF(c, d, a, b, px[14], m_bMd5ShiftTable[0][2], MDOffTable[0][14]);
		__MD5_FF(b, c, d, a, px[15], m_bMd5ShiftTable[0][3], MDOffTable[0][15]);

		// 第二轮变换  
		__MD5_GG(a, b, c, d, px[ 1], m_bMd5ShiftTable[1][0], MDOffTable[1][ 0]);
		__MD5_GG(d, a, b, c, px[ 6], m_bMd5ShiftTable[1][1], MDOffTable[1][ 1]);
		__MD5_GG(c, d, a, b, px[11], m_bMd5ShiftTable[1][2], MDOffTable[1][ 2]);
		__MD5_GG(b, c, d, a, px[ 0], m_bMd5ShiftTable[1][3], MDOffTable[1][ 3]);
		__MD5_GG(a, b, c, d, px[ 5], m_bMd5ShiftTable[1][0], MDOffTable[1][ 4]);
		__MD5_GG(d, a, b, c, px[10], m_bMd5ShiftTable[1][1], MDOffTable[1][ 5]);
		__MD5_GG(c, d, a, b, px[15], m_bMd5ShiftTable[1][2], MDOffTable[1][ 6]);
		__MD5_GG(b, c, d, a, px[ 4], m_bMd5ShiftTable[1][3], MDOffTable[1][ 7]);
		__MD5_GG(a, b, c, d, px[ 9], m_bMd5ShiftTable[1][0], MDOffTable[1][ 8]);
		__MD5_GG(d, a, b, c, px[14], m_bMd5ShiftTable[1][1], MDOffTable[1][ 9]);
		__MD5_GG(c, d, a, b, px[ 3], m_bMd5ShiftTable[1][2], MDOffTable[1][10]);
		__MD5_GG(b, c, d, a, px[ 8], m_bMd5ShiftTable[1][3], MDOffTable[1][11]);
		__MD5_GG(a, b, c, d, px[13], m_bMd5ShiftTable[1][0], MDOffTable[1][12]);
		__MD5_GG(d, a, b, c, px[ 2], m_bMd5ShiftTable[1][1], MDOffTable[1][13]);
		__MD5_GG(c, d, a, b, px[ 7], m_bMd5ShiftTable[1][2], MDOffTable[1][14]);
		__MD5_GG(b, c, d, a, px[12], m_bMd5ShiftTable[1][3], MDOffTable[1][15]);

		// 第三轮变换      
		__MD5_HH(a, b, c, d, px[ 5], m_bMd5ShiftTable[2][0], MDOffTable[2][ 0]);
		__MD5_HH(d, a, b, c, px[ 8], m_bMd5ShiftTable[2][1], MDOffTable[2][ 1]);
		__MD5_HH(c, d, a, b, px[11], m_bMd5ShiftTable[2][2], MDOffTable[2][ 2]);
		__MD5_HH(b, c, d, a, px[14], m_bMd5ShiftTable[2][3], MDOffTable[2][ 3]);
		__MD5_HH(a, b, c, d, px[ 1], m_bMd5ShiftTable[2][0], MDOffTable[2][ 4]);
		__MD5_HH(d, a, b, c, px[ 4], m_bMd5ShiftTable[2][1], MDOffTable[2][ 5]);
		__MD5_HH(c, d, a, b, px[ 7], m_bMd5ShiftTable[2][2], MDOffTable[2][ 6]);
		__MD5_HH(b, c, d, a, px[10], m_bMd5ShiftTable[2][3], MDOffTable[2][ 7]);
		__MD5_HH(a, b, c, d, px[13], m_bMd5ShiftTable[2][0], MDOffTable[2][ 8]);
		__MD5_HH(d, a, b, c, px[ 0], m_bMd5ShiftTable[2][1], MDOffTable[2][ 9]);
		__MD5_HH(c, d, a, b, px[ 3], m_bMd5ShiftTable[2][2], MDOffTable[2][10]);
		__MD5_HH(b, c, d, a, px[ 6], m_bMd5ShiftTable[2][3], MDOffTable[2][11]);
		__MD5_HH(a, b, c, d, px[ 9], m_bMd5ShiftTable[2][0], MDOffTable[2][12]);
		__MD5_HH(d, a, b, c, px[12], m_bMd5ShiftTable[2][1], MDOffTable[2][13]);
		__MD5_HH(c, d, a, b, px[15], m_bMd5ShiftTable[2][2], MDOffTable[2][14]);
		__MD5_HH(b, c, d, a, px[ 2], m_bMd5ShiftTable[2][3], MDOffTable[2][15]);

		// 第四轮变换      
		__MD5_II(a, b, c, d, px[ 0], m_bMd5ShiftTable[3][0], MDOffTable[3][ 0]);
		__MD5_II(d, a, b, c, px[ 7], m_bMd5ShiftTable[3][1], MDOffTable[3][ 1]);
		__MD5_II(c, d, a, b, px[14], m_bMd5ShiftTable[3][2], MDOffTable[3][ 2]);
		__MD5_II(b, c, d, a, px[ 5], m_bMd5ShiftTable[3][3], MDOffTable[3][ 3]);
		__MD5_II(a, b, c, d, px[12], m_bMd5ShiftTable[3][0], MDOffTable[3][ 4]);
		__MD5_II(d, a, b, c, px[ 3], m_bMd5ShiftTable[3][1], MDOffTable[3][ 5]);
		__MD5_II(c, d, a, b, px[10], m_bMd5ShiftTable[3][2], MDOffTable[3][ 6]);
		__MD5_II(b, c, d, a, px[ 1], m_bMd5ShiftTable[3][3], MDOffTable[3][ 7]);
		__MD5_II(a, b, c, d, px[ 8], m_bMd5ShiftTable[3][0], MDOffTable[3][ 8]);
		__MD5_II(d, a, b, c, px[15], m_bMd5ShiftTable[3][1], MDOffTable[3][ 9]);
		__MD5_II(c, d, a, b, px[ 6], m_bMd5ShiftTable[3][2], MDOffTable[3][10]);
		__MD5_II(b, c, d, a, px[13], m_bMd5ShiftTable[3][3], MDOffTable[3][11]);
		__MD5_II(a, b, c, d, px[ 4], m_bMd5ShiftTable[3][0], MDOffTable[3][12]);
		__MD5_II(d, a, b, c, px[11], m_bMd5ShiftTable[3][1], MDOffTable[3][13]);
		__MD5_II(c, d, a, b, px[ 2], m_bMd5ShiftTable[3][2], MDOffTable[3][14]);
		__MD5_II(b, c, d, a, px[ 9], m_bMd5ShiftTable[3][3], MDOffTable[3][15]);  

		pState[0] += a;
		pState[1] += b;
		pState[2] += c;
		pState[3] += d;
	}

	void CMD5::_MD5_Update(st_MD5CTX_t& stContext,
		const BYTE* pbInput, DWORD dwInputLen )
	{
		DWORD i, dwIndex, dwPartLen, dwBitsNum;
		// 计算 mod 64 的字节数  
		dwIndex = (stContext.dwCount[0] >> 3) & 0x3F;
		// 更新数据位数  
		dwBitsNum = dwInputLen << 3;
		stContext.dwCount[0] += dwBitsNum;

		if(stContext.dwCount[0] < dwBitsNum)
		{
			stContext.dwCount[1]++;
		}

		stContext.dwCount[1] += dwInputLen >> 29;

		dwPartLen = 64 - dwIndex;
		if( dwInputLen < dwPartLen )
		{
			i = 0;
		}
		else  
		{
			memcpy( stContext.baBuffer+dwIndex, pbInput, dwPartLen );
			_MD5_Transform( stContext.dwState, (DWORD*)stContext.baBuffer );

			for(i = dwPartLen; i + 63 < dwInputLen; i += 64 )
			{
				_MD5_Transform( stContext.dwState, (DWORD*)(pbInput + i) );
			}

			dwIndex = 0;
		}

		memcpy( stContext.baBuffer + dwIndex, pbInput + i, dwInputLen - i );
	}

	void CMD5::_MD5_Final(st_MD5CTX_t& stContext)
	{
		DWORD dwIndex, dwPadLen;
		BYTE pBits[8];

		memcpy( pBits, stContext.dwCount, 8 );
		// 计算 mod 64 的字节数  
		dwIndex = (stContext.dwCount[0] >> 3) & 0x3F;
		// 使长度满足K*64+56个字节  
		dwPadLen = (dwIndex < 56) ? (56-dwIndex) : (120-dwIndex);
		_MD5_Update(stContext, m_bMd5Padding, dwPadLen);
		_MD5_Update(stContext, pBits, 8);
	}

