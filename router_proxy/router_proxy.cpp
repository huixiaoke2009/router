
#include <functional>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <dlfcn.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#include "router_proxy.h"
#include "ini_file/ini_file.h"
#include "router_header.h"
#include "cmd.h"

const int MAX_EPOLL_RET_EVENTS = 1024;

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

CRouterProxy::CRouterProxy()
{
    m_ServerID = 0;
    m_EpollFD = -1;
    m_RouterNum = 0;
    m_pProcessBuff = NULL;
}

CRouterProxy::~CRouterProxy()
{
    if (m_EpollFD >= 0)
    {
        close(m_EpollFD);
        m_EpollFD = -1;
    }

    if(m_pProcessBuff)
    {
        free(m_pProcessBuff);
    }
}


int CRouterProxy::Init(const char *pConfFile)
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

    int ProxyReqShmKey = 0;
    int ProxyReqShmSize = 0;

    int ProxyRspShmKey = 0;
    int ProxyRspShmSize = 0;
    
    if (IniFile.IsValid())
    {
        IniFile.GetInt("PROXY", "ServerID", 0, &m_ServerID);
        IniFile.GetInt("PROXY", "RouterNum", 0, &m_RouterNum);
        IniFile.GetInt("PROXY", "ProxyReqShmKey", 0, &ProxyReqShmKey);
        IniFile.GetInt("PROXY", "ProxyReqShmSize", 0, &ProxyReqShmSize);
        IniFile.GetInt("PROXY", "ProxyRspShmKey", 0, &ProxyRspShmKey);
        IniFile.GetInt("PROXY", "ProxyRspShmSize", 0, &ProxyRspShmSize);
        
        IniFile.GetString("LOG", "ModuleName", "proxy", ModuleName, sizeof(ModuleName));
        IniFile.GetInt("LOG", "LogLocal", 1, &LogLocal);
        IniFile.GetInt("LOG", "LogLevel", 3, &LogLevel);
        IniFile.GetString("LOG", "LogPath", "/dev/null", LogPath, sizeof(LogPath));
    }
    else
    {
        printf("ERR:conf file [%s] is not valid\n", pConfFile);
        return -1;
    }
    
    if(m_ServerID == 0)
    {
        printf("ERR:ROUTER/ServerID is not valid\n");
        return -1;
    }
    
    if(m_RouterNum == 0 || m_RouterNum > XY_MAX_ROUTER_NUM)
    {
        printf("ERR:ROUTER/RouterNum is not valid\n");
        return -1;
    }
    
    if (ModuleName[0] == 0)
    {
        printf("ERR:LOG/ModuleName is not valid\n");
        return -1;
    }

    OpenLog(CStrTool::Format("%s_%d", ModuleName, m_ServerID).c_str());
    if (LogLocal == 1)
    {
        SetLogLocal(1, LogLevel, LogPath);
    }
    
    if (0 == ProxyReqShmKey || 0 == ProxyReqShmSize)
    {
        printf("Error 0 == ProxyReqShmKey(%x) || 0 == ProxyReqShmSize(%d)", ProxyReqShmKey, ProxyReqShmSize);
        return -1;
    }

    if (0 == ProxyRspShmKey || 0 == ProxyRspShmSize)
    {
        printf("Error 0 == LoginRspShmKey(%x) || 0 == LoginRspShmSize(%d)", ProxyRspShmKey, ProxyRspShmSize);
        return -1;
    }
    
    Ret = m_ProxyReqQueue.Init(ProxyReqShmKey, ProxyReqShmSize);
    if (Ret != 0)
    {
        printf("ERR:init m_ProxyReqQueue failed, key=%d, size=%d, err=%s\n",
                ProxyReqShmKey, ProxyReqShmSize, m_ProxyReqQueue.GetErrMsg());
        return -1;
    }

    printf("init m_ProxyReqQueue succ, key=0x%x, size=%u\n", ProxyReqShmKey, ProxyReqShmSize);
    
    Ret = m_ProxyRspQueue.Init(ProxyRspShmKey, ProxyRspShmSize);
    if (Ret != 0)
    {
        printf("ERR:init m_ProxyRspQueue failed, key=%d, size=%d, err=%s\n",
                ProxyRspShmKey, ProxyRspShmSize, m_ProxyRspQueue.GetErrMsg());
        return -1;
    }
    
    printf("init m_ProxyRspQueue succ, key=0x%x, size=%u\n", ProxyRspShmKey, ProxyRspShmSize);
    
    //分配空间
    if (!m_pProcessBuff)
    {
        m_pProcessBuff = (char *)malloc(XY_PKG_MAX_LEN);
    }

    //创建EPOLL
    m_EpollFD = epoll_create(XY_MAX_ROUTER_NUM);
    if (m_EpollFD == -1)
    {
        printf("ERR:epoll create failed|%d|%d|%s\n", XY_MAX_ROUTER_NUM, errno, strerror(errno));
        return -1;
    }
    
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        char RouterIP[16] = {0};
        int RouterPort = 0;
        unsigned int RouterSvrID = 0;
        
        IniFile.GetString("PROXY", CStrTool::Format("RouterIP_%d", i).c_str(), "", RouterIP, sizeof(RouterIP));
        IniFile.GetInt("PROXY", CStrTool::Format("RouterPort_%d", i).c_str(), 0, &RouterPort);
        IniFile.GetInt("PROXY", CStrTool::Format("RouterSvrID_%d", i).c_str(), 0, (int *)&RouterSvrID);
        
        m_RouterInfo[i].RouterIP = inet_addr(RouterIP);
        m_RouterInfo[i].RouterPort = RouterPort;
        m_RouterInfo[i].RouterSvrID = RouterSvrID;
        m_RouterInfo[i].SocketID = -1;
        
        if (m_RouterInfo[i].RouterIP == 0 || m_RouterInfo[i].RouterPort == 0 || m_RouterInfo[i].RouterSvrID == 0)
        {
            printf("ROUTER/RouterIP or RouterPort or RouterSvrID is not valid|%d|%d|%d", 
                    m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, m_RouterInfo[i].RouterSvrID);
            return -1;
        }
        
        ConnectRouter(RouterSvrID);
    }

    printf("svr init success\n");
    
    return 0;
}


int CRouterProxy::Run()
{
    int Ret = 0;
    
    struct epoll_event RetEvent[MAX_EPOLL_RET_EVENTS];
    int RetEventNum = 0;

    char *pRecvBuff = (char *)malloc(XY_MAXBUFF_LEN);
    int RecvLen = 0;

    time_t LastTime = time(NULL);
    
    while(!StopFlag)
    {
        int EmptyFlag = 1;  //没有数据标志位

        RetEventNum = epoll_wait(m_EpollFD, RetEvent, MAX_EPOLL_RET_EVENTS, 1);
        if(RetEventNum > 0)
        {
            EmptyFlag = 0;
            for(int i = 0; i < RetEventNum; ++i)
            {
                XF_LOG_TRACE(0, 0, "epoll_wait return, event_num=%d, cur_event=%d, event=%d, conn_pos=%u", RetEventNum, i, RetEvent[i].events, RetEvent[i].data.u32);
                unsigned int RouterSvrID = RetEvent[i].data.u32;
                if(RetEvent[i].events & EPOLLERR)
                {
                    XF_LOG_WARN(0, 0, "EPOLL return EPOLLERR|%u", RouterSvrID);
                    DisconnetRouter(RouterSvrID);
                    continue;
                }

                if(RetEvent[i].events & EPOLLHUP)
                {
                    XF_LOG_WARN(0, 0, "EPOLL return EPOLLHUP|%u", RouterSvrID);
                    DisconnetRouter(RouterSvrID);
                    continue;
                }

                if(RetEvent[i].events & EPOLLIN)
                {
                    RecvLen = XY_MAXBUFF_LEN;
                    Ret = Recv(RouterSvrID, pRecvBuff, (unsigned int*)&RecvLen);
                    if(Ret == 1)
                    {
                        //连接被终止
                        DisconnetRouter(RouterSvrID);
                        continue;
                    }
                    else if(Ret < 0)
                    {
                        //接收失败
                        XF_LOG_WARN(0, 0, "%u|recv failed", RouterSvrID);
                        DisconnetRouter(RouterSvrID);
                        continue;
                    }
                    
                    if (RecvLen > XY_MAXBUFF_LEN)
                    {
                        XF_LOG_WARN(0, 0, "%u|%u|recv len is not valid", RouterSvrID, RecvLen);
                        DisconnetRouter(RouterSvrID);
                        continue;
                    }

                    XF_LOG_TRACE(0, 0, "Recv|%d|%s", RecvLen, CStrTool::Str2Hex(pRecvBuff, RecvLen));
                    
                    char *pCurBuffPos = pRecvBuff;
                    while(RecvLen > 0)
                    {
                        int ProcLen = ProcessPkg(RouterSvrID, pCurBuffPos, RecvLen);
                        if ((ProcLen > RecvLen)||(ProcLen == 0))
                        {
                            //需要暂存数据
                            //ProcLen返回0表示根据现有的字段还不能计算长度
                            if (AddRecvData(RouterSvrID, pCurBuffPos, RecvLen) != 0)
                            {
                                XF_LOG_WARN(0, 0, "%u|%u|store recv data failed", RouterSvrID, RecvLen);
                                DisconnetRouter(RouterSvrID);
                            }
                            
                            RecvLen = 0;
                        }
                        else if (ProcLen <= -1)
                        {
                            //直接关闭连接
                            XF_LOG_WARN(0, 0, "%u|%u|handle_input ret -1", RouterSvrID, RecvLen);
                            DisconnetRouter(RouterSvrID);
                            RecvLen = 0;
                        }
                        else
                        {
                            //数据包处理成功
                            XF_LOG_TRACE(0, 0, "process pkg succ");

                            pCurBuffPos += ProcLen;
                            RecvLen -= ProcLen;
                        }
                    }
                    
                    continue;
                }

                if(RetEvent[i].events & EPOLLOUT)
                {
                    XF_LOG_WARN(0, 0, "can't not be here, %u", RouterSvrID);
                    continue;
                }
            }
        }
        else if (RetEventNum == -1)
        {
            if(errno == EINTR)
            {
                XF_LOG_WARN(0, 0, "epoll_wait signal interruption");
            }
            else
            {
                XF_LOG_WARN(0, 0, "epoll_wait failed, errno=%d, errmsg=%s", errno, strerror(errno));
            }
        }
        else
        {
            //没有任何事件
        }
        
        
        // 检查队列是否有数据要发送
        RecvLen = XY_MAXBUFF_LEN;
        Ret = m_ProxyReqQueue.OutQueue(pRecvBuff, &RecvLen);
        if (Ret == m_ProxyReqQueue.E_SHM_QUEUE_EMPTY)
        {
            
        }
        else if(Ret != 0)
        {
             //出错了
             XF_LOG_WARN(0, 0, "out queue failed, ret=%d, errmsg=%s", Ret, m_ProxyReqQueue.GetErrMsg());
        }
        else
        {
            EmptyFlag = 0;
            Forward2Router(pRecvBuff, RecvLen);
        }
        
        // 检查链接
        time_t NowTime = time(NULL);
        if(NowTime - LastTime >= 10)
        {
            LastTime = NowTime;
            CheckConnect();
        }
        
        if(EmptyFlag)
        {
            usleep(1000);
        }
    }

    return 0;
}

int CRouterProxy::ConnectRouter(unsigned int RouterSvrID)
{
    for(int i = 0; i < XY_MAX_ROUTER_NUM && i < m_RouterNum; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == RouterSvrID)
        {
            if(m_RouterInfo[i].SocketID != -1)
            {
                close(m_RouterInfo[i].SocketID);
                m_RouterInfo[i].SocketID = -1;
            }
            
            int SocketID = socket(AF_INET, SOCK_STREAM, 0);
            if (-1 == SocketID)
            {
                XF_LOG_WARN(0, 0,"create socket faile|%d|%s", errno, strerror(errno));
                return -1;
            }
            
            int val = 0;
            if ((val = ::fcntl(SocketID, F_GETFL, 0)) < 0)
            {
                XF_LOG_WARN(0, 0,"fcntl (GET) failed, svrid=%d|%d|%d|%d|%s", RouterSvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
            }

            val |= O_NONBLOCK;
            if (::fcntl(SocketID, F_SETFL, val) < 0)
            {
                XF_LOG_WARN(0, 0,"fcntl (SET) failed, svrid=%d|%d|%d|%d|%s", RouterSvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
                return -1;
            }
            
            struct sockaddr_in stSockAddr;
            stSockAddr.sin_family = AF_INET;
            stSockAddr.sin_port = htons(m_RouterInfo[i].RouterPort);
            stSockAddr.sin_addr.s_addr = m_RouterInfo[i].RouterIP;

            if (-1 == connect(SocketID, (struct sockaddr *)&stSockAddr, sizeof(stSockAddr)))
            {
                if(errno != EINPROGRESS)
                {
                    XF_LOG_WARN(0, 0, "connect failed|%d", RouterSvrID);
                    return -1;
                }
                
                timeval tm;
                tm.tv_sec = 0;
                tm.tv_usec = 10000;
                fd_set set;
                FD_ZERO(&set);
                FD_SET(SocketID, &set);
                if(select(SocketID+1, NULL, &set, NULL, &tm) > 0)
                {
                    // 进入这个分支表示在规定的时间内套接字可读可写或者出错
                    int error = -1;
                    int len = sizeof(int);
                    
                    // 连接成功error为0
                    getsockopt(SocketID, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
                    if(error != 0)
                    {
                        close(SocketID);
                        XF_LOG_WARN(0, 0,"connect faile|srvid=%d|%d|%d|%d|%s", RouterSvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
                        return -1;
                    }
                }
                else
                {
                    // 进入这个分支表示超时或者出错
                    close(SocketID);
                    XF_LOG_WARN(0, 0, "select return error|%d", RouterSvrID);
                    return -1;
                }
            }
            
            XF_LOG_INFO(0 , 0, "ConnetRouter:%d", RouterSvrID);
            
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            ev.data.u32 = RouterSvrID;

            if(epoll_ctl(m_EpollFD, EPOLL_CTL_ADD, SocketID, &ev) != 0)
            {
                XF_LOG_WARN(0, 0, "epoll add failed|%d|%s", errno, strerror(errno));
                close(SocketID);
                return -1;
            }
            
            m_RouterInfo[i].SocketID = SocketID;
            
            SendRegisterMsg(RouterSvrID);

            break;
        }
    }
    
    return 0;
}


int CRouterProxy::DisconnetRouter(unsigned int RouterSvrID)
{
    for(int i = 0; i < XY_MAX_ROUTER_NUM && i < m_RouterNum; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == RouterSvrID)
        {
            if(m_RouterInfo[i].SocketID != -1)
            {
                epoll_ctl(m_EpollFD, EPOLL_CTL_DEL, m_RouterInfo[i].SocketID, NULL);
                close(m_RouterInfo[i].SocketID);
                m_RouterInfo[i].SocketID = -1;
                m_RouterInfo[i].strRecieveBuff.clear();
                
                XF_LOG_INFO(0 , 0, "DisconnetRouter:%d", RouterSvrID);
            }
            
            break;
        }
    }
    
    return 0;
}

void CRouterProxy::CheckConnect()
{
    for(int i = 0; i < XY_MAX_ROUTER_NUM && i < m_RouterNum; i++)
    {
        if(m_RouterInfo[i].SocketID == -1)
        {
            ConnectRouter(m_RouterInfo[i].RouterSvrID);
        }
        else
        {
            SendHeartbeatMsg(m_RouterInfo[i].RouterSvrID);
        }
    }
}


int CRouterProxy::Send(unsigned int RouterSvrID, const char *pSendBuff, int SendBuffLen)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == RouterSvrID)
        {
            int SocketID =  m_RouterInfo[i].SocketID;
            int BytesSent = 0;
            int WriteBytes = 0;
            while(true)
            {
                WriteBytes = write(SocketID, (unsigned char *)pSendBuff + BytesSent, SendBuffLen - BytesSent);
                if (WriteBytes > 0)
                {
                    BytesSent += WriteBytes;
                    
                    XF_LOG_DEBUG(0, 0, "Send SendBuffLen=%u|WriteBytes=%u|BytesSent=%d", SendBuffLen, WriteBytes, BytesSent);
                    if (BytesSent < SendBuffLen)
                    {
                        continue;
                    }
                    else
                    {
                        return 0;
                    }
                }
                else if (errno == EINTR || errno == EAGAIN)
                {
                    XF_LOG_WARN(0, 0,"Send WriteBytes=%d|errno=%d|error: %s", WriteBytes, errno, strerror(errno));
                    usleep(10);
                    continue;
                }
                else
                {
                    DisconnetRouter(RouterSvrID);
                    XF_LOG_WARN(0, 0,"Send WriteBytes=%d|errno=%d|error: %s", WriteBytes, errno, strerror(errno));
                    return -1;
                }
            }
            
            break;
        }
    }
    
    return -1;
}

int CRouterProxy::Recv(unsigned int RouterSvrID, char* pRecvBuff, unsigned int* pLen)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == RouterSvrID)
        {
            return m_RouterInfo[i].Recv(pRecvBuff, pLen);
        }
    }
    
    return 0;
}

int CRouterProxy::AddRecvData(unsigned int RouterSvrID, const char *pBuff, unsigned int Len)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == RouterSvrID)
        {
            return m_RouterInfo[i].AddRecvData(pBuff, Len);
        }
    }
    
    return 0;
}

int CRouterProxy::Forward2Router(char *pSendBuff, int SendBuffLen, int RouterSvrID /*= 0*/)
{
    char aszsSendBuff[10240] = {0};
    int SendLen = sizeof(aszsSendBuff);
    char* pBuff = aszsSendBuff;
    if(SendLen < SendBuffLen)
    {
        pBuff = (char*)malloc(SendBuffLen);
        if(pBuff == NULL)
        {
            XF_LOG_WARN(0, 0, "malloc failed");
            return -1;
        }
    }
    
    AppHeader* pAppHeader = (AppHeader*)pSendBuff;
    
    RouterHeader CurHeader;
    int HeaderLen = CurHeader.GetHeaderLen();
    CurHeader.PkgLen = SendBuffLen + HeaderLen;
    CurHeader.UserID = pAppHeader->UserID;
    CurHeader.CmdID = pAppHeader->CmdID;
    CurHeader.SrcID = m_ServerID;
    CurHeader.DstID = pAppHeader->DstID;
    CurHeader.SN = 0;
    CurHeader.Ret = 0;
    
    CurHeader.Write(pBuff);
    memcpy(pBuff+HeaderLen, pSendBuff, SendBuffLen);
    
    if(RouterSvrID == 0)
    {
        vector<int> vctResult;
        for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
        {
            if(m_RouterInfo[i].SocketID != -1)
            {
                vctResult.push_back(m_RouterInfo[i].RouterSvrID);
            }
        }
        
        if(vctResult.size() == 0)
        {
            XF_LOG_WARN(0, 0, "all router is down");
            return -1;
        }
    
        RouterSvrID = vctResult[time(NULL)%vctResult.size()];
    }
    
    int Ret = Send(RouterSvrID, pBuff, SendBuffLen+HeaderLen);
    if(pBuff != aszsSendBuff)
    {
        free(pBuff);
    }
    
    return Ret;
}


int CRouterProxy::Send2RouterByMsg(unsigned int RouterSvrID, unsigned int CmdID, const google::protobuf::Message &Rsp)
{
    RouterHeader CurHeader;
    int HeaderLen = CurHeader.GetHeaderLen();
    char acSendBuff[10240] = {0};
    int BufLen = sizeof(acSendBuff) - HeaderLen;
    char *pSendData = acSendBuff + HeaderLen;
    if (!Rsp.SerializeToArray(pSendData, BufLen))
    {
        XF_LOG_WARN(0, 0, "SerializeToArray failed, %s", Rsp.ShortDebugString().c_str());
        return -1;
    }
    
    CurHeader.PkgLen = HeaderLen + Rsp.ByteSize();
    CurHeader.UserID = 0;
    CurHeader.CmdID = CmdID;
    CurHeader.DstID = RouterSvrID;
    CurHeader.SrcID = m_ServerID;
    CurHeader.SN = 0;
    CurHeader.Ret = 0;
    CurHeader.Write(acSendBuff);
    
    return Send(RouterSvrID, acSendBuff, HeaderLen + Rsp.ByteSize());
}


int CRouterProxy::SendHeartbeatMsg(unsigned int RouterSvrID)
{
    router::HeartbeatReq CurReq;
    CurReq.set_svrid(m_ServerID);
    return Send2RouterByMsg(RouterSvrID, Cmd_Heartbeat, CurReq);
}


int CRouterProxy::SendRegisterMsg(unsigned int RouterSvrID)
{
    router::RegisterSvrReq CurReq;
    CurReq.set_svrid(m_ServerID);
    return Send2RouterByMsg(RouterSvrID, Cmd_RegisterSvr, CurReq);
}

int CRouterProxy::ProcessPkg(unsigned int RouterSvrID, const char* pCurBuffPos, int RecvLen)
{
    int Ret = 0; 
    
    RouterHeader CurHeader;
    
    int HeaderLen = CurHeader.GetHeaderLen();
    // 接受到的数据不够包头长度
    if(RecvLen < HeaderLen)
    {
        return HeaderLen;
    }

    CurHeader.Read(pCurBuffPos);
    
    int PkgLen = CurHeader.PkgLen;
    //接收到的数据不够一个包
    if(PkgLen > RecvLen)
    {
        return PkgLen;
    }

    //判断包长是否异常
    if(PkgLen > XY_PKG_MAX_LEN)
    {
        return -1;
    }
    
    if((unsigned int)m_ServerID != CurHeader.DstID && CurHeader.DstID % XY_GROUP_MOD != 0)
    {
        XF_LOG_WARN(0, 0, "this package is not mine, %u|%u|%u", m_ServerID, CurHeader.DstID, CurHeader.SrcID);
        return PkgLen;
    }
    
    // ------------------------------ 系统通讯协议处理 begin ---------------------------
    if(CurHeader.CmdID == Cmd_RegisterSvr)
    {
        router::RegisterSvrRsp CurRegisterSvrRsp;
        if (!CurRegisterSvrRsp.ParseFromArray(pCurBuffPos+HeaderLen, PkgLen-HeaderLen))
        {
            XF_LOG_WARN(0, 0, "Parse Pkg failed, CmdID=%d", CurHeader.CmdID);
            return -1;
        }
        
        int ret = CurRegisterSvrRsp.ret();
        if(ret != 0)
        {
            XF_LOG_WARN(0, 0, "RegisterSvr failed, %u|%d", RouterSvrID, ret);
            return -1;
        }
        
        XF_LOG_INFO(0, 0, "RegisterSvr success, %u", RouterSvrID);

        return PkgLen;
    }
    
    // ------------------------------ 系统通讯协议处理 end -----------------------------

    // 入队列，这里要去掉RouterHeader
    Ret = m_ProxyRspQueue.InQueue(pCurBuffPos+HeaderLen, PkgLen-HeaderLen);
    if (Ret == m_ProxyRspQueue.E_SHM_QUEUE_FULL)
    {
        XF_LOG_WARN(0, 0, "m_ProxyRspQueue in queue full");
    }
    else if (Ret != 0)
    {
        XF_LOG_WARN(0, 0,"m_ProxyRspQueue in queue fail|ret=%d",Ret);
    }
    else
    {
        //入queue成功
        XF_LOG_TRACE(0, 0, "m_ProxyRspQueue in queue success");
    }
    
    return PkgLen;
}