
#include <functional>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <dlfcn.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <vector>

#include "app_proc.h"
#include "log/log.h"
#include "util/util.h"
#include "ini_file/ini_file.h"

#include "router_header.h"
#include "cmd.h"
#include "app.pb.h"

using namespace std;
using namespace mmlib;

bool StopFlag = false;
bool ReloadFlag = false;

static void SigHandler(int iSigNo)
{
    XF_LOG_INFO(0, 0, "%s get signal %d", __func__, iSigNo);
    switch(iSigNo)
    {
    case SIGUSR1:
        StopFlag = true;
        break;

    case SIGUSR2:
        ReloadFlag = true;
        break;

    default:
        break;
    }

    return;
}


CApp::CApp()
{

}

CApp::~CApp()
{

}


int CApp::Init(const char *pConfFile)
{
    //安装信号量
    StopFlag = false;
    ReloadFlag = false;
    struct sigaction stSiga;
    memset(&stSiga, 0, sizeof(stSiga));
    stSiga.sa_handler = SigHandler;

    sigaction(SIGCHLD, &stSiga, NULL);
    sigaction(SIGUSR1, &stSiga, NULL);
    sigaction(SIGUSR2, &stSiga, NULL);

    //忽略信号量
    struct sigaction stSiga2;
    memset(&stSiga2, 0, sizeof(stSiga2));
    stSiga2.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &stSiga2, NULL);

    int Ret = 0;

    //读取配置文件
    CIniFile IniFile(pConfFile);

    char ModuleName[256] = {0};
    int LogLocal = 0;
    int LogLevel = 0;
    char LogPath[1024] = {0};

    int ProxyShmKey = 0;
    int ProxyShmSize = 0;

    int ConnShmKey = 0;
    int ConnShmSize = 0;
    
    if (IniFile.IsValid())
    {
        IniFile.GetInt("APP", "DstID", 0, &m_DstID);
        IniFile.GetInt("APP", "ProxyShmKey", 0, &ProxyShmKey);
        IniFile.GetInt("APP", "ProxyShmSize", 0, &ProxyShmSize);
        IniFile.GetInt("APP", "ConnShmKey", 0, &ConnShmKey);
        IniFile.GetInt("APP", "ConnShmSize", 0, &ConnShmSize);
        
        IniFile.GetString("LOG", "ModuleName", "app", ModuleName, sizeof(ModuleName));
        IniFile.GetInt("LOG", "LogLocal", 1, &LogLocal);
        IniFile.GetInt("LOG", "LogLevel", 3, &LogLevel);
        IniFile.GetString("LOG", "LogPath", "/dev/null", LogPath, sizeof(LogPath));
    }
    else
    {
        printf("ERR:conf file [%s] is not valid\n", pConfFile);
        return -1;
    }
    
    if (ModuleName[0] == 0)
    {
        printf("ERR:LOG/ModuleName is not valid\n");
        return -1;
    }

    OpenLog(ModuleName);
    if (LogLocal == 1)
    {
        SetLogLocal(1, LogLevel, LogPath);
    }
    
    if (0 == ProxyShmKey || 0 == ProxyShmSize)
    {
        printf("Error 0 == ProxyShmKey(%x) || 0 == ProxyShmSize(%d)", ProxyShmKey, ProxyShmSize);
        return -1;
    }

    if (0 == ConnShmKey || 0 == ConnShmSize)
    {
        printf("Error 0 == ConnShmKey(%x) || 0 == ConnShmSize(%d)", ConnShmKey, ConnShmSize);
        return -1;
    }
    
    Ret = m_ProxyQueue.Init(ProxyShmKey, ProxyShmSize);
    if (Ret != 0)
    {
        printf("ERR:init m_ProxyReqQueue failed, key=%d, size=%d, err=%s\n",
                ProxyShmKey, ProxyShmSize, m_ProxyQueue.GetErrMsg());
        return -1;
    }

    printf("init m_ProxyReqQueue succ, key=0x%x, size=%u\n", ProxyShmKey, ProxyShmSize);
    
    Ret = m_ConnQueue.Init(ConnShmKey, ConnShmSize);
    if (Ret != 0)
    {
        printf("ERR:init m_ConnQueue failed, key=%d, size=%d, err=%s\n",
                ConnShmKey, ConnShmSize, m_ConnQueue.GetErrMsg());
        return -1;
    }
    
    printf("init m_ConnQueue succ, key=0x%x, size=%u\n", ConnShmKey, ConnShmSize);

    printf("svr init success\n");
    
    return 0;
}


int CApp::Run()
{
    int Ret = 0;
    
    char *pRecvBuff = (char *)malloc(XY_MAXBUFF_LEN);
    int RecvLen = XY_MAXBUFF_LEN;
    
    while(true)
    {
        if(m_DstID == 101)
        {
            AppHeader CurHeader;
            int HeaderLen = sizeof(AppHeader);
            
            RecvLen = XY_MAXBUFF_LEN - HeaderLen;
            
            app::MyData CurReq;
            CurReq.set_data1(m_DstID);
            CurReq.set_data2(CStrTool::Format("%d", m_DstID*10));
            CurReq.mutable_data3()->set_kk(m_DstID*100);
            CurReq.mutable_data3()->set_ss(CStrTool::Format("%d", m_DstID*1000));
            if (!CurReq.SerializeToArray(pRecvBuff+HeaderLen, RecvLen))
            {
                printf("SerializeToArray failed, %s\n", CurReq.ShortDebugString().c_str());
                return -1;
            }
            
            RecvLen = CurReq.ByteSize();
            CurHeader.UserID = 1048811;
            CurHeader.CmdID = 0x10010001;
            CurHeader.DstID = 400;
            memcpy(pRecvBuff, &CurHeader, HeaderLen);
            
            m_ProxyQueue.InQueue(pRecvBuff, RecvLen+HeaderLen);
            if (Ret == m_ProxyQueue.E_SHM_QUEUE_FULL)
            {
                printf("m_ProxyQueue in queue full\n");
            }
            else if (Ret != 0)
            {
                printf("m_ProxyQueue in queue fail|ret=%d\n",Ret);
            }
            else
            {
                //入queue成功
                XF_LOG_TRACE(0, 0, "m_ProxyQueue in queue success\n");
            }
            
            usleep(100000);
        }
        else
        {
            // recieve
            RecvLen = XY_MAXBUFF_LEN;
            Ret = m_ConnQueue.OutQueue(pRecvBuff, &RecvLen);
            if (Ret == m_ConnQueue.E_SHM_QUEUE_EMPTY)
            {
                
            }
            else if(Ret != 0)
            {
                 //出错了
                 printf("out queue failed, ret=%d, errmsg=%s\n", Ret, m_ConnQueue.GetErrMsg());
            }
            else
            {
                app::MyData CurRsp;
                if (!CurRsp.ParseFromArray(pRecvBuff+sizeof(AppHeader), RecvLen-sizeof(AppHeader)))
                {
                    printf("Parse Pkg failed\n");
                    return -1;
                }
                
                printf("msg:%ld:%s\n", time(NULL), CurRsp.ShortDebugString().c_str());
            }
            
            usleep(1000);
        }
        
    }
    
    return 0;
}
