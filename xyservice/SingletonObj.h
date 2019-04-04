#if !defined(AFX_SINGLETONOBJ_H__3FC34075_6E73_4E6D_81DB_C0667A68E049__INCLUDED_)
#define AFX_SINGLETONOBJ_H__3FC34075_6E73_4E6D_81DB_C0667A68E049__INCLUDED_

#pragma once

//#include <afxtempl.h>

template<class TYPE> 
class CSingletonObj  
{
public:
	CSingletonObj(){};
	virtual ~CSingletonObj(){};

//Singleton
private:
	static TYPE *m_pInstance;
public:
	//------------------------------------------------------
	//			获取实例对象
	//------------------------------------------------------
	static TYPE* GetInstance()
	{
		
		if (m_pInstance == NULL)
		{
			m_pInstance = new TYPE();
		}

		return m_pInstance;
	};

	//------------------------------------------------------
	//			释放对象
	//------------------------------------------------------
	static void  Release()
	{
		if ( NULL != m_pInstance)
		{
			delete m_pInstance;
			m_pInstance = NULL;
		}
	};
};

template<class TYPE>
TYPE* CSingletonObj<TYPE>::m_pInstance = NULL;

#define DECLARE_SINGLETON(type) \
friend class CSingletonObj<type>; \
public: \
static type *GetInstance() \
{ \
return CSingletonObj<type>::GetInstance(); \
} \
static void Release() \
{ \
CSingletonObj<type>::Release();	\
}
#endif // !defined(AFX_SINGLETONOBJ_H__3FC34075_6E73_4E6D_81DB_C0667A68E049__INCLUDED_)