
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
#include "app.pb.h"

#include "cmd.h"
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
    
    if (IniFile.IsValid())
    {
        IniFile.GetInt("APP", "DstID", 0, &m_DstID);
        
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
    
    Ret = proxy.Init(pConfFile);
    if(Ret != 0)
    {
        printf("proxy init failed, %d", Ret);
        return -1;
    }
    
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
            CurHeader.DstID = 200;
            memcpy(pRecvBuff, &CurHeader, HeaderLen);
            
            proxy.Forward2Router(pRecvBuff, RecvLen+HeaderLen);
            
            usleep(100);
        }
        else
        {
            // recieve
            char* pLoopBuff = (char*)malloc(XY_MAXBUFF_LEN);
            unsigned int LoopLen = XY_MAXBUFF_LEN;
            
            Ret = proxy.EventLoop(pLoopBuff, LoopLen);
            
            if(Ret == -1)
            {
                printf("error\n");
            }
            
            if(Ret == 1)
            {
                AppHeader* pHeader = (AppHeader*)pLoopBuff;
                if(pHeader->CmdID == Cmd_RegisterSvr)
                {
                    printf("Register recieve\n");
                }
                else if(pHeader->CmdID == Cmd_Heartbeat)
                {
                    printf("Heartbeat recieve\n");
                }
                else
                {
                    app::MyData CurRsp;
                    if (!CurRsp.ParseFromArray(pLoopBuff+sizeof(AppHeader), LoopLen-sizeof(AppHeader)))
                    {
                        printf("Parse Pkg failed\n");
                    }
                    else
                    {
                        printf("msg:%ld:%s\n", time(NULL), CurRsp.ShortDebugString().c_str());
                    }
                }
            }
            
            if(pLoopBuff)
            {
                free(pLoopBuff);
            }
            
            usleep(1000000);
        }
        
    }
    
    return 0;
}
