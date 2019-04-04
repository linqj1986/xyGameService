#include <stdio.h> 
#include <memory.h> 
#include <stdlib.h> 
#include <string>
using namespace std;

class CEncryptWrapper
{
public:
	CEncryptWrapper();
	~CEncryptWrapper();

	static string MD5( const unsigned char* pbyData, const int nLen );

	static string UrlEncode( const char* pszSrc );
	static string UrlDecode( const char* pszSrc );
	
	static string Encrypt(string strSrc);
	static string Decrypt(string strSrc);
private:
	static string Base64Encode( const char*  pszSrc );
	static string Base64Decode( const char*  pszSrc );
	static int Base64Encode( const void* pSrc, const int nSrcLen, unsigned char* pbyDest, const int nMaxLen );
	static int Base64Decode( const unsigned char* pbySrc, const int nSrcLen, void* pDest, const int nMaxLen );
	static int UrlEncode( const char* pcsSrc, char* pcsDest, const int nMaxLen );
	static int UrlDecode( const char* pcsSrc, char* pcsDest, const int nMaxLen );

};
