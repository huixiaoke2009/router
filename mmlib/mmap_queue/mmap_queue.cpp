#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

#include "mmap_queue.h"

using namespace mmlib;

CMmapQueue::CMmapQueue()
{
    m_pMem = NULL;
    m_pHead = NULL;
    m_pobjSemLock = NULL;
    m_iQueueSize = 0;

    memset(m_szErrMsg, 0x0, sizeof(m_szErrMsg));
}

CMmapQueue::~CMmapQueue()
{
    if(NULL!=m_pobjSemLock)
    {
        delete m_pobjSemLock;
    }
}

int CMmapQueue::Init(const int iFileDes, const int aiSize)
{
    m_iQueueSize = aiSize;

    /// create sem.
    if(NULL == m_pobjSemLock)
    {
        m_pobjSemLock = new CSemLock();
    }
    int iRetVal = m_pobjSemLock->Init(IPC_PRIVATE);
    if (iRetVal != SUCCESS)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "sem_lock init failed, key=%d, ret=%d", IPC_PRIVATE, iRetVal);
        return E_MMAP_QUEUE_SEMLOCK;
    }

    /// create mmap.
    m_pMem = (char *)mmap (NULL, aiSize + sizeof(QueueHead) , PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON , -1 , 0); //MAP_ANON为匿名映射
    if (m_pMem == MAP_FAILED)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "mmap failed, errno=%d, errmsg=%s", errno, strerror(errno));
        return E_MMAP_QUEUE_MMAP;
    }

    /// first time create it.
    memset(m_pMem, 0x0, aiSize + sizeof(QueueHead));
    m_pHead = (QueueHead *) (m_pMem + aiSize);
    m_pHead->iLen = aiSize;
    m_pHead->iHead = 0;
    m_pHead->iTail = 0;
    m_pHead->iNum = 0;

    return SUCCESS;
}

int CMmapQueue::InQueue(char * pData, int iDataLen)
{
    int liRet = 0;
    char szTail[6];
    memset(szTail, 0xAA, sizeof(szTail));

    m_pobjSemLock->Lock();

    unsigned int liFreeSpace = 0;

    if (m_pHead->iHead < m_pHead->iTail || ((m_pHead->iHead == m_pHead->iTail) && (m_pHead->iNum == 0)))
    {
        liFreeSpace = m_pHead->iLen - (m_pHead->iTail - m_pHead->iHead);
    }
    else if (m_pHead->iHead > m_pHead->iTail)
    {
        liFreeSpace = m_pHead->iHead - m_pHead->iTail;
    }
    else
    {
        liFreeSpace = 0;
    }

    if (liFreeSpace < iDataLen + sizeof(BlockHead) + sizeof(szTail))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "mmap queue full");
        liRet = E_MMAP_QUEUE_FULL;
    }
    else
    {
        unsigned int liIndex = m_pHead->iTail;
        unsigned int liLeft = m_pHead->iLen - m_pHead->iTail;

        
        //       *****t********h***  head > tail
        //       *****h*******t****  left >= iDataLen + sizeof(BlockHead) + sizeof(szTail)
        if ((m_pHead->iHead > m_pHead->iTail) || (liLeft >= (iDataLen + sizeof(BlockHead) + sizeof(szTail))))
        {
            //// block.
            //liIndex = m_pHead->iTail;
            BlockHead stHead;
            stHead.iIndex = liIndex;
            stHead.iLen = iDataLen;
            memcpy(&(m_pMem[liIndex]), (char *) &stHead, sizeof(stHead));
            liIndex += sizeof(stHead);
            memcpy(&(m_pMem[liIndex]), pData, iDataLen);
            liIndex += iDataLen;
            memcpy(&(m_pMem[liIndex]), szTail, sizeof(szTail));

            m_pHead->iTail = (m_pHead->iTail + iDataLen + sizeof(BlockHead) + sizeof(szTail)) % m_pHead->iLen;
            m_pHead->iNum++;
        }
        
        //       *****h*******t**** 且 liLeft < (iDataLen + sizeof(BlockHead) + sizeof(szTail))
        else
        {
            /// split or block.
            BlockHead stHead;
            stHead.iIndex = liIndex;
            stHead.iLen = iDataLen;

            ///< set block_head.
            if (liLeft >= sizeof(BlockHead))
            {
                memcpy(&(m_pMem[liIndex]), (char *) &stHead, sizeof(stHead));
                liIndex = (liIndex + sizeof(stHead)) % m_pHead->iLen;

                liLeft = liLeft - sizeof(BlockHead);
            }
            else
            {
                memcpy(&(m_pMem[liIndex]), (char *) &stHead, liLeft);
                liIndex = 0;
                memcpy(&(m_pMem[liIndex]), ((char *) &stHead) + liLeft, sizeof(BlockHead) - liLeft);
                liIndex = (sizeof(BlockHead) - liLeft) % m_pHead->iLen;

                liLeft = m_pHead->iLen - liIndex;
            }

            /// set data.
            if ((int)liLeft >= iDataLen)
            {
                memcpy(&(m_pMem[liIndex]), pData, iDataLen);

                liIndex = (liIndex + iDataLen) % m_pHead->iLen;
                liLeft = liLeft - iDataLen;
            }
            else
            {
                memcpy(&(m_pMem[liIndex]), pData, liLeft);
                liIndex = 0;
                memcpy(&(m_pMem[liIndex]), pData + liLeft, iDataLen - liLeft);

                liIndex = (liIndex + iDataLen - liLeft) % m_pHead->iLen;
                liLeft = m_pHead->iLen - liIndex;
            }

            // set tail flag
            if (liLeft >= sizeof(szTail))
            {
                memcpy(&(m_pMem[liIndex]), szTail, sizeof(szTail));
            }
            else
            {
                memcpy(&(m_pMem[liIndex]), szTail, liLeft);
                liIndex = 0;
                memcpy(&(m_pMem[liIndex]), szTail + liLeft, sizeof(szTail) - liLeft);
            }

            m_pHead->iTail = (m_pHead->iTail + iDataLen + sizeof(BlockHead) + sizeof(szTail)) % m_pHead->iLen;
            m_pHead->iNum++;
        }
    }

    m_pobjSemLock->Release();

    return liRet;
}

int CMmapQueue::OutQueue(char * pBuf, int * iBufLen)
{
    int liRet = 0;

    m_pobjSemLock->Lock();

    if (m_pHead->iNum > 0 || m_pHead->iHead != m_pHead->iTail)
    {
        /// process.
        unsigned int liIndex = m_pHead->iHead;
        unsigned int liLeft = m_pHead->iLen - m_pHead->iHead;
        BlockHead stHead;
        memset(&stHead, 0x0, sizeof(stHead));

        if (liLeft < sizeof(BlockHead))
        {
            /// block head split.
            memcpy((char *) &stHead, &(m_pMem[liIndex]), liLeft);
            liIndex = 0;
            memcpy(((char *) &stHead) + liLeft, &(m_pMem[liIndex]), sizeof(BlockHead) - liLeft);
            liIndex = (liIndex + sizeof(BlockHead) - liLeft) % m_pHead->iLen;
        }
        else
        {
            memcpy(&stHead, &(m_pMem[liIndex]), sizeof(stHead));
            liIndex = (liIndex + sizeof(BlockHead)) % m_pHead->iLen;
        }

        // check
        if ((*iBufLen < stHead.iLen) || (stHead.iIndex != m_pHead->iHead) || (pBuf == NULL))
        {
            //mod by mama
            /*
            printf("buflen=%d, head len = %d, head idx = %d, head = %d, left = %d,"
                    " headsize=%lud, num = %u, searching next head ... \n",
                    *iBufLen, stHead.iLen, stHead.iIndex, m_pHead->iHead, liLeft, sizeof(BlockHead),
                    m_pHead->iNum);

            if (m_pHead->iNum > 0)
            {
                //searching flag data
                int iFlagDataNum = 0;
                while (1)
                {
                    if (m_pMem[liIndex] == TAIL_FLAG)
                    {
                        iFlagDataNum++;
                    }
                    else
                    {
                        iFlagDataNum = 0;
                    }

                    liIndex++;
                    liIndex = liIndex % m_pHead->iLen;

                    if (iFlagDataNum == 6)
                    {
                        m_pHead->iHead = liIndex;
                        m_pHead->iNum--;
                        printf("NOTICE: Relocate Head = %d, NUM = %d\n", liIndex, m_pHead->iNum);
                        break;
                    }
                }
            }
            else
            {
                //初始化
                m_pHead->iLen = m_iQueueSize;
                m_pHead->iHead = 0;
                m_pHead->iTail = 0;
                m_pHead->iNum = 0;

                printf("NOTICE: ALL Data has been Initialize to ZERO ... \n");
            }

            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "mmap queue empty");
            liRet = E_MMAP_QUEUE_EMPTY;
            */
            //初始化
            m_pHead->iLen = m_iQueueSize;
            m_pHead->iHead = 0;
            m_pHead->iTail = 0;
            m_pHead->iNum = 0;
            printf("NOTICE: ALL Data has been Initialize to ZERO ... \n");
            return E_MMAP_QUEUE_ERROR;
        }
        else
        {
            *iBufLen = stHead.iLen;

            ///set data.
            liLeft = m_pHead->iLen - liIndex;
            if (liLeft < stHead.iLen)
            {
                memcpy(pBuf, &(m_pMem[liIndex]), liLeft);
                liIndex = 0;
                memcpy(pBuf + liLeft, &(m_pMem[liIndex]), stHead.iLen - liLeft);

                liIndex = (liIndex + stHead.iLen - liLeft) % m_pHead->iLen;
                liLeft = m_pHead->iLen - liIndex;
            }
            else
            {
                memcpy(pBuf, &(m_pMem[liIndex]), stHead.iLen);

                liIndex = (liIndex + stHead.iLen) % m_pHead->iLen;
                liLeft = liLeft - stHead.iLen;
            }

            // verify tail
            char szTail[6];
            memset(szTail, 0xAA, sizeof(szTail));
            char szGetData[6];
            memset(szGetData, 0x0, sizeof(szGetData));

            if (liLeft >= sizeof(szGetData))
            {
                memcpy(szGetData, &(m_pMem[liIndex]), sizeof(szGetData));
            }
            else
            {
                memcpy(szGetData, &(m_pMem[liIndex]), liLeft);
                liIndex = 0;
                memcpy(szGetData + liLeft, &(m_pMem[liIndex]), sizeof(szGetData) - liLeft);
            }

            m_pHead->iHead = (m_pHead->iHead + stHead.iLen + sizeof(BlockHead) + sizeof(szTail)) % m_pHead->iLen;
            m_pHead->iNum--;

            if (memcmp(szGetData, szTail, sizeof(szTail)) != 0)
            {
                printf("ERR: Flag not match, FLAG:%d|%d|%d|%d|%d|%d, tail:%d|%d|%d|%d|%d|%d\n", szGetData[0], szGetData[1], szGetData[2], szGetData[3], szGetData[4], szGetData[5], szTail[0], szTail[1], szTail[2], szTail[3], szTail[4], szTail[5]);
                liRet = ERROR;
            }
        }
    }
    else
    {
        liRet = E_MMAP_QUEUE_EMPTY;
    }

    m_pobjSemLock->Release();

    return liRet;
}

int CMmapQueue::GetNum()
{
    int liNum = 0;

    m_pobjSemLock->Lock();

    liNum = m_pHead->iNum;

    m_pobjSemLock->Release();

    return liNum;
}

const CMmapQueue::QueueHead * CMmapQueue::GetHead()
{
    return m_pHead;
}

