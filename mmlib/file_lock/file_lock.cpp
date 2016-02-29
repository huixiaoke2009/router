#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <stdio.h>

#include "file_lock.h"

using namespace mmlib;

CFileLock::CFileLock(void)
{
    m_iFile = -1;
}

CFileLock::~CFileLock(void)
{
    if (m_iFile != -1)
    {
        close(m_iFile);
    }
}

/*
 * @summary:通过文件名创建文件锁
 * @param:pszFileName:文件名
 * @return 0 success, !=0 failed
 */
int CFileLock::Init(const char *pszFileName)
{
    m_iFile = open(pszFileName, O_RDWR|O_CREAT, 00644);
    if (-1 == m_iFile)
    {
        return E_FILE_LOCK_CREATE_FILE;
    }

    return SUCCESS;
}

/*
 * @summary:对指定的区域加锁
 * @param:iType:锁的类型， FILE_LOCK_READ:读锁 other:写锁
 * @param:iOffSet:加锁的位置
 * @param:iSize:加锁的区域
 * @return 0 success, !=0 failed
 */
int CFileLock::Lock(int iType, int iOffSet, int iSize, int iWaitFlag /*= 1*/)
{
    int iRetVal = 0;
    struct flock lstFlock;
    memset(&lstFlock, 0, sizeof(lstFlock));

    if (FILE_LOCK_READ == iType)
    {
        lstFlock.l_type = F_RDLCK;
    }
    else
    {
        lstFlock.l_type = F_WRLCK;
    }

    lstFlock.l_whence = SEEK_SET;
    lstFlock.l_start = llabs(iOffSet);
    lstFlock.l_len = iSize;

    //printf("DEBUG:Lock|%d|%d|%d\n", iType, iOffSet, iSize);
    //TODO 对于中断的情况，需要处理
    if (iWaitFlag == 1)
    {
        while (true)
        {
            iRetVal = fcntl (m_iFile, F_SETLKW, &lstFlock);
            if (iRetVal != 0)
            {
                if (errno == EINTR)
                {
                    //被中断的操作，继续锁
                    //printf("fcntl lock interrupted\n");
                    continue;
                }
                else
                {
                    //锁失败
                    //printf("fcntl lock failed, errno=%d\n", errno);
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        iRetVal = fcntl (m_iFile, F_SETLK, &lstFlock);
    }

    if ( iRetVal != 0)
    {
        return iRetVal;
    }

    return SUCCESS;
}

/*
 * @summary:对指定的区域解锁锁
 * @param:iOffSet:解锁的位置
 * @param:iSize:解锁的区域
 * @return 0 success, !=0 failed
 */
int CFileLock::UnLock(int iOffSet, int iSize)
{
    int iRetVal = 0;
    struct flock lstFlock;
    memset (&lstFlock, 0, sizeof(lstFlock));

    lstFlock.l_type = F_UNLCK;
    lstFlock.l_whence = SEEK_SET;
    lstFlock.l_start = iOffSet;
    lstFlock.l_len = iSize;

    //printf("DEBUG:UnLock|%d|%d\n", iOffSet, iSize);
    while (true)
    {
        iRetVal = fcntl (m_iFile, F_SETLKW, &lstFlock);
        if (iRetVal != 0)
        {
            if (errno == EINTR)
            {
                //被中断的操作，继续解锁
                //printf("fcntl unlock interrupted\n");
                continue;
            }
            else
            {
                //解锁失败
                //printf("fcntl unlock failed, errno=%d\n", errno);
                break;
            }
        }
        else
        {
            break;
        }
    }
    if ( iRetVal != 0)
    {
        return ERROR;
    }

    return SUCCESS;
}
