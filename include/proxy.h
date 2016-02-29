/**
    *note: m_ProxyReqQueue过来的数据必须封好了包头
           m_ProxyRspQueue发出去的数据已经去掉包头
*/


#ifndef _PROC_H_
#define _PROC_H_

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <map>
#include <string.h>
#include <vector>
#include <google/protobuf/message.h>
#include "router.pb.h"
#include "shm_queue/shm_queue.h"
#include "common.h"
#include "log/log.h"
#include "util/util.h"

typedef struct tagRouterInfo
{
    unsigned int RouterIP;
    short RouterPort;
    unsigned int RouterSvrID;
    int SocketID;
    
    std::string strRecieveBuff;
    
    tagRouterInfo()
    {
        RouterIP = 0;
        RouterPort = 0;
        RouterSvrID = 0;
        SocketID = -1;
    }
        
        
    int Recv(char* pRecvBuff, unsigned int* pLen)
    {
        char *pRecvPtr = pRecvBuff;
        unsigned int RecvBuffLen = *pLen;
        
        if (!strRecieveBuff.empty())
        {
            if (strRecieveBuff.length() > (unsigned int)XY_MAXBUFF_LEN)
            {
                XF_LOG_WARN(0, 0, "recv remain len %lu", strRecieveBuff.length());
                return -1;
            }

            pRecvPtr += strRecieveBuff.length();
            RecvBuffLen -= strRecieveBuff.length();
            XF_LOG_DEBUG(0, 0, "%s|with recv remain len %lu", __FUNCTION__, (unsigned long)strRecieveBuff.length());
        }

        int RecvBytes = read(SocketID, pRecvPtr, RecvBuffLen);
        if (RecvBytes > 0)
        {
            XF_LOG_TRACE(0, 0, "recv|%d|%s", RecvBytes, mmlib::CStrTool::Str2Hex(pRecvPtr, RecvBytes));

            *pLen = RecvBytes;
            if (!strRecieveBuff.empty())
            {
                *pLen += strRecieveBuff.length();
                memcpy(pRecvBuff, strRecieveBuff.data(), strRecieveBuff.length());
                strRecieveBuff.clear();
            }

            return 0;
        }
        else if (RecvBytes == 0)
        {
            //连接被终止
            XF_LOG_INFO(0, 0, "conn close|%d", RouterSvrID);
            return 1;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            //相当于没有接收到数据
            *pLen = 0;
            if (!strRecieveBuff.empty())
            {
                *pLen += strRecieveBuff.length();
                memcpy(pRecvBuff, strRecieveBuff.c_str(), strRecieveBuff.length());
                strRecieveBuff.clear();
            }

            return 0;
        }
        else
        {
            XF_LOG_WARN(0, 0, "recv failed, ret=%d, errno=%d, errmsg=%s", RecvBytes, errno, strerror(errno));
            return -1;
        }
        
        return 0;
    }

    int AddRecvData(const char *pBuff, unsigned int Len)
    {
        XF_LOG_DEBUG(0, 0, "%s|%lu|%d", __FUNCTION__, (unsigned long)strRecieveBuff.length(), Len);

        if (!strRecieveBuff.empty())
        {
            if (strRecieveBuff.length() + Len > (unsigned int)XY_MAXBUFF_LEN)
            {
                XF_LOG_WARN(0, 0, "%s|add recv failed, len=%lu, add_len=%d", __FUNCTION__, (unsigned long)strRecieveBuff.length(), Len);
                return -1;
            }

            //TODO 错误处理
            strRecieveBuff.append(pBuff, Len);
            XF_LOG_DEBUG(0, 0, "%s|after add string|%lu", __FUNCTION__, (unsigned long)strRecieveBuff.length());
        }
        else
        {
            if (Len > (unsigned int)XY_MAXBUFF_LEN)
            {
                XF_LOG_WARN(0, 0, "%s|add recv failed, add_len=%d", __FUNCTION__, Len);
                return -1;
            }

            //TODO 错误处理
            strRecieveBuff.assign(pBuff, Len);
            XF_LOG_DEBUG(0, 0, "%s|after new string|%lu", __FUNCTION__, (unsigned long)strRecieveBuff.length());
        }

        return 0;
    }

    
}RouterInfo;

class CProxy
{
    public:
        CProxy();
        ~CProxy();

        int Init(const char *pConfFile);
        int EventLoop(char* pLoopBuff, unsigned int& LoopLen);
        int Forward2Router(char *pSendBuff, int SendBuffLen, int SvrID = 0);
        int GetServerID(){return m_ServerID;}

    private:
        int ConnectRouter(unsigned int SvrID);
        int DisconnetRouter(unsigned int SvrID);
        void CheckConnect();
   
        int Send(unsigned int SvrID, const char *pSendBuff, int SendBuffLen);
        int Recv(unsigned int SvrID, char* pRecvBuff, unsigned int* pLen);
        int AddRecvData(unsigned int SvrID, const char *pBuff, unsigned int Len);
        
        int Send2RouterByMsg(unsigned int SvrID, unsigned int CmdID, const google::protobuf::Message &Rsp);
        int SendHeartbeatMsg(unsigned int SvrID);
        int SendRegisterMsg(unsigned int SvrID);
    
        int ProcessPkg(unsigned int SvrID, const char* pCurBuffPos, int RecvLen, char* pLoopBuff, unsigned int& LoopLen);
    private:
        int m_EpollFD;
        int m_ServerID;
        time_t m_LastTime;
        
        //接收缓冲区
        char *m_pProcessBuff;
        int m_RouterNum;
        RouterInfo m_RouterInfo[XY_MAX_ROUTER_NUM];
        
        mmlib::CShmQueue m_ProxyReqQueue;
        mmlib::CShmQueue m_ProxyRspQueue;
};


#endif
