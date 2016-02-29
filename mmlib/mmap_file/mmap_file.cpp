#include "mmap_file.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>

using namespace mmlib;

CMMapFile::CMMapFile()
{
    m_pvMem = NULL;
    m_iFileDesc = -1;
    memset(m_szFileName, 0x0, sizeof(m_szFileName));
    m_ullFileSize = 0;
}

CMMapFile::~CMMapFile()
{
    //Flush();

    if (m_iFileDesc != -1)
    {
        close(m_iFileDesc);
    }

}

/*
 * summary:创建共享内存文件映射，使用指定的文件和大小，指定文件必须存在，否则返回错误
 *         默认：权限-读写，模式-共享模式，回写方式-异步，只有回写方式可以通过ChangeSyncMode修改；
 * param:pszFileName:文件路径和名称
 * param:iFileSize:文件大小
 * return:SUCCESS表示成功，否则表示失败
 */
int CMMapFile::Open(const char *pszFileName, unsigned long long ullFileSize)
{
    int iRetVal = 0;
    if ((NULL == pszFileName)||(ullFileSize <= 0))
    {
        return ERROR;
    }

    memset(m_szFileName, 0x0, sizeof(m_szFileName));
    strncpy(m_szFileName, pszFileName, sizeof(m_szFileName)-1);

    m_ullFileSize = ullFileSize;

    if (m_iFileDesc != -1)
    {
        close(m_iFileDesc);
    }

    m_iFileDesc = open(m_szFileName, O_RDWR);
    if (m_iFileDesc < 0)
    {
        return E_MMAP_FILE_OPEN;
    }

    struct stat stFileStat;
    iRetVal = fstat(m_iFileDesc, &stFileStat);
    if (iRetVal < 0)
    {
        return E_MMAP_FILE_STAT;
    }

    unsigned long long ullFileRealSize = stFileStat.st_size;

    if (ullFileRealSize < m_ullFileSize)
    {
        return E_MMAP_FILE_SIZE;
    }

    m_pvMem = mmap(0, m_ullFileSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_iFileDesc, 0);
    if (MAP_FAILED == m_pvMem)
    {
        return E_MMAP_MAP_FAILED;
    }

    return SUCCESS;
}

/*
 * summary:创建共享内存文件映射，使用指定的文件和大小，如果文件已存在，使用现有文件，否则创建新文件
 *         打开旧文件时，原有的数据将保留，不足的部分，用0补足
 *         默认：权限-读写，模式-共享模式，回写方式-异步，只有回写方式可以通过ChangeSyncMode修改；
 * param:pszFileName:文件路径和名称
 * param:ullFileSize:文件大小
 * return:SUCCESS表示成功，否则表示失败
 */
int CMMapFile::Create(const char *pszFileName, unsigned long long ullFileSize)
{
    int iRetVal = 0;
    if ((NULL == pszFileName)||(ullFileSize <= 0))
    {
        return ERROR;
    }

    memset(m_szFileName, 0x0, sizeof(m_szFileName));
    strncpy(m_szFileName, pszFileName, sizeof(m_szFileName)-1);

    m_ullFileSize = ullFileSize;

    if (m_iFileDesc != -1)
    {
        close(m_iFileDesc);
    }

    m_iFileDesc = open(pszFileName, O_RDWR|O_CREAT, 00644);
    if (m_iFileDesc < 0)
    {
        return E_MMAP_FILE_OPEN;
    }

    struct stat stFileStat;
    iRetVal = fstat(m_iFileDesc, &stFileStat);
    if (iRetVal < 0)
    {
        return E_MMAP_FILE_STAT;
    }

    unsigned long long ullFileRealSize = stFileStat.st_size;

    if (ullFileRealSize < m_ullFileSize)
    {
        //如果文件大小不够，保留原有数据，在后面填入0
        iRetVal = lseek(m_iFileDesc, 0, SEEK_END);
        if (iRetVal < 0)
        {
            return E_MMAP_FILE_SEEK;
        }

        char szTmpBuff[1024]={0};

        //填充数据
        while(ullFileRealSize < m_ullFileSize)
        {
            int iWriteSize = ((m_ullFileSize - ullFileRealSize)>1024)?1024:(m_ullFileSize - ullFileRealSize);
            iRetVal = write(m_iFileDesc, szTmpBuff, iWriteSize);
            if (iRetVal < 0)
            {
                return E_MMAP_FILE_WRITE;
            }
            ullFileRealSize += iRetVal;
        }
    }

    m_pvMem = mmap(0, m_ullFileSize, PROT_READ|PROT_WRITE, MAP_SHARED, m_iFileDesc, 0);
    if (MAP_FAILED == m_pvMem)
    {
        return E_MMAP_MAP_FAILED;
    }

    return SUCCESS;
}

/*
 * summary:删除共享内存文件映射，不影响文件数据
 * return:SUCCESS表示成功，否则表示失败
 */
int CMMapFile::Destory()
{
    int iRetVal = 0;
    iRetVal = munmap(m_pvMem, m_ullFileSize);
    if (iRetVal < 0)
    {
        return E_MMAP_UNMAP_FAILED;
    }

    return SUCCESS;
}

/*
 * summary:将共享内存数据同步到文件
 * return:SUCCESS表示成功，否则表示失败
 */
int CMMapFile::Flush(void *pvMem, size_t Size)
{
    int iRetVal = 0;

    iRetVal = msync(pvMem, Size, MS_SYNC);
    if (iRetVal < 0)
    {
        return E_MMAP_FLUSH;
    }

    return SUCCESS;
}

