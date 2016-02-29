#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h> ///< for pthread_*()
#include <stdlib.h>
#include "log/log.h"
#include "file_lock/file_lock.h"

using namespace mmlib;

CLog g_objLog;

#define SELF_LOG(iLogID, ullUserID, iLogLevel, szFormat, args...) \
{ \
    if (iLogLevel <= m_iSelfLogLevel) \
    { \
        SelfLog(iLogID, ullUserID, iLogLevel, szFormat, ##args); \
    } \
} while(false)

const char *CLog::GetLogHeadTime(time_t tSec, time_t tUsec)
{
    static char szGetLogHeadTimeRet[64] = {0};
    struct tm *stSecTime = localtime(&tSec); //转换时间格式
    int year = 1900 + stSecTime->tm_year; //从1900年开始的年数
    int month = stSecTime->tm_mon + 1; //从0开始的月数
    int day = stSecTime->tm_mday; //从1开始的天数
    int hour = stSecTime->tm_hour; //从0开始的小时数
    int min = stSecTime->tm_min; //从0开始的分钟数
    int sec = stSecTime->tm_sec; //从0开始的秒数

    snprintf(szGetLogHeadTimeRet, sizeof(szGetLogHeadTimeRet), "%04d-%02d-%02d %02d:%02d:%02d.%06ld", year, month, day, hour, min, sec, tUsec);
    return szGetLogHeadTimeRet;
}

const char *CLog::GetFileNameTime(time_t tSec)
{
    static char szGetFileNameTimeRet[64] = {0};
    struct tm *stSecTime = localtime(&tSec); //转换时间格式
    int year = 1900 + stSecTime->tm_year; //从1900年开始的年数
    int month = stSecTime->tm_mon + 1; //从0开始的月数
    int day = stSecTime->tm_mday; //从1开始的天数
    int hour = stSecTime->tm_hour; //从0开始的小时数

    snprintf(szGetFileNameTimeRet, sizeof(szGetFileNameTimeRet), "%04d%02d%02d%02d", year, month, day, hour);
    return szGetFileNameTimeRet;
}

void CLog::CheckDir(const char * szPath)
{
    if (NULL == szPath)
    {
        return;
    }

    umask(0);

    struct stat stBuf;
    if (lstat(szPath, &stBuf) < 0)
    {
        mkdir(szPath, 0777);
    }

    umask(0022);
}


void CLog::CheckDirAll(const char * szPath)
{
    if (NULL == szPath)
    {
        return;
    }
    struct stat stBuf;
    char szDir[256] = {0};

    umask(0);

    if (lstat(szPath, &stBuf) < 0)
    { //no such path, return
        mkdir(szPath, 0777);
        lstat(szPath, &stBuf);
    }

    if (!S_ISDIR(stBuf.st_mode))
    { //not a directory, return
        return;
    }

    //check sub dir
    snprintf(szDir, sizeof(szDir), "%s/info", szPath);
    if (lstat(szDir, &stBuf) < 0)
    { // sub directory not exist,create
        mkdir(szDir, 0777);
    }

    snprintf(szDir, sizeof(szDir), "%s/error", szPath);
    if (lstat(szDir, &stBuf) < 0)
    { //sub directory not exist,create
        mkdir(szDir, 0777);
    }

    snprintf(szDir, sizeof(szDir), "%s/debug", szPath);
    if (lstat(szDir, &stBuf) < 0)
    { //sub directory not exist,create
        mkdir(szDir, 0777);
    }

    snprintf(szDir, sizeof(szDir), "%s/trace", szPath);
    if (lstat(szDir, &stBuf) < 0)
    { //sub directory not exist,create
        mkdir(szDir, 0777);
    }

    snprintf(szDir, sizeof(szDir), "%s/warn", szPath);
    if (lstat(szDir, &stBuf) < 0)
    { //sub directory not exist,create
        mkdir(szDir, 0777);
    }

    umask(0022);
}

//检查文件是否超过最大的文件大小
//注意，在该函数内部不能使用SELF_LOG调用
int CLog::CheckFileSize(FILE *fpFile, int iFileSize)
{
    int iRetVal = 0;
    struct stat stFileStat;

    if (fpFile == NULL)
    {
        return -1;
    }

    int iFD = fileno(fpFile);
    if (iFD < 0)
    {
        return 0;
    }
    else
    {
        iRetVal = fstat(iFD, &stFileStat);
        if (iRetVal != 0)
        {
            return 0;
        }
        else
        {
            //log_api这边检测文件大小时，迟后与logsvr，防止两边同时检测到
            if (stFileStat.st_size > iFileSize)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
}

//注意，在该函数内部不能使用SELF_LOG调用
int CLog::RenameFile(const char *pszFileName, int iMaxFileNum)
{
    int iRetVal = 0;
    char szCurrFileName[1024] = {0};
    CFileLock objFileLock;

    iRetVal = objFileLock.Init(pszFileName);
    if (iRetVal != 0)
    {
        return -1;
    }

    iRetVal = objFileLock.Lock(objFileLock.FILE_LOCK_WRITE, 0, 0, 0);
    if (iRetVal != 0)
    {
        //其他进程正在进行文件更名操作
        return -2;
    }

    //进行更名操作
    if (iMaxFileNum <= 0)
    {
        iMaxFileNum = LOG_DEFAULT_MAX_FILE_NUM;
    }

    int iCurrFileNum = 0;
    for(int i=1; i<iMaxFileNum; i++)
    {
        snprintf(szCurrFileName, sizeof(szCurrFileName), "%s.%02d", pszFileName, i);
        iRetVal = access(szCurrFileName, F_OK);
        if (iRetVal == 0)
        {
            //指定的文件存在
            iCurrFileNum = i;
            continue;
        }
        else
        {
            //指定的文件不存在
            break;
        }
    }

    if (iCurrFileNum == (iMaxFileNum-1))
    {
        snprintf(szCurrFileName, sizeof(szCurrFileName), "%s.%02d", pszFileName, iCurrFileNum);
        unlink(szCurrFileName);
        iCurrFileNum--;
    }

    char szOldFileName[1024] = {0};
    char szNewFileName[1024] = {0};
    for(int i=iCurrFileNum; i>0; i--)
    {
        snprintf(szOldFileName, sizeof(szOldFileName), "%s.%02d", pszFileName, i);
        snprintf(szNewFileName, sizeof(szNewFileName), "%s.%02d", pszFileName, i+1);
        rename(szOldFileName, szNewFileName);
    }

    snprintf(szNewFileName, sizeof(szNewFileName), "%s.01", pszFileName);
    rename(pszFileName, szNewFileName);

    objFileLock.UnLock(0, 0);

    return 0;
}

//注意，在该函数内部不能使用SELF_LOG调用
FILE *CLog::OpenFile(const char *pszFileName, const char *pszMode)
{
    FILE *fpRetFile;

    umask(0);
    fpRetFile = fopen(pszFileName, pszMode);
    umask(0022);

    if(fpRetFile)
    {
        //关闭标准IO的缓冲
        setbuf(fpRetFile, NULL);
    }

    if (fpRetFile == NULL)
    {
        printf("Log open file failed, file=%s\n", pszFileName);
    }

    return fpRetFile;
}


CLog::CLog()
{
    m_iInitFlag = 0;

    m_pfTraceLogFile = NULL;
    m_pfErrorLogFile = NULL;
    m_pfDebugLogFile = NULL;
    m_pfInfoLogFile = NULL;
    m_pfWarnLogFile = NULL;

    m_iCurInfoLogTime = 0;
    m_iCurErrorLogTime = 0;
    m_iCurWarnLogTime = 0;
    m_iCurDebugLogTime = 0;
    m_iCurTraceLogTime = 0;

    m_tInfoLastSizeCheckTime = 0;
    m_tErrorLastSizeCheckTime = 0;
    m_tWarnLastSizeCheckTime = 0;
    m_tDebugLastSizeCheckTime = 0;
    m_tTraceLastSizeCheckTime = 0;

    m_iSelfLogLevel = 3;

    m_pfSelfLogFile = NULL;
    m_iSelfLogTime = 0;
    m_tSelfLogLastSizeCheckTime = 0;

    snprintf(SELFLOG_PATH, sizeof(SELFLOG_PATH), "/tmp/Log");

    m_iLocalLogFlag = 0;
    memset(m_szLocalLogPath, 0, sizeof(m_szLocalLogPath));
    m_iLocalLogLevel = 0;
}

CLog::~CLog()
{
    m_iInitFlag = 0;

    if (m_pfTraceLogFile != NULL)
    {
        fclose(m_pfTraceLogFile);
        m_pfTraceLogFile = NULL;
    }

    if (m_pfErrorLogFile != NULL)
    {
        fclose(m_pfErrorLogFile);
        m_pfErrorLogFile = NULL;
    }

    if (m_pfDebugLogFile != NULL)
    {
        fclose(m_pfDebugLogFile);
        m_pfDebugLogFile = NULL;
    }

    if (m_pfInfoLogFile != NULL)
    {
        fclose(m_pfInfoLogFile);
        m_pfInfoLogFile = NULL;
    }

    if (m_pfWarnLogFile != NULL)
    {
        fclose(m_pfWarnLogFile);
        m_pfWarnLogFile = NULL;
    }
}

void CLog::Open(const char *pszModuleName, int iLogLevel)
{
    int iRetVal = 0;
    m_objModuleName = pszModuleName;

    //检查是否设置了LOG_SELFLOG_LEVEL环境变量
    char *pszSelfLogLevel = NULL;
    pszSelfLogLevel = getenv("LOG_SELFLOG_LEVEL");

    if (pszSelfLogLevel != NULL)
    {
        m_iSelfLogLevel = LOG_TRACE;
        SELF_LOG(0, 0, LOG_INFO, "LOG_selflog_level=%s", pszSelfLogLevel);
        m_iSelfLogLevel = atoi(pszSelfLogLevel);
    }

    if (m_iSelfLogLevel > 0)
    {
        CheckDir(SELFLOG_PATH);
    }

    if (m_iInitFlag == 1)
    {
        //防止多次不断初始化
        return;
    }

    int LogQuerueKey = LOG_QUEUE_KEY;
    iRetVal = m_objLogQueue.Init(LogQuerueKey, LOG_QUEUE_SIZE);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init log_queue failed, key=%d, size=%d, ret=%d", LogQuerueKey, LOG_QUEUE_SIZE, iRetVal);
        return;
    }
    int LogConfShmKey = LOG_CONF_SHM_KEY;
    iRetVal = m_objLogConfShm.Create(LogConfShmKey, LOG_CONF_SHM_SIZE, 0666);
    if ((iRetVal != 0)&&(iRetVal != 1))
    {
        SELF_LOG(0, 0, LOG_ERR, "create conf_shm failed, key=%d, size=%d, ret=%d", LogConfShmKey, LOG_CONF_SHM_SIZE, iRetVal);
        return;
    }

    iRetVal = m_objLogConfShm.Attach();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "attach conf_shm failed, ret=%d", iRetVal);
        return;
    }

    m_objLogConf.m_pstGlobalConf = (LogOutputConf *)m_objLogConfShm.GetMem();
    int iModuleConfHashListSize = m_objLogConf.m_objModuleConf.CalcSize(LOG_MAX_SPEC_MODULE_NUM, LOG_MAX_SPEC_MODULE_NUM);
    int iLogIDConfHashListSize = m_objLogConf.m_objLogIDConf.CalcSize(LOG_MAX_SPEC_LOGID_NUM, LOG_MAX_SPEC_LOGID_NUM);
    int iUserIDConfHashListSize = m_objLogConf.m_objUserIDConf.CalcSize(LOG_MAX_SPEC_USERID_NUM, LOG_MAX_SPEC_USERID_NUM);

    if ((sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize + iUserIDConfHashListSize) > ((unsigned int)LOG_CONF_SHM_SIZE))
    {
        SELF_LOG(0, 0, LOG_ERR, "all_conf_shm_size=%d, allocated_size=%d", (sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize + iUserIDConfHashListSize), LOG_CONF_SHM_SIZE);
        return;
    }

    iRetVal = m_objLogConf.m_objModuleConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf), iModuleConfHashListSize, LOG_MAX_SPEC_MODULE_NUM, LOG_MAX_SPEC_MODULE_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init module_conf_hash_list failed, ret=%d", iRetVal);
        return;
    }
    iRetVal = m_objLogConf.m_objLogIDConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf) + iModuleConfHashListSize, iLogIDConfHashListSize, LOG_MAX_SPEC_LOGID_NUM, LOG_MAX_SPEC_LOGID_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init logid_conf_hash_list failed, ret=%d", iRetVal);
        return;
    }

    iRetVal = m_objLogConf.m_objUserIDConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize, iUserIDConfHashListSize, LOG_MAX_SPEC_USERID_NUM, LOG_MAX_SPEC_USERID_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init uin_conf_hash_list failed, ret=%d", iRetVal);
        m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = 0;
    }

    iRetVal = m_objLogConf.m_objModuleConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify module_conf_hash_list failed, ret=%d", iRetVal);
        m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = 0;
    }

    iRetVal = m_objLogConf.m_objLogIDConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify logid_conf_hash_list failed, ret=%d", iRetVal);
        m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = 0;
    }

    iRetVal = m_objLogConf.m_objUserIDConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify uin_conf_hash_list failed, ret=%d", iRetVal);
        m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = 0;
    }

    if (m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag != LOG_CONFSHM_INIT_FLAG)
    {
        SELF_LOG(0, 0, LOG_DEBUG, "log_conf_shm have not inited, flag=0x%08X", m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag);

        //TODO 这里的初始化需要重新考虑一下
        m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = LOG_CONFSHM_INIT_FLAG;
        m_objLogConf.m_pstGlobalConf->iOutputLogLevel = LOG_DEFAULT_LOG_LEVEL;
        m_objLogConf.m_pstGlobalConf->iOutputLogType = 1;
        snprintf(m_objLogConf.m_pstGlobalConf->szOutputPathDir, sizeof(m_objLogConf.m_pstGlobalConf->szOutputPathDir), LOG_DEFAULT_LOG_PATH);
        m_objLogConf.m_pstGlobalConf->iMaxFileSize = LOG_DEFAULT_MAX_FILE_SIZE;
        m_objLogConf.m_pstGlobalConf->iMaxFileNum = LOG_DEFAULT_MAX_FILE_NUM;
        m_objLogConf.m_pstGlobalConf->iFlushFlag = 1;

        iRetVal = m_objLogConf.m_objModuleConf.Clear();
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "clear module_conf_hash_list failed, ret=%d", iRetVal);
            return;
        }
        iRetVal = m_objLogConf.m_objLogIDConf.Clear();
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "clear logid_conf_hash_list failed, ret=%d", iRetVal);
            return;
        }

        iRetVal = m_objLogConf.m_objUserIDConf.Clear();
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "clear uin_conf_hash_list failed, ret=%d", iRetVal);
            return;
        }
    }

    if (m_objLogConf.m_pstGlobalConf->iMaxFileSize < LOG_DEFAULT_MIN_FILE_SIZE)
    {
        //防止出现老板本的LogSvr，新版本的LogAPI
        m_objLogConf.m_pstGlobalConf->iMaxFileSize = LOG_DEFAULT_MIN_FILE_SIZE;
        m_objLogConf.m_pstGlobalConf->iMaxFileNum = LOG_DEFAULT_MAX_FILE_NUM;
        SELF_LOG(0, 0, LOG_ERR, "global_conf/max_file_size is not valid, set to default %d", LOG_DEFAULT_MIN_FILE_SIZE);
    }

    iRetVal = pthread_mutex_init(&m_mtMutexCAO, NULL);
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init pthread_mutex failed, ret=%d", iRetVal);
        return;
    }

    char szLogFilePath[1024];
    snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%s", m_objLogConf.m_pstGlobalConf->szOutputPathDir, m_objModuleName.m_szModuleName);

    CheckDirAll(szLogFilePath);

    m_iInitFlag = 1;
    return;
}

int CLog::SetLocalFlag(int iLocalFlag, int iLogLevel, const char *pszLogPath)
{
    if (iLocalFlag ==  1)
    {
        m_iLocalLogFlag = 1;
        m_iLocalLogLevel = iLogLevel;
        snprintf(m_szLocalLogPath, sizeof(m_szLocalLogPath), "%s", pszLogPath);
    }
    else
    {
        m_iLocalLogFlag = 0;
        return 0;
    }

    CheckDirAll(m_szLocalLogPath);

    return 0;
}


void CLog::LogInternal(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszLogContent)
{
    int iRetVal = 0;

    if (m_iInitFlag != 1)
    {
        //没有初始化日志
        return;
    }

    if (!IsShouldWrite(iLogID, ullUserID, iPriority))
    {
        //不需要输出日志
        return;
    }

    LogHeader *pstLogHeader = (LogHeader *)m_szLogBuff;
    memset(pstLogHeader, 0x0, sizeof(LogHeader));

    struct timeval stTimeNow;
    gettimeofday(&stTimeNow, NULL);

    pstLogHeader->uiLogTimeSec = stTimeNow.tv_sec;
    pstLogHeader->uiLogTimeUSec = stTimeNow.tv_usec;
    pstLogHeader->iLogLevel = iPriority;
    pstLogHeader->iLogID = iLogID;
    pstLogHeader->iProcID = getpid();
    strncpy(pstLogHeader->szModuleName, m_objModuleName.m_szModuleName, sizeof(pstLogHeader->szModuleName)-1);
    pstLogHeader->ullUserID = ullUserID;

    int iLogBodyLen = 0;

    iLogBodyLen = snprintf(pstLogHeader->szLogContent, LOG_LENGTH_MAX, "%s", pszLogContent);

    SELF_LOG(0, ullUserID, LOG_TRACE, "%d|%d|LOG_CONTENT(%d)[%s]", iLogID, iPriority, iLogBodyLen, pstLogHeader->szLogContent);

    if (m_iLocalLogFlag != 1)
    {
        iRetVal = m_objLogQueue.InQueue(m_szLogBuff, iLogBodyLen + sizeof(LogHeader));
        if (iRetVal == m_objLogQueue.SUCCESS)
        {
            //日志入Queue成功
            return;
        }
    }

    //对于入Queue失败，或者配置了本地日志，尝试写入文件中
    char szLogHeaderBuff[256];
    int iLogHeadLen = 0;

    iLogHeadLen = snprintf(szLogHeaderBuff, sizeof(szLogHeaderBuff), "%s|%s(%d)|%d|%u|%u|%llu||||",
            GetLogHeadTime(pstLogHeader->uiLogTimeSec, pstLogHeader->uiLogTimeUSec),
            pstLogHeader->szModuleName,
            pstLogHeader->iProcID,
            pstLogHeader->iLogID,
            *((unsigned int *)&pstLogHeader->ullUserID),
            *((unsigned short *)( (char *)&pstLogHeader->ullUserID + sizeof(int))),
            pstLogHeader->ullUserID);

    SELF_LOG(0, ullUserID, LOG_TRACE, "%d|%d|LOG_HEADER(%d)[%s]", iLogID, iPriority, iLogHeadLen, szLogHeaderBuff);

    int iCurLogTime = atoi(GetFileNameTime(pstLogHeader->uiLogTimeSec));
    char szLogFileFullName[1024];
    char szLogFilePath[1024];
    int iMaxFileSize = m_objLogConf.m_pstGlobalConf->iMaxFileSize;
    int iMaxFileNum = m_objLogConf.m_pstGlobalConf->iMaxFileNum;
    if (m_iLocalLogFlag == 1)
    {
        snprintf(szLogFilePath, sizeof(szLogFilePath), "%s", m_szLocalLogPath);
        iMaxFileSize = SELFLOG_MAX_SIZE;
        iMaxFileNum = 10;
    }
    else
    {
        snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%s", m_objLogConf.m_pstGlobalConf->szOutputPathDir, m_objModuleName.m_szModuleName);
    }

    FILE *pFile;

    time_t tTimeNow = stTimeNow.tv_sec;
    try
    {
        pthread_mutex_lock(&m_mtMutexCAO);
        switch (iPriority)
        {
        case LOG_INFO:
            if (iCurLogTime != m_iCurInfoLogTime)
            {
                if (m_pfInfoLogFile != NULL)
                {
                    fclose(m_pfInfoLogFile);
                    m_pfInfoLogFile = NULL;
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/info/%d.log", szLogFilePath, iCurLogTime);
                m_pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                m_iCurInfoLogTime = iCurLogTime;
                m_tInfoLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - m_tInfoLastSizeCheckTime) < 0)||(tTimeNow - m_tInfoLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
            {
                if (CheckFileSize(m_pfInfoLogFile, iMaxFileSize) != 0)
                {
                    if (m_pfInfoLogFile != NULL)
                    {
                        fclose(m_pfInfoLogFile);
                        m_pfInfoLogFile = NULL;
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/info/%d.log", szLogFilePath, iCurLogTime);
                    m_pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(m_pfInfoLogFile, iMaxFileSize) != 0)
                    {
                        if (m_pfInfoLogFile != NULL)
                        {
                            fclose(m_pfInfoLogFile);
                            m_pfInfoLogFile = NULL;
                        }
                        RenameFile(szLogFileFullName, iMaxFileNum);
                        m_pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    m_iCurInfoLogTime = iCurLogTime;
                }
                m_tInfoLastSizeCheckTime = tTimeNow;
            }

            pFile = m_pfInfoLogFile;

            break;
        case LOG_ERR:
            if (iCurLogTime != m_iCurErrorLogTime)
            {
                if (m_pfErrorLogFile != NULL)
                {
                    fclose(m_pfErrorLogFile);
                    m_pfErrorLogFile = NULL;
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/error/%d.log", szLogFilePath, iCurLogTime);
                m_pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                m_iCurErrorLogTime = iCurLogTime;
                m_tErrorLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - m_tErrorLastSizeCheckTime) < 0)||(tTimeNow - m_tErrorLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
            {
                if (CheckFileSize(m_pfErrorLogFile, iMaxFileSize) != 0)
                {
                    if (m_pfErrorLogFile != NULL)
                    {
                        fclose(m_pfErrorLogFile);
                        m_pfErrorLogFile = NULL;
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/error/%d.log", szLogFilePath, iCurLogTime);
                    m_pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(m_pfErrorLogFile, iMaxFileSize) != 0)
                    {
                        if (m_pfErrorLogFile != NULL)
                        {
                            fclose(m_pfErrorLogFile);
                            m_pfErrorLogFile = NULL;
                        }
                        RenameFile(szLogFileFullName, iMaxFileNum);
                        m_pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    m_iCurErrorLogTime = iCurLogTime;
                }
                m_tErrorLastSizeCheckTime = tTimeNow;
            }
            pFile = m_pfErrorLogFile;

            break;
        case LOG_WARN:
            if (iCurLogTime != m_iCurWarnLogTime)
            {
                if (m_pfWarnLogFile != NULL)
                {
                    fclose(m_pfWarnLogFile);
                    m_pfWarnLogFile = NULL;
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/warn/%d.log", szLogFilePath, iCurLogTime);
                m_pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                m_iCurWarnLogTime = iCurLogTime;
                m_tWarnLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - m_tWarnLastSizeCheckTime) < 0)||(tTimeNow - m_tWarnLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
            {
                if (CheckFileSize(m_pfWarnLogFile, iMaxFileSize) != 0)
                {
                    if (m_pfWarnLogFile != NULL)
                    {
                        fclose(m_pfWarnLogFile);
                        m_pfWarnLogFile = NULL;
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/warn/%d.log", szLogFilePath, iCurLogTime);
                    m_pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(m_pfWarnLogFile, iMaxFileSize) != 0)
                    {
                        if (m_pfWarnLogFile != NULL)
                        {
                            fclose(m_pfWarnLogFile);
                            m_pfWarnLogFile = NULL;
                        }
                        RenameFile(szLogFileFullName, iMaxFileNum);
                        m_pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    m_iCurWarnLogTime = iCurLogTime;
                }
                m_tWarnLastSizeCheckTime = tTimeNow;
            }
            pFile = m_pfWarnLogFile;

            break;
        case LOG_DEBUG:
            if (iCurLogTime != m_iCurDebugLogTime)
            {
                if (m_pfDebugLogFile != NULL)
                {
                    fclose(m_pfDebugLogFile);
                    m_pfDebugLogFile = NULL;
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/debug/%d.log", szLogFilePath, iCurLogTime);
                m_pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                m_iCurDebugLogTime = iCurLogTime;
                m_tDebugLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - m_tDebugLastSizeCheckTime) < 0)||(tTimeNow - m_tDebugLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
            {
                if (CheckFileSize(m_pfDebugLogFile, iMaxFileSize) != 0)
                {
                    if (m_pfDebugLogFile != NULL)
                    {
                        fclose(m_pfDebugLogFile);
                        m_pfDebugLogFile = NULL;
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/debug/%d.log", szLogFilePath, iCurLogTime);
                    m_pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(m_pfDebugLogFile, iMaxFileSize) != 0)
                    {
                        if (m_pfDebugLogFile != NULL)
                        {
                            fclose(m_pfDebugLogFile);
                            m_pfDebugLogFile = NULL;
                        }
                        RenameFile(szLogFileFullName, iMaxFileNum);
                        m_pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    m_iCurDebugLogTime = iCurLogTime;
                }
                m_tDebugLastSizeCheckTime = tTimeNow;
            }
            pFile = m_pfDebugLogFile;

            break;
        case LOG_TRACE:
            if (iCurLogTime != m_iCurTraceLogTime)
            {
                if (m_pfTraceLogFile != NULL)
                {
                    fclose(m_pfTraceLogFile);
                    m_pfTraceLogFile = NULL;
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/trace/%d.log", szLogFilePath, iCurLogTime);
                m_pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                m_iCurTraceLogTime = iCurLogTime;
                m_tTraceLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - m_tTraceLastSizeCheckTime) < 0)||(tTimeNow - m_tTraceLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
            {
                if (CheckFileSize(m_pfTraceLogFile, iMaxFileSize) != 0)
                {
                    if (m_pfTraceLogFile != NULL)
                    {
                        fclose(m_pfTraceLogFile);
                        m_pfTraceLogFile = NULL;
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/trace/%d.log", szLogFilePath, iCurLogTime);
                    m_pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(m_pfTraceLogFile, iMaxFileSize) != 0)
                    {
                        if (m_pfTraceLogFile != NULL)
                        {
                            fclose(m_pfTraceLogFile);
                            m_pfTraceLogFile = NULL;
                        }

                        RenameFile(szLogFileFullName, iMaxFileNum);
                        m_pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    m_iCurTraceLogTime = iCurLogTime;
                }
                m_tTraceLastSizeCheckTime = tTimeNow;
            }
            pFile = m_pfTraceLogFile;

            break;
        default: //unknow log type
            break;
        }

        pthread_mutex_unlock(&m_mtMutexCAO);
        //write log
        if (pFile != NULL && !ferror(pFile))
        {
            fprintf(pFile, "%s%s\n", szLogHeaderBuff, pstLogHeader->szLogContent);
        }
    } catch (...)
    { //if error ,do nothing
        return;
    }
}

int CLog::GetLogLevel()
{
    return m_objLogConf.m_pstGlobalConf->iOutputLogLevel;
}

bool CLog::IsShouldWrite(int iLogID, unsigned long long ullUserID, int iPriority)
{
    if (m_iInitFlag != 1)
    {
        //没有初始化日志
        return false;
    }

    if ((iPriority < LOG_INFO)||(iPriority > LOG_TRACE))
    {
        return false;
    }

    if ((m_iLocalLogFlag == 1) && (iPriority <= m_iLocalLogLevel))
    {
        //如果是本地日志，不需要判断共享内存中的配置
        return true;
    }

    //判断日志等级
    int iLogOutputFlag = 0;         //0-不要求输出 1-要求输出

    SELF_LOG(0, ullUserID, LOG_DEBUG, "IS_SHOULD_WRITE|%d|%d|%d", m_objLogConf.m_pstGlobalConf->iOutputLogLevel, iLogID, iPriority);
    LogOutputConf stLogOutputConf;
    if(iPriority > m_objLogConf.m_pstGlobalConf->iOutputLogLevel)
    {
        //判断是否配置了特殊模块输出
        if ((m_objLogConf.m_objModuleConf.GetUsedDataNum() > 1)&&((m_objLogConf.m_objModuleConf.Get(m_objModuleName, stLogOutputConf) == m_objLogConf.m_objModuleConf.SUCCESS)&&(iPriority <= stLogOutputConf.iOutputLogLevel)))
        {
            iLogOutputFlag = 1;
        }

        //判断是否配置了特殊LogID输出
        if ((m_objLogConf.m_objLogIDConf.GetUsedDataNum() > 1)&&((m_objLogConf.m_objLogIDConf.Get(iLogID, stLogOutputConf) == m_objLogConf.m_objLogIDConf.SUCCESS)&&(iPriority <= stLogOutputConf.iOutputLogLevel)))
        {
            iLogOutputFlag = 1;
        }

        //判断是否配置了特殊UserID输出
        if ((m_objLogConf.m_objUserIDConf.GetUsedDataNum() > 1)&&((m_objLogConf.m_objUserIDConf.Get(ullUserID, stLogOutputConf) == m_objLogConf.m_objUserIDConf.SUCCESS)&&(iPriority <= stLogOutputConf.iOutputLogLevel)))
        {
            iLogOutputFlag = 1;
        }
    }
    else
    {
        iLogOutputFlag = 1;
    }

    if (iLogOutputFlag != 1)
    {
        //该日志不需要输出
        return false;
    }

    return true;
}

void CLog::SelfLog(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszFormat, ...)
{
    //这里对文件进行一次开关，正常情况下不会调用到该函数，这个函数只会在定位日志问题时使用

    if (iPriority > m_iSelfLogLevel)
    {
        return;
    }

    LogHeader stLogHeader;
    memset(&stLogHeader, 0x0, sizeof(stLogHeader));

    struct timeval stTimeNow;
    gettimeofday(&stTimeNow, NULL);
    time_t tTimeNow = stTimeNow.tv_sec;

    stLogHeader.uiLogTimeSec = stTimeNow.tv_sec;
    stLogHeader.uiLogTimeUSec = stTimeNow.tv_usec;
    stLogHeader.iLogLevel = iPriority;
    stLogHeader.iLogID = iLogID;
    stLogHeader.iProcID = getpid();
    strncpy(stLogHeader.szModuleName, m_objModuleName.m_szModuleName, sizeof(stLogHeader.szModuleName)-1);
    stLogHeader.ullUserID = ullUserID;

    char szLogHeaderBuff[256];
    int iLogHeadLen = 0;
    char szLogType[32] = {0};

    switch(stLogHeader.iLogLevel)
    {
        case LOG_INFO:
        {
            snprintf(szLogType, sizeof(szLogType), "INFO");
            break;
        }
        case LOG_ERR:
        {
            snprintf(szLogType, sizeof(szLogType), "ERRO");
            break;
        }
        case LOG_WARN:
        {
            snprintf(szLogType, sizeof(szLogType), "WARN");
            break;
        }
        case LOG_DEBUG:
        {
            snprintf(szLogType, sizeof(szLogType), "DEBU");
            break;
        }
        case LOG_TRACE:
        {
            snprintf(szLogType, sizeof(szLogType), "TRAC");
            break;
        }
        default:
        {
            return;
        }
    }

    iLogHeadLen = snprintf(szLogHeaderBuff, sizeof(szLogHeaderBuff), "%s|%s|%s(%d)|%d|%u|%u|%llu||||",
            szLogType,
            GetLogHeadTime(stLogHeader.uiLogTimeSec, stLogHeader.uiLogTimeUSec),
            stLogHeader.szModuleName,
            stLogHeader.iProcID,
            stLogHeader.iLogID,
            *((unsigned int *)&stLogHeader.ullUserID),
            *((unsigned short *)( (char *)&stLogHeader.ullUserID + sizeof(int))),
            stLogHeader.ullUserID);

    int iCurLogTime = atoi(GetFileNameTime(stLogHeader.uiLogTimeSec));

    char szLogFileFullName[1024];

    if (m_iSelfLogTime != iCurLogTime)
    {
        if (m_pfSelfLogFile != NULL)
        {
            fclose(m_pfSelfLogFile);
            m_pfSelfLogFile = NULL;
        }
        snprintf(szLogFileFullName, sizeof(szLogFileFullName), "%s/%d.log", SELFLOG_PATH, iCurLogTime);
        m_pfSelfLogFile = OpenFile(szLogFileFullName, "a");
        m_iSelfLogTime = iCurLogTime;
        m_tSelfLogLastSizeCheckTime = tTimeNow;
    }
    else if (((tTimeNow - m_tSelfLogLastSizeCheckTime) < 0)||(tTimeNow - m_tSelfLogLastSizeCheckTime) > LOG_SIZE_CHECK_TIMEVAL)
    {
        if (CheckFileSize(m_pfSelfLogFile, SELFLOG_MAX_SIZE) != 0)
        {
            if (m_pfSelfLogFile != NULL)
            {
                fclose(m_pfSelfLogFile);
                m_pfSelfLogFile = NULL;
            }
            snprintf(szLogFileFullName, sizeof(szLogFileFullName), "%s/%d.log", SELFLOG_PATH, iCurLogTime);
            m_pfSelfLogFile = OpenFile(szLogFileFullName, "a");
            if (CheckFileSize(m_pfSelfLogFile, SELFLOG_MAX_SIZE) != 0)
            {
                if (m_pfSelfLogFile != NULL)
                {
                    fclose(m_pfSelfLogFile);
                    m_pfSelfLogFile = NULL;
                }
                RenameFile(szLogFileFullName, LOG_DEFAULT_MAX_FILE_NUM);
                m_pfSelfLogFile = OpenFile(szLogFileFullName, "a");
            }
            m_iSelfLogTime = iCurLogTime;
        }
        m_tSelfLogLastSizeCheckTime = tTimeNow;
    }

    char szLogContentBuff[LOG_LENGTH_MAX];
    va_list otherarg;
    va_start(otherarg, pszFormat);
    vsnprintf(szLogContentBuff, sizeof(szLogContentBuff), pszFormat, otherarg);
    va_end(otherarg);

    if (m_pfSelfLogFile != NULL)
    {
        fprintf(m_pfSelfLogFile, "%s%s\n", szLogHeaderBuff, szLogContentBuff);
    }

    return;
}

void OpenLog(const char* pszModuleName, int iLogLevel /*=0*/, const char* pszLogFilePath /*=NULL*/)
{
    g_objLog.Open(pszModuleName, iLogLevel);
}

int SetLogLocal(int iLocalFlag, int iLogLevel, const char *pszLogPath)
{
    return g_objLog.SetLocalFlag(iLocalFlag, iLogLevel, pszLogPath);
}

void LogInternal(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszFormat, ...)
{
    //TODO 这里可以定义全局变量，减少一次内存拷贝
    va_list otherarg;
    va_start(otherarg, pszFormat);
    char szLogContent[LOG_LENGTH_MAX+1];
    vsnprintf(szLogContent, LOG_LENGTH_MAX, pszFormat, otherarg);
    va_end(otherarg);
    g_objLog.LogInternal(iLogID, ullUserID, iPriority, szLogContent);
}
void CloseLog()
{
}

int GetLogLevel()
{
    return g_objLog.GetLogLevel();
}

