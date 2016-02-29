#ifndef _MMAP_FILE_H_
#define _MMAP_FILE_H_

#include <stddef.h>

namespace mmlib
{
class CMMapFile
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_MMAP_FILE_OPEN = -301;
    const static int E_MMAP_FILE_CREATE = -302;
    const static int E_MMAP_FILE_STAT = -303;
    const static int E_MMAP_FILE_SIZE = -304;
    const static int E_MMAP_MAP_FAILED = -305;
    const static int E_MMAP_FILE_WRITE = -306;
    const static int E_MMAP_FILE_SEEK = -307;
    const static int E_MMAP_UNMAP_FAILED = -308;
    const static int E_MMAP_FLUSH = -309;

public:
    const static int FILE_NAME_LEN_MAX = 1024;

    CMMapFile();
    virtual ~CMMapFile();

    /*
     * summary:创建共享内存文件映射，使用指定的文件和大小，指定文件必须存在，否则返回错误
     *         默认：权限-读写，模式-共享模式，回写方式-异步，只有回写方式可以通过ChangeSyncMode修改；
     * param:pszFileName:文件路径和名称
     * param:iFileSize:文件大小
     * return:SUCCESS表示成功，否则表示失败
     */
    int Open(const char *pszFileName, unsigned long long ullFileSize);

    /*
     * summary:创建共享内存文件映射，使用指定的文件和大小，如果文件已存在，使用现有文件，否则创建新文件
     *         打开旧文件时，原有的数据将保留，不足的部分，用0补足
     *         默认：权限-读写，模式-共享模式，回写方式-异步，只有回写方式可以通过ChangeSyncMode修改；
     * param:pszFileName:文件路径和名称
     * param:iFileSize:文件大小
     * return:SUCCESS表示成功，否则表示失败
     */
    int Create(const char *pszFileName, unsigned long long ullFileSize);

    /*
     * summary:删除共享内存文件映射，不影响文件数据
     * return:SUCCESS表示成功，否则表示失败
     */
    int Destory();

    /*
     * summary:将共享内存中指定的数据块同步到文件
     * return:SUCCESS表示成功，否则表示失败
     */
    int Flush(void *pvMem, size_t Size);

    /*
     * summary:获取内存指针
     * return:如果没有初始化，返回NULL
     */
    void *GetMem()
    {
        return m_pvMem;
    }

    /*
     * summary:获取文件描述符
     * return:如果没有初始化，返回-1
     */
    int GetFileDesc()
    {
        return m_iFileDesc;
    }

    /*
     * summary:获取文件大小
     * return:返回文件大小
     */
    unsigned long long GetFileSize()
    {
        return m_ullFileSize;
    }

    /*
     * summary:获取文件名
     * return:返回文件名指针
     */
    const char *GetFileName()
    {
        return m_szFileName;
    }

private:
    void *m_pvMem;
    int m_iFileDesc;
    char m_szFileName[FILE_NAME_LEN_MAX];
    unsigned long long m_ullFileSize;
};
}

#endif


