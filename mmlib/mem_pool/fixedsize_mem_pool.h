/*
 * @note     该模板类的存储空间可以由外部分配，内部根据数据类型的不同，将内存划分为小块进行管理
 *           该模板类内部没有锁的管理，需要外部进行管理
 *           存在下列四种锁类型：
 *           1-内存池头锁
 *           2-内存池空闲链表锁
 *           3-内存池使用标记位锁
 *           4-内存池数据节点锁
 *           对数据进行操作时，涉及到的锁如下：
 *           1-分配、回收节点：需要将内存池头锁、内存池空闲链表锁、内存池使用标记位锁全部加写锁
 *           2-外部访问数据：外部访问数据是直接通过指针进行访问的，外部要设法使各进程访问数据时不冲突
 *
 *           该类有一个结点未被使用
 */
#ifndef _FIXEDSIZE_MEM_POOL_H_
#define _FIXEDSIZE_MEM_POOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mmlib
{
template<class DATATYPE>
class CFixedsizeMemPool
{
    typedef struct tagHead
    {
        int iMemSize;
        int iNodeNum;
        int iUsedNodeNum;
        int iFreeHead;
        int iFreeTail;
    }Head;

public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_NODE_NUM = -101; //结点数不正确
    const static int E_MEM_SIZE = -102; //外部分配的内存空间不够使用
    const static int E_DATAP_ERR = -103;    //外部传入的数据指针非法
    const static int E_VERIFY_CYCLE_LIST_ERR = -104;    //校验时，检查空节点循环链表指针时出错
    const static int E_VERIFY_USEDNODE_NUM = -105;      //校验时，检查已经使用过的节点数时出错
    const static int E_VERIFY_USEDBIT_FLAG = -106;      //校验时，检查使用标记BIT位出错

private:
    Head *m_pstHead;        //用于记录头节点
    int *m_piFreeList;      //用于记录可用节点的循环列表
    unsigned char *m_pucUsedBit;      //用于记录内存块是否被使用的标记,每个内存块使用一个bit位
    DATATYPE *m_pDataBuff;  //用于存放数据的内存块

    void *m_pvMem;          //用于记录MemPool的内存地址

    int m_iGetNextPos;  //用于记录GetNextNode()函数调用的位置

public:

    CFixedsizeMemPool()
    {
        m_pstHead = NULL;
        m_piFreeList = NULL;
        m_pucUsedBit = NULL;
        m_pDataBuff = NULL;

        m_iGetNextPos = 0;
    }

    ~CFixedsizeMemPool()
    {
    }
    
    /* add by marszhang
     * @summary:计算节点数为iNodeNum时，所需要的内存大小。免得调用init的时候不知道需要多少内存。
     * @param:iNodeNum:数据节点的数量
     * @return:内存大小（字节）
     */
     static int CalculateMemSize(int iNodeNum)
     {
        return (int)((sizeof(Head))+(sizeof(int)*iNodeNum)+(sizeof(DATATYPE)*iNodeNum)+((iNodeNum+7)/8));
     }
    
    /*
     * @summary:创建一个节点数为iNodeNum的内存池
     * @param:pvMem:预分配的内存地址
     * @param:iMemSize:预分配的内存空间
     * @param:iNodeNum:数据节点的数量
     * @param:iClearFlag:清楚数据标志位，如果该位为1，表示清楚内存池的所有数据
     * @return:0 success, -1 error
     */
    int Init(void *pvMem, int iMemSize, int iNodeNum, int iClearFlag = 0)
    {
        if (iNodeNum < 0)
        {
            return E_NODE_NUM;
        }
        else if (iNodeNum == 0)
        {
            iNodeNum = (iMemSize - sizeof(Head))/(sizeof(int) + sizeof(DATATYPE) + sizeof(char));   //浪费了一点点内存 :)
        }

        if (pvMem == NULL)
        {
            //指针为空
            return E_MEM_SIZE;
        }

        if (iMemSize < (int)((sizeof(Head))+(sizeof(int)*iNodeNum)+(sizeof(DATATYPE)*iNodeNum)+((iNodeNum+7)/8)))
        {
            //内存不够
            return E_MEM_SIZE;
        }

        m_pvMem = pvMem;

        //头节点空间
        m_pstHead = (Head *)pvMem;

        //分配空节点列表空间
        m_piFreeList = (int *)((char *)m_pstHead + sizeof(Head));

        //分配内存块使用标记空间
        m_pucUsedBit = (unsigned char *)((char *)m_piFreeList + (sizeof(int)*iNodeNum));

        //分配数据空间
        m_pDataBuff = (DATATYPE *)((char *)m_pucUsedBit + ((iNodeNum+7)/8));

        if(iClearFlag == 1)
        {
            memset(m_pstHead, 0x0, sizeof(Head));
            memset(m_piFreeList, 0x0, sizeof(int)*iNodeNum);
            memset(m_pucUsedBit, 0x0, (iNodeNum+7)/8);
            memset(m_pDataBuff, 0x0, sizeof(DATATYPE)*iNodeNum);

            for(int i=0; i<iNodeNum; i++)
            {
                m_piFreeList[i] = i;
            }

            m_pstHead->iMemSize = iMemSize;
            m_pstHead->iNodeNum = iNodeNum;
            m_pstHead->iUsedNodeNum = 0;
            m_pstHead->iFreeHead = 0;
            m_pstHead->iFreeTail = iNodeNum - 1;
        }
        else
        {
            //内存大小不一致
            if (m_pstHead->iMemSize != iMemSize)
            {
                return E_MEM_SIZE;
            }

            //节点数不一致
            if (m_pstHead->iNodeNum != iNodeNum)
            {
                return E_NODE_NUM;
            }
        }

        return 0;
    }

    int Verify()
    {
        //检查FreeList循环链表中的节点指针是否有效
        for (int i=0; i<m_pstHead->iNodeNum; i++)
        {
            if (m_piFreeList[i] >= m_pstHead->iNodeNum)
            {
                return E_VERIFY_CYCLE_LIST_ERR;
            }
        }

        if ((m_pstHead->iNodeNum - ((((m_pstHead->iFreeTail + m_pstHead->iNodeNum) - m_pstHead->iFreeHead)%m_pstHead->iNodeNum) + 1)) != m_pstHead->iUsedNodeNum)
        {
            printf("m_pstHead->iNodeNum = %d, m_pstHead->iFreeTail=%d, m_pstHead->iFreeHead=%d, m_pstHead->iUsedNodeNum=%d, Must=%d\n", m_pstHead->iNodeNum, m_pstHead->iFreeTail, m_pstHead->iFreeHead, m_pstHead->iUsedNodeNum, (m_pstHead->iNodeNum - ((((m_pstHead->iFreeTail + m_pstHead->iNodeNum) - m_pstHead->iFreeHead)%m_pstHead->iNodeNum) + 1)));
            return E_VERIFY_USEDNODE_NUM;
        }

        int iUsedNumFromBitFlag = 0;
        for(int i=0; i<m_pstHead->iNodeNum; i++)
        {
            if ((m_pucUsedBit[i/8]&(1<<(i%8))) != 0)
            {
                //该节点被使用过
                iUsedNumFromBitFlag++;
            }
        }

        if (iUsedNumFromBitFlag != m_pstHead->iUsedNodeNum)
        {
            printf("iUsedNumFromBitFlag=%d m_pstHead->iUsedNodeNum=%d\n", iUsedNumFromBitFlag, m_pstHead->iUsedNodeNum);
            return E_VERIFY_USEDBIT_FLAG;
        }

        return SUCCESS;
    }

    /*
     * @summary:从内存池中分配一个节点
     * @return:NULL failed, !=NULL OK
     */
    DATATYPE *AllocateNode()
    {
        DATATYPE *pFreeNode = NULL;

        if (((m_pstHead->iFreeHead+1)%m_pstHead->iNodeNum) == m_pstHead->iFreeTail)
        {
            //没有空间可以使用了
            pFreeNode = NULL;
        }
        else
        {
            pFreeNode = &m_pDataBuff[m_piFreeList[m_pstHead->iFreeHead]];
            int iMaskBit = m_piFreeList[m_pstHead->iFreeHead]%8;
            //将该位置为已经被使用状态
            m_pucUsedBit[m_piFreeList[m_pstHead->iFreeHead]/8]|=(1<<iMaskBit);
            m_pstHead->iFreeHead = (m_pstHead->iFreeHead+1)%m_pstHead->iNodeNum;
            m_pstHead->iUsedNodeNum++;
        }

        return pFreeNode;
    }

    /*
     * @summary:释放内存池中的一个节点
     * @return:0 success, -1 error
     */
    int ReleaseNode(DATATYPE *pData)
    {
        if ((pData < m_pDataBuff)
                ||(pData > (m_pDataBuff + m_pstHead->iNodeNum))
                ||((((char *)pData-(char *)m_pDataBuff)%sizeof(DATATYPE))!= 0)
                )
        {
            //非内存池空间中的数据,或者指针不在数据块的起点位置
            return E_DATAP_ERR;
        }

        int iMaskPos = ((char *)pData-(char *)m_pDataBuff)/sizeof(DATATYPE);
        int iMaskBit = iMaskPos%8;
        if ((m_pucUsedBit[iMaskPos/8]&(1<<iMaskBit)) == 0)
        {
            //该节点是空节点，不应该被回收，不过多回收一次不做任何操作
            return 0;
        }
        else
        {
            m_piFreeList[m_pstHead->iFreeTail] = iMaskPos;
            //该节点标记为已经释放
            m_pucUsedBit[iMaskPos/8]&=(~(1<<iMaskBit));
            m_pstHead->iFreeTail = (m_pstHead->iFreeTail+1)%m_pstHead->iNodeNum;
            m_pstHead->iUsedNodeNum--;
        }

        return 0;
    }

    /*
     * @summary:从内存池中获取下一个被使用的节点
     *          第一次调用时返回第一个被使用的节点
     *          最后一次调用返回NULL
     *          该函数用于遍历所有被使用的节点
     * @param:iFirstNodeFlag:标识是否从第一个节点开始获取 0-不是 1-是
     * @return:NULL failed, !=NULL OK
     */
    DATATYPE *GetNextNode(int iFirstNodeFlag = 0)
    {
        DATATYPE *pRetData = NULL;

        if(iFirstNodeFlag != 0)
        {
            m_iGetNextPos = 0;
        }

        for (int i=m_iGetNextPos; i< m_pstHead->iNodeNum; i++)
        {
            if ((m_pucUsedBit[i/8]&(1<<(i%8))) != 0)    // 1
            {
                pRetData=m_pDataBuff+i;
                m_iGetNextPos = i+1;    // 2
                break;
            }
        }

        return pRetData;
    }

    void *GetMem()
    {
        return m_pvMem;
    }

    void *GetDataMem()
    {
        return m_pDataBuff;
    }

    /**
     * @brief 校验数据指针是否有效
     */
    int VerifyData(DATATYPE *pData)
    {
        int iMemOffSet = ((char *)pData - (char *)m_pvMem);
        int iDataOffSet = iMemOffSet - sizeof(Head) - (sizeof(int)*(m_pstHead->iNodeNum) - ((m_pstHead->iNodeNum+7)/8));

        if ((iDataOffSet%sizeof(DATATYPE)) == 0)
        {
            return SUCCESS;
        }

        return ERROR;
    }

    int GetMemSize()
    {
        return m_pstHead->iMemSize;
    }

    int GetNodeNum()
    {
        return m_pstHead->iNodeNum;
    }

    int GetUsedNodeNum()
    {
        return m_pstHead->iUsedNodeNum;
    }

    void Show()
    {
        printf("==Head==\n");
        printf("MemSize=%d\nNodeNum=%d\nUsedNodeNum=%d\nFreeHead=%d\nFreeTail=%d\n",
                m_pstHead->iMemSize,
                m_pstHead->iNodeNum,
                m_pstHead->iUsedNodeNum,
                m_pstHead->iFreeHead,
                m_pstHead->iFreeTail);
        printf("==HeadEnd==\n");


        printf("==FreeList==\n");
        for(int i=0;i<m_pstHead->iNodeNum;i++)
        {
            printf(" %02d", m_piFreeList[i]);
            if ((i%20)==19)
            {
                printf("\n");
            }
        }
        printf("\n==FreeListEnd==\n");

        printf("==UsedBit==\n");
        for (int i=0;i<m_pstHead->iNodeNum;i++)
        {
            printf("%d",((m_pucUsedBit[i/8]&(1<<(i%8)))==0)?0:1);
            if ((i%50)==49)
            {
                printf("\n");
            }
        }
        printf("\n==UsedBitEnd==\n");

    }
};

}
#endif /*_FIXEDSIZE_MEM_POOL_H_*/
