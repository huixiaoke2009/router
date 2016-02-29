
#ifndef _ROUTER_PROC_H_
#define _ROUTER_PROC_H_

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <map>
#include <string.h>
#include <vector>
#include <google/protobuf/message.h>
#include "router.pb.h"
#include "common.h"

class CConnInfo
{
    public:
        CConnInfo(unsigned int ConnPos, int SockID, const struct sockaddr_in *pRemoteAddr, const struct sockaddr_in *pLocalAddr);
        ~CConnInfo();

        // 接收数据
        int Recv(char *pBuff, unsigned int *pLen);

        // 发送数据
        int Send(const char *pBuff, unsigned int Len);

        // 缓存接收到的数据
        int AddRecvData(const char *pBuff, unsigned int Len);

        //缓存需要发送的数据
        int AddSendData(const char *pBuff, unsigned int Len);

        //发送缓存的数据
        int SendRemainData();

        // 获得上次活跃时间
        time_t GetLastActTime() {return m_LastActTime;}

        // 客户端地址
        unsigned int RemoteAddr() { return m_RemoteAddr.sin_addr.s_addr; }
        const char *RemoteAddrStr() {return inet_ntoa(m_RemoteAddr.sin_addr);}
        unsigned short RemotePort() {return ntohs(m_RemoteAddr.sin_port);}

        //本机地址
        unsigned int LocalAddr() { return m_LocalAddr.sin_addr.s_addr; }
        const char *LocalAddrStr() {return inet_ntoa(m_LocalAddr.sin_addr);}
        unsigned short LocalPort() {return ntohs(m_LocalAddr.sin_port);}

        // 获取SockID
        int GetSockID() {return m_SockID;}

        //设置服务器ID
        void SetServerID(int ServerID){m_ServerID = ServerID;}

        //获取服务器ID
        int GetServerID(){return m_ServerID;}

        //获取连接编号
        unsigned int GetConnPos(){return m_ConnPos;}

    protected:
        // 客户端唯一ID
        unsigned int m_ConnPos;

        // 套接字
        int m_SockID;

        // 地址
        struct sockaddr_in m_RemoteAddr;
        struct sockaddr_in m_LocalAddr;

        // 上次活动时间
        time_t m_LastActTime;

        // 暂存数据的buff
        std::string m_RemainSendData;
        std::string m_RemainRecvData;

        unsigned int m_ServerID;
};

enum
{
    EFTYPE_RANDOM = 0,   // 随机转发
    EFTYPE_ALL = 1,      // 转发给所有
    EFTYPE_SECTION = 2,  // 按号段转发
    EFTYPE_END,    
};

typedef struct tagGroupItem
{
    unsigned int SvrID; // 服务器id
    unsigned int Begin; // 开始号段
    unsigned int End;   // 结束号段

    tagGroupItem()
    {
        memset(this, 0x0, sizeof(tagGroupItem));
    }
}GroupItem;

typedef struct tagGroupInfo
{
    unsigned int GroupID;    // 组id
    unsigned int GroupType;  // 组类型
    unsigned int ListNum;    // 该组服务器数量
    unsigned int Section;    // 号段因子，只有组类型为按号段转发才有用
    GroupItem ListID[XY_MAX_GROUP_NUM];
    
    tagGroupInfo()
    {
        GroupID = 0;
        GroupType = 0;
        ListNum = 0;
        Section = 0;
    }
}GroupInfo;


class CRouter
{
    public:
        CRouter();
        ~CRouter();

        int Init(const char *pConfFile);
        int Run();
        int GetServerID(){return m_ServerID;}

    private:
        int AcceptConn(int ConnPos);
        void CheckConn();
        void ReleaseConn(std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap);


        int ProcessPkg(const char *pCurBuffPos, int RecvLen, std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap);
        int Send2Proxy(std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap, const char *pSendBuff, int SendBuffLen);
        int Send2ProxyByMsg(std::map<unsigned int, CConnInfo*>::iterator &pConnInfoMap, unsigned int DstID, unsigned int CmdID, const google::protobuf::Message &Rsp);

    private:
        // EPOLL句柄
        int m_EpollFD;
        int m_ServerID;
        // 生成cpos的计数
        unsigned int m_ConnPosCnt;

        std::map<unsigned int, CConnInfo*> m_PosConnMap;
        std::map<unsigned int, CConnInfo*>::iterator m_itrCurCheckConn;

        std::map<unsigned int, unsigned int> m_SvrConnMap;
        std::map<unsigned int, GroupInfo> m_GrpSvrMap;
        
        // 连接超时时间(指定时间内没有数据包,将视为超时)单位:秒
        int m_TimeOut;

        //接收缓冲区
        char *m_pProcessBuff;
};


#endif
