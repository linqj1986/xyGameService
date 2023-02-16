#include "EncryptWrapper.h"
#include "des.h"
#include "../include/MD5.h"

#ifndef BYTE 
#define BYTE unsigned char
#endif 

#ifndef UCHAR 
#define UCHAR unsigned char 
#endif 

#ifndef LPCSTR 
#define LPCSTR const char*
#endif 

#ifndef ULONG 
#define ULONG unsigned int
#endif 

const BYTE DES_KEY[] = { 65, 27, 12, 80, 111, 36, 77, 91 };
const BYTE DES_VI[] = { 88, 42, 76, 101, 28, 82, 65, 19 };

CEncryptWrapper::CEncryptWrapper()
{
}
CEncryptWrapper::~CEncryptWrapper()
{
}

string CEncryptWrapper::Encrypt(string strSrc)
{
	string strGet="";
	if(strSrc.length()>0)
	{
		//printf("Encrypt %s\n",strSrc.c_str());
		unsigned char* pbyEncrypt = (BYTE*)calloc(strSrc.length() + 9, 1);
		des_context ctx;
		des_set_key(&ctx, ( unsigned char*)DES_KEY);
		int nEncryptLen = des_cbc_encrypt(&ctx, ( unsigned char*)strSrc.c_str(), strSrc.length(), pbyEncrypt, ( unsigned char*)DES_VI);

		int nBase64Len = Base64Encode(pbyEncrypt, nEncryptLen, NULL, 0);
		char* pcsBase64 = (char*)calloc(nBase64Len + 1, sizeof(char));
		Base64Encode(pbyEncrypt, nEncryptLen, (unsigned char*)pcsBase64, nBase64Len);

		strGet = pcsBase64;
		free(pbyEncrypt);
		free(pcsBase64);
	}
	//printf("Encrypt strGet=%s\n",strGet.c_str());
	return strGet;
}
string CEncryptWrapper::Decrypt(string strSrc)
{
	string strGet="";
	if(strSrc.length()>0)
	{
		//printf("Decrypt %s\n",strSrc.c_str());
		int nBase64Len = Base64Decode((const unsigned char*)strSrc.c_str(), strSrc.length(), NULL, 0);
		char* pcsBase64 = (char*)calloc(nBase64Len + 1, sizeof(char));
		Base64Decode((const unsigned char*)strSrc.c_str(), strSrc.length(), (unsigned char*)pcsBase64, nBase64Len);

		unsigned char* pbyDecrypt = (unsigned char*)calloc(nBase64Len + 9, 1);
		des_context ctx;
		des_set_key(&ctx, ( unsigned char*)DES_KEY);
		int nRet = des_cbc_decrypt(&ctx, ( unsigned char*)pcsBase64, nBase64Len, pbyDecrypt, ( unsigned char*)DES_VI);
		if(nRet>=0)
		{
			string strDecrypt = (char*)pbyDecrypt;
			strGet = strDecrypt;//TFC::CTCodeConversion::UTF8ToAnsi(strDecrypt.c_str());
		}
		
		free(pbyDecrypt);
		free(pcsBase64);
	}
	//printf("Decrypt strGet=%s\n",strGet.c_str());
	return strGet;
}

std::string CEncryptWrapper::Base64Encode( LPCSTR pszSrc )
{
	std::string strRet = "";
	int nLen = Base64Encode( pszSrc, strlen(pszSrc), NULL, 0 );
	char* pszTmp = (char*)calloc( nLen+1, sizeof(char) );
	Base64Encode( pszSrc, strlen(pszSrc), (BYTE*)pszTmp, nLen );
	strRet = pszTmp;
	free( pszTmp );
	return strRet;
}

std::string CEncryptWrapper::Base64Decode( LPCSTR pszSrc )
{
	std::string strRet = "";
	int nLen = Base64Decode( (BYTE*)pszSrc, strlen(pszSrc), NULL, 0 );
	char* pszTmp = (char*)calloc( nLen+1, sizeof(char) );
	Base64Decode( (BYTE*)pszSrc, strlen(pszSrc), pszTmp, nLen );
	strRet = pszTmp;
	free( pszTmp );
	return strRet;
}

int CEncryptWrapper::Base64Encode( const void* pSrc, const int nSrcLen, BYTE* pbyDest, const int nMaxLen )
{
	const char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
	BYTE* p1 = (BYTE*)pSrc;
	ULONG i, v;
	int nIndex = 0;
	for (i = 0; i < nSrcLen; i += 3)
	{
		if ( nIndex + 3 >= nMaxLen )
		{
			break;
		}
		switch ( nSrcLen - i )
		{
		case 1:
			v = (p1[i] << 16);
			pbyDest[nIndex++] = base64_alphabet[v >> 18];
			pbyDest[nIndex++] = base64_alphabet[(v >> 12) & 63];
			pbyDest[nIndex++] = base64_alphabet[64];
			pbyDest[nIndex++] = base64_alphabet[64];
			break;

		case 2:
			v = (p1[i] << 16) | (p1[i + 1] << 8);
			pbyDest[nIndex++] = base64_alphabet[v >> 18];
			pbyDest[nIndex++] = base64_alphabet[(v >> 12) & 63];
			pbyDest[nIndex++] = base64_alphabet[(v >> 6) & 63];
			pbyDest[nIndex++] = base64_alphabet[64];
			break;

		default:
			v = (p1[i] << 16) | (p1[i + 1] << 8) | p1[i + 2];
			pbyDest[nIndex++] = base64_alphabet[v >> 18];
			pbyDest[nIndex++] = base64_alphabet[(v >> 12) & 63];
			pbyDest[nIndex++] = base64_alphabet[(v >> 6) & 63];
			pbyDest[nIndex++] = base64_alphabet[v & 63];
			break;
		}
	}
	if ( pbyDest && nIndex < nMaxLen )
	{
		pbyDest[nIndex] = '\0';
	}
	return (nSrcLen+2)/3*4;
}

int CEncryptWrapper::Base64Decode( const BYTE* pbySrc, const int nSrcLen, void* pDest, const int nMaxLen )
{
	const char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
	UCHAR *p2 = (UCHAR *)pDest;
	ULONG i, v, n;
	UCHAR base64_table[256];

	for (i = 0; i < sizeof(base64_table); i++)
		base64_table[i] = 255;

	for (i = 0; i < 64; i++)
		base64_table[base64_alphabet[i]] = (char)i;

	for (i = 0, n = 0; i < nSrcLen; i++)
	{
		if (base64_table[pbySrc[i]] == 255)
			break;

		if ((ULONG)(p2 - (UCHAR *)pDest) >= nMaxLen)
			break;

		v = base64_table[pbySrc[i]] | (v << 6);
		n += 6;

		if (n >= 8)
		{
			n -= 8;
			*p2++ = (UCHAR)(v >> n);
		}
	}
	//计算末尾有几个 = 号( Base64转码中 ， =号是Encode时最末不足时候追加的字符) 
	int nEndCount = 0;
	for ( int j = nSrcLen - 1; j >= 0; j-- )
	{
		if ( pbySrc[j] == base64_alphabet[64] )
		{
			nEndCount++;
		}
		else
		{
			break;
		}
	}

	return nSrcLen/4 * 3 - nEndCount;
}

string CEncryptWrapper::MD5( const unsigned char* pbyData, const int nLen )
{
	CMD5 oMd5;
	unsigned char byHash[16];
	int nHashLen = oMd5.MD5_Hash( pbyData, nLen, byHash );
	std::string strRet = "";
	for ( int i = 0; i < nHashLen; i++ )
	{
		char szBuf[3]={0};
		sprintf( szBuf, "%02x", byHash[i] );
		strRet+=szBuf;
	}
	return strRet;	
}

int CEncryptWrapper::UrlEncode( const char* pcsSrc, char* pcsDest, const int nMaxLen )
{
	int nRet = 0;
	char* src = (char*)pcsSrc;
	char p[3];
	if ( nMaxLen > 0 )
	{
		memset( pcsDest, 0, nMaxLen );
	}
	while( *src != 0 )
	{
		char ch = *src;
		if ( ch == ' ' )
		{
			nRet < nMaxLen?pcsDest[nRet++] = '+':nRet++;
		}
		else if ( (ch >= 'a' && ch <= 'z')
			|| (ch >= 'A' && ch <= 'Z') 
			|| (ch >= '0' && ch <= '9')
			|| strchr("=!~*'()", ch)
			)
		{
			nRet < nMaxLen - 1?pcsDest[nRet++] = ch:nRet++;
		}
		else
		{
			nRet < nMaxLen?pcsDest[nRet++] = '%':nRet++;
			memset( p, 0, sizeof(p) );
			sprintf( p, "%x", (BYTE)ch );
			nRet < nMaxLen?pcsDest[nRet++] = p[0]:nRet++;
			nRet < nMaxLen?pcsDest[nRet++] = p[1]:nRet++;
		}
		src++;
	}
	return nRet+1;
}

int CEncryptWrapper::UrlDecode( const char* pcsSrc, char* pcsDest, const int nMaxLen )
{
	int nRet = 0;
	char p[3];
	char szTmp[10];
	p[2] = 0;
	int nSrcLen = strlen(pcsSrc);
	for ( int i = 0; i < nSrcLen; i++ )
	{
		if ( pcsSrc[i] != '%' )
		{
			if ( pcsSrc[i] == '+' )
			{
				nRet < nMaxLen?pcsDest[nRet++] = ' ':nRet++;	
			}
			else
			{
				nRet < nMaxLen?pcsDest[nRet++] = pcsSrc[i]:nRet++;
			}
		}
		else
		{
			++i < nSrcLen?p[0] = pcsSrc[i]:p[0] = 0;
			++i < nSrcLen?p[1] = pcsSrc[i]:p[1] = 0;
			memset( szTmp, 0, sizeof(szTmp) );
			sscanf( p, "%x", szTmp );
			nRet < nMaxLen?pcsDest[nRet++] = szTmp[0]:nRet++;
		}
	}
	return nRet+3;
}

std::string CEncryptWrapper::UrlEncode( LPCSTR pszSrc )
{
	std::string strRet = "";
	int nLen = UrlEncode( pszSrc, NULL, 0 );
	char* pszTmp = (char*)calloc( nLen+1, sizeof(char) );
	UrlEncode( pszSrc, pszTmp, nLen+1 );
	strRet = pszTmp;
	free( pszTmp );
	return strRet;

}

std::string CEncryptWrapper::UrlDecode( LPCSTR pszSrc )
{
	std::string strRet = "";
	int nLen = UrlDecode( pszSrc, NULL, 0 );
	char* pszTmp = (char*)calloc( nLen, sizeof(char) );
	UrlDecode( pszSrc, pszTmp, nLen );
	strRet = pszTmp;
	free( pszTmp );
	return strRet;

}
