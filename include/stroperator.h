#ifndef _STROPERATOR_H_
#define _STROPERATOR_H_

#include "globals.h"

int StringToHexA( const char* szText, const int nTextLen, BYTE* byHex, const int nMaxHexLen );
int HexToStringA( const BYTE* byHex, const int nHexLen, char* szText, const int nMaxTextLen );

BOOL MyEncrypt( BYTE* byKey, BYTE* bySrc, const int nSrcLen, BYTE* byDest, const int nDestLen );
BOOL MyUnEncrypt( BYTE* byKey, BYTE* bySrc, const int nSrcLen, BYTE* byDest, const int nDestLen );

void FilterStringEnd( char* szBuffer, const int nMaxLen );

int FindString( const char* pszStr, const char * pszSubStr, const int nStart );
void CharUpper( char* pszStr );
//ƥ���ַ�����ʽ�Ƿ�һ��,���ͨ���'?'
BOOL MyCmpString( const char* pszSrc, const char* pszFormat, const int nLen );
BOOL CheckFnByFnFormat( const char* pszFnName, const char* pszFnFormat );

#endif