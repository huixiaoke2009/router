#include <stdio.h>
#include "shm_uniq_queue.h"

using namespace mmlib;

CShmUniqQueue::CShmUniqQueue()
{}

CShmUniqQueue::~CShmUniqQueue()
{}

int CShmUniqQueue::Init(const int aiShmKey, const int aiSize)
{
    int Ret = 0;
    m_mQueueDataMap.clear();

    Ret = m_ShmQueue.Init(aiShmKey, aiSize);
    if (Ret != 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_ShmQueue.GetErrMsg());
        return Ret;
    }

    std::vector<std::string> vsAllData;
    Ret = m_ShmQueue.GetAllQueueData(vsAllData);
    if (Ret != 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_ShmQueue.GetErrMsg());
        return Ret;
    }

    printf("%s:%d:init get datanum=%u\n", __FUNCTION__, __LINE__, static_cast<unsigned int>(vsAllData.size()));
    for(unsigned int i=0; i<vsAllData.size(); i++)
    {
        m_mQueueDataMap.insert(std::pair<std::string, int>(vsAllData[i], 0));
    }

    return SUCCESS;
}

// 插入数据
int CShmUniqQueue::InQueue(const char * pData, int iDataLen, bool lock)
{
    int Ret = 0;

    if (pData == NULL || iDataLen <= 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "data is not valid, data_len=%d", iDataLen);
        return CShmQueue::E_SHM_QUEUE_DATALEN;
    }

    std::string TmpData;
    TmpData.assign(pData, iDataLen);

    std::map<std::string, int>::iterator pTmpDataInMap = m_mQueueDataMap.find(TmpData);
    if (pTmpDataInMap == m_mQueueDataMap.end())
    {
        Ret = m_ShmQueue.InQueue(pData, iDataLen, lock);
        if (Ret == 0)
        {
            m_mQueueDataMap.insert(std::pair<std::string, int>(TmpData, 0));
        }
        else
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_ShmQueue.GetErrMsg());
        }

        return Ret;
    }
    else
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "data exist");
        return E_SHM_QUEUE_DATAEXIST;
    }
}

// 获取数据，数据长度会保存在iBufLen
int CShmUniqQueue::OutQueue(char * pBuf, int * iBufLen, bool lock)
{
    int Ret = 0;

    Ret = m_ShmQueue.OutQueue(pBuf, iBufLen, lock);
    if (Ret == 0)
    {
        std::string TmpData;
        TmpData.assign(pBuf, *iBufLen);
        m_mQueueDataMap.erase(TmpData);
    }
    else if (m_ShmQueue.E_SHM_QUEUE_EMPTY == Ret)
    {
        return E_SHM_QUEUE_EMPTY;
    }
    else
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", m_ShmQueue.GetErrMsg());
    }

    return Ret;
}


// 获取Queue中记录条数
int CShmUniqQueue::GetNum()
{
    return m_ShmQueue.GetNum();
}

unsigned int CShmUniqQueue::UsedSpaceInternal() const
{
    return m_ShmQueue.UsedSpaceInternal();
}

unsigned int CShmUniqQueue::TotalSpaceInternal() const
{
    return m_ShmQueue.TotalSpaceInternal();
}

