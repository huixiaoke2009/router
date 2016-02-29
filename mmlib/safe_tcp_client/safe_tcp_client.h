#ifndef _SAFE_TCP_CLIENT_H_
#define _SAFE_TCP_CLIENT_H_

#include <vector>
#include "share_mem/share_mem.h"

namespace mmlib
{

const int STCP_CONN_STATUS_SHM_KEY = 0x00210002;    //存放连接状态的共享内存KEY
const int STCP_CONN_STATUS_SHM_SIZE = 16777216;     //存放连接状态的共享内存大小 16MB，可以存放10w节点

const int STCP_CONN_STATUS_OK = 1;      //正常
const int STCP_CONN_STATUS_E_CONN = 2;  //连接失败
const int STCP_CONN_STATUS_E_RECV = 3;  //接收数据失败
const int STCP_CONN_STATUS_E_SEND = 4;  //发送数据失败
const int STCP_CONN_STATUS_E_FULL = 5;  //状态内存已经慢，该节点不在共享内存中

const int STCP_MAX_CONN_STATUS = 2000;  //共享内存中保存的节点，最多允许2000个
const int STCP_MAX_CONN_DATA   = 200;   //单个API，最多允许连接200台服务器

const int STCP_RETRY_TIME_INTER_DEFAULT = 600;      //默认重试时间：10分钟

const int STCP_DEFAULT_TIMEOUT = 5000;       // TCP Client 超时默认值

const int STCP_DEFAULT_PEERTIMEOUT = 180;       // TCP Client 对端超时时间

const int STCP_STATUS_MEM_MAGICNUM = 0x19811103;
const int STCP_STATUS_MEM_VERSION = 0x0100;

typedef struct tagSTcpAddr
{
    unsigned int uiIPAddr;  //IP地址
    unsigned short ushPort; //端口号
}STcpAddr;

// 移植到64位后，这个节点变成136bytes
typedef struct tagSTcpConnStatus
{
    unsigned int uiIPAddr;      //IP地址
    unsigned short ushPort;     //端口号
    unsigned short ushStatus;   //状态 参见：STCP_CONN_STATUS_XX
    unsigned int uiTimeOut;     //超时时间
    time_t tLastRetryTime;      //上次尝试重连的时间
    unsigned int uiRetryTimes;  //重试失败的次数
    unsigned int uiConnErrNum;  //连接失败总次数
    unsigned int uiRecvErrNum;  //接收失败总次数
    unsigned int uiSendErrNum;  //发送失败总次数
    unsigned int uiLastReqNum;  //从上次检查请求数以来的总请求数
    char szNULL[88];            //填充部分
}STcpConnStatus;

// 移植到64位后，这个节点变成136bytes
typedef struct tagSTcpConnStatusHeader
{
    unsigned int uiMagicNum;    //标记，0x19811103
    unsigned short ushVersion;  //版本号，0x0100
    unsigned short ushUNUSE;
    time_t tCreateTime;         //共享内存创建时间
    unsigned int uiConnStatusNum;   //连接状态的有效个数
    unsigned int uiStatusFullNum;   //插入状态时出现FULL的次数
    time_t tLastCheckTime;      //上次检测时间
    char szNULL[104];           //填充部分
    STcpConnStatus astSTcpConnStatus[0];
}STcpConnStatusHeader;

typedef struct tagSTcpConnData
{
    unsigned int uiIPAddr;
    unsigned short ushPort;
    int iSocket;
    time_t tLastUseTime;
}STcpConnData;

class CSafeTcpClient
{
public:
    CSafeTcpClient();
    ~CSafeTcpClient();

    /**
     * @brief 使用配置文件初始化
     * @param pszConfFile 配置文件名称
     * @param pszSecName 需要读取配置文件中的SEC名称
     *
     * @note
     * 配置文件格式：
     * [SAFE_TCP_CONF]
     * ;;主机列表
     * ;;优先采用该方式配置，如果该配置项为空，才会读取HostNum,HostX,PortX选项
     * HostList=172.25.40.8,172.25.40.9
     * ;;HostList对应的端口，只有当HostList不为空时有效
     * Port=2201
     * ;;超时时间，单位：毫秒
     * TimeOut=5000
     * ;;重试间隔，单位：秒
     * RetryInterval=600
     * HostNum=2
     * Host1=172.25.40.8
     * Port1=2202
     * Host2=172.25.40.9
     * Port2=2203
     *
     */
    int Init(const char *pszConfFile, const char *pszSecName = "SAFE_TCP_CONF");

    /**
     * @brief 使用多个IP地址，相同的端口初始化
     * @param pszIPList IP地址列表，多个IP地址采用逗号(,)
     *
     * @note 一般情况下，同一类服务的端口是一致的，所以采用这种方式初始化也比较方便
     */
    int Init(const char * pszIPList, int iPort, int iTimeOut = STCP_DEFAULT_TIMEOUT, int iRetryInterval = STCP_RETRY_TIME_INTER_DEFAULT, int iPeerTimeOut = STCP_DEFAULT_PEERTIMEOUT);

        /**
         * @brief 使用多个IP地址，不同的端口初始化
         * @param vstSTcpAddrList 地址/端口列表
         *
         * @note 一般情况下，同一类服务的端口是一致的，所以采用这种方式初始化也比较方便
         */
        int Init(const std::vector<STcpAddr> &vstSTcpAddrList, int iTimeOut = STCP_DEFAULT_TIMEOUT, int iRetryInterval = STCP_RETRY_TIME_INTER_DEFAULT, int iPeerTimeOut = STCP_DEFAULT_PEERTIMEOUT);

    /**
     * @brief 设置超时时间，单位毫秒
     */
    int SetTimeOut(int iTimeOut);

    /**
     * @brief 发送数据
     * @param pszBuff 需要发送数据的头指针
     * @param uiLen 需要发送数据的长度
     */
    int Send(const void *pszBuff, unsigned int uiLen);

    /**
     * @brief 接收数据
     * @param pszBuff 接收以后存放数据Buff的头指针
     * @param puiLen （输入输出参数）输入时，指定接收数据Buff的长度；输出时，表示接收到的数据的长度
     * @param uiExpectLen 期望的长度，如果达不到期望的长度，会等待继续接收，直到超时，当为0时，表示返回第一次read的长度
     */
    int Recv(void *pszBuff, unsigned int *puiLen, unsigned int uiExpectLen = 0);

    /**
     * @brief 发送并接收数据
     * @param pszSendBuff 需要发送数据的头指针
     * @param uiSendLen 需要发送数据的长度
     * @param pszRecvBuff 接收以后存放数据Buff的头指针
     * @param puiRecvLen （输入输出参数）输入时，指定接收数据Buff的长度；输出时，表示接收到的数据的长度
     * @param uiExpectLen 期望的长度，如果达不到期望的长度，会等待继续接收，直到超时，当为0时，表示返回第一次read的长度
     */
    int SendAndRecv(const void * pszSendBuff, unsigned int uiSendLen, void * pszRecvBuff, unsigned int *puiRecvLen, unsigned int uiExpectLen = 0);

    /**
     * @brief 关闭当前连接，状态相关共享内存等不操作
     * @note 只是关闭最近一次操作过的连接，关闭连接不影响状态信息
     *       一般用于长连接时，收到一些非法的数据，为了防止对后面的数据造成影响，关闭当前的连接
     */
    void CloseCurConn();

    /**
     * @brief 关闭所有连接，状态相关共享内存等不操作
     * @note 只是关闭该API初始化过的连接，关闭连接不影响状态信息
     */
    void CloseAllConn();

    /**
     * @brief 关闭所有连接，并且Detach共享内存
     * @note 只是关闭该API初始化过的连接，关闭连接不影响状态信息
     */
    void Destory();

    /**
     * @brief 获取所有连接状态
     * @param stSTcpConnStatusHeader 状态头部的一些信息，注意uiConnStatusNum和astSTcpConnStatus是无效的
     * @note 所有该主机上的连接，包括不是该API使用的连接
     *       调用该接口可以不初始化
     */
    int GetAllStatus(STcpConnStatusHeader &stSTcpConnStatusHeader, std::vector<STcpConnStatus> &vstSTcpConnStatus);

    /**
     * @brief 获取该API创建的各个连接状态
     */
    int GetMyStatus(std::vector<STcpConnStatus> &vstSTcpConnStatus);

    /**
     * @brief 设置某个IP/端口对应的状态
     * @note 调用该接口可以不初始化
     */
    int SetConnStatus(const STcpConnStatus &stSTcpConnStatus);

    /**
     * @brief 删除某个IP/端口对应的状态
     * @note 调用该接口可以不初始化
     */
    int DelConnStatus(const STcpConnStatus &stSTcpConnStatus);

    /**
     * @brief 获取错误信息
     */
    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }

private:
    int InitStatusShm();
    STcpConnStatus *GetSTcpConnStatus(unsigned int uiIPAddr, unsigned short ushPort);
    STcpConnStatus *AddSTcpConnStatus(unsigned int uiIPAddr, unsigned short ushPort);
    int DelConnStatusInternal(unsigned int uiIP, unsigned short ushPort);
    void SetSTcpConnStatusInternal(unsigned short ushConnStatus);

    int PollWait(int iFd, short shEvent, int iTimeOut);
    int RecvWait(int iFd, int iTimeOut);
    int SendWait(int iFd, int iTimeOut);

    int ConnectInternal(int iTestConnFlag = 0);
    int GetConnection();

    int SendInternal(const void *pszBuff, unsigned int uiLen);
    int RecvInternal(void *pszBuff, unsigned int *puiLen, unsigned int uiExpectLen);

private:
    STcpConnData m_astSTcpConnData[STCP_MAX_CONN_DATA];
    int m_iSTcpConnDataNum; //存储了STcp连接数据的个数
    int m_iValidConnNum;    //有效的连接数
    int m_iTimeOut;
    char m_szErrMsg[1024];

    char *m_pStatusMem;
    STcpConnStatusHeader *m_pstSTcpConnStatus;
    STcpConnStatus *m_pstCurConnStatus;
    STcpConnData *m_pstCurConnData;

    int m_iRetryInterval;
    int m_iPeerTimeOut;

    CShareMem m_objStatusMem;

    STcpConnStatus m_stFullConnStatus;  //如果共享内存中的状态节点满了，将会使用该节点参与逻辑处理

};

}
#endif
