#ifndef _MMAP_QUEUE_H_
#define _MMAP_QUEUE_H_

#include "sem_lock/sem_lock.h"

namespace mmlib
{

//在链表尾部加入识别码，当出现数据损坏时靠这个识别下一条记录

class CMmapQueue
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_MMAP_QUEUE_SEMLOCK = -601;
    const static int E_MMAP_QUEUE_MMAP = -602;
    const static int E_MMAP_QUEUE_ERROR = -603;
    const static int E_MMAP_QUEUE_FULL = -604;
    const static int E_MMAP_QUEUE_EMPTY = -605;

    const static char TAIL_FLAG = 0xAA;

    typedef struct tagQueueHead
    {
        unsigned int iLen;
        unsigned int iHead;
        unsigned int iTail;
        unsigned int iNum;
    } QueueHead;

    typedef struct tagBlockHead
    {
        unsigned int iIndex;
        unsigned short iLen;
    } BlockHead;

public:
    CMmapQueue();
    virtual ~CMmapQueue();

    int Init(const int aiShmKey, const int aiSize);

    // 插入数据
    int InQueue(char * pData, int iDataLen);

    // 获取数据，数据长度会保存在iBufLen
    int OutQueue(char * pBuf, int * iBufLen);

    // 获取Queue中记录条数
    int GetNum();

    const QueueHead * GetHead();

    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }

protected:
    char *m_pMem;
    QueueHead * m_pHead;
    CSemLock * m_pobjSemLock;
    unsigned int m_iQueueSize;

    char m_szErrMsg[256];
};

}
#endif

