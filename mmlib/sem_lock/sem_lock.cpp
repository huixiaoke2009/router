#include <stdio.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sem_lock.h"

using namespace mmlib;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

CSemLock::CSemLock()
{
    m_iSemKey = 0;
    m_iSemID = 0;
    memset(m_szErrMsg, 0x0, sizeof(m_szErrMsg));
}

CSemLock::~CSemLock()
{
}

int CSemLock::Init(int iSemKey)
{
    m_iSemKey = iSemKey;
    m_iSemID = semget(m_iSemKey, 1, 0666 | IPC_CREAT );
    if (m_iSemID == -1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Open Semaphore failed! KEY=%d, ERRMSG:%s", m_iSemKey,
                strerror(errno));
        return E_SEM_LOCK_INIT;
    }

    semun arg;

    semid_ds stSemDs;
    arg.buf = &stSemDs;
    int iRetValue = semctl(m_iSemID, 0, IPC_STAT, arg);
    if (iRetValue == -1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Open Sempore failed! KEY=%d|%d, ERRMSG:%s", m_iSemID, 0, strerror(errno));
        return E_SEM_LOCK_INIT;
    }

    //如果5分钟内没有操作，信号量将被释放
    if ((stSemDs.sem_otime == 0) || ((stSemDs.sem_otime > 0) && (time(NULL) - stSemDs.sem_otime > 5 * 60)))
    {
        semun arg;
        arg.val = 1;
        int iRetValue = semctl(m_iSemID, 0, SETVAL, arg);
        if (iRetValue == -1)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Set Sempore failed! KEY=%d, ERRMSG:%s", m_iSemKey,
                    strerror(errno));
            return E_SEM_LOCK_INIT;
        }
    }

    return SUCCESS;
}

int CSemLock::Lock()
{
    struct sembuf stSemBuf;
    memset(&stSemBuf, 0x0, sizeof(stSemBuf));

    stSemBuf.sem_num = 0;
    stSemBuf.sem_flg = SEM_UNDO;
    stSemBuf.sem_op = -1;

    while (1)
    {
        int iRetValue = -1;
        iRetValue = semop(m_iSemID, &stSemBuf, 1);
        if (iRetValue != 0)
        {
            if (errno== EINTR)
            {
                continue;
            }

            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Acquire Semaphore failed! KEY: %d | %d, ERRMSG:%s",
                    m_iSemID, 0, strerror(errno));
            return E_SEM_LOCK_LOCK;
        }
        else
        {
            break;
        }
    }

    return SUCCESS;
}

int CSemLock::Release()
{
    struct sembuf stSemBuf;
    memset(&stSemBuf, 0x0, sizeof(stSemBuf));

    stSemBuf.sem_num = 0;
    stSemBuf.sem_flg = SEM_UNDO;
    stSemBuf.sem_op = 1;

    while (1)
    {
        int iRetValue = -1;
        iRetValue = semop(m_iSemID, &stSemBuf, 1);
        if (iRetValue != 0)
        {
            if (errno== EINTR)
            {
                continue;
            }

            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Release Semaphore failed! KEY: %d | %d, ERRMSG:%s",
                    m_iSemID, 0, strerror(errno));
            return E_SEM_LOCK_RELEASE;
        }
        else
        {
            break;
        }
    }

    return 0;
}

int CSemLock::State()
{
    semun arg;
    semid_ds stSemDs;
    arg.buf = &stSemDs;
    int iRetValue = semctl(m_iSemID, 0, GETVAL, arg);
    if (iRetValue == -1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Open Sempore failed! KEY=%d | %d, ERRMSG:%s", m_iSemID,
                0, strerror(errno));
        return E_SEM_LOCK_STATE;;
    }

    return iRetValue;
}

