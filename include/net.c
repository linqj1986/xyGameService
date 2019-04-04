#include "net.h"

static struct termio oterm_attr, term_attr;

int InitTerm( int* fd )
{
	*fd = open( (char *)ttyname(1), O_RDWR );
	if ( *fd <= 0 )
	{
		return ERROR_SETTTY;
	}

	if ( ioctl( *fd, TCGETA, &oterm_attr ) < 0 || ioctl( *fd, TCGETA, &term_attr ) < 0 )
	{
		CloseTerm( fd );
		return ERROR_SETTTY;
	}
	
	term_attr.c_lflag &=~(ISIG|ECHO|ICANON|NOFLSH);
	term_attr.c_iflag &=~(IXON|ICRNL);//|ISTRIP|IGNCR|INLCR|ICRNL
	term_attr.c_oflag &=~(OPOST);//|OCRNL|ONLCR|ONLRET

	term_attr.c_cc[VMIN] = 0;
	term_attr.c_cc[VTIME] = 1;
	
	if( ioctl( *fd, TCSETAF, &term_attr )<0)
	{
		CloseTerm( fd );
		return ERROR_SETTTY;
	}
	return ERROR_SUCCESS;
}

void CloseTerm( int* fd )
{
	if ( *fd <= 0 )
	{
		return;
	}
	ioctl( *fd, TCSETAF, &oterm_attr );
	close( *fd );
	*fd = 0;
}

int LockControl( int* fd )
{
	int nRet = ERROR_SUCCESS;
	char szCommand[] = "\x1B[2h";
	int nLen = write( *fd, szCommand, strlen(szCommand) );
	if ( nLen != strlen(szCommand) )
	{
		nRet = ERROR_WRITECOMMAND;
	}
	return nRet;
}

int UnLockControl( int* fd )
{
	int nRet = ERROR_SUCCESS;
	char szCommand[] = "\x1B[2l";
	int nLen = write( *fd, szCommand, strlen(szCommand) );
	if ( nLen != strlen(szCommand) )
	{
		nRet = ERROR_WRITECOMMAND;
	}
	return nRet;
}

int OpenAuxCommand( int* fd, int nTermPort, int nBaud )
{
	return OpenAuxCommand_STAR( fd, nTermPort, nBaud );
}

int OpenAuxCommand_STAR( int* fd, int nTermPort, int nBaud )
{
	//写指令
	int nRet = ERROR_SUCCESS;
	char szCommand[MAX_PATH];
	char szBaudSign[10];
	char szPortSign[10];
	memset( szBaudSign, 0, sizeof(szBaudSign) );
	memset( szPortSign, 0, sizeof(szPortSign) );
	switch ( nBaud )
	{
	case 9600:
		SAFE_COPY( szBaudSign, "0", _countof(szBaudSign) );
		break;
	case 4800:
		SAFE_COPY( szBaudSign, "1", _countof(szBaudSign) );
		break;
	case 2400:
		SAFE_COPY( szBaudSign, "2", _countof(szBaudSign) );
		break;
	case 1200:
		SAFE_COPY( szBaudSign, "3", _countof(szBaudSign) );
		break;
	case 600:
		SAFE_COPY( szBaudSign, "5", _countof(szBaudSign) );
		break;
	case 300:
		SAFE_COPY( szBaudSign, "6", _countof(szBaudSign) );
		break;
	case 19200:
		SAFE_COPY( szBaudSign, "10", _countof(szBaudSign) );
		break;
	case 38400:
		SAFE_COPY( szBaudSign, "11", _countof(szBaudSign) );
		break;
	case 115200:
		SAFE_COPY( szBaudSign, "12", _countof(szBaudSign) );
		break;
	default:
		nRet = ERROR_UNKNOWBAUD;
		break;
	}
	switch ( nTermPort )
	{
	case 1:
		SAFE_COPY( szPortSign, "Y", _countof(szPortSign) );
		break;
	case 2:
		SAFE_COPY( szPortSign, "Z", _countof(szPortSign) );
		break;
	case 3:
		SAFE_COPY( szPortSign, "X", _countof(szPortSign) );
		break;
	case 4:
		SAFE_COPY( szPortSign, "W", _countof(szPortSign) );
		break;
	default:
		nRet = ERROR_UNKNOWPORT;
		break;
	}
	if ( nRet == ERROR_SUCCESS )
	{
		memset( szCommand, 0, sizeof(szCommand) );
		snprintf( szCommand, sizeof(szCommand)-1, "\x1B!%s;0;0;0%s\x1B[/50h\x1B[/53h\x1B[/54l\x1B[/51h", szBaudSign, szPortSign );
		int nLen = write( *fd, szCommand, strlen(szCommand) );
		if ( nLen != strlen(szCommand) )
		{
			nRet = ERROR_WRITECOMMAND;
		}
	}
	return nRet;
}

int OpenAuxCommand_NEWLAND( int* fd, int nTermPort, int nBaud )
{
	return OpenAuxCommand_STAR( fd, nTermPort, nBaud );
}

int OpenAuxCommand_GUOGUANG( int* fd, int nTermPort, int nBaud )
{
	return OpenAuxCommand_STAR( fd, nTermPort, nBaud );
}

int OpenAuxCommand_GROUDWALL( int* fd, int nTermPort, int nBaud )
{
	return OpenAuxCommand_STAR( fd, nTermPort, nBaud );
}

int CloseAuxCommand( int* fd )
{
	return CloseAuxCommand_STAR( fd );
}

int CloseAuxCommand_STAR( int* fd )
{
	int nRet = ERROR_SUCCESS;
	char szCommand[] = "\x1B[/51l\x1B[/50l";
	int nLen = write( *fd, szCommand, strlen( szCommand ) );
	if ( nLen != strlen(szCommand) )
	{
		nRet = ERROR_WRITECOMMAND;
	}
	return nRet;
}

int CloseAuxCommand_NEWLAND( int* fd )
{
	return CloseAuxCommand_STAR( fd );
}

int CloseAuxCommand_GUOGUANG( int* fd )
{
	return CloseAuxCommand_STAR( fd );
}

int CloseAuxCommand_GROUDWALL( int* fd )
{
	return CloseAuxCommand_STAR( fd );
}

int GetUserInputAndShowBack( int* fd, char* szBuf, const int nMaxLen, const int nTimeOut, const BYTE byShowBackType )//要求回显
{
	int nRet = 0;
	fd_set fds;
	struct timeval timeout;
	time_t ntStart, ntEnd;
	time(&ntStart);
	memset( szBuf, 0, nMaxLen );
	char szTemp[MAX_PATH];
	while ( 1 )
	{ 
		int ret_code = read( *fd, &szBuf[nRet], 1 );
		time(&ntEnd);
		if ( ret_code > 0 )
		{
			if ( szBuf[nRet] == '\x0D' || szBuf[nRet] == '\x0A' )//回车，输入结束
			{
				szBuf[nRet] = '\0';
				break;
			}
			else if( szBuf[nRet] == '\x08' )//退格
			{
				szBuf[nRet] = '\0';
				if ( nRet > 0 )
				{
					nRet--;
					szBuf[nRet] = '\0';
					write( *fd, "\x08\x20\x08", 3 );
				}
			}
			else
			{
				nRet+=ret_code;
				if ( byShowBackType == SHOWBACK_PASSWORD )
				{
					int i = 0;
					for ( i = 0; i < ret_code; i++ )
					{
						write( *fd, "*", 1 );
					}
				}
				else if ( byShowBackType == SHOWBACK_NULL )
				{
				}
				else
				{
					write( *fd, &szBuf[nRet-ret_code], ret_code );
				}
			}
			if ( nRet == nMaxLen )
			{
				break;
			}
			ntStart = ntEnd;
		}
		else if ( ret_code < 0 )
		{
			break;
		}
		if ( nTimeOut > 0 && ntEnd - ntStart > nTimeOut )
		{
			break;
		}
	}
	return nRet;
}

int GetResponse( int* fd, char* szBuf, const int nMaxLen, const char* szEnd, const int nEndLen, const int nTimeOut )
{
	int nRet = 0;
	fd_set fds;
	struct timeval timeout;
	time_t ntStart, ntEnd;
	time(&ntStart);
	memset( szBuf, 0, nMaxLen );
	char szTemp[MAX_PATH];
	while ( 1 )
	{ 
		int ret_code = read( *fd, &szBuf[nRet], 1 );
		time(&ntEnd);
		if ( ret_code > 0 )
		{
			nRet+=ret_code;
			//判断结束符
			if ( nRet >= nEndLen )
			{
				if ( strncmp( &szBuf[nRet-nEndLen], szEnd, nEndLen ) == 0 )
				{
					break;
				}
			}
			
			if ( nRet == nMaxLen )
			{
				break;
			}
			ntStart = ntEnd;
		}
		else if ( ret_code < 0 )
		{
			break;
		}
		if ( ntEnd - ntStart > nTimeOut )
		{
			break;
		}
	}
	return nRet;
}