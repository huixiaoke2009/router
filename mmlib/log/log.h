#ifndef _LOG_H_
#define _LOG_H_
#include <stdio.h>
#include <sys/time.h>

#include "hash_list/hash_list.h"
#include "shm_queue/shm_queue.h"
#include "share_mem/share_mem.h"

const int LOG_INFO  = 1;
const int LOG_ERR   = 2;
const int LOG_WARN  = 3;
const int LOG_DEBUG = 4;
const int LOG_TRACE = 5;

const int LOG_MODULE_NAME_LEN = 16;
const int LOG_LENGTH_MAX = 12288;

const int LOG_QUEUE_KEY = 0x500002;
const int LOG_QUEUE_SIZE = 67108864; //64M

const int LOG_CONF_SHM_KEY = 0x500001;
const int LOG_CONF_SHM_SIZE = 1048576; //1M

const int LOG_MAX_SPEC_MODULE_NUM = 100;
const int LOG_MAX_SPEC_LOGID_NUM = 100;
const int LOG_MAX_SPEC_USERID_NUM = 100;

const int LOG_CONFSHM_INIT_FLAG = 0x1AFFAAFF;    //用于标识内存是否被初始化过

const int LOG_DEFAULT_LOG_LEVEL = LOG_WARN;
const char LOG_DEFAULT_LOG_PATH[] = "/tmp/log";

const int LOG_WRITE_LOCAL = 99;

const int LOG_SIZE_CHECK_TIMEVAL = 10;   //每10秒检测一次文件大小，是否超过1G
const int LOG_DEFAULT_MAX_FILE_NUM = 16;
const int LOG_DEFAULT_MAX_FILE_SIZE = 1024000000;
const int LOG_DEFAULT_MIN_FILE_SIZE = 10240000;

const int LOG_OUTPUT_TYPE_NORMAL = 1;
const int LOG_OUTPUT_TYPE_SPEC_MODULE = 2;
const int LOG_OUTPUT_TYPE_SPEC_LOGID = 3;
const int LOG_OUTPUT_TYPE_SPEC_USERID = 4;

typedef struct tagLogHeader
{
    unsigned int uiLogTimeSec;
    unsigned int uiLogTimeUSec;
    int iLogLevel;
    int iLogID;
    int iProcID;
    char szModuleName[LOG_MODULE_NAME_LEN+1];
    unsigned long long ullUserID;
    char szLogContent[0];
}LogHeader;

typedef struct tagLogOutputConf
{
    int iLogConfShmInitFlag;
    int iOutputLogLevel;
    int iOutputLogType;     //1-normal 2-spec_module 3-spec_log_id 4-spec_uin
    char szOutputPathDir[256];
    int iMaxFileSize;
    int iFlushFlag;
    int iMaxFileNum;        //循环日志中，一个小时最多日志文件数量
    char szReserved[1012];
}LogOutputConf;

class CModuleName
{
    //对字符串做了一层封装，主要是用于在HashList中间做Key
public:
    char m_szModuleName[LOG_MODULE_NAME_LEN + 1];

    bool operator==(const CModuleName &objModuleName)
    {
        return (strncmp(m_szModuleName, objModuleName.m_szModuleName, sizeof(m_szModuleName)-1) == 0);
    }

    bool operator!=(const CModuleName &objModuleName)
    {
        return (strncmp(m_szModuleName, objModuleName.m_szModuleName, sizeof(m_szModuleName)-1) != 0);
    }

    CModuleName& operator=(const char *pszModuleName)
    {
        memset(this->m_szModuleName, 0x0, sizeof(this->m_szModuleName));
        strncpy(this->m_szModuleName, pszModuleName, sizeof(this->m_szModuleName)-1);
        return *this;
    }

    int Hash() const
    {
        int h = 0;
        const char *key = m_szModuleName;
        while(*key)
        {
            h = (h << 4) + *key++;
            int g = h & 0xF0000000;
            if(g)
                h ^= g >> 24;
            h &= ~g;
        }
        return h;
    }

    int operator%(int iHashIndexNum) const
    {
        return Hash()%iHashIndexNum;
    }
};

class CLogAllConf
{
public:
    LogOutputConf *m_pstGlobalConf;
    mmlib::CHashList<CModuleName, LogOutputConf> m_objModuleConf; //用于指定某个特殊模块的日志输出等级
    mmlib::CHashList<unsigned int, LogOutputConf> m_objLogIDConf; //用于将某个特殊日志ID输出到特定位置
    mmlib::CHashList<unsigned long long, LogOutputConf> m_objUserIDConf;   //用于将某个特殊UserID相关的日志输出到特定位置

    CLogAllConf()
    {
        m_pstGlobalConf = NULL;
    }
};

class CLog
{
public:
    const static int SELFLOG_MAX_SIZE = 1024000000;
    const static int SELFLOG_SIZE_CHECK_TIMEVAL = 10;
    char SELFLOG_PATH[64];

public:
    CLog();
    ~CLog();

    void Open(const char *pszModuleName, int iLogLevel);

    /* 将日志输出到本地，不输出到SHM QUEUE中 */
    int SetLocalFlag(int iLocalFlag, int iLogLevel, const char *pszLogPath);

    void LogInternal(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszLogContent);
    int GetLogLevel();
    void WriteMyLog(const char* pszLogContent);

    /*
     * 判断是否要输出该日志
     * true-需要输出
     * false-不需要输出
     */
    bool IsShouldWrite(int iLogID, unsigned long long ullUserID, int iPriority);

private:
    int m_iInitFlag;
    CModuleName m_objModuleName;
    CLogAllConf m_objLogConf;
    mmlib::CShmQueue m_objLogQueue;
    mmlib::CShareMem m_objLogConfShm;

    //这些日志文件，只是在入Queue失败的时候，直接将日志写入文件中
    FILE* m_pfInfoLogFile;
    FILE* m_pfErrorLogFile;
    FILE* m_pfWarnLogFile;
    FILE* m_pfDebugLogFile;
    FILE* m_pfTraceLogFile;

    int m_iCurInfoLogTime;
    int m_iCurErrorLogTime;
    int m_iCurWarnLogTime;
    int m_iCurDebugLogTime;
    int m_iCurTraceLogTime;

    time_t m_tInfoLastSizeCheckTime;
    time_t m_tErrorLastSizeCheckTime;
    time_t m_tWarnLastSizeCheckTime;
    time_t m_tDebugLastSizeCheckTime;
    time_t m_tTraceLastSizeCheckTime;

    pthread_mutex_t m_mtMutexCAO;
    char m_szLogBuff[sizeof(LogHeader)+LOG_LENGTH_MAX+1];

    //用于自身写日志
    FILE *m_pfSelfLogFile;
    int m_iSelfLogTime;
    time_t m_tSelfLogLastSizeCheckTime;
    int m_iSelfLogLevel;

    //用于本地写日志
    int m_iLocalLogFlag;
    char m_szLocalLogPath[1024];
    char m_iLocalLogLevel;

private:
    const char *GetLogHeadTime(time_t tSec, time_t tUsec);
    const char *GetFileNameTime(time_t tSec);
    void CheckDir(const char * szPath);
    void CheckDirAll(const char * szPath);
    int CheckFileSize(FILE *fpFile, int iFileSize);
    int RenameFile(const char *pszFileName, int iMaxFileNum);

    void SelfLog(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszFormat, ...);
    FILE *OpenFile(const char *pszFileName, const char *pszMode);
};

extern CLog g_objLog;


#ifdef __cplusplus
extern "C"
{
#endif

void OpenLog(const char* pszModuleName, int iLogLevel = 0, const char* pszLogFilePath = NULL);
int SetLogLocal(int iLocalFlag, int iLogLevel, const char *pszLogPath);
void LogInternal(int iLogID, unsigned long long ullUserID, int iPriority, const char* pszFormat, ...)
    __attribute__((format(printf,4,5)));
void CloseLog();

int GetLogLevel();

#ifdef __cplusplus
}
#endif


#define Log(iLogID, ullUserID, iLogLevel, szFormat, args...) \
if (g_objLog.IsShouldWrite(iLogID, ullUserID, iLogLevel)) \
{ \
    LogInternal(iLogID, ullUserID, iLogLevel, szFormat, ##args); \
}

#ifdef LOG_CHECK_PARAM
#undef Log
#define Log(iLogID, ullUserID, iLogLevel, szFormat, args...) printf(szFormat, ##args)
#endif


#define XF_LOG(iLogID, ullUserID, log_level, format, args ...) \
    do\
    {\
        Log(iLogID, ullUserID, log_level, "%s:%u(%s)|" format,  __FILE__, __LINE__, __FUNCTION__,\
                ##args);\
    } while(0)

#define XF_LOG_INFO(iLogID, ullUserID, format, args ...) \
    do\
    {\
        XF_LOG(iLogID, ullUserID, LOG_INFO, format, ##args);\
    } while(0)

#define XF_LOG_ERROR(iLogID, ullUserID, format, args ...) \
    do\
    {\
        XF_LOG(iLogID, ullUserID, LOG_ERR, format, ##args);\
    } while(0)

#define XF_LOG_WARN(iLogID, ullUserID, format, args ...) \
    do\
    {\
        XF_LOG(iLogID, ullUserID, LOG_WARN, format, ##args);\
    } while(0)



#define XF_LOG_DEBUG(iLogID, ullUserID, format, args ...) \
    do\
    {\
        XF_LOG(iLogID, ullUserID, LOG_DEBUG, format, ##args);\
    } while(0)

  
#define XF_LOG_TRACE(iLogID, ullUserID, format, args ...) \
    do\
    {\
        XF_LOG(iLogID, ullUserID, LOG_TRACE, format, ##args);\
    } while(0)


struct FuncTraceStruct
{
    FuncTraceStruct(const char* file_name, size_t file_line,
        const char* func_name, unsigned long long ullUserID) :
        file_name_(file_name),
        file_line_(file_line),
        func_name_(func_name),
        ullUserID_(ullUserID)
    {
        gettimeofday(&start,NULL);

        Log(0, ullUserID_, LOG_TRACE, "%s:%lu(%s)|Enter Function", file_name_, file_line_, func_name_);
    }

    ~FuncTraceStruct()
    {
        struct timeval end;
        unsigned int timeuse;
        gettimeofday(&end,NULL);
        timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;

        Log(0, ullUserID_, LOG_TRACE, "%s:%lu(%s)|Leave Function, usetime:%d", file_name_, file_line_, func_name_, timeuse);
    }

private:
    const char* file_name_;
    size_t file_line_;
    const char* func_name_;
    unsigned long long ullUserID_;
    struct timeval start;
    
};

#ifndef FUNC_TRACE
#define FUNC_TRACE(ullUserID) \
    FuncTraceStruct temp_func_trace_struct(__FILE__, __LINE__, __PRETTY_FUNCTION__, ullUserID)
#endif
    

#endif
