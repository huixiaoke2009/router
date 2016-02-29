
#include <functional>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <dlfcn.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#include "proxy.h"
#include "router_header.h"
#include "cmd.h"
#include "ini_file/ini_file.h"

using namespace std;
using namespace mmlib;


CProxy::CProxy()
{
    m_ServerID = 0;
    m_EpollFD = -1;
    m_RouterNum = 0;
    m_LastTime = 0;
}

CProxy::~CProxy()
{
    if (m_EpollFD >= 0)
    {
        close(m_EpollFD);
        m_EpollFD = -1;
    }
}


int CProxy::Init(const char *pConfFile)
{
    //读取配置文件
    CIniFile IniFile(pConfFile);
    
    if (IniFile.IsValid())
    {
        IniFile.GetInt("PROXY", "ServerID", 0, &m_ServerID);
        IniFile.GetInt("PROXY", "RouterNum", 0, &m_RouterNum);
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
        int RouterSvrID = 0;
        
        IniFile.GetString("PROXY", CStrTool::Format("RouterIP_%d", i).c_str(), "", RouterIP, sizeof(RouterIP));
        IniFile.GetInt("PROXY", CStrTool::Format("RouterPort_%d", i).c_str(), 0, &RouterPort);
        IniFile.GetInt("PROXY", CStrTool::Format("RouterSvrID_%d", i).c_str(), 0, &RouterSvrID);
        
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
    
    m_LastTime = time(NULL);

    printf("svr init success\n");
    
    return 0;
}


/*
 * return: 0表示没数据或者数据包不完整，-1表示错误，1表示收到完整数据包  
*/
int CProxy::EventLoop(char* pLoopBuff, unsigned int& LoopLen)
{
    int Ret = 0;
    
    struct epoll_event RetEvent[1];
    int RetEventNum = 0;

    char aszRecieveBuff[XY_MAXBUFF_LEN];
    char *pRecvBuff = aszRecieveBuff;
    int RecvLen = 0;

    // 检查链接
    time_t NowTime = time(NULL);
    if(NowTime - m_LastTime >= 10)
    {
        m_LastTime = NowTime;
        CheckConnect();
    }
    
    int ErrFlag = 0;
    
    do
    {
        RetEventNum = epoll_wait(m_EpollFD, RetEvent, 1, 0);
        if(RetEventNum > 0)
        {
            XF_LOG_TRACE(0, 0, "epoll_wait return, event_num=%d, cur_event=%d, event=%d, conn_pos=%u", RetEventNum, 0, RetEvent[0].events, RetEvent[0].data.u32);
            int SvrID = RetEvent[0].data.u32;
            if(RetEvent[0].events & EPOLLERR)
            {
                XF_LOG_WARN(0, 0, "EPOLL return EPOLLERR|%u", SvrID);
                DisconnetRouter(SvrID);
                ErrFlag = -1;
                break;
            }

            if(RetEvent[0].events & EPOLLHUP)
            {
                XF_LOG_WARN(0, 0, "EPOLL return EPOLLHUP|%u", SvrID);
                DisconnetRouter(SvrID);
                ErrFlag = -1;
                break;
            }
            
            if(RetEvent[0].events & EPOLLOUT)
            {
                XF_LOG_WARN(0, 0, "can't not be here, %u", SvrID);
                ErrFlag = -1;
                break;
            }

            if(RetEvent[0].events & EPOLLIN)
            {
                RecvLen = XY_MAXBUFF_LEN;
                Ret = Recv(SvrID, pRecvBuff, (unsigned int*)&RecvLen);
                if(Ret == 1)
                {
                    //连接被终止
                    DisconnetRouter(SvrID);
                    ErrFlag = -1;
                    break;
                }
                else if(Ret < 0)
                {
                    //接收失败
                    XF_LOG_WARN(0, 0, "%u|recv failed", SvrID);
                    DisconnetRouter(SvrID);
                    ErrFlag = -1;
                    break;
                }
                
                if (RecvLen > XY_MAXBUFF_LEN)
                {
                    XF_LOG_WARN(0, 0, "%u|%u|recv len is not valid", SvrID, RecvLen);
                    DisconnetRouter(SvrID);
                    ErrFlag = -1;
                    break;
                }

                XF_LOG_TRACE(0, 0, "Recv|%d|%s", RecvLen, CStrTool::Str2Hex(pRecvBuff, RecvLen));
                
                char *pCurBuffPos = pRecvBuff;
                
                int ProcLen = ProcessPkg(SvrID, pCurBuffPos, RecvLen, pLoopBuff, LoopLen);
                if ((ProcLen > RecvLen)||(ProcLen == 0))
                {
                    //需要暂存数据
                    //ProcLen返回0表示根据现有的字段还不能计算长度
                    if (AddRecvData(SvrID, pCurBuffPos, RecvLen) != 0)
                    {
                        XF_LOG_WARN(0, 0, "%u|%u|store recv data failed", SvrID, RecvLen);
                        DisconnetRouter(SvrID);
                        ErrFlag = -1;
                    }
                    else
                    {
                        ErrFlag = 0;
                    }
                }
                else if (ProcLen <= -1)
                {
                    //直接关闭连接
                    XF_LOG_WARN(0, 0, "%u|%u|handle_input ret -1", SvrID, RecvLen);
                    DisconnetRouter(SvrID);
                    ErrFlag = -1;
                }
                else
                {
                    //数据包处理成功
                    XF_LOG_TRACE(0, 0, "process pkg succ");

                    pCurBuffPos += ProcLen;
                    RecvLen -= ProcLen;
                    
                    if(RecvLen > 0 && AddRecvData(SvrID, pCurBuffPos, RecvLen) != 0)
                    {
                        XF_LOG_WARN(0, 0, "%u|%u|store recv data failed", SvrID, RecvLen);
                        DisconnetRouter(SvrID);
                        ErrFlag = -1;
                    }
                    else
                    {
                        ErrFlag = 1;
                    }
                }
                
                break;
            }
        }
        else if (RetEventNum == -1)
        {
            if(errno == EINTR)
            {
                XF_LOG_WARN(0, 0, "epoll_wait signal interruption");
                ErrFlag = -1;
            }
            else
            {
                XF_LOG_WARN(0, 0, "epoll_wait failed, errno=%d, errmsg=%s", errno, strerror(errno));
                ErrFlag = -1;
            }
            
            break;
        }
        else
        {
            //没有任何事件
            ErrFlag = 0;
            break;
        }
    }while(true);
    
    return ErrFlag;
}



int CProxy::Forward2Router(char *pSendBuff, int SendBuffLen, int SvrID /*= 0*/)
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
    
    if(SvrID == 0)
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
    
        SvrID = vctResult[time(NULL)%vctResult.size()];
    }
    
    int Ret = Send(SvrID, pBuff, SendBuffLen+HeaderLen);
    if(pBuff != aszsSendBuff)
    {
        free(pBuff);
    }
    
    return Ret;
}


int CProxy::ConnectRouter(unsigned int SvrID)
{
    for(int i = 0; i < XY_MAX_ROUTER_NUM && i < m_RouterNum; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == SvrID)
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
                XF_LOG_WARN(0, 0,"fcntl (GET) failed, svrid=%d|%d|%d|%d|%s", SvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
            }

            val |= O_NONBLOCK;
            if (::fcntl(SocketID, F_SETFL, val) < 0)
            {
                XF_LOG_WARN(0, 0,"fcntl (SET) failed, svrid=%d|%d|%d|%d|%s", SvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
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
                    XF_LOG_WARN(0, 0, "connect failed|%d", SvrID);
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
                        XF_LOG_WARN(0, 0,"connect faile|SvrID=%d|%d|%d|%d|%s", SvrID, m_RouterInfo[i].RouterIP, m_RouterInfo[i].RouterPort, errno, strerror(errno));
                        return -1;
                    }
                }
                else
                {
                    // 进入这个分支表示超时或者出错
                    close(SocketID);
                    XF_LOG_WARN(0, 0, "select return error|%d", SvrID);
                    return -1;
                }
            }
            
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            ev.data.u32 = SvrID;

            if(epoll_ctl(m_EpollFD, EPOLL_CTL_ADD, SocketID, &ev) != 0)
            {
                XF_LOG_WARN(0, 0, "epoll add failed|%d|%s", errno, strerror(errno));
                close(SocketID);
                return -1;
            }
            
            m_RouterInfo[i].SocketID = SocketID;
            
            SendRegisterMsg(SvrID);

            break;
        }
    }
    
    return 0;
}


int CProxy::DisconnetRouter(unsigned int SvrID)
{
    for(int i = 0; i < XY_MAX_ROUTER_NUM && i < m_RouterNum; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == SvrID)
        {
            if(m_RouterInfo[i].SocketID != -1)
            {
                epoll_ctl(m_EpollFD, EPOLL_CTL_DEL, m_RouterInfo[i].SocketID, NULL);
                close(m_RouterInfo[i].SocketID);
                m_RouterInfo[i].SocketID = -1;
                m_RouterInfo[i].strRecieveBuff.clear();
            }
            
            break;
        }
    }
    
    return 0;
}

void CProxy::CheckConnect()
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


int CProxy::Send(unsigned int SvrID, const char *pSendBuff, int SendBuffLen)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == SvrID)
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
                    DisconnetRouter(SvrID);
                    XF_LOG_WARN(0, 0,"Send WriteBytes=%d|errno=%d|error: %s", WriteBytes, errno, strerror(errno));
                    return -1;
                }
            }
            
            break;
        }
    }
    
    return -1;
}


int CProxy::Recv(unsigned int SvrID, char* pRecvBuff, unsigned int* pLen)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == SvrID)
        {
            return m_RouterInfo[i].Recv(pRecvBuff, pLen);
        }
    }
    
    return 0;
}


int CProxy::AddRecvData(unsigned int SvrID, const char *pBuff, unsigned int Len)
{
    for(int i = 0; i < m_RouterNum && i < XY_MAX_ROUTER_NUM; i++)
    {
        if(m_RouterInfo[i].RouterSvrID == SvrID)
        {
            return m_RouterInfo[i].AddRecvData(pBuff, Len);
        }
    }
    
    return 0;
}


int CProxy::Send2RouterByMsg(unsigned int SvrID, unsigned int CmdID, const google::protobuf::Message &Rsp)
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
    CurHeader.DstID = SvrID;
    CurHeader.SrcID = m_ServerID;
    CurHeader.SN = 0;
    CurHeader.Ret = 0;
    CurHeader.Write(acSendBuff);
    
    return Send(SvrID, acSendBuff, HeaderLen + Rsp.ByteSize());
}


int CProxy::SendHeartbeatMsg(unsigned int SvrID)
{
    router::HeartbeatReq CurReq;
    CurReq.set_svrid(m_ServerID);
    return Send2RouterByMsg(SvrID, Cmd_Heartbeat, CurReq);
}


int CProxy::SendRegisterMsg(unsigned int SvrID)
{
    router::RegisterSvrReq CurReq;
    CurReq.set_svrid(m_ServerID);
    return Send2RouterByMsg(SvrID, Cmd_RegisterSvr, CurReq);
}


int CProxy::ProcessPkg(unsigned int SvrID, const char* pCurBuffPos, int RecvLen, char* pLoopBuff, unsigned int& LoopLen)
{
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
            XF_LOG_WARN(0, 0, "RegisterSvr failed, %u|%d", SvrID, ret);
            return -1;
        }
        
        XF_LOG_INFO(0, 0, "RegisterSvr success, %u", SvrID);

        return PkgLen;
    }
    // ------------------------------ 系统通讯协议处理 end -----------------------------

    if(LoopLen < (unsigned)(PkgLen - HeaderLen))
    {
        XF_LOG_WARN(0, 0, "LoopBuff = %d is too small, need = %d", LoopLen, PkgLen - HeaderLen);
        return -1;
    }
    
    LoopLen = PkgLen - HeaderLen;
    memcpy(pLoopBuff, pCurBuffPos+HeaderLen, LoopLen);
    
    return PkgLen;
}