#include "fileoperator.h"

FILE* MyOpenFile( const char* pszFn, const unsigned char byOpenMode )
{
	//以附加的方式打开读写文件文件
	switch( byOpenMode )
	{
		case BIN_APPEND:
			return fopen(pszFn, "ab+");
		case ASCII_APPEND:
			return fopen(pszFn, "ab+");
		case BIN_READ:
			return fopen(pszFn, "rb");
		case ASCII_READ:
			return fopen( pszFn, "r" );
		case BIN_CREATE:
			return fopen(pszFn, "wb+");
		case ASCII_CREATE:
			return fopen(pszFn, "w+");
		default:
			return NULL;
	}
}


size_t MyWriteFileEx( const char* pszFn, const char* pszBuffer, size_t nLen )
{
	const unsigned int MAX_WRITE_LEN = 64*1024;
	unsigned int wt_len = 0;
	unsigned int n = 0;
	unsigned int total_wt_len = 0;
	FILE* pFile = MyOpenFile( pszFn, BIN_APPEND );
	if ( pFile == NULL )
		return 0;

	while(1)
	{
		wt_len = 
			(nLen - total_wt_len) >= MAX_WRITE_LEN ? 
			MAX_WRITE_LEN : 
			(nLen - total_wt_len);

		if(wt_len == 0)
		{
			fflush( pFile );
			return total_wt_len;
		}

		n = fwrite((pszBuffer + total_wt_len), sizeof(char), wt_len, pFile);

		if(-1 == n)
			return -1;
		
		total_wt_len += n;

		if(n != wt_len)
		{
			fflush( pFile );
			return total_wt_len;
		}
	}
	fflush( pFile );
	MyCloseFile( pFile );
	return total_wt_len;
}

size_t MyWriteFile( FILE* pFile, const char* pszBuffer, size_t nLen )
{
	const unsigned int MAX_WRITE_LEN = 64*1024;
	unsigned int wt_len = 0;
	unsigned int n = 0;
	unsigned int total_wt_len = 0;
	if ( pFile == NULL )
		return 0;

	while(1)
	{
		wt_len = 
			(nLen - total_wt_len) >= MAX_WRITE_LEN ? 
			MAX_WRITE_LEN : 
			(nLen - total_wt_len);

		if(wt_len == 0)
		{
			fflush( pFile );
			return total_wt_len;
		}
		n = fwrite((pszBuffer + total_wt_len), sizeof(char), wt_len, pFile);

		if(-1 == n)
			return -1;
		
		total_wt_len += n;

		if(n != wt_len)
		{
			fflush( pFile );
			return total_wt_len;
		}
	}
	fflush( pFile );
	return total_wt_len;
}

size_t MyReadFile( FILE* pFile, size_t nLen, char* pszBuffer )
{
	const size_t MAX_READ_LEN = 64*1024;
	size_t rd_len = 0;
	size_t n = 0;
	size_t total_rd_len = 0;
	if ( pFile == NULL )
		return 0;
	while(1)
	{
		rd_len = 
			(nLen - total_rd_len) >= MAX_READ_LEN ? 
			MAX_READ_LEN : 
			(nLen - total_rd_len);

		if(rd_len == 0)
		{
			return total_rd_len;
		}

		n = fread( pszBuffer + total_rd_len, sizeof(char), rd_len, pFile);
		if(-1 == n)
			return -1;

		total_rd_len += n;

		if(n != rd_len)
		{
			return total_rd_len;
		}
	}
	return total_rd_len;
}

void MySaveFile(FILE* pFile )
{	
	if ( pFile == NULL )
		return;
	fflush( pFile );
}

void MyCloseFile( FILE* pFile )
{
	if ( pFile == NULL )
		return;
	fclose(pFile);
	pFile = NULL;
}

bool MyCopyFile( const char* pszSrcFn, const char* szDestFn )
{
	bool bRet = false;
	FILE* fnSrc = MyOpenFile( pszSrcFn, BIN_READ );
	if ( !fnSrc )//如果文件不存在，创建一个
	{
		fnSrc = MyOpenFile( pszSrcFn, BIN_CREATE );
	}
	if ( fnSrc )
	{
		FILE* fnDest = MyOpenFile( szDestFn, BIN_CREATE );
		if ( fnDest )
		{
			char szBuffer[256];
			int nRead = 0;
			while ( !feof( fnSrc ) )
			{
				nRead = MyReadFile( fnSrc, 256, szBuffer );
				if ( nRead > 0 )
				{
					MyWriteFile( fnDest, szBuffer, nRead );
				}
				else
				{
					break;
				}
			}
			bRet = true;
			MyCloseFile( fnDest );
		}
		MyCloseFile( fnSrc );
	}
	return bRet;
}
