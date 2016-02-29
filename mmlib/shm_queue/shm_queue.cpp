#include "shm_queue.h"

namespace mmlib
{

CShmQueue::CShmQueue()
{
    m_pMsgPeekBuf = NULL;
}

CShmQueue::~CShmQueue()
{
    m_objShm.Detach();
}

int CShmQueue::Init(const int aiShmKey, const int aiSize)
{
    unsigned long long size = abs(aiSize);
    int iret = m_objShm.Create((key_t)aiShmKey, size);
    if(CShareMem::SHM_EXIST == iret)
    {
        iret = m_objShm.Attach();
        if(CShareMem::SUCCESS != iret)
        {
            return E_SHM_QUEUE_AT_SHM;
        }
    }
    else if(CShareMem::SUCCESS != iret)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Create Shm Err|ret=%d", iret);
        return E_SHM_QUEUE_GET_SHM;
    }
    else
    {
        iret = m_objShm.Attach();
        if(CShareMem::SUCCESS != iret)
        {
            return E_SHM_QUEUE_AT_SHM;
        }
        m_pQueueHead = (_QueueHead*)(m_objShm.GetMem());
        m_pQueueHead->uiHead = 0;
        m_pQueueHead->uiTail = 0;
    }
    m_pQueueHead = (_QueueHead*)(m_objShm.GetMem());
    m_pQueueDataStart = (char*)m_pQueueHead + sizeof(_QueueHead);
    m_pQueueDataEnd = (char*)m_pQueueHead + m_objShm.ShmSize();
    m_QueueDataSize = m_pQueueDataEnd - m_pQueueDataStart;

    //初始化锁
    iret = m_objQueueLock.creat((key_t)aiShmKey, 2, true);
    if(CSemLockPro::SUCCESS != iret)
    {
        return E_SHM_QUEUE_SEMINIT;
    }
    return SUCCESS;
}


int CShmQueue::InQueue(const char *pData, unsigned int uiDataLen, bool lock)
{
    _AutoUnlock inqueue_lock(&m_objQueueLock, 0/*入queue用0号互斥锁*/);
    if(lock)
    {
        if(CSemLockPro::SUCCESS != inqueue_lock.Lock())
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "lock err");
            return E_SHM_QUEUE_INLOCK;
        }
        inqueue_lock.set_auto_unlock();
    }

    if(0 == uiDataLen)
    {
        return E_SHM_QUEUE_INQUEUE_LEN;
    }

    unsigned int node_size = uiDataLen+sizeof(unsigned int);
    if(node_size > FreeSpace(&(m_pQueueHead->uiHead), &(m_pQueueHead->uiTail)))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "shm full");
        return E_SHM_QUEUE_FULL;
    }
    CopyDataInQueue(pData, uiDataLen, m_pQueueHead->uiTail);
    AddTail(node_size); //入队列只操作Tail
    return SUCCESS;
}

int CShmQueue::OutQueue(char *pData, int *iDataLen, bool lock)
{
    unsigned int len(0);
    if(*iDataLen > 0)
    {
        len = static_cast<unsigned int>(*iDataLen);
        int iret = OutQueue(pData, &len, lock);
        if(0 != iret)
        {
            return iret;
        }
        *iDataLen = static_cast<int>(len);
        return SUCCESS;
    }
    return E_SHM_QUEUE_PARAM;
}

int CShmQueue::OutQueue(char *pData, unsigned int *uiDataLen, bool lock)
{
    _AutoUnlock outqueue_lock(&m_objQueueLock, 1/*出queue用1号互斥锁*/);
    if(lock)
    {
        if(CSemLockPro::SUCCESS != outqueue_lock.Lock())
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "lock err");
            return E_SHM_QUEUE_OUTLOCK;
        }
        outqueue_lock.set_auto_unlock();
    }

    unsigned int used_space = UsedSpace(&(m_pQueueHead->uiHead), &(m_pQueueHead->uiTail));
    if(used_space < sizeof(unsigned int))
    {
        return E_SHM_QUEUE_EMPTY;
    }
    int iret = CopyDataOutQueue(pData, uiDataLen, m_pQueueHead->uiHead);
    unsigned int need_add_head = *uiDataLen+sizeof(unsigned int);
#if 0 //TODO
    //如果出来的数据长度大于头尾指针计算出来的长度，说明数据有问题
    //虽然我觉得不可能发生，但是还是做个保护
    if(need_add_head > used_space)
    {
        need_add_head = used_space; //相当于清空了queue
    }
#endif

    if(E_SHM_QUEUE_DATALEN == iret)
    {
        //跳过这个数据，数据缓存太小
        AddHead(need_add_head);
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "buff to small");
        return iret;
    }
    else if(0 != iret)
    {
        //目前不会到这里
    }
    AddHead(need_add_head);
    return SUCCESS;
}

int CShmQueue::MsgPeek(char **ppDate, unsigned int *piDataLen)
{
    unsigned int used_space = UsedSpace(&(m_pQueueHead->uiHead), &(m_pQueueHead->uiTail));
    if(used_space < sizeof(unsigned int))
    {
        return E_SHM_QUEUE_EMPTY;
    }
    char *p_data_start(NULL);
    unsigned int end_used = m_QueueDataSize - m_pQueueHead->uiHead;

    if(end_used >= sizeof(unsigned int))
    {
        memcpy(piDataLen, ToP(m_pQueueHead->uiHead), sizeof(unsigned int));
//        printf("%s|%u|%u|%u|end_used=%u|piDataLen=%u\n", __func__, m_QueueDataSize, m_pQueueHead->uiHead,
//                m_pQueueHead->uiTail, end_used, *piDataLen);
        end_used -= sizeof(unsigned int);
        if(end_used == 0)
        {
            p_data_start = m_pQueueDataStart;
        }
        else
        {
            if(end_used < *piDataLen)
            {
                //此条消息不连续
                if(m_pMsgPeekBuf)
                    free(m_pMsgPeekBuf);
                m_pMsgPeekBuf = (char*)malloc(*piDataLen);
                if(!m_pMsgPeekBuf)
                    return E_SHM_QUEUE_MALLOC;
                unsigned int remain_size = *piDataLen - end_used;
//                printf("%s|%u|%u|%u|end_used=%u|piDataLen=%u|remain_size=%u\n", __func__, m_QueueDataSize, m_pQueueHead->uiHead,
//                        m_pQueueHead->uiTail, end_used, *piDataLen, remain_size);
                memcpy(m_pMsgPeekBuf, ToP(m_pQueueHead->uiHead)+sizeof(unsigned int), end_used);
                memcpy(m_pMsgPeekBuf+end_used, m_pQueueDataStart, remain_size);
                *ppDate = m_pMsgPeekBuf;
                return SUCCESS;
            }
            p_data_start = ToP(m_pQueueHead->uiHead) + sizeof(unsigned int);
        }
    }
    else
    {
        unsigned int remain_size = sizeof(unsigned int) - end_used;
        memcpy(piDataLen, ToP(m_pQueueHead->uiHead), end_used);
        memcpy(((char*)(piDataLen))+end_used, m_pQueueDataStart, remain_size);
        p_data_start = m_pQueueDataStart + remain_size;
    }
    *ppDate = p_data_start;
    return SUCCESS;
}

void CShmQueue::ReleaseMsg()
{
    unsigned int used_space = UsedSpace(&(m_pQueueHead->uiHead), &(m_pQueueHead->uiTail));
    if(used_space < sizeof(unsigned int))
    {
        return; //队列已经空了
    }
    unsigned int need_add_head = DataLen(m_pQueueHead->uiHead)+sizeof(unsigned int);
#if 0 //TODO
    if(need_add_head > used_space)
    {
        need_add_head = used_space; //相当于清空了queue
    }
#endif
    AddHead(need_add_head);
    if(m_pMsgPeekBuf)
    {
        free(m_pMsgPeekBuf);
        m_pMsgPeekBuf = NULL;
    }
}

int CShmQueue::GetNum()
{
    int num(0);
    unsigned int offset_head = m_pQueueHead->uiHead;
    while(UsedSpace(&(offset_head), &(m_pQueueHead->uiTail)) >= sizeof(unsigned int))
    {
        AddOffset(&offset_head, DataLen(offset_head) + sizeof(unsigned int));
        num++;
    }
    return num;
}

int CShmQueue::GetAllQueueData(std::vector<std::string> &vsAllData)
{
    //锁住，不允许入队列和出队列
    _AutoUnlock inqueue_lock(&m_objQueueLock, 0/*入queue用0号互斥锁*/);
    if(CSemLockPro::SUCCESS != inqueue_lock.Lock())
    {
        return E_SHM_QUEUE_INLOCK;
    }
    inqueue_lock.set_auto_unlock();
    _AutoUnlock outqueue_lock(&m_objQueueLock, 1/*出queue用1号互斥锁*/);
    if(CSemLockPro::SUCCESS != outqueue_lock.Lock())
    {
        return E_SHM_QUEUE_INLOCK;
    }
    outqueue_lock.set_auto_unlock();

    unsigned int offset_head = m_pQueueHead->uiHead;
    unsigned int len(0);
    std::string str;
    while(UsedSpace(&(offset_head), &(m_pQueueHead->uiTail)) >= sizeof(unsigned int))
    {
        str.clear();
        len = DataLen(offset_head);
        if((offset_head + sizeof(unsigned int) + len) <= m_QueueDataSize)
        {
            str.append(ToP(offset_head)+sizeof(unsigned int), len);
        }
        else if(offset_head + sizeof(unsigned int) < m_QueueDataSize)
        {
            unsigned int end_len = m_QueueDataSize - offset_head - sizeof(unsigned int);
            str.append(ToP(offset_head)+sizeof(unsigned int), end_len);
            str.append(m_pQueueDataStart, len - end_len);
        }
        else
        {
            unsigned int start_len = sizeof(unsigned int) - (m_QueueDataSize - offset_head);
            str.append(m_pQueueDataStart + start_len, len);
        }
        vsAllData.push_back(str);
        AddOffset(&offset_head, len + sizeof(unsigned int));
    }
    return SUCCESS;
}

void CShmQueue::GetHeadTail(unsigned int *head, unsigned int *tail, unsigned int *len, unsigned int *data_len) const
{
    *head = m_pQueueHead->uiHead;
    *tail = m_pQueueHead->uiTail;
    *len = m_QueueDataSize;
    *data_len = DataLen(m_pQueueHead->uiHead);
}

void CShmQueue::Clean()
{
    m_pQueueHead->uiHead = 0;
    m_pQueueHead->uiTail = 0;
}

char *CShmQueue::ToP(const unsigned int offset) const
{
    return (m_pQueueDataStart + offset);
}

unsigned int CShmQueue::FreeSpace(volatile unsigned int * const head, volatile unsigned int * const tail) const
{
    unsigned int uiTail = *tail;
    unsigned int uiHead = *head;
    if(uiTail >= uiHead)
    {
        return (m_QueueDataSize - (uiTail - uiHead) - 1);
    }
    return (uiHead - uiTail - 1);
}

unsigned int CShmQueue::UsedSpace(volatile unsigned int * const head, volatile unsigned int * const tail) const
{
    unsigned int uiTail = *tail;
    unsigned int uiHead = *head;
    if(uiTail >= uiHead)
    {
        return (uiTail - uiHead);
    }
    return (m_QueueDataSize - (uiHead - uiTail));
}

void CShmQueue::AddHead(unsigned int size)
{
    AddOffset(&m_pQueueHead->uiHead, size);
}

void CShmQueue::AddTail(unsigned int size)
{
    AddOffset(&m_pQueueHead->uiTail, size);
}

void CShmQueue::AddOffset(volatile unsigned int *poffset, unsigned int size)
{
    //原子操作
    unsigned int uiOldOffset = *poffset;
    unsigned int uiNewOffset = (uiOldOffset + size) % m_QueueDataSize;
    __sync_bool_compare_and_swap(poffset, uiOldOffset, uiNewOffset);
    //*poffset = (*poffset + size) % m_QueueDataSize;
}

void CShmQueue::CopyDataInQueue(const char *pData, unsigned int uiDataLen, const unsigned int offset)
{
    unsigned int end_free = m_QueueDataSize - offset;
    if(end_free >= sizeof(unsigned int))
    {
        memcpy(ToP(offset), &uiDataLen, sizeof(unsigned int));
        end_free -= sizeof(unsigned int);
        if(end_free >= uiDataLen)
        {
            memcpy(ToP(offset)+sizeof(unsigned int), pData, uiDataLen);
        }
        else if(end_free > 0)
        {
            memcpy(ToP(offset)+sizeof(unsigned int), pData, end_free);
            memcpy(m_pQueueDataStart, pData+end_free, uiDataLen-end_free);
        }
        else
        {
            memcpy(m_pQueueDataStart, pData, uiDataLen);
        }
    }
    else
    {
        unsigned int remain_size = sizeof(unsigned int)-end_free;
        memcpy(ToP(offset), &uiDataLen, end_free);
        memcpy(m_pQueueDataStart, ((char*)(&uiDataLen))+end_free, remain_size);
        memcpy(m_pQueueDataStart+remain_size, pData, uiDataLen);
    }
}

int CShmQueue::CopyDataOutQueue(char *pData, unsigned int *uiDataLen, const unsigned int offset)
{
    unsigned int len(0);
    char *p_data_start(NULL);
    unsigned int end_used = m_QueueDataSize - offset;
    if(end_used >= sizeof(unsigned int))
    {
        memcpy(&len, ToP(offset), sizeof(unsigned int));
        end_used -= sizeof(unsigned int);
        if(end_used == 0)
        {
            p_data_start = m_pQueueDataStart;
        }
        else
        {
            p_data_start = ToP(offset) + sizeof(unsigned int);
        }
    }
    else
    {
        unsigned int remain_size = sizeof(unsigned int) - end_used;
        memcpy(&len, ToP(offset), end_used);
        memcpy(((char*)(&len))+end_used, m_pQueueDataStart, remain_size);
        p_data_start = m_pQueueDataStart + remain_size;
        end_used = 0;
    }

    if(len > *uiDataLen)
    {
        //需要把Head指针向后移动，否则进程会一直取此条数据
        *uiDataLen = len;
        return E_SHM_QUEUE_DATALEN;
    }

    *uiDataLen = len;
    if(end_used >= len)
    {
        memcpy(pData, p_data_start, len);
    }
    else if(end_used > 0)
    {
        unsigned int remain_size = len - end_used;
        memcpy(pData, p_data_start, end_used);
        memcpy(pData+end_used, m_pQueueDataStart, remain_size);
    }
    else
    {
        memcpy(pData, p_data_start, len);
    }
    return SUCCESS;
}

unsigned int CShmQueue::DataLen(const unsigned int offset) const
{
    unsigned int len(0);
    unsigned int end_used = m_QueueDataSize - offset;
    if(end_used >= sizeof(unsigned int))
    {
        memcpy(&len, ToP(offset), sizeof(unsigned int));
    }
    else
    {
        unsigned int remain_size = sizeof(unsigned int) - end_used;
        memcpy(&len, ToP(offset), end_used);
        memcpy(((char*)(&len))+end_used, m_pQueueDataStart, remain_size);
    }
    return len;
}

unsigned int CShmQueue::UsedSpaceInternal() const
{
    unsigned int uiTail = m_pQueueHead->uiTail;
    unsigned int uiHead = m_pQueueHead->uiHead;
    if(uiTail >= uiHead)
    {
        return (uiTail - uiHead);
    }
    return (m_QueueDataSize - (uiHead - uiTail));

}

unsigned int CShmQueue::TotalSpaceInternal() const
{
    return m_QueueDataSize;
}

}
