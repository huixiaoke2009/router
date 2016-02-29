#ifndef _HASH_LIST_H_
#define _HASH_LIST_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

namespace mmlib
{

//所有内部指针都使用偏移量
template<class KEYTYPE, class DATATYPE>
class CHashList
{

#pragma pack(1)
    typedef struct tagHead
    {
        int iSize;
        int iTotalDataNodeNum;
        int iUsedDataNodeNum;
        int iIndexNodeNum;
        int ipFreeDataNodeList;
    }Head;

    typedef struct tagIndexNode
    {
        int iHashVal;
        int iDataNodeNum;
        int ipDataNodeList;
    }IndexNode;

    typedef struct tagDataNode
    {
        KEYTYPE KeyVal;
        int ipNextDataNode;
        DATATYPE Data;
    }DataNode;
#pragma pack()

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
    const static int HEAD_SIZE = sizeof(Head);
    const static int INDEX_NODE_SIZE = sizeof(IndexNode);
    const static int DATA_NODE_SIZE = sizeof(DataNode);

    const static int HASH_LIST_LOCK_READ = 1;
    const static int HASH_LIST_LOCK_WRITE = 2;

protected:
    unsigned char *m_pvMem;
    int m_iIndexNodeNum;
    int m_iDataNodeNum;
    Head *m_pstHead;
    IndexNode *m_pstIndex;
    DataNode *m_pstData;

public:
    CHashList(void)
    {
        m_pvMem = NULL;
        m_pstHead = NULL;
        m_pstIndex = NULL;
        m_pstData = NULL;
    }

    virtual ~CHashList(void)
    {
    }

    virtual int Init(void *pvMem, int iMemSize, int iIndexNum, int iDataNum)
    {
        m_pvMem = (unsigned char*)pvMem;
        m_pstHead = (Head *)m_pvMem;
        m_pstIndex = (IndexNode *)(m_pvMem + HEAD_SIZE);
        m_pstData = (DataNode *)(m_pvMem + HEAD_SIZE + (iIndexNum * INDEX_NODE_SIZE) );

        int iTotalMemSize = HEAD_SIZE + (iIndexNum * INDEX_NODE_SIZE) + (iDataNum * DATA_NODE_SIZE);
        if (iTotalMemSize > iMemSize )
        {
            return E_HASH_LIST_MEM_SIZE;
        }

        if (iIndexNum <1)
        {
            return E_HASH_LIST_INDEX_NUM;
        }

        if (iDataNum < 2)
        {
            //第一个节点不能使用
            return E_HASH_LIST_DATA_NUM;
        }

        LockHead(HASH_LIST_LOCK_WRITE);
        m_pstHead->iSize = iMemSize;
        m_pstHead->iIndexNodeNum = iIndexNum;
        m_pstHead->iTotalDataNodeNum = iDataNum;
        UnLockHead();

        m_iIndexNodeNum = iIndexNum;
        m_iDataNodeNum = iDataNum;

        srand(getpid()+time(NULL));
        return SUCCESS;
    }

    virtual void Destory()
    {
        return;
    }

    int Verify()
    {
        //TODO 校验内存中的数据是否存在错误
        LockAll(HASH_LIST_LOCK_READ);

        int iUsedDataNodeNum = m_pstHead->iUsedDataNodeNum;
        int iRealUsedDataNodeNum = 1;
        int ipData = 0;

        for(int i=0; i<m_pstHead->iIndexNodeNum; i++)
        {
            if(m_pstIndex[i].iHashVal != i)
            {
                UnLockAll();
                return E_HASH_LIST_VERIFY_INDEX_HASHVAL;
            }

            int iDataNodeNum = m_pstIndex[i].iDataNodeNum;
            int iRealDataNodeNum = 0;

            ipData = m_pstIndex[i].ipDataNodeList;

            while (ipData != 0)
            {
                iRealDataNodeNum++;
                if (ipData > m_pstHead->iTotalDataNodeNum)
                {
                    UnLockAll();
                    return E_HASH_LIST_VERIFY_DATA_PTR;
                }
                ipData = m_pstData[ipData].ipNextDataNode;
            }

            if (iRealDataNodeNum != iDataNodeNum)
            {
                UnLockAll();
                return E_HASH_LIST_VERIFY_INDEX_DATANUM;
            }

            iRealUsedDataNodeNum += iRealDataNodeNum;
        }

        if (iRealUsedDataNodeNum != iUsedDataNodeNum)
        {
            UnLockAll();
            return E_HASH_LIST_VERIFY_HEAD_USEDDATANUM;
        }

        ipData = m_pstHead->ipFreeDataNodeList;
        int iFreeDataNodeNum = 0;
        while (ipData != 0)
        {
            iFreeDataNodeNum++;
            if (ipData > m_pstHead->iTotalDataNodeNum)
            {
                UnLockAll();
                return E_HASH_LIST_VERIFY_DATA_PTR;
            }
            ipData = m_pstData[ipData].ipNextDataNode;
        }

        if ((iFreeDataNodeNum + iUsedDataNodeNum) != m_pstHead->iTotalDataNodeNum)
        {
            UnLockAll();
            return E_HASH_LIST_VERIFY_HEAD_TOTALDATANUM;
        }

        UnLockAll();
        return 0;
    }

    int Clear()
    {
        LockAll(HASH_LIST_LOCK_WRITE);

        int iMemSize = m_pstHead->iSize;
        int iIndexNodeNum = m_pstHead->iIndexNodeNum;
        int iDataNodeNum = m_pstHead->iTotalDataNodeNum;

        int iUsedMemSize=HEAD_SIZE + (m_iIndexNodeNum * INDEX_NODE_SIZE) + (m_iDataNodeNum * DATA_NODE_SIZE);


        memset(m_pvMem, 0x0, iUsedMemSize);

        m_pstHead->iSize = iMemSize;
        m_pstHead->iIndexNodeNum = iIndexNodeNum;
        m_pstHead->iTotalDataNodeNum = iDataNodeNum;
        m_pstHead->iUsedDataNodeNum = 1;
        m_pstHead->ipFreeDataNodeList = 1;    //第0个节点表示空指针

        for (int i=0; i<m_pstHead->iIndexNodeNum; i++)
        {
            m_pstIndex[i].iHashVal = i;
            m_pstIndex[i].iDataNodeNum = 0;
            m_pstIndex[i].ipDataNodeList = 0;
        }

        for (int i=1; i<m_pstHead->iTotalDataNodeNum; i++)
        {
            m_pstData[i].ipNextDataNode = i+1;
        }

        m_pstData[m_pstHead->iTotalDataNodeNum-1].ipNextDataNode = 0;

        UnLockAll();

        return 0;
    }

    //插入指定的Data
    int Insert(const KEYTYPE &Key, const DATATYPE &Data)
    {
        int iHashVal = Hash(Key);
        int ipData = 0;
        int iRetVal = 0;

        LockIndex(HASH_LIST_LOCK_WRITE, iHashVal);
        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            int ipNextData = m_pstData[ipData].ipNextDataNode;
            ipData = ipNextData;
        }

        if (ipData > 0)
        {
            iRetVal = E_HASH_LIST_NODE_EXIST;

        }
        else
        {
            int ipFreeNode=GetFreeNode();
            if(ipFreeNode > 0)
            {
                m_pstData[ipFreeNode].KeyVal = Key;
                m_pstData[ipFreeNode].ipNextDataNode = m_pstIndex[iHashVal].ipDataNodeList;
                memcpy(&m_pstData[ipFreeNode].Data, &Data, sizeof(DATATYPE));
                m_pstIndex[iHashVal].ipDataNodeList = ipFreeNode;
                m_pstIndex[iHashVal].iDataNodeNum ++;

                iRetVal = SUCCESS;
            }
            else
            {

                iRetVal = ipFreeNode;
            }
        }

        UnLockIndex(iHashVal);
        return iRetVal;

    }

    //插入指定的Data，如果该数据对应的KEY已经存在，将会把数据通过Data返回调用者
    int InsertWithRet(const KEYTYPE &Key, DATATYPE &Data)
    {
        int iHashVal = Hash(Key);
        int ipData = 0;
        int iRetVal = 0;

        LockIndex(HASH_LIST_LOCK_WRITE, iHashVal);
        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            int ipNextData = m_pstData[ipData].ipNextDataNode;
            ipData = ipNextData;
        }

        if (ipData > 0)
        {
            memcpy(&Data, &(m_pstData[ipData].Data), sizeof(DATATYPE));
            iRetVal = E_HASH_LIST_NODE_EXIST;
        }
        else
        {
            int ipFreeNode = GetFreeNode();
            if(ipFreeNode > 0)
            {
                m_pstData[ipFreeNode].KeyVal = Key;
                m_pstData[ipFreeNode].ipNextDataNode = m_pstIndex[iHashVal].ipDataNodeList;
                memcpy(&m_pstData[ipFreeNode].Data, &Data, sizeof(DATATYPE));
                m_pstIndex[iHashVal].ipDataNodeList = ipFreeNode;
                m_pstIndex[iHashVal].iDataNodeNum ++;

                iRetVal = SUCCESS;
            }
            else
            {

                iRetVal = ipFreeNode;
            }
        }

        UnLockIndex(iHashVal);
        return iRetVal;
    }

    int Update(const KEYTYPE &Key, const DATATYPE &Data)
    {
        int iRetVal = SUCCESS;

        int iHashVal = Hash(Key);
        int ipData = 0;

        LockIndex(HASH_LIST_LOCK_WRITE, iHashVal);
        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            int ipNextData = m_pstData[ipData].ipNextDataNode;
            ipData = ipNextData;
        }

        if (ipData > 0)
        {
            memcpy(&m_pstData[ipData].Data, &Data, sizeof(DATATYPE));
        }
        else
        {
            iRetVal = E_HASH_LIST_NO_NODE;
        }

        UnLockIndex(iHashVal);

        return iRetVal;
    }

    //删除指定Key的节点
    int Remove(const KEYTYPE &Key)
    {
        int iRetVal = SUCCESS;
        int iHashVal = Hash(Key);
        int ipData = 0;
        int ipLastData = 0;

        LockIndex(HASH_LIST_LOCK_WRITE, iHashVal);

        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            ipLastData = ipData;
            ipData = m_pstData[ipData].ipNextDataNode;
        }

        if (ipData > 0)
        {
            //从链表中去除该数据节点
            if (0 == ipLastData)
            {
                m_pstIndex[iHashVal].ipDataNodeList = m_pstData[ipData].ipNextDataNode;
            }
            else
            {
                m_pstData[ipLastData].ipNextDataNode = m_pstData[ipData].ipNextDataNode;
            }
            m_pstIndex[iHashVal].iDataNodeNum--;

            iRetVal = RecycleNode(ipData);
        }
        else
        {
            iRetVal = E_HASH_LIST_NO_NODE;
        }

        UnLockIndex(iHashVal);

        return iRetVal;
    }

    //获取指定Key的节点
    int Get(const KEYTYPE &Key, DATATYPE &Data)
    {
        int iRetVal = SUCCESS;
        int iHashVal = Hash(Key);
        int ipData = 0;

        LockIndex(HASH_LIST_LOCK_READ, iHashVal);
        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            int ipNextData = m_pstData[ipData].ipNextDataNode;
            ipData = ipNextData;
        }

        if (ipData > 0)
        {
            memcpy(&Data, &m_pstData[ipData].Data, sizeof(DATATYPE));
            iRetVal = SUCCESS;
        }
        else
        {
            iRetVal = E_HASH_LIST_NO_NODE;
        }

        UnLockIndex(iHashVal);

        return iRetVal;
    }

    //对于指定Key的节点，进行回调处理
    int Process(const KEYTYPE &Key, void (*CallBackFunc)(KEYTYPE Key, DATATYPE &Data), void *pPrivateData)
    {
        int iRetVal = SUCCESS;
        int iHashVal = Hash(Key);
        int ipData = 0;

        LockIndex(HASH_LIST_LOCK_WRITE, iHashVal);
        ipData = m_pstIndex[iHashVal].ipDataNodeList;

        while ((ipData != 0)&&(m_pstData[ipData].KeyVal != Key))
        {
            int ipNextData = m_pstData[ipData].ipNextDataNode;
            ipData = ipNextData;
        }

        if (ipData > 0)
        {
            //回调处理函数
            CallBackFunc(m_pstData[ipData].KeyVal, m_pstData[ipData].Data, pPrivateData);
            iRetVal = SUCCESS;
        }
        else
        {
            iRetVal = E_HASH_LIST_NO_NODE;
        }

        UnLockIndex(iHashVal);

        return iRetVal;
    }

    //对所有节点进行回调处理
    int ProcessAll(void (*CallBackFunc)(KEYTYPE Key, DATATYPE &Data), void *pPrivateData)
    {
        int iRetVal = SUCCESS;
        int ipData = 0;
        int ipNextData = 0;
        
        for(int i=0; i<m_iIndexNodeNum; i++)
        {
            LockIndex(HASH_LIST_LOCK_WRITE, i);

            if (m_pstIndex[i].iDataNodeNum > 0)
            {
                ipData = m_pstIndex[i].ipDataNodeList;
                while (ipData != 0)
                {
                    //回调处理函数
                    ipNextData = m_pstData[ipData].ipNextDataNode;  //为了防止回调内部删除该节点
                    CallBackFunc(m_pstData[ipData].KeyVal, m_pstData[ipData].Data, pPrivateData);
                    ipData = ipNextData;
                }
            }

            UnLockIndex(i);
        }

        return iRetVal;
    }

    //获取Index数目
    int GetIndexNum()
    {
        return m_iIndexNodeNum;
    }

    //获取已经使用的节点数
    int GetUsedDataNum()
    {
        //该数据获取时不加锁，可能有微小的偏差
        //由于该数据不能作为HashList是否满的判断依据，所以偏差不影响
        //这里不进行加锁
        return m_pstHead->iUsedDataNodeNum;
    }

    //输出HashList头信息
    virtual void ShowHeadInfo()
    {
        LockHead(HASH_LIST_LOCK_READ);
        printf("HashList Header Info\n");
        printf("BuffSize=%d ", m_pstHead->iSize);
        printf("NodeNum=%d ", m_pstHead->iTotalDataNodeNum);
        printf("UsedNodeNum=%d ", m_pstHead->iUsedDataNodeNum);
        printf("IndexNum=%d\n", m_pstHead->iIndexNodeNum);
        UnLockHead();
    }

    //输出HashList的Index信息
    virtual void ShowIndexInfo()
    {
        for (int i=0; i<m_iIndexNodeNum; i++)
        {
            LockIndex(HASH_LIST_LOCK_READ, i);
            printf("Hash[%d],NodeNum=%d\n", m_pstIndex[i].iHashVal, m_pstIndex[i].iDataNodeNum);
            UnLockIndex(i);
        }
    }

    //输出所有的数据信息
    virtual void ShowDataInfo()
    {
        char szKeyStr[1024] = { 0 };
        char szDataStr[1024] = { 0 };
        int ipData = 0;
        for (int i=0; i<m_iIndexNodeNum; i++)
        {
            LockIndex(HASH_LIST_LOCK_READ, i);
            if(m_pstIndex[i].iDataNodeNum > 0)
            {
                printf("Hash[%d],NodeNum=%d\n", m_pstIndex[i].iHashVal, m_pstIndex[i].iDataNodeNum);
                ipData = m_pstIndex[i].ipDataNodeList;

                while(ipData != 0)
                {
                    printf("  Data[%s]=%s\n",
                        FormatKey(m_pstData[ipData].KeyVal, szKeyStr, sizeof(szKeyStr)),
                        FormatData(m_pstData[ipData].Data, szDataStr, sizeof(szDataStr)));
                    int ipNextData = m_pstData[ipData].ipNextDataNode;
                    ipData = ipNextData;
                }
            }
            UnLockIndex(i);
        }

    }
    //获取某个Key的Hash值
    virtual int Hash(const KEYTYPE &Key)
    {
        return Key % m_pstHead->iIndexNodeNum;
    }

    //读、写锁
    //summary:如果要实现访问HashList互斥，需要重载这两个函数
    //        可以通过区域锁，实现部分区域读写锁，也可以通过信号量实现
    //        整个存储区域锁
    //param:iType表示锁的类型 1-ReadLock 2-WriteLock
    //param:iOffSet表示开始锁的位置
    //param:iSize表示需要锁的区域
    //return: 0-成功 !=0-失败
    virtual int Lock(int iType, int iOffSet, int iSize)
    {
        return 0;
    }

    //释放锁
    virtual int UnLock(int iPos, int iSize)
    {
        return 0;
    }

    //Key的屏幕输出，用于打印调试信息
    virtual const char *FormatKey(const KEYTYPE &Key, char *pBuff, int iBuffSize)
    {
        pBuff[0] = '\0';
        return pBuff;
    }

    //Data的屏幕输出，用于打印调试信息
    virtual const char *FormatData(const DATATYPE &Data, char *pBuff, int iBuffSize)
    {
        pBuff[0] = '\0';
        return pBuff;
    }

    //给定IndexNum和NodeNum，计算所需的内存大小
    int CalcSize(int iIndexNum, int iNodeNum)
    {
        return HEAD_SIZE + (iIndexNum * INDEX_NODE_SIZE) + (iNodeNum * DATA_NODE_SIZE);
    }

private:
    int GetFreeNode()
    {
        int iFreeNode = 0;
        int iFreeNodeNum = 0;

        LockHead(HASH_LIST_LOCK_WRITE);
        iFreeNodeNum = m_pstHead->iTotalDataNodeNum - m_pstHead->iUsedDataNodeNum;


        iFreeNode = m_pstHead->ipFreeDataNodeList;

        if ((iFreeNodeNum <= 0)||(iFreeNode <= 0))
        {
            iFreeNode = E_HASH_LIST_NO_SPACE;
        }
        else
        {
            m_pstHead->iUsedDataNodeNum++;
            m_pstHead->ipFreeDataNodeList = m_pstData[iFreeNode].ipNextDataNode;
        }

        UnLockHead(); //保证节点分配完毕以后，再释放头节点的锁

        return iFreeNode;
    }

    int RecycleNode(int iNode)
    {
        int iRetVal = SUCCESS;

        LockHead(HASH_LIST_LOCK_WRITE);
        if ((0 == iNode) || (iNode > m_pstHead->iTotalDataNodeNum))
        {
            iRetVal = ERROR;
        }
        else
        {
            m_pstHead->iUsedDataNodeNum--;
            m_pstData[iNode].ipNextDataNode = m_pstHead->ipFreeDataNodeList;
            m_pstHead->ipFreeDataNodeList = iNode;
        }
        UnLockHead();

        return iRetVal;
    }

    int LockHead(int iType)
    {
        return Lock(iType, 0, HEAD_SIZE);
    }

    int UnLockHead()
    {
        return UnLock(0, HEAD_SIZE);
    }

    int LockIndex(int iType, int iOffSet)
    {
        //LockIndex包含了锁定该Index对应的数据链表上所有的数据节点
        return Lock(iType, HEAD_SIZE+(iOffSet*INDEX_NODE_SIZE), INDEX_NODE_SIZE);
    }

    int UnLockIndex(int iOffSet)
    {
        return UnLock(HEAD_SIZE+(iOffSet*INDEX_NODE_SIZE), INDEX_NODE_SIZE);
    }

    /*
    int LockData(int iType, int iOffSet)
    {
        return Lock(iType, HEAD_SIZE+(m_iIndexNodeNum*INDEX_NODE_SIZE)+(iOffSet*DATA_NODE_SIZE), DATA_NODE_SIZE);
    }

    int UnLockData(int iOffSet)
    {
        return UnLock(HEAD_SIZE+(m_iIndexNodeNum*INDEX_NODE_SIZE)+(iOffSet*DATA_NODE_SIZE), DATA_NODE_SIZE);
    }
    */

    int LockAll(int iType)
    {
        return Lock(iType, 0, HEAD_SIZE+(m_iIndexNodeNum*INDEX_NODE_SIZE)+(m_iDataNodeNum*DATA_NODE_SIZE));
    }

    int UnLockAll()
    {
        return UnLock(0, HEAD_SIZE+(m_iIndexNodeNum*INDEX_NODE_SIZE)+(m_iDataNodeNum*DATA_NODE_SIZE));
    }
};

}
#endif
