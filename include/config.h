#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "globals.h"
#include "fileoperator.h"

#define CONFIG_FILE						"/home/abis/acbs/lib/centerm/drv/key.cfg"
#define CONFIG_FILE_OLD				"/home/abis/acbs/lib/centerm/drv/oldkey.cfg"
#define CONFIG_FILE_TMP				"/home/abis/acbs/lib/centerm/drv/key.tmp"
#define CONFIG_FILE_OLD_TMP		"/home/abis/acbs/lib/centerm/drv/oldkey.tmp"

#define SYSTEM_KEY_FILE				"/home/abis/acbs/lib/centerm/dev/key.cfg"
#define SYSTEM_CONFIG_FILE		"/home/abis/acbs/lib/centerm/dev/system.cfg"

#define ENCRYPT_KEY						"\x37\x92\xF8\x13\xDC\x2B\x73\x50"


#define CFGTYPE_SYSTEMPASS			0		//系统密码
#define CFGTYPE_AUX							1		//辅口号
#define CFGTYPE_BAUD						2		//波特率
#define CFGTYPE_USERTIMEOUT			3		//用户操作超时设置
#define CFGTYPE_PASSTYPE				4		//密码键盘工作方式
#define CFGTYPE_PASSMAXLEN			5		//密码最大长度
#define CFGTYPE_PASSORDER				6		//布局
#define CFGTYPE_OPWHENTIMEOUT		7		//超时上送结果
#define CFGTYPE_WAITFORIMAGE		8		//进入广告等待时间
#define CFGTYPE_SOUND						9		//音量设置

#define MAX_WORKKEY_NO					8		//工作密钥，序号支持0-7
#define MAX_MAINKEY_NO					16	//主密钥，序号支持0-15

int GetSystemConfig( const int nConfigType, char* szBuffer, const int nMaxLen );
void SetSystemConfig( const int nConfigType, const char* szBuffer, const int nBufLen );
int GetSystemConfigEx( const char* szConfigFile, const int nConfigType, char* szBuffer, const int nMaxLen );
void SetSystemConfigEx( const char* szFile, const int nConfigType, const char* szBuffer, const int nBufLen );
int GetPassKey( const char* szCfg, const int nKeyNo, char* szKey, const int nMaxLen );
BOOL SetPassKey( const int nKeyNo, const char* szKey, const int nMaxLen );
BOOL SetPassKeyEx( const char* szCfg, const int nKeyNo, const char* szKey, const int nKeyLen );
int FilterPassKey( char* szKey, const int nMaxLen );
void GetDevConfigFile( char* szFile, const int nMaxLen );
void GetDevKeyFile( char* szFile, const int nMaxLen );
void GetTTYName( char* szTtyName, const int nMaxLen );
void GetTTYConfigFile( const char* szTty, char* szTtyFile, const int nMaxLen );
void FilterTTYName( char* szTtyName );
int GetRandomWorkingKey( char* szKey, const int nMaxLen );
int GetRandomKeyNo( const int nMaxNo );
#endif