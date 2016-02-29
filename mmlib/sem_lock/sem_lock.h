#ifndef _SEM_LOCK_H_
#define _SEM_LOCK_H_

namespace mmlib
{
class CSemLock
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_SEM_LOCK_INIT = -501;
    const static int E_SEM_LOCK_LOCK = -502;
    const static int E_SEM_LOCK_RELEASE = -503;
    const static int E_SEM_LOCK_STATE = -504;

public:
    CSemLock();
    ~CSemLock();

    int Init(int iSemKey);

    int Lock();
    int Release();
    int State();

    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }

private:
    int m_iSemKey;
    int m_iSemID;

    char m_szErrMsg[256];
};
}
#endif

