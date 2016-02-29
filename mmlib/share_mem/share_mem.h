#ifndef _SHARE_MEM_H_
#define _SHARE_MEM_H_

#include <sys/ipc.h>
#include <sys/shm.h>

namespace mmlib
{

class CShareMem
{
public:
    const static int SHM_EXIST = 1;

    const static int SUCCESS = 0;
    const static int ERROR = -1;


public:
    CShareMem();
    virtual ~CShareMem();

    int Get(key_t tKey,unsigned long long ullSize, int iMode = 0666);
    int Create(key_t tKey,unsigned long long ullSize,int iMode = 0666);
    int Attach();
    int Detach();
    int Remove();

    unsigned long long ShmSize();

    void *GetMem()
    {
        return m_pvMem;
    }

protected:
    key_t m_tShmKey;    //share memory key
    unsigned long long m_ullShmSize;        //share memory size
    int m_iShmId;        //share memory id
    void* m_pvMem;        //point to share memory
};

}
#endif


