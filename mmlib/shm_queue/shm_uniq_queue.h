#ifndef _SHM_UNIQ_QUEUE_H_
#define _SHM_UNIQ_QUEUE_H_

#include <map>
#include <string>
#include "shm_queue.h"

namespace mmlib
{

class CShmUniqQueue
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_SHM_QUEUE_EMPTY     = -605;
    const static int E_SHM_QUEUE_DATAEXIST = -701;

public:
    CShmUniqQueue();
    virtual ~CShmUniqQueue();

    int Init(const int aiShmKey, const int aiSize);

    // 插入数据
    int InQueue(const char * pData, int iDataLen, bool lock = true);

    // 获取数据，数据长度会保存在iBufLen
    int OutQueue(char * pBuf, int * iBufLen, bool lock = true);

    // 获取Queue中记录条数
    int GetNum();

    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }
    CShmQueue& GetShmQueue()
    {
        return  m_ShmQueue;
    }

    unsigned int UsedSpaceInternal() const;
    unsigned int TotalSpaceInternal() const;

protected:
    std::map<std::string, int> m_mQueueDataMap;
    CShmQueue m_ShmQueue;
    char m_szErrMsg[256];

};

}
#endif

