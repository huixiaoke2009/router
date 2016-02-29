#ifndef _HASH_LIST_MMAP_H_
#define _HASH_LIST_MMAP_H_

#include "hash_list.h"
#include "mmap_file/mmap_file.h"
#include "file_lock/file_lock.h"

//所有内部指针都使用偏移量
namespace mmlib
{

template<class KEYTYPE, class DATATYPE>
class CHashListMMap: public CHashList<KEYTYPE, DATATYPE>
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_HASH_LIST_MEM_SIZE = -101;
    const static int E_HASH_LIST_INDEX_NUM = -102;
    const static int E_HASH_LIST_DATA_NUM = -103;
    const static int E_HASH_LIST_NO_SPACE = -104;
    const static int E_HASH_LIST_NO_NODE = -105;
    const static int E_HASH_LIST_NODE_EXIST = -106;
    const static int E_HASH_LIST_INVALID_KEY = -107;
    const static int E_HASH_LIST_VERIFY_INDEX_HASHVAL = -108;
    const static int E_HASH_LIST_VERIFY_INDEX_DATANUM = -109;
    const static int E_HASH_LIST_VERIFY_DATA_PTR = -110;
    const static int E_HASH_LIST_VERIFY_HEAD_USEDDATANUM = -111;
    const static int E_HASH_LIST_VERIFY_DATA_FREEDATAPTR = -112;
    const static int E_HASH_LIST_VERIFY_HEAD_TOTALDATANUM = -113;

public:
    CHashListMMap(void)
    {
    }

    virtual ~CHashListMMap(void)
    {
    }

    int Init(void *pvMem, int iMemSize, int iIndexNum, int iDataNum)
    {
        //不允许使用该函数进行初始化
        return ERROR;
    }
    virtual int Init(const char *pszMapFile, int iMemSize, int iIndexNum, int iDataNum)
    {

        int iRetVal = 0;

        //初始化内存共享文件
        iRetVal = m_objMMapFile.Create(pszMapFile, iMemSize);
        if (iRetVal != SUCCESS)
        {
            return iRetVal;
        }

        void *pMem = m_objMMapFile.GetMem();

        //初始化HashList链表
        iRetVal = CHashList<KEYTYPE, DATATYPE>::Init(pMem, iMemSize, iIndexNum, iDataNum);
        if (iRetVal != SUCCESS)
        {
            return iRetVal;
        }

        //初始化文件锁
        iRetVal = m_objFileLock.Init(pszMapFile);
        if (iRetVal != SUCCESS)
        {
            return iRetVal;
        }

        return SUCCESS;
    }

    //读、写锁
    //summary:如果要实现访问HashList互斥，需要重载这两个函数
    //        可以通过区域锁，实现部分区域读写锁，也可以通过信号量实现
    //        整个存储区域锁
    //param:iType表示锁的类型 1-ReadLock 2-WriteLock
    //param:iOffSet表示开始锁的位置
    //param:iSize表示需要锁的区域
    //return: 0-成功 !=0-失败
    int Lock(int iType, int iOffSet, int iSize)
    {
        return m_objFileLock.Lock(iType, iOffSet, iSize);
    }

    //释放锁
    int UnLock(int iOffSet, int iSize)
    {
        return m_objFileLock.UnLock(iOffSet, iSize);
    }

protected:
    CFileLock m_objFileLock;
    CMMapFile m_objMMapFile;
};

}

#endif
