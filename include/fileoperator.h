#ifndef _FILEOPERATOR_H_
#define _FILEOPERATOR_H_

#include <stdio.h>

#define ASCII_APPEND				(unsigned char)0x01
#define ASCII_READ				(unsigned char)0x02
#define ASCII_CREATE				(unsigned char)0x03
#define BIN_APPEND				(unsigned char)0x04
#define BIN_READ				(unsigned char)0x05
#define BIN_CREATE				(unsigned char)0x06

FILE* MyOpenFile( const char* pszFn, const unsigned char byOpenMode );
size_t MyWriteFileEx( const char* pszFn, const char* pszBuffer, size_t nLen );
size_t MyWriteFile( FILE* pFile, const char* pszBuffer, size_t nLen );
size_t MyReadFile( FILE* pFile, size_t nLen, char* pszBuffer );
void MySaveFile(FILE* pFile );
void MyCloseFile( FILE* pFile );
bool MyCopyFile( const char* pszSrcFn, const char* szDestFn );

#endif
