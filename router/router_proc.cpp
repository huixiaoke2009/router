
#include <functional>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <dlfcn.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#include "router_proc.h"
#include "log/log.h"
#include "util/util.h"
#include "ini_file/ini_file.h"

#include "router_header.h"
#include "cmd.h"


using namespace std;
using namespace mmlib;

const unsigned int LISTEN_SOCK_CPOS_MIN = 100;    //100-199的ConnPos用于监听的Socket
const unsigned int LISTEN_SOCK_CPOS_MAX = 199;

const unsigned int CLIENT_SOCK_CPOS_MIN = 500;    //500以上才是客户端连接上来的Socket

const unsigned int MAX_EPOLL_RET_EVENTS = 1024;

bool StopFlag = false;
bool ReloadFlag = false;

template<typename T>
void SAFE_DELETE(T *& p)
{
    if (p)
    {
        delete p;
        p = NULL;
    }
}

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

CConnInfo::CConnInfo(unsigned int ConnPos, int SockID, const struct sockaddr_in *pRemoteAddr, const struct sockaddr_in *pLocalAddr)
{

    m_ConnPos = ConnPos;
    m_SockID = SockID;
    m_RemoteAddr = *pRemoteAddr;
    m_LocalAddr = *pLocalAddr;
    m_RemainSendData.clear();
    m_RemainRecvData.clear();
    m_LastActTime = time(NULL);
    m_ServerID = 0;
}

CConnInfo::~CConnInfo()
{

}

// 接收数据
//@return 0-成功接收数据 1-连接关闭 <0-接受出错
int CConnInfo::Recv(char *pBuff, unsigned int *pLen)
{
    char *pRecvPtr = pBuff;
    unsigned int RecvBuffLen = *pLen;
    if (!m_RemainRecvData.empty())
    {
        if (m_RemainRecvData.length() > (unsigned int)XY_MAXBUFF_LEN)
        {
            XF_LOG_WARN(0, 0, "%s|recv remain len %lu", RemoteAddrStr(), (unsigned long)m_RemainRecvData.length());
            return -1;
        }

        pRecvPtr += m_RemainRecvData.length();
        RecvBuffLen -= m_RemainRecvData.length();
        XF_LOG_DEBUG(0, 0, "%s|%s|with recv remain len %lu", __FUNCTION__, RemoteAddrStr(), (unsigned long)m_RemainRecvData.length());
    }

    int RecvBytes = read(m_SockID, pRecvPtr, RecvBuffLen);
    if (RecvBytes > 0)
    {
        m_LastActTime = time(NULL);

        XF_LOG_TRACE(0, 0, "%s|recv|%d|%s", RemoteAddrStr(), RecvBytes, CStrTool::Str2Hex(pRecvPtr, RecvBytes));

        *pLen = RecvBytes;
        if (!m_RemainRecvData.empty())
        {
            *pLen += m_RemainRecvData.length();
            memcpy(pBuff, m_RemainRecvData.c_str(), m_RemainRecvData.length());
            m_RemainRecvData.clear();
        }

        return 0;
    }
    else if (RecvBytes == 0)
    {
        //连接被终止
        XF_LOG_DEBUG(0, 0, "conn close|%s|%d|%d", RemoteAddrStr(), RemotePort(), m_ConnPos);
        return 1;
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        //相当于没有接收到数据
        *pLen = 0;
        if (!m_RemainRecvData.empty())
        {
            *pLen += m_RemainRecvData.length();
            memcpy(pBuff, m_RemainRecvData.c_str(), m_RemainRecvData.length());
            m_RemainRecvData.clear();
        }

        return 0;
    }
    else
    {
        XF_LOG_WARN(0, 0, "%s|recv failed, ret=%d, errno=%d, errmsg=%s", RemoteAddrStr(), RecvBytes, errno, strerror(errno));
        return -1;
    }
}

// 发送数据
//@return 0-成功发送数据 <0-发送失败
int CConnInfo::Send(const char *pBuff, unsigned int Len)
{
    int WriteBytes = 0;
    if (!m_RemainSendData.empty())
    {
        WriteBytes = write(m_SockID, m_RemainSendData.c_str(), m_RemainSendData.length());
        if (WriteBytes > 0 && WriteBytes < (int)m_RemainSendData.length())
        {
            XF_LOG_TRACE(0, 0, "Send|%d|%s", WriteBytes, CStrTool::Str2Hex(m_RemainSendData.c_str(), WriteBytes));
            m_LastActTime = time(NULL);
            //说明Reamin的数据还没有发完，需要继续发Remain的数据
            if ((int)m_RemainSendData.length() > 5120000)
            {
                //如果存在超过5MB的数据没有发送出去，输出warn日志
                XF_LOG_WARN(0, 0, "%s|send reamin data failed, remain_send_data_len=%lu, send_len=%d", RemoteAddrStr(), (unsigned long)m_RemainSendData.length(), WriteBytes);
            }
            else
            {
                XF_LOG_DEBUG(0, 0, "%s|send reamin data failed, remain_send_data_len=%lu, send_len=%d", RemoteAddrStr(), (unsigned long)m_RemainSendData.length(), WriteBytes);
            }
            m_RemainSendData.erase(0, WriteBytes);
            return 0;
        }
        else if (WriteBytes == (int)m_RemainSendData.length())
        {
            XF_LOG_TRACE(0, 0, "Send|%d|%s", WriteBytes,  CStrTool::Str2Hex(m_RemainSendData.c_str(), WriteBytes));
            m_RemainSendData.clear();
        }
        else if (WriteBytes <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0;
            }
            else
            {
                XF_LOG_WARN(0, 0, "%s|send remain data failed, ret=%d, errno=%d, errmsg=%s", RemoteAddrStr(), WriteBytes, errno, strerror(errno));
                return -1;
            }
        }
    }

    WriteBytes = write(m_SockID, pBuff, Len);
    if (WriteBytes > 0)
    {
        XF_LOG_TRACE(0, 0, "Send|%d|%s", Len, CStrTool::Str2Hex(pBuff, Len));
        m_LastActTime = time(NULL);
        return WriteBytes;
    }
    else if (WriteBytes <= 0)
    {
        //连接被终止
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        else
        {
            XF_LOG_WARN(0, 0, "%s|send data failed, ret=%d, errno=%d, errmsg=%s", RemoteAddrStr(), WriteBytes, errno, strerror(errno));
            return -1;
        }
    }

    return 0;
}

// 缓存接收到的数据
int CConnInfo::AddRecvData(const char *pBuff, unsigned int Len)
{
    XF_LOG_DEBUG(0, 0, "%s|%s|%u|%s|%lu|%d", __FUNCTION__, RemoteAddrStr(), m_ConnPos, __FUNCTION__, (unsigned long)m_RemainRecvData.length(), Len);

    if (!m_RemainRecvData.empty())
    {
        if (m_RemainRecvData.length() + Len > (unsigned int)XY_MAXBUFF_LEN)
        {
            XF_LOG_WARN(0, 0, "%s|add recv failed, len=%lu, add_len=%d", RemoteAddrStr(), (unsigned long)m_RemainRecvData.length(), Len);
            return -1;
        }

        //TODO 错误处理
        m_RemainRecvData.append(pBuff, Len);
        XF_LOG_DEBUG(0, 0, "%s|%s|%u|after add string|%lu", __FUNCTION__, RemoteAddrStr(), m_ConnPos, (unsigned long)m_RemainRecvData.length());
    }
    else
    {
        if (Len > (unsigned int)XY_MAXBUFF_LEN)
        {
            XF_LOG_WARN(0, 0, "%s|add recv failed, add_len=%d", RemoteAddrStr(), Len);
            return -1;
        }

        //TODO 错误处理
        m_RemainRecvData.assign(pBuff, Len);
        XF_LOG_DEBUG(0, 0, "%s|%s|%u|after new string|%lu", __FUNCTION__, RemoteAddrStr(), m_ConnPos, (unsigned long)m_RemainRecvData.length());
    }

    return 0;
}

//缓存需要发送的数据
int CConnInfo::AddSendData(const char *pBuff, unsigned int Len)
{
    XF_LOG_DEBUG(0, 0, "%s|%s|%u|%s|%lu|%d", __FUNCTION__, RemoteAddrStr(), m_ConnPos, __FUNCTION__, (unsigned long)m_RemainSendData.length(), Len);

    if (!m_RemainSendData.empty())
    {
        if (m_RemainSendData.length() + Len > (unsigned int)XY_MAXBUFF_LEN)
        {
            XF_LOG_WARN(0, 0, "%s|add recv failed, len=%lu, add_len=%d", RemoteAddrStr(), (unsigned long)m_RemainSendData.length(), Len);
            return -1;
        }

        //TODO 错误处理
        m_RemainSendData.append(pBuff, Len);
        XF_LOG_DEBUG(0, 0, "%s|%s|%u|after add string|%lu", __FUNCTION__, RemoteAddrStr(), m_ConnPos, (unsigned long)m_RemainSendData.length());
    }
    else
    {
        if (Len > (unsigned int)XY_MAXBUFF_LEN)
        {
            XF_LOG_WARN(0, 0, "%s|add send failed, add_len=%d", RemoteAddrStr(), Len);
            return -1;
        }

        //TODO 错误处理
        m_RemainSendData.assign(pBuff, Len);
        XF_LOG_DEBUG(0, 0, "%s|%s|%u|after new string|%lu", __FUNCTION__, RemoteAddrStr(), m_ConnPos, (unsigned long)m_RemainSendData.length());
    }

    return 0;
}

//发送缓存的数据
int CConnInfo::SendRemainData()
{
    int WriteBytes = 0;
    if (!m_RemainSendData.empty())
    {
        WriteBytes = write(m_SockID, m_RemainSendData.c_str(), m_RemainSendData.length());
        if (WriteBytes > 0 && WriteBytes < (int)m_RemainSendData.length())
        {
            m_LastActTime = time(NULL);
            //说明Reamin的数据还没有发完，需要继续发Remain的数据
            if ((int)m_RemainSendData.length() > 5120000)
            {
                //如果存在超过5MB的数据没有发送出去，输出warn日志
                XF_LOG_WARN(0, 0, "%s|send reamin data success, remain_send_data_len=%lu, send_len=%d", RemoteAddrStr(), (unsigned long)m_RemainSendData.length(), WriteBytes);
            }
            else
            {
                XF_LOG_DEBUG(0, 0, "%s|send reamin data success, remain_send_data_len=%lu, send_len=%d", RemoteAddrStr(), (unsigned long)m_RemainSendData.length(), WriteBytes);
            }


            m_RemainSendData.erase(0, WriteBytes);
            return 1;   //数据未发完
        }
        else if (WriteBytes == (int)m_RemainSendData.length())
        {
            m_RemainSendData.clear();
        }
        else if (WriteBytes <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 1;   //数据未发完
            }
            else
            {
                XF_LOG_WARN(0, 0, "%s|send remain data failed, ret=%d, errno=%d, errmsg=%s", RemoteAddrStr(), WriteBytes, errno, strerror(errno));
                return -1;
            }
        }
    }

    return 0;
}


CRouter::CRouter()
{
    m_ServerID = 0;
    m_EpollFD = -1;
    m_ConnPosCnt = 0;
    m_PosConnMap.clear();
    m_itrCurCheckConn = m_PosConnMap.begin();
    m_TimeOut = 0;
    m_pProcessBuff = NULL;
}

CRouter::~CRouter()
{
    if (m_EpollFD >= 0)
    {
        close(m_EpollFD);
        m_EpollFD = -1;
    }

    if(m_pProcessBuff)
    {
        SAFE_DELETE(m_pProcessBuff);
    }
}


int CRouter::Init(const char *pConfFile)
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


    //读取配置文件
    CIniFile IniFile(pConfFile);

    char ListenStr[1024] = {0};
    int GroupNum = 0;
    
    char ModuleName[256] = {0};
    int LogLocal = 0;
    int LogLevel = 0;
    char LogPath[1024] = {0};

    if (!IniFile.IsValid())
    {
        printf("ERR:conf file [%s] is not valid\n", pConfFile);
        return -1;
    }

    IniFile.GetInt("ROUTER", "ServerID", 0, &m_ServerID);
    IniFile.GetInt("ROUTER", "TimeOut", 3600, &m_TimeOut);
    IniFile.GetString("ROUTER", "Listen", "", ListenStr, sizeof(ListenStr));
    IniFile.GetInt("ROUTER", "GroupNum", 0, &GroupNum);
    
    IniFile.GetString("LOG", "ModuleName", "router", ModuleName, sizeof(ModuleName));
    IniFile.GetInt("LOG", "LogLocal", 1, &LogLocal);
    IniFile.GetInt("LOG", "LogLevel", 3, &LogLevel);
    IniFile.GetString("LOG", "LogPath", "/dev/null", LogPath, sizeof(LogPath));

    if(m_ServerID == 0)
    {
        printf("ERR:ROUTER/ServerID is not valid\n");
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

    //分配空间
    if (!m_pProcessBuff)
    {
        m_pProcessBuff = (char *)malloc(XY_PKG_MAX_LEN);
    }

    //创建EPOLL
    m_EpollFD = epoll_create(XY_MAX_CONN_NUM);
    if (m_EpollFD == -1)
    {
        printf("ERR:epoll create failed|%d|%d|%s\n", XY_MAX_CONN_NUM, errno, strerror(errno));
        return -1;
    }

    //解析监听地址
    char *pszOneIPAddr = NULL;
    int iCurAddrNum = 0;
    for(char *pszSecVal = ListenStr; (pszOneIPAddr = strtok(pszSecVal, ",")) != NULL; pszSecVal=NULL)
    {
        //去掉空格
        while ((*pszOneIPAddr) == ' ')
        {
            pszOneIPAddr++;
        }

        char *AddrDiv = strstr(pszOneIPAddr, ":");
        if (AddrDiv == NULL)
        {
            printf("ROUTER/Listen is not valid, str=%s, addr_idx=%d", ListenStr, iCurAddrNum);
            return -1;
        }

        char CurIPStr[32];
        char CurPortStr[16];
        memset(CurIPStr, 0, sizeof(CurIPStr));
        memset(CurPortStr, 0, sizeof(CurPortStr));

        memcpy(CurIPStr, pszOneIPAddr, (AddrDiv - pszOneIPAddr));
        memcpy(CurPortStr, AddrDiv+1, (strlen(pszOneIPAddr) - (AddrDiv - pszOneIPAddr) - 1));
        printf("CONN_INFO|%s|%s|%s\n", pszOneIPAddr, CurIPStr, CurPortStr);

        unsigned int LocalIP = inet_addr(CurIPStr);
        int LocalPort = atoi(CurPortStr);

        if (LocalPort == 0)
        {
            printf("ROUTER/Listen is not valid, str=%s, addr[%s] or port [%s] is not valid", ListenStr, CurIPStr, CurPortStr);
            return -1;
        }

        int CurListenSockID = socket(AF_INET, SOCK_STREAM, 0);

        if(-1 == CurListenSockID)
        {
            printf("ERR:create listen socket failed|%d|%s\n", errno, strerror(errno));
            return -1;
        }

        int ReuseVal = 1;
        if(-1 == setsockopt(CurListenSockID, SOL_SOCKET, SO_REUSEADDR, &ReuseVal, sizeof(ReuseVal)))
        {
            printf("ERR:setsockopt failed|%d|%s\n", errno, strerror(errno));
            return -1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0x0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = LocalIP;
        addr.sin_port = htons(LocalPort);

        if(-1 == TEMP_FAILURE_RETRY(::bind(CurListenSockID, (struct sockaddr*)&addr, sizeof(addr))))
        {
            printf("ERR:bind failed|%s|%u|%d|%s\n", CStrTool::IPString(addr.sin_addr.s_addr), ntohs(addr.sin_port), errno, strerror(errno));
            return -1;
        }

        if(-1 == TEMP_FAILURE_RETRY(::listen(CurListenSockID, SOMAXCONN)))
        {
            printf("ERR:listen failed|%d|%s\n", errno, strerror(errno));
            return -1;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.u32 = LISTEN_SOCK_CPOS_MIN + iCurAddrNum;

        if(0 != epoll_ctl(m_EpollFD, EPOLL_CTL_ADD, CurListenSockID, &ev))
        {
            printf("ERR:epoll_ctl failed|%d|%s\n", errno, strerror(errno));
            return -1;
        }

        struct sockaddr_in remote_addr;
        memset(&remote_addr, 0x0, sizeof(remote_addr));

        CConnInfo *pCurConnInfo = new CConnInfo(LISTEN_SOCK_CPOS_MIN + iCurAddrNum, CurListenSockID, &remote_addr, &addr);

        if(m_PosConnMap.insert(std::pair<unsigned int, CConnInfo*>(LISTEN_SOCK_CPOS_MIN + iCurAddrNum, pCurConnInfo)).second == false)
        {
            printf("conn map add failed\n");
            epoll_ctl(m_EpollFD, EPOLL_CTL_DEL, CurListenSockID, NULL);
            close(CurListenSockID);
            SAFE_DELETE(pCurConnInfo);
            return -1;
        }

        printf("INFO:service listen ok, ip=%s, port=%d\n", inet_ntoa(*(struct in_addr *)(&addr.sin_addr.s_addr)), ntohs(addr.sin_port));

        iCurAddrNum++;
    }

    if(iCurAddrNum == 0)
    {
        printf("ROUTER/Listen is not valid, str=%s\n", ListenStr);
        return -1;
    }

    for(int i = 0; i < GroupNum; i++)
    {
        int GroupID = 0;
        int GroupType = -1;
        char strListID[1024] = {0};
        
        string strGroup = CStrTool::Format("GROUP_%d", i);
        IniFile.GetInt(strGroup.c_str(), "GroupID", 0, &GroupID);
        IniFile.GetInt(strGroup.c_str(), "GroupType", -1, &GroupType);
        IniFile.GetString(strGroup.c_str(), "ListID", "", strListID, sizeof(strListID));
        
        if(GroupID == 0 || GroupID % XY_GROUP_MOD != 0 || GroupType < 0 || GroupType >= EFTYPE_END)
        {
            printf("GroupID:%d GroupType:%d is not valid\n", GroupID, GroupType);
            return -1;
        }
        
        GroupInfo Info;
        Info.GroupID = GroupID;
        Info.GroupType = GroupType;
        
        if(GroupType == EFTYPE_SECTION)
        {
            int Section = 0;
            IniFile.GetInt(strGroup.c_str(), "Section", 0, &Section);
            
            if(Section == 0)
            {
                printf("%s/Section is not valid\n", strGroup.c_str());
                return -1;
            }
            
            //解析ListID
            char *pszOneListID = NULL;
            int iCurListNum = 0;
            for(char *pszSecVal = strListID; (pszOneListID = strtok(pszSecVal, ",")) != NULL; pszSecVal=NULL)
            {
                if(iCurListNum >= XY_MAX_GROUP_NUM)
                {
                    printf("iCurListNum = %d is not valid\n", iCurListNum);
                    return -1;
                }
                
                //去掉空格
                while ((*pszOneListID) == ' ')
                {
                    pszOneListID++;
                }
                
                char *Div = strstr(pszOneListID, ":");
                if (Div == NULL)
                {
                    printf("%s/ListID is not valid, str=%s, idx=%d\n", strGroup.c_str(), strListID, iCurAddrNum);
                    return -1;
                }

                char CurIDStr[8] = {0};
                char CurSectionStr[32] = {0};
                
                memcpy(CurIDStr, pszOneListID, (Div - pszOneListID));
                memcpy(CurSectionStr, Div+1, (strlen(pszOneListID) - (Div - pszOneListID) - 1));

                char* Div2 = strstr(CurSectionStr, ":");
                if (Div2 == NULL)
                {
                    printf("%s/ListID is not valid, str=%s, idx=%d\n", strGroup.c_str(), strListID, iCurAddrNum);
                    return -1;
                }
                
                char CurSectionBegin[16] = {0};
                char CurSectionEnd[16] = {0};
                
                memcpy(CurSectionBegin, CurSectionStr, (Div2 - CurSectionStr));
                memcpy(CurSectionEnd, Div2+1, (strlen(CurSectionStr) - (Div2 - CurSectionStr) - 1));
                
                int ListID = atoi(CurIDStr);
                int Begin = atoi(CurSectionBegin);
                int End = atoi(CurSectionEnd);
                
                Info.ListID[iCurListNum].SvrID = ListID;
                Info.ListID[iCurListNum].Begin = Begin;
                Info.ListID[iCurListNum].End = End;
                
                iCurListNum++;
            }
            
            if(iCurListNum == 0)
            {
                printf("iCurListNum is not valid\n");
                return -1;
            }
            
            Info.ListNum = iCurListNum;
            Info.Section = Section;
            
            m_GrpSvrMap[GroupID] = Info;
        }
        else
        {
            //解析ListID
            char *pszOneListID = NULL;
            int iCurListNum = 0;
            for(char *pszSecVal = strListID; (pszOneListID = strtok(pszSecVal, ",")) != NULL; pszSecVal=NULL)
            {
                if(iCurListNum >= XY_MAX_GROUP_NUM)
                {
                    printf("iCurListNum = %d is not valid\n", iCurListNum);
                    return -1;
                }
                
                //去掉空格
                while ((*pszOneListID) == ' ')
                {
                    pszOneListID++;
                }

                int ListID = atoi(pszOneListID);
                Info.ListID[iCurListNum].SvrID = ListID;
                iCurListNum++;
            }
            
            if(iCurListNum == 0)
            {
                printf("iCurListNum is not valid\n");
                return -1;
            }
            
            Info.ListNum = iCurListNum;
            
            m_GrpSvrMap[GroupID] = Info;
        }
    }

    return 0;
}


int CRouter::Run()
{
    int Ret = 0;
    StopFlag = false;

    struct epoll_event RetEvent[MAX_EPOLL_RET_EVENTS];
    int RetEventNum = 0;

    char *pRecvBuff = (char *)malloc(XY_MAXBUFF_LEN);
    int RecvLen = 0;

    while(!StopFlag)
    {
        int EmptyFlag = 1;  //没有数据标志位

        RetEventNum = epoll_wait(m_EpollFD, RetEvent, MAX_EPOLL_RET_EVENTS, 1);
        if(RetEventNum > 0)
        {
            EmptyFlag = 0;
            unsigned int CurConnPos = 0;
            for(int i = 0; i < RetEventNum; ++i)
            {
                XF_LOG_TRACE(0, 0, "epoll_wait return, event_num=%d, cur_event=%d, event=%d, conn_pos=%u", RetEventNum, i, RetEvent[i].events, RetEvent[i].data.u32);

                CurConnPos = RetEvent[i].data.u32;
                if((CurConnPos >= LISTEN_SOCK_CPOS_MIN) && (CurConnPos <= LISTEN_SOCK_CPOS_MAX))
                {
                    if(RetEvent[i].events & EPOLLIN)
                    {
                        AcceptConn(CurConnPos);
                    }
                    else
                    {
                        XF_LOG_WARN(0, 0, "listening socket recv event %d", RetEvent[i].events);
                    }
                }
                else
                {
                    std::map<unsigned int, CConnInfo*>::iterator pConnInfoMap = m_PosConnMap.find(CurConnPos);
                    if (pConnInfoMap == m_PosConnMap.end())
                    {
                        XF_LOG_WARN(0, 0, "EPOLL find invalid cpos|%u", CurConnPos);
                        continue;
                    }

                    CConnInfo *pCurConnInfo = pConnInfoMap->second;
                    if(RetEvent[i].events & EPOLLERR)
                    {
                        XF_LOG_WARN(0, 0, "EPOLL return EPOLLERR|%u", CurConnPos);
                        ReleaseConn(pConnInfoMap);
                        continue;
                    }

                    if(RetEvent[i].events & EPOLLHUP)
                    {
                        XF_LOG_WARN(0, 0, "EPOLL return EPOLLHUP|%u", CurConnPos);
                        ReleaseConn(pConnInfoMap);
                        continue;
                    }

                    if(RetEvent[i].events & EPOLLIN)
                    {
                        //读取数据
                        //流程：1)读取 2)判断是否OK 3)如果OK就入Queue 4)如果不OK就将数据缓存或关闭连接
                        RecvLen = XY_MAXBUFF_LEN;
                        Ret = pCurConnInfo->Recv(pRecvBuff, (unsigned int *)&RecvLen);
                        if (Ret == 1)
                        {
                            //连接被终止
                            ReleaseConn(pConnInfoMap);
                            continue;
                        }
                        else if (Ret < 0)
                        {
                            //接收失败
                            XF_LOG_WARN(0, 0, "%u|recv failed", CurConnPos);
                            ReleaseConn(pConnInfoMap);
                            continue;
                        }

                        if (RecvLen > XY_MAXBUFF_LEN)
                        {
                            XF_LOG_WARN(0, 0, "%u|%u|recv len is not valid", CurConnPos, RecvLen);
                            ReleaseConn(pConnInfoMap);
                            continue;
                        }

                        XF_LOG_TRACE(0, 0, "Recv|%d|%s", RecvLen, CStrTool::Str2Hex(pRecvBuff, RecvLen));

                        char *pCurBuffPos = pRecvBuff;
                        while(RecvLen > 0)
                        {
                            int ProcLen = ProcessPkg(pCurBuffPos, RecvLen, pConnInfoMap);
                            if ((ProcLen > RecvLen)||(ProcLen == 0))
                            {
                                //需要暂存数据
                                //ProcLen返回0表示根据现有的字段还不能计算长度
                                int Ret2 = pCurConnInfo->AddRecvData(pCurBuffPos, RecvLen);
                                if (Ret2 != 0)
                                {
                                    XF_LOG_WARN(0, 0, "%u|%u|store recv data failed", CurConnPos, RecvLen);
                                    ReleaseConn(pConnInfoMap);
                                }
                                RecvLen = 0;
                            }
                            else if (ProcLen == -1)
                            {
                                //直接关闭连接
                                XF_LOG_WARN(0, 0, "%u|%u|handle_input ret -1", CurConnPos, RecvLen);
                                ReleaseConn(pConnInfoMap);
                                RecvLen = 0;
                            }
                            else if (ProcLen < -1)
                            {
                                XF_LOG_WARN(0, 0, "%u|%u|handle_input ret %d", CurConnPos, RecvLen, ProcLen);
                                ReleaseConn(pConnInfoMap);
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
                        //发送数据
                        //流程：1)发送 2)判断是否需要等待 3)如果不需要等待，直接就OK 4)如果需要等待，将数据缓存
                        int Ret = pCurConnInfo->SendRemainData();
                        if (Ret == 1)
                        {
                            //数据未发完
                            continue;
                        }
                        else if (Ret != 0)
                        {
                            XF_LOG_WARN(0, 0, "send remain data failed|%u", CurConnPos);
                            ReleaseConn(pConnInfoMap);
                            continue;
                        }
                        else
                        {
                            //删除EPOLLOUT事件
                            struct epoll_event ev;
                            ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
                            ev.data.u32 = CurConnPos;

                            int Ret2 = epoll_ctl(m_EpollFD, EPOLL_CTL_MOD, pCurConnInfo->GetSockID(), &ev);
                            if(Ret2 != 0)
                            {
                                XF_LOG_WARN(0, 0, "epoll mod del epoll_out failed|%d|%s", errno, strerror(errno));
                                //TODO 这里需要考虑后续处理
                            }
                        }
                    }
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

        // 对连接进行检查扫描
        CheckConn();
        
        if(EmptyFlag)
        {
            usleep(1000);
        }
    }

    return 0;
}



int CRouter::AcceptConn(int ConnPos)
{
    int Ret = 0;

    struct sockaddr_in RemoteAddr;
    memset(&RemoteAddr, 0x0, sizeof(RemoteAddr));
    socklen_t RemoteAddrLen = sizeof(RemoteAddr);

    std::map<unsigned int, CConnInfo*>::iterator pConnInfoMap = m_PosConnMap.find(ConnPos);
    if (pConnInfoMap == m_PosConnMap.end())
    {
        XF_LOG_WARN(0, 0, "Accept find invalid cpos|%u", ConnPos);
        return -1;
    }

    CConnInfo *pCurConnInfo = pConnInfoMap->second;

    int NewSockID = TEMP_FAILURE_RETRY(::accept(pCurConnInfo->GetSockID(), (struct sockaddr*)&RemoteAddr, &RemoteAddrLen));
    if(NewSockID > 0)
    {
        //经过一定的周期，可能产生的ConnPos正在使用中
        m_ConnPosCnt++;
        unsigned int CurConnPos;
        if (m_ConnPosCnt < CLIENT_SOCK_CPOS_MIN)
        {
            CurConnPos = CLIENT_SOCK_CPOS_MIN+1;
            m_ConnPosCnt = CurConnPos;
        }
        else
        {
            CurConnPos = m_ConnPosCnt;
        }

        //检索当前的map，防止出现冲突的ConnPos
        while (true)
        {
            if (m_PosConnMap.find(CurConnPos) == m_PosConnMap.end())
            {
                break;
            }

            CurConnPos++;

            if (CurConnPos < CLIENT_SOCK_CPOS_MIN)
            {
                CurConnPos = CLIENT_SOCK_CPOS_MIN + 1;
            }
        }

        //设置为非阻塞模式
        int FdFlags = 0;
        FdFlags = fcntl(NewSockID, F_GETFL, 0);
        if (FdFlags == -1)
        {
            XF_LOG_WARN(0, 0, "fcntl get flags failed|%d|%s", errno, strerror(errno));
            close(NewSockID);
            return -1;
        }

        Ret = fcntl(NewSockID, F_SETFL, FdFlags | O_NONBLOCK);
        if (Ret == -1)
        {
            XF_LOG_WARN(0, 0, "fcntl set flags failed|%d|%s", errno, strerror(errno));
            close(NewSockID);
            return -1;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        ev.data.u32 = CurConnPos;

        Ret = epoll_ctl(m_EpollFD, EPOLL_CTL_ADD, NewSockID, &ev);
        if(Ret != 0)
        {
            XF_LOG_WARN(0, 0, "epoll add failed|%d|%s", errno, strerror(errno));
            close(NewSockID);
            return -1;
        }

        struct sockaddr_in local_addr;
        memset(&local_addr, 0x0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = pCurConnInfo->LocalAddr();
        local_addr.sin_port = pCurConnInfo->LocalPort();

        CConnInfo *pCurConnInfo = new CConnInfo(CurConnPos, NewSockID, &RemoteAddr, &local_addr);

        if(m_PosConnMap.insert(std::pair<unsigned int, CConnInfo*>(CurConnPos, pCurConnInfo)).second == false)
        {
            XF_LOG_WARN(0, 0, "conn map add failed");
            epoll_ctl(m_EpollFD, EPOLL_CTL_DEL, NewSockID, NULL);
            close(NewSockID);
            SAFE_DELETE(pCurConnInfo);
            return -1;
        }

        XF_LOG_DEBUG(0, 0, "conn in|%s|%u|%s|%d|%u", mmlib::CStrTool::IPString(pCurConnInfo->LocalAddr()), ntohs(pCurConnInfo->LocalPort()), mmlib::CStrTool::IPString(RemoteAddr.sin_addr.s_addr), ntohs(RemoteAddr.sin_port), CurConnPos);
    }
    else
    {
        XF_LOG_WARN(0, 0, "accept conn failed|%d|%s", errno, strerror(errno));
        return -1;
    }

    return 0;
}


void CRouter::CheckConn()
{
    //每次只检查一个链接是否超时
    if (m_itrCurCheckConn == m_PosConnMap.end())
    {
        m_itrCurCheckConn = m_PosConnMap.begin();
        return;
    }

    if ((m_itrCurCheckConn->first >= LISTEN_SOCK_CPOS_MIN) && (m_itrCurCheckConn->first <= LISTEN_SOCK_CPOS_MAX))
    {
        m_itrCurCheckConn++;
        return;
    }
    
    // 判断数据是否一致
    unsigned int ConnPos = m_itrCurCheckConn->first;
    unsigned int SvrID = m_itrCurCheckConn->second->GetServerID();
    if(SvrID != 0)
    {
        std::map<unsigned int, unsigned int>::iterator iter = m_SvrConnMap.find(SvrID);
        if(iter != m_SvrConnMap.end() && iter->second != ConnPos)
        {
            XF_LOG_WARN(0, 0, "conn timeout|%s|%d|%u|%s", m_itrCurCheckConn->second->RemoteAddrStr(), m_itrCurCheckConn->second->RemotePort(), m_itrCurCheckConn->first, CStrTool::TimeString(m_itrCurCheckConn->second->GetLastActTime()));
            std::map<unsigned int, CConnInfo*>::iterator itrTmpConn = m_itrCurCheckConn;
            ReleaseConn(itrTmpConn);
            return;
        }
    }
    
    // 判断是否超时
    time_t TineNow = time(NULL);
    time_t TimeDiff = TineNow - m_itrCurCheckConn->second->GetLastActTime();
    if (TimeDiff > m_TimeOut)
    {
        XF_LOG_WARN(0, 0, "conn timeout|%s|%d|%u|%s", m_itrCurCheckConn->second->RemoteAddrStr(), m_itrCurCheckConn->second->RemotePort(), m_itrCurCheckConn->first, CStrTool::TimeString(m_itrCurCheckConn->second->GetLastActTime()));
        std::map<unsigned int, CConnInfo*>::iterator itrTmpConn = m_itrCurCheckConn;
        ReleaseConn(itrTmpConn);
    }
    else
    {
        m_itrCurCheckConn++;
    }

    return;
}


void CRouter::ReleaseConn(std::map<unsigned int, CConnInfo*>::iterator &itrConnInfoMap)
{
    CConnInfo *pCurConnInfo = itrConnInfoMap->second;
    epoll_ctl(m_EpollFD, EPOLL_CTL_DEL, pCurConnInfo->GetSockID(), NULL);
    close(pCurConnInfo->GetSockID());
    if (itrConnInfoMap == m_itrCurCheckConn)
    {
        m_itrCurCheckConn++;
    }

    XF_LOG_INFO(0, 0, "DEL_POS_CONN_MAP|%d", itrConnInfoMap->first);
    m_PosConnMap.erase(itrConnInfoMap);
    
    unsigned int SvrID = pCurConnInfo->GetServerID();
    if(SvrID != 0)
    {
        std::map<unsigned int, unsigned int>::iterator iter = m_SvrConnMap.find(SvrID);
        if(iter != m_SvrConnMap.end())
        {
            XF_LOG_INFO(0, 0, "DEL_SVR_CONN_MAP|%d|%d", iter->first, iter->second);
            m_SvrConnMap.erase(iter);
        }
    }
    
    SAFE_DELETE(pCurConnInfo);
}


//返回值:0~PkgLen-1表示包不够大，-1表示出错要删除链接， PkgLen表示正常。如果要删除链接不要在这里删，返回-1即可
int CRouter::ProcessPkg(const char *pCurBuffPos, int RecvLen, std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap)
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
    
    if(CurHeader.DstID == (unsigned int)m_ServerID)
    {
        // 注册服务器
        if(Cmd_RegisterSvr == CurHeader.CmdID)
        {
            router::RegisterSvrReq CurRegisterSvrReq;
            router::RegisterSvrRsp CurRegisterSvrRsp;
            if (!CurRegisterSvrReq.ParseFromArray(pCurBuffPos+HeaderLen, PkgLen-HeaderLen))
            {
                XF_LOG_WARN(0, 0, "Parse Pkg failed, CmdID=%d", CurHeader.CmdID);
                CurRegisterSvrRsp.set_ret(-1);
                Send2ProxyByMsg(pConnInfoMap, CurHeader.SrcID, Cmd_RegisterSvr, CurRegisterSvrRsp);
                return -1;
            }
            
            unsigned int SvrID = CurRegisterSvrReq.svrid();
            std::map<unsigned int, unsigned int>::iterator iter = m_SvrConnMap.find(SvrID);
            if(iter != m_SvrConnMap.end())
            {
                XF_LOG_WARN(0, 0, "SvrID %d is already register", SvrID);
                CurRegisterSvrRsp.set_ret(-2);
                Send2ProxyByMsg(pConnInfoMap, CurHeader.SrcID, Cmd_RegisterSvr, CurRegisterSvrRsp);
                return -1;
            }
            
            if(SvrID != CurHeader.SrcID)
            {
                XF_LOG_WARN(0, 0, "can't register for other server");
                CurRegisterSvrRsp.set_ret(-3);
                Send2ProxyByMsg(pConnInfoMap, CurHeader.SrcID, Cmd_RegisterSvr, CurRegisterSvrRsp);
                return -1;
            }
            
            if(SvrID % XY_GROUP_MOD == 0)
            {
                XF_LOG_WARN(0, 0, "can't register for all group");
                CurRegisterSvrRsp.set_ret(-4);
                Send2ProxyByMsg(pConnInfoMap, CurHeader.SrcID, Cmd_RegisterSvr, CurRegisterSvrRsp);
                return -1;
            }
            
            XF_LOG_INFO(0, 0, "Register server %d", SvrID);
            
            CurRegisterSvrRsp.set_ret(0);
            Send2ProxyByMsg(pConnInfoMap, CurHeader.SrcID, Cmd_RegisterSvr, CurRegisterSvrRsp);
            
            m_SvrConnMap[SvrID] = pConnInfoMap->first;
            pConnInfoMap->second->SetServerID(SvrID);
            
            return PkgLen;
        }
        
        // 心跳包
        if(Cmd_Heartbeat == CurHeader.CmdID)
        {
            router::HeartbeatReq CurHeartbeatReq;
            if (!CurHeartbeatReq.ParseFromArray(pCurBuffPos+HeaderLen, PkgLen-HeaderLen))
            {
                XF_LOG_WARN(0, 0, "Parse Pkg failed, CmdID=%d", CurHeader.CmdID);
                return -1;
            }
            
            unsigned int SvrID = CurHeartbeatReq.svrid();
                        
            if(SvrID != CurHeader.SrcID)
            {
                XF_LOG_WARN(0, 0, "heartbeat for other server is illage");
                return -1;
            }
            
            XF_LOG_TRACE(0, 0, "Recieve Heartbeat from SvrID %d", SvrID);
            
            return PkgLen;
        }
        
        XF_LOG_WARN(0, 0, "unknown cmdid %x", CurHeader.CmdID);
        
        return PkgLen;
    }
    
    // 转发数据
    
    // 判断源服务器是否已注册, 否则不允许发送数据
    std::map<unsigned int, unsigned int>::iterator iterSrc = m_SvrConnMap.find(CurHeader.SrcID);
    if(iterSrc == m_SvrConnMap.end() || pConnInfoMap->first != iterSrc->second)
    {
        XF_LOG_WARN(0, 0, "SvrID %d|%d|%d don't register", CurHeader.SrcID, pConnInfoMap->first, iterSrc->second);
        return -1;
    }
    
    // 指定了目的服务器
    if(CurHeader.DstID % XY_GROUP_MOD != 0)
    {
        // 判断目标服务器是否存在
        std::map<unsigned int, unsigned int>::iterator iterDst = m_SvrConnMap.find(CurHeader.DstID);
        if(iterDst == m_SvrConnMap.end())
        {
            XF_LOG_WARN(0, 0, "find invalid SvrID %d", CurHeader.DstID);
            return PkgLen;
        }
        
        std::map<unsigned int, CConnInfo*>::iterator pDstConnInfoMap = m_PosConnMap.find(iterDst->second);
        if (pDstConnInfoMap == m_PosConnMap.end())
        {
            XF_LOG_WARN(0, 0, "find invalid PosConn %d", iterDst->second);
            m_SvrConnMap.erase(iterDst);
            return PkgLen;
        }
        
        Send2Proxy(pDstConnInfoMap, pCurBuffPos, PkgLen);
    }
    // 指定了一个Group
    else
    {
        std::map<unsigned int, GroupInfo>::iterator iterGrp = m_GrpSvrMap.find(CurHeader.DstID);
        if(iterGrp == m_GrpSvrMap.end())
        {
            XF_LOG_WARN(0, 0, "find invalid GroupID %d", CurHeader.DstID);
            return PkgLen;
        }
        
        GroupInfo& Info = iterGrp->second;
        if(Info.GroupType == EFTYPE_RANDOM)
        {
            std::vector<unsigned int> vctSvrID;
            for(unsigned int i = 0; i < Info.ListNum && i < (unsigned int)XY_MAX_GROUP_NUM; i++)
            {
                std::map<unsigned int, unsigned int>::iterator iterDst = m_SvrConnMap.find(Info.ListID[i].SvrID);
                if(iterDst != m_SvrConnMap.end())
                {
                    vctSvrID.push_back(Info.ListID[i].SvrID);
                }
            }
            
            if(vctSvrID.size() == 0)
            {
                XF_LOG_WARN(0, 0, "all svr is down");
                return PkgLen;
            }
            
            unsigned int SvrID = vctSvrID[time(NULL) % vctSvrID.size()];
            
            std::map<unsigned int, unsigned int>::iterator iterDst = m_SvrConnMap.find(SvrID);
            if(iterDst == m_SvrConnMap.end())
            {
                XF_LOG_WARN(0, 0, "find invalid SvrID %d", SvrID);
                return PkgLen;
            }
            
            std::map<unsigned int, CConnInfo*>::iterator pDstConnInfoMap = m_PosConnMap.find(iterDst->second);
            if (pDstConnInfoMap == m_PosConnMap.end())
            {
                XF_LOG_WARN(0, 0, "find invalid PosConn %d", iterDst->second);
                m_SvrConnMap.erase(iterDst);
                return PkgLen;
            }
            
            Send2Proxy(pDstConnInfoMap, pCurBuffPos, PkgLen);
            
            return PkgLen;
        }
        else if(Info.GroupType == EFTYPE_ALL)
        {
            for(unsigned int i = 0; i < Info.ListNum && i < (unsigned int)XY_MAX_GROUP_NUM; i++)
            {
                // 判断目标服务器是否存在
                std::map<unsigned int, unsigned int>::iterator iterDst = m_SvrConnMap.find(Info.ListID[i].SvrID);
                if(iterDst == m_SvrConnMap.end())
                {
                    XF_LOG_WARN(0, 0, "find invalid SvrID %d", Info.ListID[i].SvrID);
                    continue;
                }
                
                std::map<unsigned int, CConnInfo*>::iterator pDstConnInfoMap = m_PosConnMap.find(iterDst->second);
                if (pDstConnInfoMap == m_PosConnMap.end())
                {
                    XF_LOG_WARN(0, 0, "find invalid PosConn %d", iterDst->second);
                    m_SvrConnMap.erase(iterDst);
                    continue;
                }
                
                Send2Proxy(pDstConnInfoMap, pCurBuffPos, PkgLen);
            }
            
            return PkgLen;
        }
        else if(Info.GroupType == EFTYPE_SECTION)
        {
            unsigned int SvrID = 0;
            unsigned int Sec = CurHeader.UserID % Info.Section;
            for(unsigned int i = 0; i < Info.ListNum && i < (unsigned int)XY_MAX_GROUP_NUM; i++)
            {
                if(Sec >= Info.ListID[i].Begin && Sec <= Info.ListID[i].End)
                {
                    SvrID = Info.ListID[i].SvrID;
                    break;
                }
            }
            
            if(SvrID == 0)
            {
                XF_LOG_WARN(0, 0, "section %d don't fixed any svr", Sec);
                return PkgLen;
            }
            
            std::map<unsigned int, unsigned int>::iterator iterDst = m_SvrConnMap.find(SvrID);
            if(iterDst == m_SvrConnMap.end())
            {
                XF_LOG_WARN(0, 0, "find invalid SvrID %d", SvrID);
                return PkgLen;
            }
            
            std::map<unsigned int, CConnInfo*>::iterator pDstConnInfoMap = m_PosConnMap.find(iterDst->second);
            if (pDstConnInfoMap == m_PosConnMap.end())
            {
                XF_LOG_WARN(0, 0, "find invalid PosConn %d", iterDst->second);
                m_SvrConnMap.erase(iterDst);
                return PkgLen;
            }
            
            Send2Proxy(pDstConnInfoMap, pCurBuffPos, PkgLen);
            
            return PkgLen;
        }
        else
        {
            XF_LOG_WARN(0, 0, "unknow GroupType %d", Info.GroupType);
            return PkgLen;
        }
    }
    
    return PkgLen;
}

int CRouter::Send2Proxy(std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap, const char *pSendBuff, int SendBuffLen)
{
    unsigned int CurConnPos = pConnInfoMap->first;
    CConnInfo *pCurConnInfo = pConnInfoMap->second;

    int Ret2 = pCurConnInfo->Send(pSendBuff, SendBuffLen);
    if (Ret2 >= 0 && Ret2 < SendBuffLen)
    {
        //发送数据被阻塞了
        int Ret3 = pCurConnInfo->AddSendData(pSendBuff + Ret2, SendBuffLen - Ret2);
        if (Ret3 != 0)
        {
            XF_LOG_WARN(0, 0, "add send data failed|%u", CurConnPos);
            ReleaseConn(pConnInfoMap);
            return -2;
        }

        //添加EPOLLOUT事件
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT;
        ev.data.u32 = CurConnPos;

        Ret3 = epoll_ctl(m_EpollFD, EPOLL_CTL_MOD, pCurConnInfo->GetSockID(), &ev);
        if(Ret3 != 0)
        {
            XF_LOG_WARN(0, 0, "epoll mod add epoll_out failed|%d|%s", errno, strerror(errno));
            //TODO 这里需要考虑后续处理
        }

        XF_LOG_TRACE(0, 0, "Send2Proxy|%d|%d|Add epoll out event", CurConnPos, pCurConnInfo->GetSockID());
    }
    else if (Ret2 < 0)
    {
        //发送数据失败
        XF_LOG_WARN(0, 0, "send data failed|%u", CurConnPos);
        ReleaseConn(pConnInfoMap);
        return -3;
    }
    else
    {
        //发送数据成功
    }

    return 0;
}


int CRouter::Send2ProxyByMsg(std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap, unsigned int DstID, unsigned int CmdID, const google::protobuf::Message &Rsp)
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
    CurHeader.DstID = DstID;
    CurHeader.SrcID = m_ServerID;
    CurHeader.SN = 0;
    CurHeader.Ret = 0;
    CurHeader.Write(acSendBuff);
    
    return Send2Proxy(pConnInfoMap, acSendBuff, HeaderLen + Rsp.ByteSize());
}
