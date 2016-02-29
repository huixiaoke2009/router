#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "mem_pool/fixedsize_mem_pool.h"

const int MAP_BLOCK_DATA_NUM = 50;

namespace mmlib
{
template<class KEYTYPE, class DATATYPE>
class CMemPool
{

    typedef struct tagAllocBlock
    {
        unsigned int flag;          // 用来标识某个块是否已经被分配,0:未分配 1:分配
        unsigned int next;          // 用来链接空闲块
    } AllocBlock;


    typedef struct tagDataBlock
    {
        int iUsedDataNum;
        int iNextHandle;
        KEYTYPE  astKey[MAP_BLOCK_DATA_NUM];
        DATATYPE astData[MAP_BLOCK_DATA_NUM];
    }DataBlock;

public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int MEM_POOL_INIT_FAILED   = -101;
    const static int MEM_POOL_VERIFY_FAILED = -102;
    const static int MEM_POOL_NOT_INITED    = -103;
    const static int INVALID_MAP_HANDLE     = -104;
    const static int NODE_NOT_EXIST         = -105;
    const static int NODE_EXIST             = -106;
    const static int MEM_POOL_ALLOCATE_FAILED = -107;
    const static int MEM_POOL_RELEASE_FAILED  = -108;

private:
    CFixedsizeMemPool<DataBlock> m_objFMP;   //用于分配、回收内存使用
    int m_iMapHandle;                        //MAP句柄，用于外部在整个MemPool中定位链的头节点，Handle存储的内容是所在MemPool内部数据块偏移量
    DataBlock *m_pHeadDataBlock;             //链的头节点
    int m_iFMPInitFlag;

    void *m_pvMem;

public:

    CMemPool()
    {
        m_iMapHandle = -1;
        m_iFMPInitFlag = 0;
        m_pHeadDataBlock = NULL;
    }

    ~CMemPool()
    {

    }

    /*
     * @summary:初始化内存池
     * @param:pvMem:预分配的内存地址
     * @param:iMemSize:预分配的内存空间
     * @param:iNodeNum:数据节点的数量
     * @param:iClearFlag:清除数据标志位，如果该位为1，表示清楚内存池的所有数据
     * @return:0 success, -1 error
     */
    int InitMemPool(void *pvMem, int iMemSize, int iNodeNum, int iClearFlag = 0)
    {
        int iRetVal = 0;

        iRetVal = m_objFMP.Init(pvMem, iMemSize, iNodeNum, iClearFlag);

        if (iRetVal != m_objFMP.SUCCESS)
        {
            return MEM_POOL_INIT_FAILED;
        }

        m_iFMPInitFlag = 1;

        return SUCCESS;
    }

    int VerifyMemPool()
    {
        int iRetVal = 0;

        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        iRetVal = m_objFMP.Verify();

        if (iRetVal != m_objFMP.SUCCESS)
        {
            printf("FMP Verify ret=%d\n", iRetVal);
            return MEM_POOL_VERIFY_FAILED;
        }

        return SUCCESS;
    }

    int NewMap()
    {
        int iNewDataBlockHandle = -1;

        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        DataBlock *pNewDataBlock = m_objFMP.AllocateNode();
        if (pNewDataBlock == NULL)
        {
            //分配节点失败
            return MEM_POOL_ALLOCATE_FAILED;
        }
        else
        {
            //分配节点成功
            memset(pNewDataBlock, 0x0, sizeof(DataBlock));
            pNewDataBlock->iNextHandle = -1;
            iNewDataBlockHandle = pNewDataBlock - (DataBlock *)(m_objFMP.GetDataMem());
        }

        m_iMapHandle = iNewDataBlockHandle;
        m_pHeadDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + m_iMapHandle;

        return iNewDataBlockHandle;
    }

    int ReleaseMap()
    {
        int iRetVal = 0;

        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((m_iMapHandle<0)||(m_iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        DataBlock *pDataBlock = m_pHeadDataBlock;
        DataBlock *pLastDataBlock = NULL;
        while(pDataBlock != ((DataBlock *)(m_objFMP.GetDataMem())-1))
        {
            pLastDataBlock = pDataBlock;
            pDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + pDataBlock->iNextHandle;

            iRetVal = m_objFMP.ReleaseNode(pLastDataBlock);
            if (iRetVal != m_objFMP.SUCCESS)
            {
                return MEM_POOL_RELEASE_FAILED;
            }
        }

        m_iMapHandle = -1;

        return SUCCESS;
    }

    int InitWithHandle(int iMapHandle)
    {
        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((iMapHandle<0)||(iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        m_iMapHandle = iMapHandle;
        m_pHeadDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + m_iMapHandle;

        return SUCCESS;
    }

    int GetNode(KEYTYPE Key, DATATYPE **ppData)
    {
        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((m_iMapHandle<0)||(m_iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        DataBlock *pDataBlock = m_pHeadDataBlock;
        while(pDataBlock != ((DataBlock *)(m_objFMP.GetDataMem())-1))
        {
            for(int i=0; i<pDataBlock->iUsedDataNum; i++)
            {
                if (pDataBlock->astKey[i] == Key)
                {
                    *ppData = &pDataBlock->astData[i];
                    return SUCCESS;
                }
            }

            pDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + pDataBlock->iNextHandle;
        }

        return NODE_NOT_EXIST;
    }

    int GetAllNode(std::vector<DATATYPE> &vstAllData)
    {
        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((m_iMapHandle<0)||(m_iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        vstAllData.clear();

        DataBlock *pDataBlock = m_pHeadDataBlock;
        while(pDataBlock != ((DataBlock *)(m_objFMP.GetDataMem())-1))
        {
            for(int i=0; i<pDataBlock->iUsedDataNum; i++)
            {
                vstAllData.push_back(pDataBlock->astData[i]);
            }

            pDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + pDataBlock->iNextHandle;
        }

        return SUCCESS;
    }

    int InsertNode(KEYTYPE Key, const DATATYPE &Data)
    {
        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((m_iMapHandle<0)||(m_iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        //查询是否存在该数据
        DataBlock *pDataBlock = m_pHeadDataBlock;
        DataBlock *pLastDataBlock = NULL;
        while(pDataBlock != ((DataBlock *)(m_objFMP.GetDataMem())-1))
        {
            for(int i=0; i<pDataBlock->iUsedDataNum; i++)
            {
                if (pDataBlock->astKey[i] == Key)
                {
                    return NODE_EXIST;
                }
            }
            pLastDataBlock = pDataBlock;
            pDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + pDataBlock->iNextHandle;
        }

        //插入数据
        if (pLastDataBlock->iUsedDataNum == MAP_BLOCK_DATA_NUM)
        {
            //需要分配新的块
            DataBlock *pNewDataBlock = m_objFMP.AllocateNode();
            if (pNewDataBlock == NULL)
            {
                //分配节点失败
                return MEM_POOL_ALLOCATE_FAILED;
            }
            else
            {
                //分配节点成功
                memset(pNewDataBlock, 0x0, sizeof(DataBlock));
                pNewDataBlock->iNextHandle = -1;
                int iNewDataBlockHandle = pNewDataBlock - (DataBlock *)(m_objFMP.GetDataMem());
                pLastDataBlock->iNextHandle = iNewDataBlockHandle;
                pLastDataBlock = pNewDataBlock;
            }
        }

        int iDataNum = pLastDataBlock->iUsedDataNum;
        pLastDataBlock->astKey[iDataNum] = Key;
        pLastDataBlock->astData[iDataNum] = Data;
        pLastDataBlock->iUsedDataNum++;

        return SUCCESS;
    }

    int ShowMapInfo()
    {
        if (m_iFMPInitFlag != 1)
        {
            return MEM_POOL_NOT_INITED;
        }

        if ((m_iMapHandle<0)||(m_iMapHandle>=m_objFMP.GetNodeNum()))
        {
            return INVALID_MAP_HANDLE;
        }

        //查询是否存在该数据
        DataBlock *pDataBlock = m_pHeadDataBlock;

        printf("==MapInfo==\n");
        printf("Handle=%d\n", m_iMapHandle);
        while(pDataBlock != ((DataBlock *)(m_objFMP.GetDataMem())-1))
        {
            printf("[%d]->", pDataBlock->iUsedDataNum);
            pDataBlock = (DataBlock *)(m_objFMP.GetDataMem()) + pDataBlock->iNextHandle;
        }
        printf("NULL\n");
        printf("==MapInfoEnd==\n");

        return SUCCESS;
    }

    int GetMapInfo(int &iMemSize, int &iDataNum, int &iUsedDataNum)
    {
        iMemSize = m_objFMP.GetMemSize();
        iDataNum = m_objFMP.GetNodeNum();
        iUsedDataNum = m_objFMP.GetUsedNodeNum();

        return SUCCESS;
    }
    void ShowMemPool()
    {
        m_objFMP.Show();
        return;
    }
};

}
#endif
