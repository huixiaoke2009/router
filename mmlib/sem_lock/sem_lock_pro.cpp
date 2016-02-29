#include "sem_lock_pro.h"

namespace mmlib
{

CSemLockPro::RCODE CSemLockPro::creat(const key_t key, const int nsems/*=1信号数量*/, const bool auto_get/*=true如果存在则自动获取*/)
{
    int iret(0);
    semun uin_sem;
    iret = semget(key, nsems, IPC_CREAT|IPC_EXCL|0666);
    if(-1 == iret)
    {
        if(EEXIST == errno)
        {
            //已存在
            if(auto_get)
            {
                return get(key);
            }
            return E_EXSIT;
        }
        snprintf(msg, sizeof(msg), "%s|semget err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
        return E_FAILD;
    }
    m_semid = iret;
    m_semnum = nsems;
    for(int i = 0;i < m_semnum; ++i)
    {
        uin_sem.val = 1;
        iret = semctl(m_semid, i, SETVAL, uin_sem);
        if(-1 == iret)
        {
            snprintf(msg, sizeof(msg), "%s|semctl set val err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
            return E_FAILD;
        }
    }
    return SUCCESS;
}

CSemLockPro::RCODE CSemLockPro::get(const key_t key)
{
    int iret(0);
    semun uin_sem;
    iret = semget(key, 0, 0);
    if(-1 == iret)
    {
        snprintf(msg, sizeof(msg), "%s|sem exsit but semget err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
        return E_FAILD;
    }
    m_semid = iret;
    struct semid_ds st_semds;
    uin_sem.buf = &st_semds;
    iret = semctl(m_semid, 0, IPC_STAT, uin_sem);
    if(-1 == iret)
    {
        snprintf(msg, sizeof(msg), "%s|semctl err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
        return E_FAILD;
    }
    m_semnum = st_semds.sem_nsems;
    return SUCCESS;
}

CSemLockPro::RCODE CSemLockPro::lock(const int semnum/*=0*/)
{
    if(semnum >= m_semnum)
    {
        snprintf(msg, sizeof(msg), "%s|semnum out of range",__PRETTY_FUNCTION__);
        return E_SEMNUM_ERR;
    }
    int iret(0);
    struct sembuf op_lock;
    op_lock.sem_num = semnum;
    op_lock.sem_op = -1;
    op_lock.sem_flg = SEM_UNDO;
    iret = semop(m_semid, &op_lock, 1);
    if(-1 == iret)
    {
        snprintf(msg, sizeof(msg), "%s|semop err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
        return E_FAILD;
    }
    return SUCCESS;
}

CSemLockPro::RCODE CSemLockPro::unlock(const int semnum/*=0*/)
{
    if(semnum >= m_semnum)
    {
        snprintf(msg, sizeof(msg), "%s|semnum out of range",__PRETTY_FUNCTION__);
        return E_SEMNUM_ERR;
    }
    int iret(0);
    struct sembuf op_unlock;
    op_unlock.sem_num = semnum;
    op_unlock.sem_op = 1;
    op_unlock.sem_flg = SEM_UNDO;
    iret = semop(m_semid, &op_unlock, 1);
    if(-1 == iret)
    {
        snprintf(msg, sizeof(msg), "%s|semop err|msg=%s",__PRETTY_FUNCTION__,strerror(errno));
        return E_FAILD;
    }
    return SUCCESS;
}

int CSemLockPro::semnum() const
{
    return m_semnum;
}

char *CSemLockPro::err_msg()
{
    return msg;
}

}
