#include <sys/stat.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "logsvr_proc.h"
#include "util/util.h"
#include "ini_file/ini_file.h"
#include "file_lock/file_lock.h"
#include <signal.h>

using namespace mmlib;

char g_szSelfLogPath[1024];

#define SELF_LOG(iLogID, ullUserID, iLogLevel, szFormat, args...) \
{ \
    if (iLogLevel <= m_iSelfLogLevel) \
    { \
        SelfLog(iLogID, ullUserID, iLogLevel, (char *)szFormat, ##args); \
    } \
} while(false)

bool StopFlag = false;

static void SigHandler2(int iSigNo)
{
    XF_LOG_INFO(0, 0, "%s get signal %d", __func__, iSigNo);
    StopFlag = true;
    return;
}



const char *CLogSvrProc::GetLogHeadTime(time_t tSec, time_t tUsec)
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

const char *CLogSvrProc::GetFileNameTime(time_t tSec)
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

void CLogSvrProc::CheckDir(const char * szPath)
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


void CLogSvrProc::CheckDirAll(const char * szPath)
{
    if (NULL == szPath)
    {
        return;
    }
    struct stat stBuf;
    char szDir[256] = { 0 };

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
int CLogSvrProc::CheckFileSize(FILE *fpFile, int iFileSize)
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

int CLogSvrProc::RenameFile(const char *pszFileName, int iMaxFileNum)
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

FILE *CLogSvrProc::OpenFile(const char *pszFileName, const char *pszMode)
{
    FILE *fpRetFile;

    umask(0);
    fpRetFile = fopen(pszFileName, pszMode);
    umask(0022);

    return fpRetFile;
}

CLogSvrProc::CLogSvrProc()
{
    m_pvModuleLogFilePMem = NULL;
    m_iSelfLogLevel = 3;

    m_pfSelfLogFile = NULL;
    m_iSelfLogTime = 0;
    m_tSelfLogLastSizeCheckTime = 0;
    mi_fileNo = 0;
    mi_fileHandle = -1;
    mt_checkTime = 0;
}

CLogSvrProc::~CLogSvrProc()
{
    if (m_pvModuleLogFilePMem)
    {
        free(m_pvModuleLogFilePMem);
    }
}

int CLogSvrProc::Init(const char *pszConfFile)
{
    int iRetVal = 0;

    int iLogLevel = 3;
    char szLogPath[1024] = "/usr/local/log";
    int iMaxFileSize = 1024000000;
    int iMaxFileNum = 16;
    int iFlushFlag = 1;

    CIniFile objIniFile(pszConfFile);
    if (objIniFile.IsValid())
    {
        objIniFile.GetInt("LOG_SVR", "LogLevel", 3, &iLogLevel);
        objIniFile.GetString("LOG_SVR", "LogPath", "/usr/local/log", szLogPath, sizeof(szLogPath));
        objIniFile.GetInt("LOG_SVR", "MaxFileSize", 1024000000, &iMaxFileSize);
        objIniFile.GetInt("LOG_SVR", "FlushFlag", 1, &iFlushFlag);
        objIniFile.GetInt("LOG_SVR", "SleepTimeVal", 10000, &m_iSleepTimeVal);
        objIniFile.GetInt("LOG_SVR", "MaxModuleNum", 100, &m_iMaxModuleNum);
        objIniFile.GetInt("LOG_SVR", "SizeCheckTimeVal", 1, &m_iSizeCheckTimeVal);
        objIniFile.GetInt("LOG_SVR", "SelfLogLevel", 3, &m_iSelfLogLevel);
        objIniFile.GetInt("LOG_SVR", "MaxFileNum", 16, &iMaxFileNum);

        snprintf(g_szSelfLogPath, sizeof(g_szSelfLogPath), "%s/logsvr", szLogPath);
        CheckDir(g_szSelfLogPath);
    }
    else
    {
        printf("init log_svr_conf_file[%s] is not valid\n", pszConfFile);
        return ERROR;
    }

    int LogQuerueKey = LOG_QUEUE_KEY;
    iRetVal = m_objLogQueue.Init(LogQuerueKey, LOG_QUEUE_SIZE);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init log_queue failed, key=%d, size=%d, ret=%d, errmsg=%s", LogQuerueKey, LOG_QUEUE_SIZE, iRetVal, m_objLogQueue.GetErrMsg());
        return ERROR;
    }

    int LogConfShmKey = LOG_CONF_SHM_KEY;
    iRetVal = m_objLogConfShm.Create(LogConfShmKey, LOG_CONF_SHM_SIZE, 0666);
    if ((iRetVal != 0)&&(iRetVal != 1))
    {
        SELF_LOG(0, 0, LOG_ERR, "create conf_shm failed, key=%d, size=%d, ret=%d", LogConfShmKey, LOG_CONF_SHM_SIZE, iRetVal);
        return ERROR;
    }
    SELF_LOG(0, 0, LOG_INFO, "init conf_shm succ, key=0x%x, size=%d", LogConfShmKey, LOG_CONF_SHM_SIZE);

    iRetVal = m_objLogConfShm.Attach();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "attach conf_shm failed, ret=%d", iRetVal);
        return ERROR;
    }

    m_objLogConf.m_pstGlobalConf = (LogOutputConf *)m_objLogConfShm.GetMem();
    int iModuleConfHashListSize = m_objLogConf.m_objModuleConf.CalcSize(LOG_MAX_SPEC_MODULE_NUM, LOG_MAX_SPEC_MODULE_NUM);
    int iLogIDConfHashListSize = m_objLogConf.m_objLogIDConf.CalcSize(LOG_MAX_SPEC_LOGID_NUM, LOG_MAX_SPEC_LOGID_NUM);
    int iUserIDConfHashListSize = m_objLogConf.m_objUserIDConf.CalcSize(LOG_MAX_SPEC_USERID_NUM, LOG_MAX_SPEC_USERID_NUM);

    if ((sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize + iUserIDConfHashListSize) > ((unsigned int)LOG_CONF_SHM_SIZE))
    {
        SELF_LOG(0, 0, LOG_ERR, "all_conf_shm_size=%d, allocated_size=%d", (sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize + iUserIDConfHashListSize), LOG_CONF_SHM_SIZE);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objModuleConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf), iModuleConfHashListSize, LOG_MAX_SPEC_MODULE_NUM, LOG_MAX_SPEC_MODULE_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init module_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }
    iRetVal = m_objLogConf.m_objLogIDConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf) + iModuleConfHashListSize, iLogIDConfHashListSize, LOG_MAX_SPEC_LOGID_NUM, LOG_MAX_SPEC_LOGID_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init logid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objUserIDConf.Init((char *)m_objLogConf.m_pstGlobalConf + sizeof(LogOutputConf) + iModuleConfHashListSize + iLogIDConfHashListSize, iUserIDConfHashListSize, LOG_MAX_SPEC_USERID_NUM, LOG_MAX_SPEC_USERID_NUM);
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init userid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    //TODO 这里的初始化需要重新考虑一下
    m_objLogConf.m_pstGlobalConf->iLogConfShmInitFlag = LOG_CONFSHM_INIT_FLAG;
    m_objLogConf.m_pstGlobalConf->iOutputLogLevel = iLogLevel;
    m_objLogConf.m_pstGlobalConf->iOutputLogType = 1;
    snprintf(m_objLogConf.m_pstGlobalConf->szOutputPathDir, sizeof(m_objLogConf.m_pstGlobalConf->szOutputPathDir), "%s", szLogPath);
    m_objLogConf.m_pstGlobalConf->iMaxFileSize = iMaxFileSize;
    m_objLogConf.m_pstGlobalConf->iMaxFileNum = iMaxFileNum;
    m_objLogConf.m_pstGlobalConf->iFlushFlag = iFlushFlag;

    iRetVal = m_objLogConf.m_objModuleConf.Clear();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear module_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }
    iRetVal = m_objLogConf.m_objLogIDConf.Clear();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear logid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objUserIDConf.Clear();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear userid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objModuleConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify module_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objLogIDConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify logid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objLogConf.m_objUserIDConf.Verify();
    if (iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "verify userid_conf_hash_list failed, ret=%d", iRetVal);
        return ERROR;
    }

    //读取特殊的日志配置
    int iSpecModuleConfNum=0;
    int iSpecLogIDConfNum=0;
    int iSpecUserIDConfNum=0;

    objIniFile.GetInt("LOG_SVR", "SpecModuleConfNum", 0, &iSpecModuleConfNum);
    objIniFile.GetInt("LOG_SVR", "SpecLogIDConfNum", 0, &iSpecLogIDConfNum);
    objIniFile.GetInt("LOG_SVR", "SpecUserIDConfNum", 0, &iSpecUserIDConfNum);

    SELF_LOG(0, 0, LOG_INFO, "SpecModuleConfNum=%d SpecLogIDConfNum=%d SpecUserIDConfNum=%d", iSpecModuleConfNum, iSpecLogIDConfNum, iSpecUserIDConfNum);

    char szSecName[256] = {0};
    LogOutputConf stLogOutputConf;
    for(int i=1; i<=iSpecModuleConfNum; i++)
    {
        char szModuleName[32] = {0};
        CModuleName objModuleName;
        memset(&stLogOutputConf, 0x0, sizeof(stLogOutputConf));
        snprintf(szSecName, sizeof(szSecName), "SPEC_MODULE_%d", i);
        objIniFile.GetString(szSecName, "ModuleName", "", szModuleName, sizeof(szModuleName));
        if (szModuleName[0] == '\0')
        {
            SELF_LOG(0, 0, LOG_ERR, "config item %s|%s is not valid", szSecName, "ModuleName");
            continue;
        }

        objModuleName = szModuleName;
        objIniFile.GetInt(szSecName, "LogLevel", 0, &stLogOutputConf.iOutputLogLevel);
        stLogOutputConf.iOutputLogType = LOG_OUTPUT_TYPE_SPEC_MODULE;

        iRetVal = m_objLogConf.m_objModuleConf.Insert(objModuleName, stLogOutputConf);
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "insert into spec_module_hash_list failed, ret=%d, module_name=%s", iRetVal, szModuleName);
            continue;
        }
        else
        {
            SELF_LOG(0, 0, LOG_INFO, "insert into spec_module_hash_list succ, module_name=%s, level=%d", szModuleName, stLogOutputConf.iOutputLogLevel);
        }
    }

    for(int i=1; i<=iSpecLogIDConfNum; i++)
    {
        int iSpecLogID = 0;
        memset(&stLogOutputConf, 0x0, sizeof(stLogOutputConf));
        snprintf(szSecName, sizeof(szSecName), "SPEC_LOGID_%d", i);
        objIniFile.GetInt(szSecName, "LogID", 0, &iSpecLogID);
        if (iSpecLogID == 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "config item %s|%s is not valid", szSecName, "LogID");
            continue;
        }

        objIniFile.GetInt(szSecName, "LogLevel", 0, &stLogOutputConf.iOutputLogLevel);
        objIniFile.GetString(szSecName, "LogPath", "", stLogOutputConf.szOutputPathDir, sizeof(stLogOutputConf.szOutputPathDir));
        stLogOutputConf.iOutputLogType = LOG_OUTPUT_TYPE_SPEC_LOGID;

        iRetVal = m_objLogConf.m_objLogIDConf.Insert(iSpecLogID, stLogOutputConf);
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "insert into spec_logid_hash_list failed, ret=%d, logid=%d", iRetVal, iSpecLogID);
            continue;
        }
        else
        {
            SELF_LOG(0, 0, LOG_INFO, "insert into spec_logid_hash_list succ, logid=%d, level=%d, path=%s", iSpecLogID, stLogOutputConf.iOutputLogLevel, stLogOutputConf.szOutputPathDir);
        }
    }

    for(int i=1; i<=iSpecUserIDConfNum; i++)
    {
        unsigned long long ullSpecUserID = 0;
        memset(&stLogOutputConf, 0x0, sizeof(stLogOutputConf));
        snprintf(szSecName, sizeof(szSecName), "SPEC_USERID_%d", i);
        objIniFile.GetULongLong(szSecName, "UserID", 0, &ullSpecUserID);
        if (ullSpecUserID == 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "config item %s|%s is not valid", szSecName, "USERID");
            continue;
        }

        objIniFile.GetInt(szSecName, "LogLevel", 0, &stLogOutputConf.iOutputLogLevel);
        objIniFile.GetString(szSecName, "LogPath", "", stLogOutputConf.szOutputPathDir, sizeof(stLogOutputConf.szOutputPathDir));
        stLogOutputConf.iOutputLogType = LOG_OUTPUT_TYPE_SPEC_USERID;

        iRetVal = m_objLogConf.m_objUserIDConf.Insert(ullSpecUserID, stLogOutputConf);
        if (iRetVal != 0)
        {
            SELF_LOG(0, 0, LOG_ERR, "insert into spec_userid_hash_list failed, ret=%d, userid=%llu", iRetVal, ullSpecUserID);
            continue;
        }
        else
        {
            SELF_LOG(0, 0, LOG_INFO, "insert into spec_logid_hash_list succ, userid=%llu, level=%d, path=%s", ullSpecUserID, stLogOutputConf.iOutputLogLevel, stLogOutputConf.szOutputPathDir);
        }
    }

    iRetVal = pthread_mutex_init(&m_mtMutexCAO, NULL);
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init pthread_mutex failed, ret=%d", iRetVal);
        return ERROR;
    }

    //根据模块名输出日志的文件句柄HASH
    int iModuleLogFilePMemSize = m_objModuleLogFilePHash.CalcSize(m_iMaxModuleNum, m_iMaxModuleNum);
    m_pvModuleLogFilePMem = malloc(iModuleLogFilePMemSize);
    if ( m_pvModuleLogFilePMem == NULL)
    {
        SELF_LOG(0, 0, LOG_ERR, "allocate ModuleLogFilePMem failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objModuleLogFilePHash.Init(m_pvModuleLogFilePMem, iModuleLogFilePMemSize, m_iMaxModuleNum, m_iMaxModuleNum);
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init ModuleLogFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objModuleLogFilePHash.Clear();
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear ModuleLogFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    //根据特殊日志ID输出日志的文件句柄HASH
    int iSpecLogIDFilePMemSize = m_objSpecLogIDFilePHash.CalcSize(m_iMaxModuleNum, m_iMaxModuleNum);
    m_pvSpecLogIDFilePMem = malloc(iSpecLogIDFilePMemSize);
    if ( m_pvSpecLogIDFilePMem == NULL)
    {
        SELF_LOG(0, 0, LOG_ERR, "allocate SpecLogIDFilePMem failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objSpecLogIDFilePHash.Init(m_pvSpecLogIDFilePMem, iSpecLogIDFilePMemSize, m_iMaxModuleNum, m_iMaxModuleNum);
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init SpecLogIDFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objSpecLogIDFilePHash.Clear();
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear SpecLogIDFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    int iSpecUserIDFilePMemSize = m_objSpecUserIDFilePHash.CalcSize(m_iMaxModuleNum, m_iMaxModuleNum);
    m_pvSpecUserIDFilePMem = malloc(iSpecUserIDFilePMemSize);
    if ( m_pvSpecUserIDFilePMem == NULL)
    {
        SELF_LOG(0, 0, LOG_ERR, "allocate SpecUserIDFilePMem failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objSpecUserIDFilePHash.Init(m_pvSpecUserIDFilePMem, iSpecUserIDFilePMemSize, m_iMaxModuleNum, m_iMaxModuleNum);
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "init SpecUserIDFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    iRetVal = m_objSpecUserIDFilePHash.Clear();
    if ( iRetVal != 0)
    {
        SELF_LOG(0, 0, LOG_ERR, "clear SpecUserIDFilePHash failed, ret=%d", iRetVal);
        return ERROR;
    }

    CheckDir(m_objLogConf.m_pstGlobalConf->szOutputPathDir);

    SELF_LOG(0, 0, LOG_INFO, "%d|%s|%d|%d|%d|%d|%d|%d|init log_svr succ",
            iLogLevel,
            szLogPath,
            iMaxFileSize,
            iFlushFlag,
            m_iSleepTimeVal,
            m_iMaxModuleNum,
            m_iSizeCheckTimeVal,
            m_iSelfLogLevel);


    //捕捉信号SIGTERM(用于killall)
    StopFlag = false;
    struct sigaction stSiga2;
    memset(&stSiga2, 0, sizeof(stSiga2));
    stSiga2.sa_handler = SigHandler2;
    sigaction(SIGTERM, &stSiga2, NULL);

    //忽略信号量
    signal(SIGPIPE, SIG_IGN);

    printf("INFO:init signal succ\n");

    return SUCCESS;
}

int CLogSvrProc::Run()
{
    int iRetVal = 0;

    while (!StopFlag)
    {
        int iRecvLen = sizeof(m_szLogFromQueueBuff);
        iRetVal = m_objLogQueue.OutQueue(m_szLogFromQueueBuff, &iRecvLen);
        if (iRetVal == m_objLogQueue.E_SHM_QUEUE_EMPTY)
        {
            usleep(m_iSleepTimeVal);
            continue;
        }
        else if (iRetVal != m_objLogQueue.SUCCESS)
        {
            SELF_LOG(0, 0, LOG_ERR, "outqueue failed, ret=%d", iRetVal);
            usleep(m_iSleepTimeVal);
            continue;
        }

        if (iRecvLen < (int)sizeof(LogHeader))
        {
            SELF_LOG(0, 0, LOG_ERR, "outqueue len is not valid, len=%d", iRecvLen);
            continue;
        }

        SELF_LOG(0, 0, LOG_TRACE, "out_queue(%d)[%s]", iRecvLen, CStrTool::Str2Hex(m_szLogFromQueueBuff, iRecvLen));
        m_szLogFromQueueBuff[iRecvLen]='\0';

        LogHeader *pstLogHeader = (LogHeader *)m_szLogFromQueueBuff;
        if ((pstLogHeader->uiLogTimeSec==0)
                ||(pstLogHeader->iLogLevel<=0)
                ||(pstLogHeader->iLogLevel>5)
                ||(pstLogHeader->iProcID == 0)
                ||(pstLogHeader->szModuleName[0] == 0))
        {
            SELF_LOG(0, 0, LOG_ERR, "LogHeader is not valid, RECV_BUFF(%d)[%s]", iRecvLen, CStrTool::Str2Hex(m_szLogFromQueueBuff, iRecvLen));
            continue;
        }

        CModuleName objModuleName;
        objModuleName = pstLogHeader->szModuleName;
        LogFilePInfo stLogFilePInfo;
        LogOutputConf stLogOutputConf;
        int iLogFilePInfoUpdateFlag = 0;
        char szLogFilePath[1024];

        //正常按照模块输出日志
        iRetVal = m_objModuleLogFilePHash.Get(objModuleName, stLogFilePInfo);
        if (iRetVal == m_objModuleLogFilePHash.E_HASH_LIST_NO_NODE)
        {
            iLogFilePInfoUpdateFlag = 1;
            memset(&stLogFilePInfo, 0x0, sizeof(stLogFilePInfo));

            //TODO 这里需要考虑淘汰机制，否则数据会越积累越多
            iRetVal = m_objModuleLogFilePHash.Insert(objModuleName, stLogFilePInfo);
            if (iRetVal != m_objModuleLogFilePHash.SUCCESS)
            {
                SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "insert file_p_info into module_hash_list failed, ret=%d", iRetVal);
                continue;
            }

            snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%s", m_objLogConf.m_pstGlobalConf->szOutputPathDir, pstLogHeader->szModuleName);
            CheckDirAll(szLogFilePath);
        }
        else if (iRetVal != m_objModuleLogFilePHash.SUCCESS)
        {
            SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "get file_p_info from hash_list failed, ret=%d", iRetVal);
            continue;
        }

        //正常模块日志输出
        iRetVal = WriteLogToFile(stLogFilePInfo, *m_objLogConf.m_pstGlobalConf);
        if (iRetVal == 1)
        {
            iLogFilePInfoUpdateFlag = 1;
        }

        if (iLogFilePInfoUpdateFlag == 1)
        {
            iRetVal = m_objModuleLogFilePHash.Update(objModuleName, stLogFilePInfo);
            if (iRetVal != 0)
            {
                SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "update file_p_info into module_hash_list failed, ret=%d", iRetVal);
                continue;
            }

            SELF_LOG(0, pstLogHeader->ullUserID, LOG_INFO, "update file_p_info into module_hash_list succ");
        }


        //按照特殊的LogID输出日志
        if ((m_objLogConf.m_objLogIDConf.GetUsedDataNum() > 1)&&(m_objLogConf.m_objLogIDConf.Get(pstLogHeader->iLogID, stLogOutputConf) == 0))
        {
            //需要按照某个特殊的日志ID配置输出
            iRetVal = m_objSpecLogIDFilePHash.Get(pstLogHeader->iLogID, stLogFilePInfo);
            if (iRetVal == m_objSpecLogIDFilePHash.E_HASH_LIST_NO_NODE)
            {
                iLogFilePInfoUpdateFlag = 1;
                memset(&stLogFilePInfo, 0x0, sizeof(stLogFilePInfo));

                //TODO 这里需要考虑淘汰机制，否则数据会越积累越多
                iRetVal = m_objSpecLogIDFilePHash.Insert(pstLogHeader->iLogID, stLogFilePInfo);
                if (iRetVal != m_objSpecLogIDFilePHash.SUCCESS)
                {
                    SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "insert file_p_info into spec_logid_hash_list failed, ret=%d", iRetVal);
                    continue;
                }

                if (stLogOutputConf.szOutputPathDir[0] == '\0')
                {
                    snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%d", m_objLogConf.m_pstGlobalConf->szOutputPathDir, pstLogHeader->iLogID);
                }
                else
                {
                    snprintf(szLogFilePath, sizeof(szLogFilePath), "%s", stLogOutputConf.szOutputPathDir);
                }
                CheckDirAll(szLogFilePath);
            }
            else if (iRetVal != m_objSpecLogIDFilePHash.SUCCESS)
            {
                SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "get file_p_info from spec_logid_hash_list failed, ret=%d", iRetVal);
                continue;
            }

            iRetVal = WriteLogToFile(stLogFilePInfo, stLogOutputConf);
            if (iRetVal == 1)
            {
                iLogFilePInfoUpdateFlag = 1;
            }

            if (iLogFilePInfoUpdateFlag == 1)
            {
                iRetVal = m_objSpecLogIDFilePHash.Update(pstLogHeader->iLogID, stLogFilePInfo);
                if (iRetVal != 0)
                {
                    SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "update file_p_info into spec_logid_hash_list failed, ret=%d", iRetVal);
                    continue;
                }
            }

        }

        //按照特殊的USERID输出日志
        if ((m_objLogConf.m_objUserIDConf.GetUsedDataNum() > 1)&&(m_objLogConf.m_objUserIDConf.Get(pstLogHeader->ullUserID, stLogOutputConf) == 0))
        {
            //需要按照某个特殊的USERID配置输出
            iRetVal = m_objSpecUserIDFilePHash.Get(pstLogHeader->ullUserID, stLogFilePInfo);
            if (iRetVal == m_objSpecUserIDFilePHash.E_HASH_LIST_NO_NODE)
            {
                iLogFilePInfoUpdateFlag = 1;
                memset(&stLogFilePInfo, 0x0, sizeof(stLogFilePInfo));

                //TODO 这里需要考虑淘汰机制，否则数据会越积累越多
                iRetVal = m_objSpecUserIDFilePHash.Insert(pstLogHeader->ullUserID, stLogFilePInfo);
                if (iRetVal != m_objSpecUserIDFilePHash.SUCCESS)
                {
                    SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "insert file_p_info into spec_userid_hash_list failed, ret=%d", iRetVal);
                    continue;
                }

                if (stLogOutputConf.szOutputPathDir[0] == '\0')
                {
                    snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%llu", m_objLogConf.m_pstGlobalConf->szOutputPathDir, pstLogHeader->ullUserID);
                }
                else
                {
                    snprintf(szLogFilePath, sizeof(szLogFilePath), "%s", stLogOutputConf.szOutputPathDir);
                }
                CheckDirAll(szLogFilePath);
            }
            else if (iRetVal != m_objSpecUserIDFilePHash.SUCCESS)
            {
                SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "get file_p_info from spec_userid_hash_list failed, ret=%d", iRetVal);
                continue;
            }

            iRetVal = WriteLogToFile(stLogFilePInfo, stLogOutputConf);
            if (iRetVal == 1)
            {
                iLogFilePInfoUpdateFlag = 1;
            }

            if (iLogFilePInfoUpdateFlag == 1)
            {
                iRetVal = m_objSpecUserIDFilePHash.Update(pstLogHeader->ullUserID, stLogFilePInfo);
                if (iRetVal != 0)
                {
                    SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "update file_p_info into spec_userid_hash_list failed, ret=%d", iRetVal);
                    continue;
                }
            }

        }
    }

    return 0;
}

int CLogSvrProc::WriteLogToFile(LogFilePInfo &stLogFilePInfo, const LogOutputConf &stLogOutputConf)
{
    int iRetVal = 0;

    LogHeader *pstLogHeader = (LogHeader *)m_szLogFromQueueBuff;

    char szLogHeaderBuff[256];
    int iLogHeadLen = 0;

    iLogHeadLen = snprintf(szLogHeaderBuff, sizeof(szLogHeaderBuff), "%s|%s(%d)|%d|%llu||||",
            GetLogHeadTime(pstLogHeader->uiLogTimeSec, pstLogHeader->uiLogTimeUSec),
            pstLogHeader->szModuleName,
            pstLogHeader->iProcID,
            pstLogHeader->iLogID,
            pstLogHeader->ullUserID);

    SELF_LOG(0, pstLogHeader->ullUserID, LOG_DEBUG, "WriteLogToFile");


    int iCurLogTime = atoi(GetFileNameTime(pstLogHeader->uiLogTimeSec));
    time_t tTimeNow = time(NULL);
    char szLogFileFullName[1024];
    char szLogFilePath[1024];

    switch (stLogOutputConf.iOutputLogType)
    {
        case LOG_OUTPUT_TYPE_SPEC_LOGID:
        {
            if (stLogOutputConf.szOutputPathDir[0] == '\0')
            {
                snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%d", m_objLogConf.m_pstGlobalConf->szOutputPathDir, pstLogHeader->iLogID);
            }
            else
            {
                snprintf(szLogFilePath, sizeof(szLogFilePath), "%s", stLogOutputConf.szOutputPathDir);
            }
            break;
        }
        case LOG_OUTPUT_TYPE_SPEC_USERID:
        {
            if (stLogOutputConf.szOutputPathDir[0] == '\0')
            {
                snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%llu", m_objLogConf.m_pstGlobalConf->szOutputPathDir, pstLogHeader->ullUserID);
            }
            else
            {
                snprintf(szLogFilePath, sizeof(szLogFilePath), "%s", stLogOutputConf.szOutputPathDir);
            }

            break;
        }
        default:
        {
            snprintf(szLogFilePath, sizeof(szLogFilePath), "%s/%s", stLogOutputConf.szOutputPathDir, pstLogHeader->szModuleName);
            break;
        }
    }

    FILE *pFile = NULL;

    int iLogFilePInfoUpdateFlag = 0;

    try
    {
        pthread_mutex_lock(&m_mtMutexCAO);
        switch (pstLogHeader->iLogLevel)
        {
        case LOG_INFO:
            //get log file by time
            if (iCurLogTime != stLogFilePInfo.iCurInfoLogTime)
            {
                if (stLogFilePInfo.pfInfoLogFile != NULL)
                {
                    fclose(stLogFilePInfo.pfInfoLogFile);
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/info/%d.log", szLogFilePath, iCurLogTime);
                stLogFilePInfo.pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                stLogFilePInfo.iCurInfoLogTime = iCurLogTime;
                iLogFilePInfoUpdateFlag = 1;
                stLogFilePInfo.tInfoLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - stLogFilePInfo.tInfoLastSizeCheckTime) < 0)||(tTimeNow - stLogFilePInfo.tInfoLastSizeCheckTime) > m_iSizeCheckTimeVal)
            {
                if (CheckFileSize(stLogFilePInfo.pfInfoLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                {
                    if (stLogFilePInfo.pfInfoLogFile != NULL)
                    {
                        fclose(stLogFilePInfo.pfInfoLogFile);
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/info/%d.log", szLogFilePath, iCurLogTime);
                    stLogFilePInfo.pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                    //可能刚刚关闭掉的那个文件，已经被改名过了
                    if (CheckFileSize(stLogFilePInfo.pfInfoLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                    {
                        if (stLogFilePInfo.pfInfoLogFile != NULL)
                        {
                            fclose(stLogFilePInfo.pfInfoLogFile);
                        }
                        RenameFile(szLogFileFullName, m_objLogConf.m_pstGlobalConf->iMaxFileNum);
                        stLogFilePInfo.pfInfoLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    stLogFilePInfo.iCurInfoLogTime = iCurLogTime;
                    iLogFilePInfoUpdateFlag = 1;
                }
                stLogFilePInfo.tInfoLastSizeCheckTime = tTimeNow;
            }
            pFile = stLogFilePInfo.pfInfoLogFile;

            break;
        case LOG_ERR:
            //get log file by time
            if (iCurLogTime != stLogFilePInfo.iCurErrorLogTime)
            {
                if (stLogFilePInfo.pfErrorLogFile != NULL)
                {
                    fclose(stLogFilePInfo.pfErrorLogFile);
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/error/%d.log", szLogFilePath, iCurLogTime);
                stLogFilePInfo.pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                stLogFilePInfo.iCurErrorLogTime = iCurLogTime;
                iLogFilePInfoUpdateFlag = 1;
                stLogFilePInfo.tErrorLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - stLogFilePInfo.tErrorLastSizeCheckTime) < 0)||(tTimeNow - stLogFilePInfo.tErrorLastSizeCheckTime) > m_iSizeCheckTimeVal)
            {
                if (CheckFileSize(stLogFilePInfo.pfErrorLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                {
                    if (stLogFilePInfo.pfErrorLogFile != NULL)
                    {
                        fclose(stLogFilePInfo.pfErrorLogFile);
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/error/%d.log", szLogFilePath, iCurLogTime);
                    stLogFilePInfo.pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(stLogFilePInfo.pfErrorLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                    {
                        if (stLogFilePInfo.pfErrorLogFile != NULL)
                        {
                            fclose(stLogFilePInfo.pfErrorLogFile);
                        }
                        RenameFile(szLogFileFullName, m_objLogConf.m_pstGlobalConf->iMaxFileNum);
                        stLogFilePInfo.pfErrorLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    stLogFilePInfo.iCurErrorLogTime = iCurLogTime;
                    iLogFilePInfoUpdateFlag = 1;
                }
                stLogFilePInfo.tErrorLastSizeCheckTime = tTimeNow;
            }
            pFile = stLogFilePInfo.pfErrorLogFile;

            break;
        case LOG_WARN:
            //get log file by time
            if (iCurLogTime != stLogFilePInfo.iCurWarnLogTime)
            {
                if (stLogFilePInfo.pfWarnLogFile != NULL)
                {
                    fclose(stLogFilePInfo.pfWarnLogFile);
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/warn/%d.log", szLogFilePath, iCurLogTime);
                stLogFilePInfo.pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                stLogFilePInfo.iCurWarnLogTime = iCurLogTime;
                iLogFilePInfoUpdateFlag = 1;
                stLogFilePInfo.tWarnLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - stLogFilePInfo.tWarnLastSizeCheckTime) < 0)||(tTimeNow - stLogFilePInfo.tWarnLastSizeCheckTime) > m_iSizeCheckTimeVal)
            {
                if (CheckFileSize(stLogFilePInfo.pfWarnLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                {
                    if (stLogFilePInfo.pfWarnLogFile != NULL)
                    {
                        fclose(stLogFilePInfo.pfWarnLogFile);
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/warn/%d.log", szLogFilePath, iCurLogTime);
                    stLogFilePInfo.pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(stLogFilePInfo.pfWarnLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                    {
                        if (stLogFilePInfo.pfWarnLogFile != NULL)
                        {
                           fclose(stLogFilePInfo.pfWarnLogFile);
                        }
                        RenameFile(szLogFileFullName, m_objLogConf.m_pstGlobalConf->iMaxFileNum);
                        stLogFilePInfo.pfWarnLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    stLogFilePInfo.iCurWarnLogTime = iCurLogTime;
                    iLogFilePInfoUpdateFlag = 1;
                }
                stLogFilePInfo.tWarnLastSizeCheckTime = tTimeNow;
            }
            pFile = stLogFilePInfo.pfWarnLogFile;

            break;
        case LOG_DEBUG:
            //get log file name by time
            if (iCurLogTime != stLogFilePInfo.iCurDebugLogTime)
            {
                if (stLogFilePInfo.pfDebugLogFile != NULL)
                {
                    fclose(stLogFilePInfo.pfDebugLogFile);
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/debug/%d.log", szLogFilePath, iCurLogTime);
                stLogFilePInfo.pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                stLogFilePInfo.iCurDebugLogTime = iCurLogTime;
                iLogFilePInfoUpdateFlag = 1;
                stLogFilePInfo.tDebugLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - stLogFilePInfo.tDebugLastSizeCheckTime) < 0)||(tTimeNow - stLogFilePInfo.tDebugLastSizeCheckTime) > m_iSizeCheckTimeVal)
            {
                if (CheckFileSize(stLogFilePInfo.pfDebugLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                {
                    if (stLogFilePInfo.pfDebugLogFile != NULL)
                    {
                        fclose(stLogFilePInfo.pfDebugLogFile);
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/debug/%d.log", szLogFilePath, iCurLogTime);
                    stLogFilePInfo.pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(stLogFilePInfo.pfDebugLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                    {
                        if (stLogFilePInfo.pfDebugLogFile != NULL)
                        {
                            fclose(stLogFilePInfo.pfDebugLogFile);
                        }
                        RenameFile(szLogFileFullName, m_objLogConf.m_pstGlobalConf->iMaxFileNum);
                        stLogFilePInfo.pfDebugLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    stLogFilePInfo.iCurDebugLogTime = iCurLogTime;
                    iLogFilePInfoUpdateFlag = 1;
                }
                stLogFilePInfo.tDebugLastSizeCheckTime = tTimeNow;
            }
            pFile = stLogFilePInfo.pfDebugLogFile;

            break;
        case LOG_TRACE:
            //get log file name by time
            if (iCurLogTime != stLogFilePInfo.iCurTraceLogTime)
            {
                if (stLogFilePInfo.pfTraceLogFile != NULL)
                {
                    fclose(stLogFilePInfo.pfTraceLogFile);
                }
                snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                        "%s/trace/%d.log", szLogFilePath, iCurLogTime);
                stLogFilePInfo.pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                stLogFilePInfo.iCurTraceLogTime = iCurLogTime;
                iLogFilePInfoUpdateFlag = 1;
                stLogFilePInfo.tTraceLastSizeCheckTime = tTimeNow;
            }
            else if (((tTimeNow - stLogFilePInfo.tTraceLastSizeCheckTime) < 0)||(tTimeNow - stLogFilePInfo.tTraceLastSizeCheckTime) > m_iSizeCheckTimeVal)
            {
                if (CheckFileSize(stLogFilePInfo.pfTraceLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                {
                    if (stLogFilePInfo.pfTraceLogFile != NULL)
                    {
                        fclose(stLogFilePInfo.pfTraceLogFile);
                    }
                    snprintf(szLogFileFullName, sizeof(szLogFileFullName),
                            "%s/trace/%d.log", szLogFilePath, iCurLogTime);
                    stLogFilePInfo.pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                    if (CheckFileSize(stLogFilePInfo.pfTraceLogFile, m_objLogConf.m_pstGlobalConf->iMaxFileSize) != 0)
                    {
                        if (stLogFilePInfo.pfTraceLogFile != NULL)
                        {
                            fclose(stLogFilePInfo.pfTraceLogFile);
                        }
                        RenameFile(szLogFileFullName, m_objLogConf.m_pstGlobalConf->iMaxFileNum);
                        stLogFilePInfo.pfTraceLogFile = OpenFile(szLogFileFullName, "a");
                    }
                    stLogFilePInfo.iCurTraceLogTime = iCurLogTime;
                    iLogFilePInfoUpdateFlag = 1;
                }
                stLogFilePInfo.tTraceLastSizeCheckTime = tTimeNow;
            }
            pFile = stLogFilePInfo.pfTraceLogFile;

            break;
        default: //unknow log type
            break;
        }

        pthread_mutex_unlock(&m_mtMutexCAO);
        //write log
        if (pFile != NULL && !ferror(pFile))
        {
            fprintf(pFile, "%s%s\n", szLogHeaderBuff, pstLogHeader->szLogContent);
            if (m_objLogConf.m_pstGlobalConf->iFlushFlag == 1)
            {
                fflush(pFile);
            }
        }
    } catch (...)
    { //if error ,do nothing
        SELF_LOG(0, pstLogHeader->ullUserID, LOG_ERR, "catch a execption, ret=%d", iRetVal);
        return -3;
    }

    if (iLogFilePInfoUpdateFlag == 1)
    {
        return iLogFilePInfoUpdateFlag;
    }

    return 0;
}

void CLogSvrProc::SelfLog(int iLogID, unsigned long long ullUserID, int iPriority, char* pszFormat, ...)
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
    strncpy(stLogHeader.szModuleName, "logsvr", sizeof(stLogHeader.szModuleName)-1);
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

    iLogHeadLen = snprintf(szLogHeaderBuff, sizeof(szLogHeaderBuff), "%s|%s|%s(%d)|%d|%llu||||",
            szLogType,
            GetLogHeadTime(stLogHeader.uiLogTimeSec, stLogHeader.uiLogTimeUSec),
            stLogHeader.szModuleName,
            stLogHeader.iProcID,
            stLogHeader.iLogID,
            stLogHeader.ullUserID);

    int iCurLogTime = atoi(GetFileNameTime(stLogHeader.uiLogTimeSec));

    char szLogFileFullName[1024];

    if (m_iSelfLogTime != iCurLogTime)
    {
        if (m_pfSelfLogFile != NULL)
        {
            fclose(m_pfSelfLogFile);
        }
        snprintf(szLogFileFullName, sizeof(szLogFileFullName), "%s/%d.log", g_szSelfLogPath, iCurLogTime);
        m_pfSelfLogFile = OpenFile(szLogFileFullName, "a");
        m_iSelfLogTime = iCurLogTime;
        m_tSelfLogLastSizeCheckTime = tTimeNow;
    }
    else if (((tTimeNow - m_tSelfLogLastSizeCheckTime) < 0)||(tTimeNow - m_tSelfLogLastSizeCheckTime) > SELFLOG_SIZE_CHECK_TIMEVAL)
    {
        if (CheckFileSize(m_pfSelfLogFile, SELFLOG_MAX_SIZE) != 0)
        {
            if (m_pfSelfLogFile != NULL)
            {
                fclose(m_pfSelfLogFile);
            }
            snprintf(szLogFileFullName, sizeof(szLogFileFullName), "%s/%d.log", g_szSelfLogPath, iCurLogTime);
            m_pfSelfLogFile = OpenFile(szLogFileFullName, "a");
            if (CheckFileSize(m_pfSelfLogFile, SELFLOG_MAX_SIZE) != 0)
            {
                if(m_pfSelfLogFile)
                {
                    fclose(m_pfSelfLogFile);
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

    if (m_pfSelfLogFile != NULL && !ferror(m_pfSelfLogFile))
    {
        fprintf(m_pfSelfLogFile, "%s%s\n", szLogHeaderBuff, szLogContentBuff);
        fflush(m_pfSelfLogFile);
    }
    else
    {
        printf("m_pfSelfLogFile=0x%p\n", m_pfSelfLogFile);
    }

    return;
}

