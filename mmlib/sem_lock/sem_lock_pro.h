#include <cstring>
#include <cstdio>
#include <errno.h>
#include <sys/sem.h>
#include <sys/types.h>

#ifndef _SEM_LOCK_POR_H_
#define _SEM_LOCK_POR_H_

namespace mmlib
{

class CSemLockPro
{
public:
    enum RCODE
    {
        SUCCESS = 0, //操作成功
        E_FAILD = -1,
        E_EXSIT = 100, //已经存在
        E_SEMNUM_ERR, //信号量编号错误
    };
    CSemLockPro():m_semid(0),m_semnum(0)
    {
        msg[0] = 0;
    }
    virtual ~CSemLockPro(){}
    RCODE creat(const key_t key, const int nsems = 1/*信号数量*/, const bool auto_get = true/*如果存在则自动获取*/);
    RCODE get(const key_t key);
    RCODE lock(const int semnum = 0);
    RCODE unlock(const int semnum = 0);
    int semnum() const;
    char *err_msg();
private:
    //信号量集的ID
    int m_semid;
    //信号量集中的信号数量
    int m_semnum;
    //错误描述
    char msg[128];
};


union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

}
#endif /* _SEM_LOCK_POR_H_ */
