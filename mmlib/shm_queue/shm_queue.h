/* *****************************************************************
 *                         -->
 *      head(pop)          tail(push)
 *        |#######used########|-------free------|
 *  |_____|___:_____|___:_____|_____|.....|_____|
 *   Head |len:data1|len:data2|     |     |dataN|
 *        |                                     |
 *        |_____________________________________|
 *                         <--
 *
 *  if(head == tail) 队列空
 *  if((tail+1)%size == head) 队列满
 *
 * *****************************************************************/

#ifndef SHM_QUEUE_H_
#define SHM_QUEUE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include "sem_lock/sem_lock_pro.h"
#include "share_mem/share_mem.h"
#include "shm_queue.h"

namespace mmlib
{

class CShmQueue
{
public:
    CShmQueue();
    virtual ~CShmQueue();

    const static int SUCCESS                 = 0;
    const static int ERROR                   = -1;
    const static int E_SHM_QUEUE_SEMINIT     = -601;
    const static int E_SHM_QUEUE_AT_SHM      = -602;
    const static int E_SHM_QUEUE_GET_SHM     = -603;
    const static int E_SHM_QUEUE_FULL        = -604;
    const static int E_SHM_QUEUE_EMPTY       = -605;
    const static int E_SHM_QUEUE_DATALEN     = -606;
    const static int E_SHM_QUEUE_INLOCK      = -607;
    const static int E_SHM_QUEUE_OUTLOCK     = -608;
    const static int E_SHM_QUEUE_PARAM       = -609;
    const static int E_SHM_QUEUE_MALLOC      = -610;
    const static int E_SHM_QUEUE_INQUEUE_LEN = -611;

    int Init(const int aiShmKey, const int aiSize);

    int InQueue(const char *pData, unsigned int uiDataLen, bool lock = true);

    int OutQueue(char *pData, int *iDataLen, bool lock = true);
    int OutQueue(char *pData, unsigned int *uiDataLen, bool lock = true);

    //这个接口减少一次内存拷贝，这个接口不加锁。一般只能在单进程出queue的情况下使用。使用之后一定要记得调用ReleaseMsg
    int MsgPeek(char **ppDate, unsigned int *iDataLen);
    void ReleaseMsg();

    int GetNum();

    int GetAllQueueData(std::vector<std::string> &vsAllData);

    void GetHeadTail(unsigned int *head, unsigned int *tail, unsigned int *len, unsigned int *data_len) const;

    void Clean();
    unsigned int GetQueueDataSize()
    {
         return m_QueueDataSize;
    }

    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }
    unsigned int UsedSpaceInternal() const;
    unsigned int TotalSpaceInternal() const;
protected:

    struct _QueueHead
    {
        volatile unsigned int uiHead;
        volatile unsigned int uiTail;
    };

    class _AutoUnlock
    {
    public:
        _AutoUnlock(CSemLockPro *pLock, int num):m_pLock(pLock),m_sem_num(num),m_auto_unlock(false){}
        void set_auto_unlock()
        {
            m_auto_unlock = true;
        }
        ~_AutoUnlock()
        {
            if(m_auto_unlock)
                m_pLock->unlock(m_sem_num);
        }
        int Lock()
        {
            return m_pLock->lock(m_sem_num);
        }
    private:
        CSemLockPro *m_pLock;
        int m_sem_num;
        bool m_auto_unlock;
    };

    inline char *ToP(const unsigned int offset) const;
    inline unsigned int FreeSpace(volatile unsigned int * const head, volatile unsigned int * const tail) const;
    inline unsigned int UsedSpace(volatile unsigned int * const head, volatile unsigned int * const tail) const;


    inline void AddHead(unsigned int size);
    inline void AddTail(unsigned int size);
    inline void AddOffset(volatile unsigned int *poffset, unsigned int size);

    inline void CopyDataInQueue(const char *pData, unsigned int uiDataLen, const unsigned int offset);
    inline int  CopyDataOutQueue(char *pData, unsigned int *uiDataLen, const unsigned int offset);

    unsigned int DataLen(const unsigned int offset) const;

    _QueueHead *m_pQueueHead;
    char *m_pQueueDataStart;
    char *m_pQueueDataEnd;
    unsigned int m_QueueDataSize;

    CShareMem m_objShm;
    CSemLockPro m_objQueueLock;
    char *m_pMsgPeekBuf;
    char m_szErrMsg[128];
};

}
#endif /* SHM_QUEUE_H_ */
