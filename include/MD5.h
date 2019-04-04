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
	// MD5 ��غ궨��
	//////////////////////////////////////////////////////////////////////////
	// MD5����λ��������
#define __MD5_F(x, y, z)(((x) & (y)) | ((~x) & (z)))
#define __MD5_G(x, y, z)(((x) & (z)) | ((y) & (~z)))
#define __MD5_H(x, y, z)((x) ^ (y) ^ (z))
#define __MD5_I(x, y, z)((y) ^ ((x) | (~z)))
	// λѭ������λ����
#define __MD5_ROL(x, n) (((x) << (n)) | ((x) >> (32-(n))))
	// ---------�任����----------
	// ��һ�ֱ任��������
#define __MD5_FF(a, b, c, d, x, s, t) { \
	(a) += __MD5_F((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// �ڶ��ֱ任��������
#define __MD5_GG(a, b, c, d, x, s, t) { \
	(a) += __MD5_G((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// �����ֱ任��������
#define __MD5_HH(a, b, c, d, x, s, t) { \
	(a) += __MD5_H((b), (c), (d)) + (x) + (t); \
	(a) = __MD5_ROL((a), (s)); \
	(a) += (b); \
}
	// �����ֱ任��������
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
		*  ��  �ܣ������������ݵ�MD5��ϣֵ
		*  ��  ����pbInputBuffer����������
		*         uiInputCount���������ݳ��ȣ��ֽ�����
		*         pbOutputBuffer���������ݵĹ�ϣֵ
		*  ����ֵ����ϣֵ����Ч���ȣ��ֽ�����
		*/
		int MD5_Hash( const BYTE* pbInputBuffer, UINT uiInputCount, BYTE* pbOutputBuffer);

		/*
		*  ��  �ܣ������������ݵ�HMAC-MD5��ϣֵ
		*  ��  ����pbInputBuffer����������
		*         uiInputCount���������ݳ��ȣ��ֽ�����
		*         pbUserKey���û���Կ
		*         uiUserKeyLen���û���Կ����
		*         pbOutputBuffer���������ݵĹ�ϣֵ
		*  ����ֵ����ϣֵ����Ч���ȣ��ֽ�����
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
			DWORD dwState[4];    // ��¼���ݵı仯״̬  
			DWORD dwCount[2];    // ��¼���ݵ�ԭʼ����(��bitΪ��λ)
			BYTE baBuffer[64];   // ԭʼ����  
		};

	private:
		void _MD5_Init(st_MD5CTX_t& pstruContext);
		void _MD5_Transform(PDWORD pdwState, PDWORD pdwX);
		void _MD5_Update(st_MD5CTX_t& stContext, const BYTE* pbInput,
			DWORD dwInputLen);
		void _MD5_Final(st_MD5CTX_t& stContext);

	private:
		// MD5�ؼ��������޸ļ����γɲ�ͬ�ı���
		st_Md5Data_t m_stMd5Arguments;
		BYTE m_bMd5ShiftTable[4][4];    // �任������λ��
		BYTE m_bMd5Padding[64];         // �������
	};

