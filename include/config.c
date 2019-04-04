#include "config.h"
const char* g_szDefaultConfigParam[] = {
	"002396",						//系统设置密码
	"1",								//辅口号
	"9600",							//波特率
	"60",								//用户操作超时
	"1",								//加密模式，默认农行加密
	"6",								//密码最大长度，默认6
	"0",								//排序模式，默认正序可选
	"0",								//超时上送结果，默认不上送
	"0",								//进入广告等待时间，默认不等待
	"3"									//音量设置，默认3，最大
};


int GetPassKey( const char* szCfg, const int nKeyNo, char* szKey, const int nMaxLen )
{
	const char szDefaultKey[] = "3838383838383838";
	int nRet = 0;
	if ( szKey != NULL )
	{
		memset( szKey, 0, nMaxLen );
		FILE* fn = MyOpenFile( szCfg, BIN_READ );
		if ( fn )
		{
			char szBuffer[MAX_PATH];
			int i = 0;
			while ( !feof( fn ) )
			{
				memset( szBuffer, 0, sizeof(szBuffer) );
				fgets( szBuffer, _countof(szBuffer)-1, fn );
				if ( i == nKeyNo )
				{
					memset( szKey, 0, nMaxLen );
					FilterStringEnd( szBuffer, strlen(szBuffer) );//去除行末换行符
					if ( MyUnEncrypt( ENCRYPT_KEY, (BYTE*)szBuffer, strlen(szBuffer), (BYTE*)szKey, nMaxLen ) )
					{
						nRet = strlen(szKey);
					}
					break;
				}
				i++;
			}
			MyCloseFile( fn );
		}
		if ( nRet == 0 )//未读到，则取默认的Key
		{
			SAFE_COPY( szKey, szDefaultKey, nMaxLen );
			nRet = strlen(szKey);
		}
	}
	return nRet;
}


BOOL SetPassKeyEx( const char* szCfg, const int nKeyNo, const char* szKey, const int nKeyLen )
{
	BOOL bRet = FALSE;
	char szCfgTmp[MAX_PATH];
	SAFE_COPY( szCfgTmp, szCfg, _countof(szCfgTmp) );
	SAFE_CAT( szCfgTmp, ".tmp", _countof(szCfgTmp) );
	MyCopyFile( szCfg, szCfgTmp );//备份文件
	int i = 0;
	char szBuffer[MAX_PATH];
	char szText[MAX_PATH];
	
	//更新密钥配置文件
	FILE* fnKey = MyOpenFile( szCfg, BIN_CREATE );
	if ( fnKey )
	{
		for ( i = 0; i < 16; i++ )
		{
			memset( szText, 0, sizeof(szText) );
			if ( i != nKeyNo )
			{
				memset( szBuffer, 0, sizeof(szBuffer) );
				GetPassKey( szCfgTmp, i, szBuffer, _countof(szBuffer) );
				MyEncrypt( ENCRYPT_KEY, szBuffer, strlen(szBuffer), szText, _countof(szText) );				
			}
			else
			{
				MyEncrypt( ENCRYPT_KEY, szKey, strlen(szKey), szText, _countof(szText) );				
			}
			MyWriteFile( fnKey, szText, strlen(szText) );
			MyWriteFile( fnKey, "\n", 1 );
		}
		MyCloseFile( fnKey );
		bRet = TRUE;
	}
	return bRet;
}

BOOL SetPassKey( const int nKeyNo, const char* szKey, const int nMaxLen )
{
	BOOL bRet = FALSE;
	//备份密钥配置文件
	if ( !MyCopyFile( CONFIG_FILE, CONFIG_FILE_TMP ) )
	{
		return bRet;
	}
	//备份历史密钥配置文件
	if ( !MyCopyFile( CONFIG_FILE_OLD, CONFIG_FILE_OLD_TMP ) )
	{
		return bRet;
	}
	int i = 0;
	char szBuffer[MAX_PATH];
	char szText[MAX_PATH];
	
	//更新密钥配置文件
	FILE* fnKey = MyOpenFile( CONFIG_FILE, BIN_CREATE );
	if ( fnKey )
	{
		for ( i = 0; i < 16; i++ )
		{
			memset( szText, 0, sizeof(szText) );
			if ( i != nKeyNo )
			{
				memset( szBuffer, 0, sizeof(szBuffer) );
				GetPassKey( CONFIG_FILE_TMP, i, szBuffer, _countof(szBuffer) );
				MyEncrypt( ENCRYPT_KEY, szBuffer, strlen(szBuffer), szText, _countof(szText) );				
			}
			else
			{
				MyEncrypt( ENCRYPT_KEY, szKey, strlen(szKey), szText, _countof(szText) );				
			}
			MyWriteFile( fnKey, szText, strlen(szText) );
			MyWriteFile( fnKey, "\n", 1 );
		}
		MyCloseFile( fnKey );
		bRet = TRUE;
	}
	if ( bRet )
	{
		//更新上一次的密钥文件
		FILE* fnOldKey = MyOpenFile( CONFIG_FILE_OLD, BIN_CREATE );
		if ( fnOldKey )
		{
			for ( i = 0; i < 16; i++ )
			{
				memset( szText, 0, sizeof(szText) );
				if ( i != nKeyNo )
				{
					memset( szBuffer, 0, sizeof(szBuffer) );
					GetPassKey( CONFIG_FILE_OLD_TMP, i, szBuffer, _countof(szBuffer) );
					MyEncrypt( ENCRYPT_KEY, szBuffer, strlen(szBuffer), szText, _countof(szText) );
				}
				else
				{
					memset( szBuffer, 0, sizeof(szBuffer) );
					GetPassKey( CONFIG_FILE_TMP, i, szBuffer, _countof(szBuffer) );
					MyEncrypt( ENCRYPT_KEY, szBuffer, strlen(szBuffer), szText, _countof(szText) );
				}
				MyWriteFile( fnOldKey, szText, strlen(szText) );
				MyWriteFile( fnOldKey, "\n", 1 );
			}
			MyCloseFile( fnOldKey );
		}
	}
	return bRet;
}

int FilterPassKey( char* szKey, const int nMaxLen )
{
	int nRet = 0;
	int i;
	nRet = strlen(szKey);
	for ( i = 0; i < nRet; i++ )
	{
		if ( ( szKey[i] >= '0' && szKey[i] <= '9' )
			|| ( szKey[i] >= 'a' && szKey[i] <= 'f' )
			|| ( szKey[i] >= 'A' && szKey[i] <= 'F' )
			)
		{
		}
		else
		{
			return -1;//检测到不合法字符，直接返回
		}
	}
	
	//不足8/16/24位时补0，因为这里是HEX转成字符串显示，因此x2比较
	if ( nRet <= (8*2) )
	{
		while ( nRet < (8*2) && nRet < nMaxLen )
		{
			szKey[nRet] = '0';
			nRet++;
		}
		szKey[nRet] = '\0';
	}
	else if ( nRet <= (16*2) )
	{
		while ( nRet < (16*2) && nRet < nMaxLen )
		{
			szKey[nRet] = '0';
			nRet++;
		}
		szKey[nRet] = '\0';
	}
	else if ( nRet <= (24*2) )
	{
		while ( nRet < (24*2) && nRet < nMaxLen )
		{
			szKey[nRet] = '0';
			nRet++;
		}
		szKey[nRet] = '\0';
	}
	else if ( nRet > (24*2) )
	{
		nRet = -1;
	}
	return nRet;
}

void SetSystemConfigEx( const char* szFile, const int nConfigType, const char* szBuffer, const int nBufLen )
{
	char szFileTmp[MAX_PATH];
	SAFE_COPY( szFileTmp, szFile, _countof(szFileTmp) );
	SAFE_CAT( szFileTmp, ".tmp", _countof(szFileTmp) );
	MyCopyFile( szFile, szFileTmp );//备份文件
	FILE *fn = MyOpenFile( szFile, BIN_CREATE );
	if ( fn )
	{
		//开始逐行设置
		int i = 0;
		char szParam[MAX_PATH];
		char szText[MAX_PATH];
		int nParamLen;
		for ( i = 0; i < _countof(g_szDefaultConfigParam); i++ )
		{
			memset( szParam, 0, sizeof(szParam) );
			memset( szText, 0, sizeof(szText) );
			if ( i == nConfigType )
			{
				nParamLen = nBufLen;
				memcpy( szParam, szBuffer, min(_countof(szParam)-1,nBufLen) );
			}
			else
			{
				nParamLen = GetSystemConfigEx( szFileTmp, i, szParam, _countof(szParam) );
			}
			MyEncrypt( ENCRYPT_KEY, szParam, nParamLen, szText, _countof(szText) );
			MyWriteFile( fn, szText, strlen(szText) );
			MyWriteFile( fn, "\n", 1 );
		}
		MyCloseFile( fn );
	}
}

void SetSystemConfig( const int nConfigType, const char* szBuffer, const int nBufLen )
{
	char szFile[MAX_PATH];
	GetDevConfigFile( szFile, _countof(szFile) );
	SetSystemConfigEx( szFile, nConfigType, szBuffer, nBufLen );
}

int GetSystemConfigEx( const char* szConfigFile, const int nConfigType, char* szBuffer, const int nMaxLen )
{
	int nRet = 0;
	if ( szBuffer != NULL )
	{
		memset( szBuffer, 0, nMaxLen );
		FILE* fn = MyOpenFile( szConfigFile, BIN_READ );
		if ( fn )
		{
			char szTmp[MAX_PATH];
			int i = 0;
			while ( !feof( fn ) )
			{
				memset( szTmp, 0, sizeof(szTmp) );
				fgets( szTmp, _countof(szTmp)-1, fn );
				if ( i == nConfigType )
				{
					memset( szBuffer, 0, nMaxLen );
					FilterStringEnd( szTmp, strlen(szTmp) );//去除行末换行符
					if ( MyUnEncrypt( ENCRYPT_KEY, (BYTE*)szTmp, strlen(szTmp), (BYTE*)szBuffer, nMaxLen ) )
					{
						nRet = strlen(szBuffer);
					}
					break;
				}
				i++;
			}
			MyCloseFile( fn );
		}
		if ( nRet == 0 )//未检索到，使用默认
		{
			if ( nConfigType < _countof(g_szDefaultConfigParam) )
			{
				SAFE_COPY( szBuffer, g_szDefaultConfigParam[nConfigType], nMaxLen );
				nRet = strlen(szBuffer);
			}
		}
	}
	return nRet;
}

int GetSystemConfig( const int nConfigType, char* szBuffer, const int nMaxLen )
{
	int nRet = 0;
	char szFile[MAX_PATH];
	GetDevConfigFile( szFile, _countof(szFile) );
	if ( szBuffer != NULL )
	{
		memset( szBuffer, 0, nMaxLen );
		FILE* fn = MyOpenFile( szFile, BIN_READ );
		if ( fn )
		{
			char szTmp[MAX_PATH];
			int i = 0;
			while ( !feof( fn ) )
			{
				memset( szTmp, 0, sizeof(szTmp) );
				fgets( szTmp, _countof(szTmp)-1, fn );
				if ( i == nConfigType )
				{
					memset( szBuffer, 0, nMaxLen );
					FilterStringEnd( szTmp, strlen(szTmp) );//去除行末换行符
					if ( MyUnEncrypt( ENCRYPT_KEY, (BYTE*)szTmp, strlen(szTmp), (BYTE*)szBuffer, nMaxLen ) )
					{
						nRet = strlen(szBuffer);
					}
					break;
				}
				i++;
			}
			MyCloseFile( fn );
		}
		if ( nRet == 0 )//未检索到，使用默认
		{
			if ( nConfigType < _countof(g_szDefaultConfigParam) )
			{
				SAFE_COPY( szBuffer, g_szDefaultConfigParam[nConfigType], nMaxLen );
				nRet = strlen(szBuffer);
			}
		}
	}
	return nRet;
}

void GetDevConfigFile( char* szFile, const int nMaxLen )
{
	memset( szFile, 0, nMaxLen );
	SAFE_COPY( szFile, SYSTEM_CONFIG_FILE, nMaxLen );
	SAFE_CAT( szFile, "_", nMaxLen );
	char szTtyName[MAX_PATH];
	GetTTYName( szTtyName, _countof(szTtyName) );
	SAFE_CAT( szFile, szTtyName, nMaxLen );
}

void GetDevKeyFile( char* szFile, const int nMaxLen )
{
	memset( szFile, 0, nMaxLen );
	SAFE_COPY( szFile, SYSTEM_KEY_FILE, nMaxLen );
	SAFE_CAT( szFile, "_", nMaxLen );
	char szTtyName[MAX_PATH];
	GetTTYName( szTtyName, _countof(szTtyName) );
	SAFE_CAT( szFile, szTtyName, nMaxLen );
}

void GetTTYConfigFile( const char* szTty, char* szTtyFile, const int nMaxLen )
{
	memset( szTtyFile, 0, nMaxLen );
	SAFE_COPY( szTtyFile, SYSTEM_CONFIG_FILE, nMaxLen );
	SAFE_CAT( szTtyFile, "_", nMaxLen );
	char szTtyName[MAX_PATH];
	SAFE_COPY( szTtyName, szTty, _countof(szTtyName) );
	FilterTTYName( szTtyName );
	SAFE_CAT( szTtyFile, szTtyName, nMaxLen );
}

void GetTTYName( char* szTtyName, const int nMaxLen )
{
	SAFE_COPY( szTtyName, (char*)ttyname(1), nMaxLen );
	FilterTTYName( szTtyName );
}

void FilterTTYName( char* szTtyName )
{
	int i = 0;
	for ( i = 0; i < strlen(szTtyName); i++ )
	{
		if ( szTtyName[i] == '/' )
		{
			szTtyName[i] = '_';
		}
	}
}

int GetRandomWorkingKey( char* szKey, const int nMaxLen )
{
	time_t ntSeed;
	time(&ntSeed);
	srand( ntSeed );
	int nType = rand( )%3;
	int nKeyLen;
	switch ( nType )
	{
	case 1:
		nKeyLen = 16;
		break;
	case 2:
		nKeyLen = 24;
		break;
	default:
		nKeyLen = 8;
		break;
	}
	if ( nKeyLen > nMaxLen )
	{
		nKeyLen = 8;
		if ( nKeyLen > nMaxLen )
		{
			nKeyLen = nMaxLen;
		}
	}
	int i = 0;
	for ( i = 0; i < nKeyLen; i++ )
	{
		szKey[i] = (char)(rand()%0x100);
	}
	return i;
}

int GetRandomKeyNo( const int nMaxNo )
{
	time_t ntSeed;
	time(&ntSeed);
	srand( ntSeed );
	return rand( )%nMaxNo;
}