#ifndef _LOGSVR_PROC_H_
#define _LOGSVR_PROC_H_

#include <set>
#include "log/log.h"

typedef struct tagLogFilePInfo
{
    FILE* pfInfoLogFile;
    FILE* pfErrorLogFile;
    FILE* pfWarnLogFile;
    FILE* pfDebugLogFile;
    FILE* pfTraceLogFile;

    int iCurInfoLogTime;
    int iCurErrorLogTime;
    int iCurWarnLogTime;
    int iCurDebugLogTime;
    int iCurTraceLogTime;

    time_t tInfoLastSizeCheckTime;
    time_t tErrorLastSizeCheckTime;
    time_t tWarnLastSizeCheckTime;
    time_t tDebugLastSizeCheckTime;
    time_t tTraceLastSizeCheckTime;
}LogFilePInfo;

class CLogSvrProc
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;
    const static int SELFLOG_MAX_SIZE = 1024000000;
    const static int SELFLOG_SIZE_CHECK_TIMEVAL = 10;

public:
    CLogSvrProc();
    ~CLogSvrProc();

    int Init(const char *pszConfFile);
    int Run();

private:
    CLogAllConf m_objLogConf;
    mmlib::CShmQueue m_objLogQueue;
    mmlib::CShareMem m_objLogConfShm;

    pthread_mutex_t m_mtMutexCAO;
    LogHeader m_stLoagHeader;
    char m_szLogFromQueueBuff[sizeof(LogHeader)+LOG_LENGTH_MAX+1];

    void * m_pvModuleLogFilePMem;
    mmlib::CHashList<CModuleName, LogFilePInfo> m_objModuleLogFilePHash;
    void * m_pvSpecLogIDFilePMem;
    mmlib::CHashList<unsigned int, LogFilePInfo> m_objSpecLogIDFilePHash;
    void * m_pvSpecUserIDFilePMem;
    mmlib::CHashList<unsigned int, LogFilePInfo> m_objSpecUserIDFilePHash;

    int m_iSleepTimeVal;
    int m_iMaxModuleNum;
    int m_iSizeCheckTimeVal;

    //用于logsvr写自己的日志
    void SelfLog(int iLogID, unsigned long long ullUserID, int iPriority, char* pszFormat, ...);
    int m_iSelfLogLevel;
    FILE *m_pfSelfLogFile;
    int m_iSelfLogTime;
    time_t m_tSelfLogLastSizeCheckTime;

    inline bool needToAgent(int logId){return mset_neededLogId.count(logId);}
    void toAgent(int logId, const char *pszLog, int len);
    int getJsonConf(const char *pszJsonConf);
    std::set<int> mset_neededLogId;
    mmlib::CShmQueue logAgentQueue;
    int mi_fileHandle;
    int mi_fileNo;
    time_t mt_checkTime;

private:
    const char *GetLogHeadTime(time_t tSec, time_t tUsec);
    const char *GetFileNameTime(time_t tSec);
    void CheckDir(const char * szPath);
    void CheckDirAll(const char * szPath);
    int CheckFileSize(FILE *fpFile, int iFileSize);
    int RenameFile(const char *pszFileName, int iMaxFileNum);

    /*
     * @brief 将m_szLogFromQueueBuff的内容写入到stLogFilePInfo对应的文件中
     * @param stLogFilePInfo 记录了每个日志文件（info debug trace warn error）对应的FILE*指针
     * @param stLogOutputConf 记录了日志输出的目录配置
     *
     * @return 0-正常写入，不需要更新stLogFilePInfo
     *         1-正常写入，需要更新stLogFilePInfo
     *         其他-写入失败
     */
    int WriteLogToFile(LogFilePInfo &stLogFilePInfo, const LogOutputConf &stLogOutputConf);
    FILE *OpenFile(const char *pszFileName, const char *pszMode);
};

#endif
