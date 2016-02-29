#ifndef _FILE_LOCK_H_
#define _FILE_LOCK_H_

namespace mmlib
{

class CFileLock
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_FILE_LOCK_CREATE_FILE = -201;

public:
    const static int FILE_LOCK_READ = 1;
    const static int FILE_LOCK_WRITE = 2;

    CFileLock(void);
    ~CFileLock(void);

    int Init(const char *pszFileName);

    /**
     * @brief 加文件锁，如果加锁失败，将等待
     * @param iOffSet 加锁的偏移量
     * @param iSize 加锁的区域大小
     * @param iWaitFlag 加锁失败是否等待，默认为等待
     *
     * @return 0-成功 其他-失败
     */
    int Lock(int iType, int iOffSet, int iSize, int iWaitFlag = 1);

    /**
     * @brief 解除文件锁
     * @param iOffSet 加锁的偏移量
     * @param iSize 加锁的区域大小
     *
     * @return 0-成功 其他-失败
     */
    int UnLock(int iOffSet, int iSize);

private:
    int m_iFile;

};

}

#endif
