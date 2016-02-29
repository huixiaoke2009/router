#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "share_mem.h"

using namespace mmlib;

CShareMem::CShareMem()
{
    m_tShmKey = 0;
    m_ullShmSize = 0;
    m_iShmId = 0;
    m_pvMem = NULL;
}

CShareMem::~CShareMem()
{

}

int CShareMem::Create(key_t tKey, unsigned long long ullSize, int iMode)
{
    if(tKey != m_tShmKey)
    {
        Detach();
    }
    m_tShmKey = tKey;
    m_ullShmSize = ullSize;

    //create share mem
    if ((m_iShmId = shmget(m_tShmKey, m_ullShmSize, IPC_CREAT| IPC_EXCL | iMode)) < 0) //try to create
    {
        if (errno != EEXIST)
        {
            return ERROR;
        }
        //exist,get
        if ((m_iShmId = shmget(m_tShmKey, m_ullShmSize, iMode)) < 0)
        {
            return ERROR;
        }

        return SHM_EXIST;
    }
    else
    {
        return SUCCESS;
    }
}

int CShareMem::Get(key_t tKey, unsigned long long ullSize, int iMode)
{
    if(tKey != m_tShmKey)
    {
        Detach();
    }
    m_tShmKey = tKey;
    m_ullShmSize = ullSize;

    //exist,get
    if ((m_iShmId = shmget(tKey, ullSize, iMode)) < 0)
    {
        return ERROR;
    }

    //检查一下长度是否匹配，某些情况下长度不匹配将出现问题
    struct shmid_ds shmDs;
    int iret = shmctl(m_iShmId, IPC_STAT, &shmDs);
    if(0 != iret)
    {
        return ERROR;
    }
    if(shmDs.shm_segsz != m_ullShmSize)
    {
        return ERROR;
    }

    return SUCCESS;
}

int CShareMem::Attach()
{
    if(m_pvMem)
        return SUCCESS;

    if ((m_pvMem = shmat(m_iShmId, NULL, 0)) == (void *)-1)
    {
        m_pvMem = NULL;
        return ERROR;
    }

    return SUCCESS;
}

int CShareMem::Detach()
{
    if(!m_pvMem)
        return SUCCESS;
    if (shmdt(m_pvMem) != 0)
        return ERROR;
    m_pvMem = NULL;
    return SUCCESS;
}

int CShareMem::Remove()
{
    if (shmctl(m_iShmId, IPC_RMID, NULL) < 0)
        return ERROR;
    return SUCCESS;
}

unsigned long long CShareMem::ShmSize()
{
    return m_ullShmSize;
}

