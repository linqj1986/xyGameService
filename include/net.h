#ifndef _NET_H_
#define _NET_H_

#include "globals.h"

#define		ERROR_NET_TIMEOUT		0
#define		ERROR_NET_ERROR			-1

#define		SHOWBACK_NORMAL			0
#define		SHOWBACK_PASSWORD		1
#define		SHOWBACK_NULL				2

int InitTerm( int* fd );
void CloseTerm( int* fd );


int LockControl( int* fd );
int UnLockControl( int* fd );
int OpenAuxCommand( int* fd, int nTermPort, int nBaud );
int OpenAuxCommand_STAR( int* fd, int nTermPort, int nBaud );
int OpenAuxCommand_NEWLAND( int* fd, int nTermPort, int nBaud );
int OpenAuxCommand_GUOGUANG( int* fd, int nTermPort, int nBaud );
int OpenAuxCommand_GROUDWALL( int* fd, int nTermPort, int nBaud );
int CloseAuxCommand( int* fd );
int CloseAuxCommand_STAR( int* fd );
int CloseAuxCommand_NEWLAND( int* fd );
int CloseAuxCommand_GUOGUANG( int* fd );
int CloseAuxCommand_GROUDWALL( int* fd );
int GetResponse( int* fd, char* szBuf, const int nMaxLen, const char* szEnd, const int nEndLen, const int nTimeOut );
int GetUserInputAndShowBack( int* fd, char* szBuf, const int nMaxLen, const int nTimeOut, const BYTE byShowBackType );

#endif