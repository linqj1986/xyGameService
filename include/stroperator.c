#include "stroperator.h"
#include "des.h"

int StringToHexA( const char* szText, const int nTextLen, BYTE* byHex, const int nMaxHexLen )
{
	IS_SAFE_CALL( szText, FALSE );
	IS_SAFE_CALL( byHex, FALSE );
	int nRet = 0;
	if ( nTextLen <= 0 || nMaxHexLen < (nTextLen+1)/2 )
	{
		return nRet;
	}
	memset( byHex, 0, nMaxHexLen );
	int i = 0;
	for ( i = 0; i < nTextLen; i+=2 )
	{
		char szHex[MAX_PATH];
		char szTmp[3];
		memset( szHex, 0, sizeof(szHex) );
		memset( szTmp, 0, sizeof(szTmp) );
		memcpy( szTmp, &szText[i], 2 );
		sscanf( szTmp, "%x", szHex);
		memcpy( &byHex[i/2], (BYTE*)szHex, 1 );
		nRet++;
	}
	return nRet;
}

int HexToStringA( const BYTE* byHex, const int nHexLen, char* szText, const int nMaxTextLen )
{
	IS_SAFE_CALL( szText, FALSE );
	IS_SAFE_CALL( byHex, FALSE );
	int nRet = 0;
	if ( nHexLen <= 0 || nMaxTextLen < nHexLen*2+1 )
	{
		return 0;
	}

	memset( szText, 0, nMaxTextLen );
	int i = 0;
	for ( i = 0; i < nHexLen; i++ )
	{
		sprintf( &szText[i*2], "%02x", byHex[i] );
		nRet += 2;
	}
	return nRet;
}

BOOL MyEncrypt( BYTE* byKey, BYTE* bySrc, const int nSrcLen, BYTE* byDest, const int nDestLen )
{
	if ( nDestLen < ((nSrcLen-1)/8+8)*2 || byKey == NULL || bySrc == NULL || byDest == NULL )
	{
		return FALSE;
	}
	BYTE byTempSrc[8];
	BYTE byTempDest[8];
	int i = 0;
	for ( i = 0; i < nSrcLen; i+=8 )
	{
		int nLen = min( nSrcLen-i, 8 );
		memset( byTempSrc, 0, sizeof(byTempSrc) );
		memset( byTempDest, 0, sizeof(byTempDest) );
		memcpy( byTempSrc, &bySrc[i], nLen );
		DES( byKey, byTempSrc, byTempDest );
		HexToStringA( byTempDest, 8, (char*)&byDest[i*2], nDestLen-(i*2) );
	}
	return TRUE;
}

BOOL MyUnEncrypt( BYTE* byKey, BYTE* bySrc, const int nSrcLen, BYTE* byDest, const int nDestLen )
{
	if ( nDestLen < nSrcLen/2 || byKey == NULL || bySrc == NULL || byDest == NULL )
	{
		return FALSE;
	}
	BYTE byTempSrc[8];
	BYTE byTempDest[8];
	int i = 0;
	for ( i = 0; i < nSrcLen; i+=16 )
	{
		int nLen = min( nSrcLen-i, 16 );
		memset( byTempSrc, 0, sizeof(byTempSrc) );
		memset( byTempDest, 0, sizeof(byTempDest) );
		StringToHexA( (char*)&bySrc[i], nLen, byTempSrc, _countof(byTempSrc) );
		_DES( byKey, byTempSrc, byTempDest );
		memcpy( &byDest[i/2], byTempDest, 8 );
	}
	return TRUE;
}

/************************************************************************
功能：将输入的字符串中的回车过滤，检测到最末一个字符，若该字符为回车即截断为字符串结束符
入参：
	nMaxLen		待转换字符串缓冲的长度
入/出参：
	szBuffer	待转换字符串缓冲
返回值：
	无
************************************************************************/
void FilterStringEnd( char* szBuffer, const int nMaxLen )
{
	int i = 0;
	for ( i = nMaxLen-1; i >= 0; i-- )
	{
		if ( szBuffer[i] == '\x0D' || szBuffer[i] == '\x0A' )
		{
			szBuffer[i] = 0;
		}
		else
		{
			break;
		}
	}
}

int FindString( const char* pszStr, const char * pszSubStr, const int nStart )
{
	int nIndex = 0;
	int nLen = strlen( pszStr ) - strlen( pszSubStr );
	int nSubLen = strlen(pszSubStr);
	int i = 0;
	int j = 0;

	for ( i = nStart; i <= nLen; i++ )
	{
		for ( j = 0; j < nSubLen; j++ )
		{
			if ( pszStr[i+j] != pszSubStr[j] )
			{
				break;
			}
			if ( j == nSubLen-1 )
			{
				return i;
			}
		}
	}
	return -1;
}

void CharUpper( char* pszStr )
{
	int nLen = 0;
	int i = 0;
	assert( pszStr != NULL );
	nLen = strlen( pszStr );
	for ( i = 0; i < nLen; i++ )
	{
		if ( pszStr[i] >= 'a' && pszStr[i] <= 'z' )
		{
			pszStr[i] += ('A' - 'a');
		}
	}
}

//匹配字符串格式是否一致,针对通配符'?'
BOOL MyCmpString( const char* pszSrc, const char* pszFormat, const int nLen )
{
	int i = 0;
	for ( i = 0; i < nLen; i++ )
	{
		if ( pszSrc[i] != pszFormat[i] 
			&& pszFormat[i] != '?'
			&& pszSrc[i] != '\0' )
		{
			return FALSE;
		}
	}
	return TRUE;
}

BOOL CheckFnByFnFormat( const char* pszFnName, const char* pszFnFormat )//检测指定文件名是否符合通配符
{
	BOOL bRet = TRUE;
	int nCurIndex = 0;
	int nTmp = 0;
	int nFormatIndex = 0;
	int nNextXIndex = FindString( pszFnFormat, "*", nFormatIndex );
	int nLen = strlen(pszFnName) > strlen(pszFnFormat) ? strlen(pszFnName) : strlen(pszFnFormat);
	char *pszTmpFnName = (char *)calloc( strlen(pszFnName)+1, sizeof(char) );
	char *pszTmpFnFormat = (char *)calloc( strlen(pszFnFormat)+1, sizeof(char) );
	char *pszSubStr = (char *)calloc( nLen+1, sizeof(char) );
	
	strcpy( pszTmpFnName, pszFnName );
	strcpy( pszTmpFnFormat, pszFnFormat );
	CharUpper( pszTmpFnName );
	CharUpper( pszTmpFnFormat );

	if ( nNextXIndex > 0 )//开头无通配符
	{
		if ( !MyCmpString( pszTmpFnName, pszTmpFnFormat, nNextXIndex ) )
		{
			bRet = FALSE;
			free( pszSubStr );
			free( pszTmpFnName );
			free( pszTmpFnFormat );
			pszSubStr = NULL;
			pszTmpFnName = NULL;
			pszTmpFnFormat = NULL;
			return bRet;
		}
	}
	
	if( nNextXIndex == -1 ) //无通配符,需完全匹配
	{
		if ( !MyCmpString( pszTmpFnName, pszTmpFnFormat, strlen(pszTmpFnFormat)+1 ) )
		{
			bRet = FALSE;
		}
		free( pszSubStr );
		free( pszTmpFnName );
		free( pszTmpFnFormat );
		pszSubStr = NULL;
		pszTmpFnName = NULL;
		pszTmpFnFormat = NULL;
		return bRet;
	}
	else
	{
		while( nNextXIndex != -1 )//存在通配符
		{
			if( nNextXIndex - nFormatIndex == 0 )
			{
				nFormatIndex = nNextXIndex + 1;
			}
			else
			{
				memset( pszSubStr, 0, sizeof(char)*nLen );
				strncpy( pszSubStr, &pszTmpFnFormat[nFormatIndex], nNextXIndex - nFormatIndex );
				nTmp = FindString( pszTmpFnName , pszSubStr, nCurIndex );
				if ( nTmp == -1 )
				{
					bRet = FALSE;
					free( pszSubStr );
					free( pszTmpFnName );
					free( pszTmpFnFormat );
					pszSubStr = NULL;
					pszTmpFnName = NULL;
					pszTmpFnFormat = NULL;
					return bRet;
				}
				else
				{
					nCurIndex = nTmp + strlen( pszSubStr ) + 1;
				}
			}
			nFormatIndex = nNextXIndex + 1;
			nNextXIndex = FindString( pszTmpFnFormat, "*", nFormatIndex );
		}
	}
	nTmp = strlen(pszTmpFnName)-(strlen(pszTmpFnFormat) - nFormatIndex);
	if ( nTmp < 0 )
	{
		bRet = FALSE;
	}
	else if( !MyCmpString( &pszTmpFnName[nTmp], 
							&pszTmpFnFormat[nFormatIndex],
							strlen(pszTmpFnFormat) - nFormatIndex ) )
	{
		bRet = FALSE;
	}

	free( pszSubStr );
	free( pszTmpFnName );
	free( pszTmpFnFormat );
	pszSubStr = NULL;
	pszTmpFnName = NULL;
	pszTmpFnFormat = NULL;
	return bRet;
}