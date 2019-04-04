#pragma once

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

#ifndef UINT 
#define UINT unsigned int
#endif 

#ifndef DWORD 
#define DWORD unsigned int
#endif 

#ifndef NULL 
#define NULL 0
#endif 

#ifndef PDWORD 
#define PDWORD DWORD*
#endif 

	//////////////////////////////////////////////////////////////////////////
	// MD5 相关宏定义
	//////////////////////////////////////////////////////////////////////////
	// MD5基本位操作函数
#define __MD5_F(x, y, z)(((x) & (y)) | ((~x) & (z)))
#define __MD5_G(x, y, z)(((x) & (z)) | ((y) & (~z)))
#define __MD5_H(x, y, z)((x) ^ (y) ^ (z))
#define __MD5_I(x, y, z)((y) ^ ((x) | (~z)))
	// 位循环左移位操作
#define __MD5_ROL(x, n) (((x) << (n)) | ((x) >> (32-(n))))
	// ---------变换操作----------
	// 第一轮变换基本操作
#define __MD5_FF(a, b, c, d, x, s, t) { \
	(a) += __MD5_F((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// 第二轮变换基本操作
#define __MD5_GG(a, b, c, d, x, s, t) { \
	(a) += __MD5_G((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// 第三轮变换基本操作
#define __MD5_HH(a, b, c, d, x, s, t) { \
	(a) += __MD5_H((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// 第四轮变换基本操作
#define __MD5_II(a, b, c, d, x, s, t) { \
	(a) += __MD5_I((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}

	class CMD5
	{
	public:
		CMD5();
		~CMD5(){};
		/*
		*  功  能：计算输入数据的MD5哈希值
		*  参  数：pbInputBuffer：输入数据
		*         uiInputCount：输入数据长度（字节数）
		*         pbOutputBuffer：输入数据的哈希值
		*  返回值：哈希值的有效长度（字节数）
		*/
		int MD5_Hash( const BYTE* pbInputBuffer, UINT uiInputCount, BYTE* pbOutputBuffer);

		/*
		*  功  能：计算输入数据的HMAC-MD5哈希值
		*  参  数：pbInputBuffer：输入数据
		*         uiInputCount：输入数据长度（字节数）
		*         pbUserKey：用户密钥
		*         uiUserKeyLen：用户密钥长度
		*         pbOutputBuffer：输入数据的哈希值
		*  返回值：哈希值的有效长度（字节数）
		*/
		int HMAC_MD5_Hash( const BYTE* pbInputBuffer,
			UINT uiInputCount,  const BYTE* pbUserKey,
			UINT uiUserKeyLen, BYTE* pbOutputBuffer);
	private:
		struct st_Md5Data_t
		{
			DWORD dwaP[4];
			DWORD dwaQ[4][16];
		};

		struct st_MD5CTX_t
		{
			DWORD dwState[4];    // 记录数据的变化状态  
			DWORD dwCount[2];    // 记录数据的原始长度(以bit为单位)
			BYTE baBuffer[64];   // 原始数据  
		};

	private:
		void _MD5_Init(st_MD5CTX_t& pstruContext);
		void _MD5_Transform(PDWORD pdwState, PDWORD pdwX);
		void _MD5_Update(st_MD5CTX_t& stContext, const BYTE* pbInput,
			DWORD dwInputLen);
		void _MD5_Final(st_MD5CTX_t& stContext);

	private:
		// MD5关键参数，修改即可形成不同的变体
		st_Md5Data_t m_stMd5Arguments;
		BYTE m_bMd5ShiftTable[4][4];    // 变换操作移位表
		BYTE m_bMd5Padding[64];         // 填充数据
	};

